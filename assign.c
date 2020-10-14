//  Program to assign ICARUS shifts in response to a questionnaire.

// This version is for the 2020 period, September 2020 through January 2021
  
/* This version has two running modes:  
     If it is run without a command line argument, it will test 1,000,000 random 
       seeds and output just the seed index numbers for the best candidates 
       based on minimizing the difference between the institutional quotas and 
       the institutional assignments 
     If it is run with a seed index as the command line argument, it will give
       a full output for that case. */ 

/* The program requires 4 files:
     Pri.cvs    A file with the individual special priorities
     Inst.cvs   A file with the institutional quotas
     Shift.cvs  A file with the point values of each shift
     Ind.cvs    A file with the individual requests from the questionnaire */

               /* Dependency Table

main
  prepareRandomSeeds        // prepare 1,000,000 seeds
  initialization
  parseInstFile             // input institution file
    clearBuffer             // clears temporary buffer                         
    readBuffer              // reads an entry from the buffer   
  parseShiftFile            // input shift file
    clearBuffer             // clears temporary buffer                         
    readBuffer              // reads an entry from the buffer     
  parsePriFile              // input priority file
    clearBuffer             // clears temporary buffer                
    readBuffer              // reads an entry from the buffer            
  parseIndFile              // input shifter file from the questionnaire
    clearBuffer             // clears temporary buffer
    readBuffer              // reads an entry from the buffer
    randP                   // returns a random priority    
  algorithm                 // run the assignment algorithm   
    prepareShifts           // finds requester info for all open shifts
      qualified             // determines if a shifter is qualified 
      dumpPreparedShifts    // debugging tool; not normally called
    findNextShift           // picks next shift to be filled
    findConsecShift         // deals with consecutive shift requests
      prepareConsecutive    // makes decisons on consecutive shift requests
      assignShift           // does the paper work
        killInst            // removes institutional priority
        cautionInst         // sets caution flag 
    assignShift             // does the paper work
      killInst              // removes institutional priority
      cautionInst           // sets caution flag  
    getNewRandPri           // generates new random priorities
      randP                 // returns a random priority 
  switchLoP                 // switch active file to LoP-2                  
  donationTime              // wealthy groups donate to the poor ones
    findDonors              // sets donor list and priorities
    findNextDonor           // finds the next potential donor from the list
    findReceiver            // finds a receiver
      assignDonorShift      // does the paper work
        killInst            // removes institutional priority
        cautionInst         // sets a caution flag
        findDonors          // resets donor list and priorities          
  shiftTable                // print shift table and ECL input file
  shifterTable              // print shifter table                      
  institutionTable          // print institution table
    deficiencyReport        // prints deficiency reports online
    report                  // calculate final metrics  
  report                    // calculate final metrics           */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define  NSHIFTS 156        // number of shifts this period
#define  NSHIFTS 90        // number of shifts this period

// codes for base priority 

#define N 1.0            // normal or no priority 
#define L 1.5            // low priority (teaching 1 term) 
#define M 2.0            // medium priority (having 1 month free) 
#define H 2.5            // high priority (teaching 2 terms) 
#define X 3.0            // extraordinary priority (need specific shifts) 
#define VIRGIN 0.3       /* virginity bonus -- it disappears after the 1st
                            shift is assigned to an individual  */
#define EXTRA_V 0.5      /* this is an extra virginity bonus for individuals
                            who were shut out of the previous period */
#define NO_OVERAGE 1     /* whether or not going over the requested number of
                            points for a multi-point shift is allowed */
#define OVERAGE_ALLOWED 2    
#define YES 1            // standard Qualtrics codes 
#define NO 2
#define SHORT_REST 1     // minimum gap between consecutive shifts 
#define LONG_REST 2
#define STRICT 2         // consecutive shift required 
#define NOT_STRICT 1     /* these 2 are backwards from the usual convention
                            sorry about that */

typedef enum {false,true} bool;

int directedShifts[10] = {-1};  /* directed shifts are shifts that need
                                   to be run first to be filled */
bool defRep = false;     // output deficiency reports 
bool  verbose = false;   /* It will be set to false for looping over random 
                             seed */
int nDumpShift = 0;     // number of prepared shifts to be dumped for debug 
int tDumpShift = 0;     // number of times prepared shifts will be dumped  
char buffer[1024];      // general purpose buffer for temporary file storage 

bool lop1 = true;       // flag to indicate which LoP is active 
int donorShift;         /* global parameter to simplify communication in the 
                           donation section */
bool noMultiPoint = false;   /* global parameter to simplify communication in 
                             // consecutive shift section */
FILE *fl;               //  pointer to the log

// total quantities

int totShifters;
int totShifts;
int totPoints;
int totRequests;
int totQuotas; 

struct individual {
  char name[80];
  char ECLID[80];
  char email[80];
  int home;                // institution number  
  char homeName[20];       // institution short name  
  int request;             // number of shift points requested  
  int over;                // 1 => do not exceed request; 2 => ok to exceed  
  int special;             // 1 => yes; 2 => no  
  char just[80];           // justification for special  
  int consec;              // 1 = consecutive shifts needed; 2 = not needed  
  int rest;                // 1 8 hours ok; 2 need 16  
  int strict;              // 2 => abandon if not consecutive; 1 => keep  
  int nonConsec;           /* 1 => no break; 2 => 1 shift; 3 => 2 shifts; 
                              4 => 4 shifts */
  bool lop1[NSHIFTS];      // 1 => request; 0 => not requested  
  bool lop2[NSHIFTS];
  bool active[NSHIFTS];    // the active lop values  
  int nLoP1;               // number of LoP1 requests  
  int nLoP2;               // numbr of LoP2 requests  
  float basePri;           // base priority  
  float virginPri;         // virgin priority  
  float bonusPri;          // bonus priority  
  float randPri;           // random priority  
  float totPri;            // total priority sum of the 3 above   
  int nPAssigned;          // shift points assigned  
  int nSAssigned;          // shifts assigned  
  int assigned[20];        // shift numbers of assigned shifts  
  bool open;               // 1 = open; 0 = closed (i.e. points assigned)
  bool caution;            // 1 => institution is within 1 of its quota  
} ind[250];    
  int nInd = -1;           // number of individuals  

struct institution {
  char name[20];
  int quota;
  int nPRequested;         // number of points requested by individuals  
  int nPAssigned;          // number of points assigned  
} inst[50];      
  int nInst = 0;           // number of institutions  

struct shifts {
  bool open;               // 1 = open; 0 = closed  
  char date[8];
  char type[8];
  enum {night, day, swing} stype;
  char ECLType[16];        // for ECL input, e.g. Weekend Night  
  char ECLDate[12];        // for ECL input, e.g. 2016-10-03  
  int points;              // number of shift points   
  int nRequests;           // number of requesters  
  int requesters[100];     // requester numbers  
  int topRequester;        // highest priority requester  
  int assigned;            // assigned requester  
  int donPri;              // donation priority   
} shift[NSHIFTS];
int nShift = -1;           // index of shifts {0...(NSHIFTS -1)}  

// Global struct for base priorities  

struct priority {
  char ECLID[80];
  float basePri;         // base priority  
} pri[250];
int nPri = -1;

// Globals for readBuffer  

typedef enum {INTEGER,STRING} typeCalledFor;
int bIndex;         // current buffer location  
int nChar;          // number of buffer bytes for current field  
int iValue;         // integer return  
char sValue[80];    // string return

int nextShift;  // global for findNextShift  

// Global struct for consecutive shift finding */

const int conIndex[3][6] = {  // list of possiblee consecutive shift   
  {-3,3,4,5,-2,0},      // distances with a minimum of 16 hours rest   
  {-3,3,-4,4,-2,2},     // for the 4 and 8 hours rest for the 5th and or 6th  
  {-3,3,-4,-5,2,0}      // The first index is the type of shift  
};
struct consecutive {
  int nCand;            // number of candidate shifts  
  int shift[6];         // possible consecutive shifts  
  bool requested[6];    // shift has been requested  
  bool open[6];         // shift is not assigned  
  int occupant[6];      // if shift is not open, the occupant  
  int occOpen[6];       /* possible trade shift -- must be same type 
                           and points */
  int decision;         // 0 = not possible; 1 = open shift; 2 = trade  
  int conShift;         // consecutive shift assigned  
  int tradeShift;       // trade shift assigned  
} con;

/*
Here is the plan:

(1) read in each of the 4 files and initiate all of the global variables and
    structs.
(2) start loop: start with fewest requisted, ealiest shifts.  search for 
    highest priority shifter. Assign shift and clean up
(3) switch to lop-2 and repeat
(4) see if donations from wealthy groups to poor groups is possible
(5) output assignments and statistics
 */          

/**************************************************************************/
void initialization() {

  lop1 = true;         // start with lop1 
  nInd = -1;           // initialize variables to be read in 
  nInst = 0;
  nShift = -1;
  nPri = -1;  

  return;
}

/*************************************************************************/
float randP() {          /* random number generator returns a number between 0
                           and 0.1.  It looks more complex than it needs to 
                           be, but the properties of just using rand were
                           not good.  It is not flat, but it does not have to
                           be for this application. */

  return ((rand() & 1000)/10000.0)*((rand() & 1000)/1000.0)*
                                   ((rand() & 1000)/1000.0);
}


/*************************************************************************/ 
void readBuffer(typeCalledFor typeRB) {

  /* This routine reads the buffer by looking for commas in buffer.
     The buffer index is bIndex.  The number of bytes is nChar.
     Output is iValue or sValue.  No value is returned if nChar = 0. */ 

  int index0 = bIndex++;  // index0 is at the previous comma 
  while (buffer[bIndex] != ',')  bIndex++; 
                          // bIndex is at the comma terminating the field 
  nChar = bIndex - index0 - 1; 
  if (nChar == 0) return;
  if (typeRB == INTEGER){
    buffer[bIndex] = '\0';       // make it a string 
    const char *a = &buffer[bIndex-nChar];
    iValue = atoi(a);
  }
  else {                         // string case 
    if (nChar > 79) nChar = 79;  // protection  
    int n = 1 - nChar;           // location of first byte to be copied 
    for (; n <= 0; n++) 
      sValue[n - 1 + nChar] = buffer[bIndex + n - 1];
    sValue[nChar] = '\0';        // make it a string 
  }
  return;
}

/*************************************************************************/
void clearBuffer(int n) {  /* This routine should not be necessary, but
                              it is -- Excel does strange things. */
  for (int i = 0; i < n; i++) buffer[i] = ' ';
  return; 
}    

/**************************************************************************/
void parsePriFile() {

  /* This routine uses a specially prepared .csv file which has only two      
   fields, the shifter's ECLID and the assigned priority code. 

   Priority codes are N, L, M, H, and X */

  int bytesRead = 0;
  int c;

  FILE *fp;
  fp = fopen("Pri.csv","r");

  // Read in the data one line at a time  

  while (true){
    clearBuffer(20);
    for (bytesRead = 0; (c = getc(fp)) && (c != (int)'\r') && (c != EOF); 
       bytesRead++) buffer[bytesRead] = (char)c; 
    // I'm not sure why, but the () in the above for command are necessary
    bytesRead++;                // make room for the final comma 
    buffer[bytesRead] = ',';    // insert final comma 
    nPri++;

    // Start filling the priorites struct. ECLID first

    bIndex = -1;     // This is the location of the virtual preceeding comma 
    readBuffer(STRING);
    strcpy (pri[nPri].ECLID, sValue);
    readBuffer (STRING);
    switch (sValue[0]) {
    case 'N' : pri[nPri].basePri = N; break;
    case 'L' : pri[nPri].basePri = L; break;
    case 'M' : pri[nPri].basePri = M; break;
    case 'H' : pri[nPri].basePri = H; break;
    case 'X' : pri[nPri].basePri = X; break;
    default  : printf ("\nPriority code %s not recognized for %s.\n",
		       sValue[0], pri[nPri].ECLID);
      exit(1);
    }
    if (c == EOF) break;
  }
  fclose(fp);
  return;
}
  

/*************************************************************************/
void parseInstFile() {

  /* This routine uses a specially prepared .csv file which has only two
fields, the institution name and the institution quota.

 The quota is rounded to an integer to make the sum of the quotas equal to the
total number of shift points for the shift period.   
The institution number is the index. */

  totQuotas = 0;
  int bytesRead = 0;
  int c;

  FILE *fp;
  fp = fopen("Inst.csv","r");

  // Read in the questionnaire data one line at a time  

  while (true){
    clearBuffer(20);
    for (bytesRead = 0; (c = getc(fp)) && (c != (int)'\r') && (c != EOF);
	 bytesRead++) buffer[bytesRead] = (char)c;
    // I'm not sure why, but the () in the above for command are necessary  
    bytesRead++;                // make room for the final comma        
    buffer[bytesRead] = ',';    // insert final comma 
    nInst++;

    // Start filling the institutions struct. Institution shorname first

    bIndex = -1;     // This is the location of the virtual preceeding comma 
    readBuffer(STRING);
    strcpy (inst[nInst].name, sValue);

    readBuffer (INTEGER);
    inst[nInst].quota = iValue;
    inst[nInst].nPAssigned = 0;
    inst[nInst].nPRequested = 0;
    totQuotas += iValue; 
  
    if (c == EOF) break;    
  }
  fclose(fp);
}

/*************************************************************************/
void parseShiftFile()
{

  /* This routine uses a specially prepared .csv file which has six fields,
two string fields giving the date and shift, the shift expressed as an 
integer, the number of shift points, the shift type in ECL input form, and 
the shift date in ECL input form.  The shift number begins with zero. */

  totPoints = 0;
  totShifts = 0;
  int bytesRead = 0;
  int c;

  FILE *fp;
  fp = fopen("Shift.csv","r");

  // Read in the data one line at a time  

  while (true){
    clearBuffer(20);
    for (bytesRead = 0; (c = getc(fp)) && (c != (int)'\r') && (c != EOF);
	 bytesRead++) buffer[bytesRead] = (char)c;
    // I'm not sure why, but the () in the above for command are necessary 
    bytesRead++;                // make room for the final comma    
    buffer[bytesRead] = ',';    // insert final comma 
    nShift++; 

    /* Start filling the shifts struct. Date and shift first*/

    bIndex = -1;   /* This is the location of the virtual preceeding comma */
    readBuffer(STRING);
    strcpy (shift[nShift].date, sValue);

    readBuffer(STRING);
    strcpy (shift[nShift].type, sValue);

    /* Now the two integer fields */

    readBuffer (INTEGER);
    shift[nShift].stype = iValue;
    readBuffer (INTEGER);
    shift[nShift].points = iValue;
    totPoints += iValue;
    if (iValue > 0) totShifts++;
    readBuffer (STRING);
    strcpy (shift[nShift].ECLType, sValue);
    readBuffer (STRING);
    strcpy (shift[nShift].ECLDate, sValue);
    sValue[10] = '\0';                  // the ECLDate must be exactly 10 characters

    shift[nShift].open = true;          /* open the shift */
    shift[nShift].assigned = -1;        /* no assignment yet */

    if (c == EOF) break; 
  }
  fclose(fp);
  if (totShifts != NSHIFTS) {
    printf("totShifts = %d != NSHIFTS, exitiing\n",totShifts);
    exit(0);
  }

}

/*************************************************************************/
void dumpIndividual(int ii) { /* dumps the individual struct for debugging */

    printf("name = %s\n",ind[ii].name);
    printf("ECLID = %s\n",ind[ii].ECLID);
    printf("email = %s\n",ind[ii].email);
    printf("home = %d\n",ind[ii].home);
    printf("homeName = %s\n",ind[ii].homeName);
    printf("request = %d\n",ind[ii].request);
    printf("over = %d\n",ind[ii].over);
    printf("consec = %d\n",ind[ii].consec);
    printf("rest = %d\n",ind[ii].rest);
    printf("strict = %d\n",ind[ii].strict);
    printf("nonConsec = %d\n",ind[ii].nonConsec);
    printf("basePri = %f\n",ind[ii].basePri);
    printf("virginPri = %f\n",ind[ii].virginPri);
    printf("bonusPri = %f\n",ind[ii].bonusPri);
    printf("randPri = %f\n",ind[ii].randPri);
    printf("totPri = %f\n",ind[ii].totPri);
    printf("nPAssigned = %d\n", ind[ii].nPAssigned);
    printf("nSAssigned = %d\n", ind[ii].nSAssigned);
    printf("open = %d\n", ind[ii].open);

    fprintf(fl,"name = %s\n",ind[ii].name);
    fprintf(fl,"ECLID = %s\n",ind[ii].ECLID);
    fprintf(fl,"email = %s\n",ind[ii].email);
    fprintf(fl,"home = %d\n",ind[ii].home);
    fprintf(fl,"homeName = %s\n",ind[ii].homeName);
    fprintf(fl,"request = %d\n",ind[ii].request);
    fprintf(fl,"over = %d\n",ind[ii].over);
    fprintf(fl,"consec = %d\n",ind[ii].consec);
    fprintf(fl,"rest = %d\n",ind[ii].rest);
    fprintf(fl,"strict = %d\n",ind[ii].strict);
    fprintf(fl,"nonConsec = %d\n",ind[ii].nonConsec);
    fprintf(fl,"basePri = %f\n",ind[ii].basePri);
    fprintf(fl,"virginPri = %f\n",ind[ii].virginPri);
    fprintf(fl,"bonusPri = %f\n",ind[ii].bonusPri);
    fprintf(fl,"randPri = %f\n",ind[ii].randPri);
    fprintf(fl,"totPri = %f\n",ind[ii].totPri);
    fprintf(fl,"nPAssigned = %d\n", ind[ii].nPAssigned);
    fprintf(fl,"nSAssigned = %d\n", ind[ii].nSAssigned);
    fprintf(fl,"open = %d\n", ind[ii].open);

}
/*************************************************************************/
void dumpInstitution(int ii) { /* dumps the institution struct for debugging */

  printf("name = %s\n", inst[ii].name);
  printf("quota = %d\n", inst[ii].quota);

  fprintf(fl,"name = %s\n",inst[ii].name);
  fprintf(fl,"quota = %d\n", inst[ii].quota);
}
/*************************************************************************/
void dumpShift(int ii) { /* dumps the shift struct for debugging */

  printf("date = %s\n",shift[ii].date);
  printf("type = %s\n",shift[ii].type);
  printf("stype = %d\n",shift[ii].stype);
  printf("points = %d\n",shift[ii].points);

  fprintf(fl,"date = %s\n",shift[ii].date);
  fprintf(fl,"type = %s\n",shift[ii].type);
  fprintf(fl,"stype = %d\n",shift[ii].stype);
  fprintf(fl,"points = %d\n",shift[ii].points);
}

/*************************************************************************/
void parseIndFile() {

  /* This routine uses a specially prepared .csv file which has only the 
     answers to questions. */ 

  totShifters = 0;
  totRequests = 0;
  int bytesRead = 0; 
  int c;

  FILE *fp;
  fp = fopen("Ind.csv","r");
  if (fp == NULL) printf("Null pointer retruned\n");

  if (verbose) {
    printf("\nShifter List:\n");
    fprintf(fl, "\nShifter List:");
  }

  // Read in the questionnaire data one line at a time  

  while (true){
    clearBuffer(20);
    for (bytesRead = 0; (c = getc(fp)) && (c != (int)'\r') && (c != EOF);
	 bytesRead++) buffer[bytesRead] = (char)c;
    // I'm not sure why, but the () in the above for command are necessary  
    bytesRead++;                // make room for the final comma         
    buffer[bytesRead] = ',';    // insert final comma    
    nInd++;
    
    // Start filling the individual struct  

    bIndex = -1;     // This is the location of the virtual preceeding comma  
    // readBuffer(STRING); // Q1 has no useful information --- ignore  
    
    // Next 3 are strings  
 
    readBuffer(STRING);
    strcpy (ind[nInd].name, sValue);   // Q2 name  
    readBuffer(STRING);    
    strcpy (ind[nInd].ECLID, sValue);  // Q3 ECLID  
    readBuffer(STRING);
    strcpy (ind[nInd].email, sValue);  // Q4 email  

    // The next batch are integers with a couple of strings 
    
    readBuffer(INTEGER);
    ind[nInd].home = iValue;           // Q5 institution  
    int home = iValue;     // save for later index  
    strcpy(ind[nInd].homeName, inst[iValue].name); // put in the short name  
    readBuffer(INTEGER);
    ind[nInd].request = iValue;        // Q6 requested number of points
    totRequests += iValue;
    if (iValue > 0) totShifters++;  
    inst[home].nPRequested += iValue;  // used in the Institution Table 
    readBuffer(STRING);                /* Q7 This is the justification for 
                                          requesting more than 7 shifts.  The
                                          requested number of shifts will be
                                          inserted by hand. The justification
                                          will not be saved*/
    readBuffer(INTEGER);
    ind[nInd].over = iValue;           /* Q8 whether overage is allowed for
                                          multi-point shifts */
    readBuffer(INTEGER);               // Q9 require consecutive shifts 
    ind[nInd].consec = (nChar == 0) ? NO : iValue;  // defaults for no answer
    readBuffer(INTEGER);               // Q10 rest between consecutive shifts 
    ind[nInd].rest = (nChar == 0) ? LONG_REST : iValue;
    readBuffer(INTEGER);               // Q11 is consecutive request strict? 
    ind[nInd].strict = (nChar == 0) ? STRICT : iValue;

    /* Q12 and Q13 will be split into one field in the individual
       struct:  The first answer == 2 => nonConsec = 1 (This is also the 
       default for no answer.) If the first answer == 1, then nonConsec = the 
       second answer + 1. */

    int nonConsec;                // Stores the value of the 1st question  
    readBuffer(INTEGER);
    nonConsec = ind[nInd].nonConsec = (nChar == 0) ? 2 : iValue;
    readBuffer(INTEGER);             // No answer does not matter  
    ind[nInd].nonConsec = (nonConsec == 2) ? 1 : iValue + 1;
    readBuffer(INTEGER);             // Q14 extra virginity request.  
    ind[nInd].virginPri = VIRGIN;
    if (nChar != 0 && iValue == YES) ind[nInd].virginPri += EXTRA_V; 
    readBuffer(INTEGER);             // Q15 request for priority  
    ind[nInd].special = (nChar == 0) ? NO : iValue;
    readBuffer(STRING);              
    strcpy (ind[nInd].just, sValue); // Q16 justification for priority  
    
    /* Q17 and Q18: Now LoP-1 and LoP-2 read in; 
       LoP 2 is also loaded with LoP 1 so that zero priority shifters can get 
       shifts in LoP-2 */

    ind[nInd].nLoP1 = 0;
    ind[nInd].nLoP2 = 0;
    for (int n = 0; n < NSHIFTS; n++) {
      readBuffer(INTEGER);
      ind[nInd].lop1[n] = (nChar == 0) ? false : true;
      ind[nInd].active[n] = ind[nInd].lop1[n];  // Load active shift array */
      if (ind[nInd].lop1[n]) ind[nInd].nLoP1++;// count number */
    }
    for (int n = 0; n < NSHIFTS; n++) {
      readBuffer(INTEGER);
      ind[nInd].lop2[n] = (nChar == 0) ? false : true;
      if (ind[nInd].lop2[n]) ind[nInd].nLoP2++;// count number */
      ind[nInd].lop2[n] |= ind[nInd].lop1[n];  // load lop1 into lop2 */
    }
    readBuffer(STRING);  /* Q19 had no useful information.  However, leave it 
                            here to avoid an Excel problem */

    // Zero dynamic entires */

    ind[nInd].nPAssigned = 0;
    ind[nInd].nSAssigned = 0;
    ind[nInd].open = true;
    ind[nInd].caution = false;

    // Set the priorities */
 
    bool foundIt = true;
    if (ind[nInd].special == NO) ind[nInd].basePri = 1.0;
    else {                                // search for assigned priority */
      foundIt = false;
      for (int ie = 0; ie <= nPri; ie++) { 
        if (strcmp(ind[nInd].ECLID, pri[ie].ECLID) == 0) {
          ind[nInd].basePri = pri[ie].basePri;
          foundIt = true;
          break;
        }
      }
    } 
    if (! foundIt) {                    // ask for it */
      printf("\nPriority not found for %s (%s).\n",
             ind[nInd].name, ind[nInd].ECLID);
      printf("Justification: %s\n", ind[nInd].just);
      printf("Please enter the priority.\n");
      float basePri;
      scanf("%f", &basePri);
      ind[nInd].basePri = basePri;   
    }

    // zero base priorities for institutions with zero quota */

    int iInst = ind[nInd].home;
    if (inst[iInst].quota == 0) ind[nInd].basePri = 0.0;

    // exceptional institutional priorities */
    
    //if (ind[nInd].home == ?) ind[nInd].basePri += 0.5; */
    

    // set priorities */
     
    ind[nInd].bonusPri = 0.0;
    ind[nInd].randPri = randP();
    ind[nInd].totPri = ind[nInd].basePri + ind[nInd].virginPri 
      + ind[nInd].bonusPri + ind[nInd].randPri;

    // Optional print for debugging */

    // if (nInd == 74) dumpIndividual(nInd); */ 

    // Basic output -- also goes to the log*/

    char str[80];
    if (verbose) { 
      printf("\n\nShifter %d, %s (%s) from %s has requested %d point(s).\n",
	     nInd,ind[nInd].name, ind[nInd].ECLID, ind[nInd].homeName,
	ind[nInd].request);
      fprintf(fl,"\n\nShifter %d, %s (%s) from %s has requested %d point(s).\n"
	      ,nInd,ind[nInd].name, ind[nInd].ECLID, ind[nInd].homeName,
	ind[nInd].request);
      printf("(S)he has base priority %4.1f\n", ind[nInd].basePri);
      fprintf(fl,"(S)he has base priority %4.1f\n", ind[nInd].basePri);

      // LoP-1 requests */

      printf("The request for LoP-1 was\n");
      fprintf(fl,"The request for LoP-1 was\n");
      int nsh = 1;            // count requested shifts for formating */
      for (int nr = 0; nr < NSHIFTS; nr++) {
	if (ind[nInd].lop1[nr]) {
          printf("%-7s%-8s", shift[nr].date, shift[nr].type);
	  fprintf(fl,"%-7s%-8s", shift[nr].date, shift[nr].type);
          if (nsh++ % 5 == 0) {printf("\n"); fprintf(fl,"\n");}
        }
      }

      // LoP-2 requests */
      
      if (nsh % 5 != 1) {printf("\n"); fprintf(fl,"\n");}      
      printf("The request for LoP-2 was\n");
      fprintf(fl,"The request for LoP-2 was\n");
      nsh = 1;            // count requested shifts for formating */
      for (int nr = 0; nr < NSHIFTS; nr++) {
	if (ind[nInd].lop2[nr]) {
	  printf("%-7s%-8s", shift[nr].date, shift[nr].type);
	  fprintf(fl,"%-7s%-8s", shift[nr].date, shift[nr].type);
	  if (nsh++ % 5 == 0) {printf("\n"); fprintf(fl,"\n");}
        }
      } 
    }
    if (c == EOF) break;
  }
  nInd++;  // Note there are nInd shifters with the index [0,...,nInd-1] */
  if (verbose) {
    printf("\nThere are a total of %d shifters requesting a total of %d points.\n"
	   ,nInd, totRequests);
    printf("There are %d total shift points available this period.\n", totPoints);
    fprintf(fl, "\nThere are a total of %d shifters requesting a total of %d points.\n" , nInd, totRequests);
    fprintf(fl, "There are %d total shift points available this period.\n"
            , totPoints);
  }
  fclose(fp);
} 

/*************************************************************************/
bool qualified(int ii, int is){    /* determines whether a shifter is 
                                      qualified */
/*
    The requirements for a qualified shifter are
(1) the shifter requested the shift                                      
(2) the shifter sufficient requeted points to cover the shift or has agreed
    to overage 
(3) the shifter does not have base priority = 0 if in lop1 (only allowed
    in lop2 ) 
(4) the shifter does not have a caution flag in lop1 and the shift is 
     multipoint                            
(5) the shifter does not have a conflicting assigned shift:         
      if nonConsec = 1, < 3 shifts;  = 2, < 6 shifts; 
        = 3 > 9 shifts; = 4, > 15 shifts;               
                    
    The consecutive shift requirement must also be met, if requested,
    but that will be dealt with later */

      if (! ind[ii].active[is]) return false;  // no request 
  int togo = ind[ii].request - ind[ii].nPAssigned;
  if (togo <= 0) return false;                 // shifter is closed 
  if (togo - shift[is].points < 0 && ind[ii].over == NO_OVERAGE) return false;
  if (ind[ii].basePri <= 0.0 && lop1) return false;
//  if (ind[ii].caution && shift[is].points > 1 && lop1) return false;
                                        // isa is the index of assigned shift 
  if (ind[ii].caution && shift[is].points > 10 && lop1) return false;
  for (int isa = 0; isa < ind[ii].nSAssigned; isa++) {
    int adif = abs(is - ind[ii].assigned[isa]);  // absolute distance 
    if (adif < 3) return false;
    if (ind[ii].nonConsec == 2 && adif < 6) return false;
    if (ind[ii].nonConsec == 3 && adif < 9) return false;
    if (ind[ii].nonConsec == 4 && adif < 15) return false;
  }
  return true;                 // all tests passed 
}

/*************************************************************************/
void killInst(int iInst, float diff) {   /* set priorities to diff for iInst
                                         due to fulfillment of quota */
  for (int i = 0; i < nInd; i++) {
    if (ind[i].home == iInst) {
      ind[i].basePri = diff;       /* totals will be calculated before
                                     next shift assignment */
      ind[i].bonusPri = 0.0;
    }
  }
}

/*************************************************************************/
void cautionInst(int iInst) {      /* set caution flag if quota is just  
				        one point short */
  for (int i = 0; i < nInd; i++) {
    if (ind[i].home == iInst) ind[i].caution = true;              
  }
}

/*************************************************************************/
void dumpPreparedShifts() { // number controled by global nDumpShift */
  if (! tDumpShift) return;
  tDumpShift--;
  for (int is = 0; is < nDumpShift; is++) {
    printf("\nDump of prepared struct shifts %d %s %s\n",
           is, shift[is].date, shift[is].type);
    printf("open = %d\n", shift[is].open);
    printf("points = %d\n", shift[is].points);
    int nr = shift[is].nRequests;
    printf("nRequests = %d\n", nr);
    for (int ir = 0; ir < nr; ir++) 
      printf("requester %d  = %d\n", ir, shift[is].requesters[ir]);
    printf("topRequester = %d\n", shift[is].topRequester);
    printf("assigned = %d\n", shift[is].assigned);
  }
}
  
/*************************************************************************/
void prepareShifts() {       // collects shift data 

  /* The requirement for a valid shift is that it be open.
     The requirements for a valid shifter are listed in the 
     qualified function.  */

  for (int is = 0; is < NSHIFTS; is++) {        // cycle thru all shifts 
    if (! shift[is].open) continue;        
    shift[is].nRequests = 0;
    float topPriority = -99.;
    int nCand = 0;                             // number of candidates 
    for (int ii = 0; ii < nInd; ii++) {         // cycle thru all shifters 
      if (qualified(ii, is)) {
        nCand++;
        shift[is].nRequests++;
	shift[is].requesters[nCand - 1] = ii;
        if (ind[ii].totPri > topPriority) {
          topPriority = ind[ii].totPri;
          shift[is].topRequester = ii;
        }    
      }
    }  
  }     
  dumpPreparedShifts();  // controled by global dumpShift parameter 
} 

/*************************************************************************/
bool findNextShift() {  // returns false if no more shifts; sets nextShift 
  
  /* 1st priority is least number of requesters; 2nd priority is earliest
     date */

  int minRequests = 999;

  for (int is = 0; is < NSHIFTS; is++) {     // cycle thru all shifts 
    if (! shift[is].open || shift[is].nRequests == 0)  continue;  
    if (shift[is].nRequests < minRequests) { // looking for min requests    
      minRequests = shift[is].nRequests;
      nextShift= is;
    }
  }
  return (minRequests == 999) ? false : true; // done with this LoP 
}

/*************************************************************************/
void prepareConsecutive() {  // fills the consecutive struct  
  

  int ii = shift[nextShift].topRequester;  // indentify requester  
  int type = shift[nextShift].stype;       // indentify type of shift  
  int nCand = 4;                           // default number to search  
  if (ind[ii].rest == SHORT_REST) nCand = 5;  // 8 hours rest enough  
  if (ind[ii].rest == SHORT_REST && type == day) nCand = 6; 
                                           // day gets another  
  con.nCand = nCand;                       // fill the struct value  
  for (int iCand = 0; iCand < nCand; iCand++) { // loop over shift candidates  
    int cand = nextShift + conIndex[type][iCand]; // candidate shift  
    if (cand < 0 || cand >= NSHIFTS) continue;    // shift must be in bounds

    // check if this is an multipoint shift for which there are insufficint
    // points

    if (noMultiPoint && shift[cand].points > 1) continue;   
    con.shift[iCand] = cand;               // fill the struct value  
    con.requested[iCand] = ind[ii].active[cand];  // shift requested ?  
    con.open[iCand] = shift[cand].open;    // shift open ?  
    con.occupant[iCand] = -1;              // for debugging clarity  
    con.occOpen[iCand] = -1;
    if (! lop1) continue;                  // trades allowed only in LoP 1  
    if (! shift[cand].open) {              /* if the shift is closed, we will
       we will consider the possibility of trades here.  Open shifts will be 
       considered later */
      int iOcc = con.occupant[iCand] = shift[cand].assigned;
     
      /* We now want to cycle through all of the shifts to find the best one
         for a trade.  The occupant must have requested the shift, not have 
         requested consecutive shifts (just too complicated to deal with), 
         the shift must be open, it must be of the same type and same number 
         of points, not be the nextShift, and it must not conflict with any of 
         the occupant's other shifts. The shift that meets all of these 
         conditions which has the fewest number of requesters, then the 
         earliest, will be selected. If no shift meets these requirements,
         -1 will be entered. */

      if (ind[iOcc].consec == YES) continue;  // see above        
      int minRequests = 999;              // minimium number of requests found
      int iTrade = 0;                       // needs to be outside for scope   
      for (; iTrade < NSHIFTS; iTrade++) {  // cycle thru shifts  
        if (! shift[iTrade].open || ! ind[iOcc].active[iTrade] || 
	    shift[iTrade].stype != shift[cand].stype ||
	    shift[iTrade].points != shift[cand].points || iTrade == nextShift) 
          continue;                                   // see above 

        // look for conflicts with iOcc current assignments  

        bool testFail = false;
        for (int iTest = 0; iTest < ind[iOcc].nSAssigned; iTest++) {
          int testShift = ind[iOcc].assigned[iTest];
          if (abs(testShift - iTrade) < 3) { testFail = true; break;}
        }
        if (testFail) continue;                // There is a conflict  

        /* We have found a possible trade shift; check if it is better
           than any previous one.  If so, record its number and number
           of requesters.  */

        if (shift[iTrade].nRequests < minRequests) {
          con.occOpen[iCand] = iTrade;
          minRequests = shift[iTrade].nRequests;
        }  
      }   
      if (minRequests == 999) con.occOpen[iCand] = -1;  // no trade possible 
    }
  }

  // Make a decision and post it  

  con.conShift = -1;
  con.tradeShift = -1;
  for (int iCand = 0; iCand < nCand; iCand++) {    // look for an open shift
    if (noMultiPoint && shift[con.shift[iCand]].points > 1) continue;  
    if (con.open[iCand] && con.requested[iCand]) { // we have a winner  
      con.decision = 1;                            // open shift assigned  
      con.conShift = con.shift[iCand];
      con.tradeShift = -1;                         // just for clarity 
      noMultiPoint = false;              // reset noMultiPoint flag  
      return;                                      
    }
  }
  for (int iCand = 0; iCand < nCand; iCand++) {    // look for a trade  
    if (con.occOpen[iCand] != -1 && con.requested[iCand]) {
      if (noMultiPoint && shift[con.shift[iCand]].points > 1) continue;
      con.decision = 2;                            // we have a trade  
      con.conShift = con.shift[iCand];
      con.tradeShift = con.occOpen[iCand];
      noMultiPoint = false;              // reset noMultiPoint flag 
      return;
    } 
  }                                              
  con.decision = 0;                                // no cigar  
  con.conShift = -1;
  con.tradeShift = -1;
  noMultiPoint = false;              // reset noMultiPoint flag          
  return;
}


/*************************************************************************/
void assignShift(int callType) {   // assigns shift and cleans up 

  /* if callType =
     0: normal assinment:shift is nextShift 
     1  consecutive shift assignment:  shift is con.conShift
     2  trade shift assignment : shift is con.tradeShift  */

  int thisShift;      // general variable for the shift to be assigned 
  int thisInd;        // shifter to whom the shift is being assigned   
  switch (callType) { // set up each type of call 
  case 0 : thisShift = nextShift;
           thisInd = shift[nextShift].topRequester;
           break;
  case 1 : thisShift = con.conShift;
           thisInd = shift[nextShift].topRequester;
           if (verbose && con.decision == 1) {
             printf ("An open shift was available:\n");
	     fprintf (fl,"An open shift was available:\n");
           }
           if (verbose && con.decision == 2) {
             printf ("\nThis is a shift made available by a trade:");
             fprintf (fl,"\nThis is a shift made available by a trade:");
           }
           break;
  case 2 : thisShift = con.tradeShift;
    int thatShift = con.conShift; // the starting shift of the trade 
           thisInd = shift[thatShift].assigned; 
           if (verbose) {
	     printf ("A trade shift was available:\n");
             fprintf (fl,"A trade shift was available:\n");
	   }
  }

  // log shift assignment; first the initial conditions 
   
  if (verbose) {
    int nRequests = shift[thisShift].nRequests;
    printf("\nShift %d %s %s: %d qualified requester(s):\n", thisShift,
         shift[thisShift].date, shift[thisShift].type, nRequests);
    fprintf(fl,"\nShift %d %s %s: %d qualified requester(s):\n", thisShift,
         shift[thisShift].date, shift[thisShift].type, nRequests);
    for (int ir = 0; ir < nRequests; ir++) {
      int nr = shift[thisShift].requesters[ir];   // get requester number 
      printf("%s with priority %5.3f\n", ind[nr].name , ind[nr].totPri);
      fprintf(fl,"%s with priority %5.3f\n", ind[nr].name , ind[nr].totPri);
    }
  } 

  // assign shift and clean up struct shifts 

  shift[thisShift].open = false;
  shift[thisShift].assigned = thisInd;

  // clean up struct institution; killInst changes basePri to diff
  // cautionInst sets a flag that diff == 1, so that multipoint shifts
  // will only be allowed in LoP-2 

  int iInst = ind[thisInd].home;
  if (callType < 2) { // It is a wash for the trade case 
    inst[iInst].nPAssigned += shift[thisShift].points;
    float diff = inst[iInst].quota - inst[iInst].nPAssigned;
    if (diff <= 0) killInst(iInst, diff);
//    if (diff == 1 && inst[iInst].quota > 1) cautionInst(iInst);
    if (diff == 10 && inst[iInst].quota > 10) cautionInst(iInst);

    // clean up struct individual 

    ind[thisInd].nPAssigned += shift[thisShift].points;
    ind[thisInd].nSAssigned++;
  
    int nShiftsAssigned = ind[thisInd].nSAssigned;
    ind[thisInd].assigned[nShiftsAssigned - 1] = thisShift;
    if (ind[thisInd].nPAssigned >= ind[thisInd].request) 
      ind[thisInd].open = false;
  }
  if (callType == 2) { // need to update assigned list  
    for (int iss = 0; iss < ind[thisInd].nSAssigned; iss++)  
      if (ind[thisInd].assigned[iss] == con.conShift) 
        ind[thisInd].assigned[iss] = thisShift;  
  }   

  // adjust bonus priorities 

  int nreq = shift[thisShift].nRequests;
  for (int ireq = 0; ireq < nreq; ireq++) {	 
    int iInd = shift[thisShift].requesters[ireq];
    if (iInd == thisInd && callType < 2) {
      ind[iInd].bonusPri *= 0.5;      // give someone else a chance 
      ind[iInd].virginPri = 0.0;      // not a virgin any more 
    }
    else  ind[iInd].bonusPri += 0.1;  /* thisInd gets a bonus too for 
                                         the inconvenience of a trade */
    // the priorities get summed in getNewRandPri 
  }

    // log shift assignment; final conditions

  if (verbose) {
    printf("Shift has been assigned to %s (%s) from %s.\n",
	   ind[thisInd].name, ind[thisInd].ECLID, inst[iInst].name);
    fprintf(fl,"Shift has been assigned to %s (%s) from %s.\n",
	    ind[thisInd].name, ind[thisInd].ECLID, inst[iInst].name);
    printf("%s has %d of %d requested points.\n", ind[thisInd].name,
           ind[thisInd].nPAssigned, ind[thisInd].request);
    fprintf(fl,"%s has %d of %d requested points.\n", ind[thisInd].name,
	   ind[thisInd].nPAssigned, ind[thisInd].request);
    printf("%s has %d of %d quota points.\n", inst[iInst].name,
           inst[iInst].nPAssigned, inst[iInst].quota);
    fprintf(fl,"%s has %d of %d quota points.\n", inst[iInst].name,
	    inst[iInst].nPAssigned, inst[iInst].quota);
  }
}

/*************************************************************************/
int findConsecShift() { // called from algorithm after findNextShift 

  int ii = shift[nextShift].topRequester;  // identify requester 
  if (ind[ii].consec == NO) return 0;      // consecutive shift not requested
   if (verbose) {
     printf("\n%s has requested a consecutive shift in anticipation\n",
	    ind[ii].name);
     printf("of being assigned to shift %d: %s  %s.\n", nextShift,
            shift[nextShift].date, shift[nextShift].type);
     fprintf(fl,"\n%s has requested a consecutive shift in anticipation\n",
            ind[ii].name);
     fprintf(fl,"of being assigned to shift %d: %s %s.\n", nextShift,
            shift[nextShift].date, shift[nextShift].type);
   }
   if (ind[ii].nPAssigned + shift[nextShift].points >= ind[ii].request) {
     if (verbose) {
       printf("But this person does not sufficient requested points.\n");
       fprintf(fl,"But this person does not sufficient requested points\n.");
     }
     return 1;                        // no action needed 
   }
   int iInst = ind[ii].home;
   if (inst[iInst].nPAssigned + shift[nextShift].points >= 
      inst[iInst].quota) {
     if (verbose) {
       printf("But %s does not sufficient quota points.\n", inst[iInst].name);
       fprintf(fl,"But %s does not sufficient quota points.\n", 
         inst[iInst].name);
     }
     return 2;                        // no action needed 
   }

   // warn prepareConsecutive not to assign a multipoint shift

   if (inst[iInst].nPAssigned + shift[nextShift].points ==
       inst[iInst].quota - 1 && lop1) noMultiPoint = true; 

   prepareConsecutive();

   /* actions on the consecutive shifts */

   if (con.decision == 0) {
     if (ind[ii].strict == NOT_STRICT) { // no consecutive shift, but go ahead
       if (verbose) {
         printf("A consecutive shift could not be found, but this person will\n");
         printf("take the shift anyway.\n");
	 fprintf(fl,"A consecutive shift could not be found, but this person\n");
	 fprintf(fl,"will take the shift anyway.\n");
       }
       return 3;
     }
     if (ind[ii].strict == STRICT) { // need to recycle the shift 
       if (verbose) {
         printf("A consecutive shift could not be found; the shift\n");
         printf("will be recycled\n");
	 fprintf(fl,"A consecutive shift could not be found; the shift\n");
         fprintf(fl,"will be recycled\n");
       }
       ind[ii].active[nextShift] = false;  // remove the request 
       return 4;  // This will prevent algorithm from assigning the shift 
     }
   }
   if (con.decision == 1) assignShift(1);
   if (con.decision == 2) {assignShift(2); assignShift(1);}
} 

/*************************************************************************/
void getNewRandPri() {
  int i = 0;
  for (int i = 0; i < nInd; i++) {
    ind[i].randPri = randP();
    if (ind[i].basePri <= 0.0) ind[i].bonusPri = 0.0; // but virgin stays 
    ind[i].totPri = ind[i].basePri + + ind[i].virginPri + ind[i].bonusPri 
      + ind[i].randPri; 
  }
}

/*************************************************************************/
void algorithm() {

  if (lop1 && verbose) {
    printf("\nAssignments at LoP1:\n");
    fprintf(fl, "\nAssignments at LoP1:\n");
  }

  while (true) {
    prepareShifts();

    for (int id = 0; true; id++) {   // do the directed shifts first  
      int ds = directedShifts[id];
      if (ds >= 0) {                 // the list is terminated by -1 
        if (shift[ds].open) {nextShift = ds; break;}
      }                      // list of diredted shifts has terminated 
      if (! findNextShift()) return;  // no more shifts at this LoP 
      break;                          // need to escape the while
    }
    
    /* A return of 4 from findConsecShift indicates that a consecutive
       shift could not be found and that the shift needs to be 
       recycled.   */ 

    if (findConsecShift() != 4) assignShift(0); 
    getNewRandPri();
  }   
}

/*************************************************************************/
void switchLoP() {           // switches from LoP-1 to LoP-2  
  lop1 = false;              // lop2 is now active 
  for (int ii = 0; ii <= nInd; ii++) 
    for (int is = 0; is < NSHIFTS; is++) 
      ind[ii].active[is] = ind[ii].lop2[is];
  if (verbose) {
    printf("\nSwitching to LoP-2:\n");
    fprintf(fl,"\nSwitching to LoP-2:\n");
  }
}

/*************************************************************************/
void shiftTable() {    // prints the shift table and the shiftECLInput.csv
  FILE *fp;
  FILE *fe;
  fp = fopen("ShiftTable.txt","w");
  fe = fopen("ShiftECLInput.csv", "w");

  printf("\nShift Table\n\n");
  fprintf(fp,"\nShift Table\n\n");
  int iOpen = 0;
  int iFill = 0;
  for (int is = 0; is < NSHIFTS; is++) {
    if (shift[is].open){
      iOpen++;  
      printf("%3d %6s %5s: Open\n", is, shift[is].date, shift[is].type);
      fprintf(fp,"%3d %6s %5s: Open\n", is, shift[is].date, shift[is].type);
    }
    else {
      iFill++;
      int i = shift[is].assigned;           // assigned  shifter 
      int h = ind[i].home;
      printf("%3d %6s %5s: %-25s%-20s%-15s\n", is,
	shift[is].date, shift[is].type, ind[i].name, ind[i].ECLID, inst[h].name);
      fprintf(fp,"%3d %6s %5s: %-25s%-20s%-15s\n", is,
	shift[is].date, shift[is].type, ind[i].name, ind[i].ECLID, inst[h].name);
      shift[is].ECLDate[10] = '\0';   // I do not know why this is necessary, but it is
      fprintf(fe,"%s,%s,Control Room,%s\r",ind[i].ECLID,shift[is].ECLType,
	      shift[is].ECLDate);
    }
  }
  printf("\nNumber of filled shifts = %d; number of open shifts = %d\n",
         iFill, iOpen);
  fprintf(fp,"\nNumber of filled shifts = %d; number of open shifts = %d\n",
	 iFill, iOpen);
  fclose(fp);
  fclose(fe);
}

/*************************************************************************/
void shifterTable() {  // prints the shifter table 
  FILE *fp;
  fp = fopen("ShifterTable.txt", "w");
  
  printf("\nShifter Table\n\n");
  fprintf(fp,"\nShifter Table\n\n");
  printf("Fields are points requested, points assigned, shifts assigned,\n");
  printf("shifts requested at LoP1, and shifts requested at LoP2\n");
  fprintf(fp, "Fields are points requested, points assigned, shifts assigned,\n");
  fprintf(fp, "shifts requested at LoP1, and shifts requested at LoP2\n");
  for (int i = 0; i < nInd; i++) {
    int h = ind[i].home;
    printf("%-25s%-15s%3d%3d%3d%5d%5d\n", ind[i].name, inst[h].name,
         ind[i].request, ind[i].nPAssigned, ind[i].nSAssigned, 
         ind[i].nLoP1, ind[i].nLoP2);
    fprintf(fp,"%-25s%-15s%3d%3d%3d%5d%5d\n", ind[i].name, inst[h].name,
         ind[i].request, ind[i].nPAssigned, ind[i].nSAssigned, 
         ind[i].nLoP1, ind[i].nLoP2);
  }
  fclose(fp);
}

/*************************************************************************/
void deficiencyReport(int i) {   

  /* writes a deficiency Report for institution i. These reports are ordered
     by insstitutionalTable under the control of defRep.*/

  printf("\n\n\n%-14s has requested %d points for a quota of %d points\n\n",
	 inst[i].name, inst[i].nPRequested, inst[i].quota); 
  printf("Current requests:\n\n");
  
  for (int ii = 0; ii < nInd; ii++) {
    if (ind[ii].home != i) continue;
    printf("%-25s%5d points\n",ind[ii].name, ind[ii].request);
  }

}

/*************************************************************************/
int openShifts;  // globals                     
int chisq;
int chisqInd;    // Individual chisq shifted so 0 is the lowest possible
/*************************************************************************/
void report () {

  openShifts = 0;
  chisq = 0;
  for (int i = 1; i <= nInst; i++) {
    int diff = -inst[i].nPAssigned + inst[i].quota;
    openShifts += diff;
    chisq += diff*diff;
  }
  chisqInd = 0;
  for (int i = 0; i < nInd; i++) chisqInd += (ind[i].request - ind[i].nPAssigned)*
				   (ind[i].request - ind[i].nPAssigned);
  chisqInd += totPoints - totRequests; 
  return;
}

/*************************************************************************/
void institutionTable() {  // print the institution table 
  FILE *fp;
  fp = fopen("InstitutionTable.txt", "w");

  printf("\nInstitution Table\n\n");
  fprintf(fp,"\nInstitution Table\n\n");

  // printf("Institutions that have met, exceeded, or come within 1 point of\n");
  // printf("Institutions\n\n");
  // fprintf(fp,"Institutions that have met, exceeded, or come within 1 point\n");
  // fprintf(fp,"Institutions\n\n");

  printf("%-15s%14s%7s%10s%11s\n", " ", "Points ", " ", "Points ", " ");
  printf("%-15s%14s%7s%10s%12s\n\n", "Institution","Requested", "Quota", 
         "Assigned", "  Difference");
  fprintf(fp,"%-15s%14s%7s%10s%11s\n", " ","Points ", " ", "Points ", " ");
  fprintf(fp,"%-15s%14s%7s%10s%12s\n\n", "Institution","Requested", "Quota",
         "Assigned", "  Difference");

  for (int i = 1; i <= nInst; i++) { 
    int diff = inst[i].nPAssigned - inst[i].quota;  
    if (true) {  
      printf("%-15s%11d%8d%9d%10d\n", inst[i].name, inst[i].nPRequested, 
	     inst[i].quota, inst[i].nPAssigned, diff);
      fprintf(fp,"%-15s%11d%8d%9d%10d\n", inst[i].name, inst[i].nPRequested,
             inst[i].quota, inst[i].nPAssigned, diff);
    }
  }
  /* printf("\nInstitutions that are more than 1 point short of  their quota.\n\n");
  fprintf(fp,"\nInstitutions that are more than 1 point short of  their quota.\n\n");
  printf("%-15s%14s%7s%10s%11s\n", " ","Points ", " ", "Points ", " ");
  printf("%-15s%14s%7s%10s%12s\n\n", "Institution","Requested", "Quota",
         "Assigned", "  Difference");
  fprintf(fp,"%-15s%14s%7s%10s%11s\n", " ","Points ", " ", "Points ", " ");
  fprintf(fp,"%-15s%14s%7s%10s%12s\n\n", "Institution","Requested", "Quota",
	  "Assigned", "  Difference");

  for (int i = 1; i <= nInst; i++) {
    int diff = inst[i].nPAssigned - inst[i].quota;
    if (diff < - 1) { 
      printf("%-15s%11d%8d%9d%10d\n", inst[i].name, inst[i].nPRequested,
             inst[i].quota, inst[i].nPAssigned, diff);
      fprintf(fp,"%-15s%11d%8d%9d%10d\n", inst[i].name, inst[i].nPRequested,
	      inst[i].quota, inst[i].nPAssigned, diff);
    }
    } */
  fclose(fp);

  
  for (int i = 1; i <= nInst; i++) {       // issue deficiency reports 
    int diff = inst[i].nPRequested - inst[i].quota;
    if (diff < 0 && defRep) deficiencyReport(i);
    // deficiencyReport(i);
  }

  report();
  printf ("\nAbsolute points sum difference = %d\nChisq = %d\nIndChisq = %d\n", openShifts, chisq, chisqInd);  

}


/*************************************************************************/
void findDonors() {

  /* Find and rate shifts that are eligible for donation. 
     The criteria are that the institution has exeeded its quota and the 
      assigned shifter has not requested a consecutive shift, has not 
      been assigned a consecutive shift, or has not specified that the 
      consecutive shift is a strict requirement. */

  int nd = 0;                                   // number of potential shifts  
  for (int is = 0; is < NSHIFTS; is++) {
    shift[is].donPri = 0;                       // default not a donor 
    if (shift[is].open) continue;
    int ii = shift[is].assigned;                // potential donor 
    int in = ind[ii].home;                      // doner institution 
    int diff = inst[in].nPAssigned - inst[in].quota;  
    if (diff <= 0) continue;                    // no excess shifts */
    if (ind[ii].consec == YES && ind[ii].strict == STRICT) { 
      bool consecu = false;
      for (int ir = 0; ir < ind[ii].nSAssigned; ir++) {// look for consec shift
        int rdiff = abs(is - ind[ii].assigned[ir]);
	if (rdiff && rdiff <= 6) consecu = true;
      } 
      if (consecu) continue; 
    }                                             // donor shift found 
    shift[is].donPri = 10*diff + ind[ii].nPAssigned - ind[ii].request;
    nd++;
  }
}

/*************************************************************************/
bool findNextDonor() { /* returns true if it found a donor shift and 
                          the shift number in the global variable donorShift */
  int maxPri = 0;
  int is = -1;                        // default for no donors 
  for (int i =0; i < NSHIFTS; i++) {  // find the highest donor priority shift 
    if (shift[i].donPri > maxPri) {
      maxPri = shift[i].donPri;
      is = i;
    }  
  }
  if (is >= 0) shift[is].donPri = 0;  /* turn off donPri so it will not be
                                         picked up on the next pass */ 
  donorShift = is;
  return (is >= 0);
} 

/*************************************************************************/
void assignDonorShift(int is, int ir) {

  /* This routine transfers the assignment of shift "is" to receiver "ir".
     Tasks:
     (1) Insert new receiver.
     (2) Adjust receiver individul points and shifts.
     (3) Adjust receiver institution points.
     (4) Adjust donor receiver points and shifts.
     (5) Adjust donor institution points.
     (6) Print a log entry.
     (7) Call findDonors to reset donors and priorities.   */

  int id = shift[is].assigned;      // index of the donor 
  int idInst = ind[id].home;        // index of the donor institution 
  int irInst = ind[ir].home;        // index of the receiver institution  
  int points = shift[is].points; 

  shift[is].assigned = ir;                             // #1 above 
  int nShift = ind[ir].nSAssigned;
  ind[ir].assigned[nShift] = is;  
  ind[ir].nSAssigned++;                                // #2 above 
  ind[ir].nPAssigned += points;
  if (ind[ir].request - ind[ir].nPAssigned <= 0) ind[ir].open = false; 
  inst[irInst].nPAssigned += points;                   // #3 above 
  float diff = inst[irInst].quota - inst[irInst].nPAssigned;
  if (diff <= 0) killInst(irInst, diff);
//  if (diff == 1 && inst[irInst].quota > 1) cautionInst(irInst);
  if (diff == 10 && inst[irInst].quota > 10) cautionInst(irInst);

                                      // find and delete donor shift 
 
  for (int iss = 0; iss < ind[id].nSAssigned; iss++) {  
    if (ind[id].assigned[iss] != is) continue;
      ind[id].assigned[iss] = ind[id].assigned[iss + 1];
  }
  ind[id].nSAssigned--;                                // #4 above 
  ind[id].nPAssigned -= points;
  inst[idInst].nPAssigned -= points;                   // #5 above 
  diff = inst[idInst].quota - inst[idInst].nPAssigned;
  if (diff <= 0) killInst(idInst, diff);
//  if (diff == 1 && inst[idInst].quota > 1) cautionInst(irInst);
  if (diff == 10 && inst[idInst].quota > 10) cautionInst(irInst);

  if (verbose) {
    printf("\n%s from %s has graciously donated shift %d %s %s\n",
      ind[id].name, ind[id].homeName, is, shift[is].date, shift[is].type); 
    printf("to %s from %s.\n",ind[ir].name, ind[ir].homeName);
    fprintf(fl,"\n%s from %s has graciously donated shift %d %s %s\n",
	 ind[id].name, ind[id].homeName, is, shift[is].date, shift[is].type);
    fprintf(fl,"to %s from %s.\n",ind[ir].name, ind[ir].homeName);
  }
  findDonors();

}

/*************************************************************************/
void findReceiver() {  

  /* Qualifications for a donation receiver: 
     (1) Must have requested the shift.
     (2) Must not have reached or exceed its request. 
     (3) For a multi-point assignment, must have either enough points
         or has specified "over".
     (4) Its institution must not have reached or exceeded its quota.
     (5) The donation must improve the overal balance (relevant only for
         multi-point shifts).

     The priority for the receiver is 10*institutional deficit + personal
     deficit - 2*number of shifts assigned to the receiver. */

  int is = donorShift;
  int maxPri = -99;
  int ir = -1;
  int id = shift[is].assigned;
  int idInst = ind[id].home;
  int points = shift[is].points;
  for (int i = 0; i < nInd; i++) {                      // individual search   
    if (! ind[i].active[is]) continue;                     // #1 above 
    if (ind[i].request - ind[i].nPAssigned <= 0) continue; // #2 above 
    if (ind[i].request - ind[i].nPAssigned - points < 0 && 
        ind[i].over == NO_OVERAGE) continue;               // #3 above 
    int irInst = ind[i].home;                          
    int irDiff = inst[irInst].quota - inst[irInst].nPAssigned;
    if (irDiff <= 0) continue;                             // # 4 above 
    int idDiff = inst[idInst].quota - inst[idInst].nPAssigned;
    if (abs(irDiff - points) + abs(idDiff + points) >= 
        abs(irDiff) + abs(idDiff)) continue;               // # 5 above
   
    // We have a receiver candidate -- calculate priority and save info 

    int priority = 10*irDiff + ind[i].request - ind[i].nPAssigned -
      2*ind[i].nSAssigned;
    if (priority > maxPri) {
      maxPri = priority;
      ir = i;
    }
  }
  if (ir < 0) return;                          /* no receiver found */   
  assignDonorShift(is, ir);
  return;
}

/*************************************************************************/
void donationTime() {

  int is;
  
  if (verbose) {
    printf("\nStarting the donation process in which institutions with excess\n");
    printf("points donate shifts to institutions with a deficit of points\n");
    fprintf(fl,"\nStarting the donation process in which institutions with excess\n");
    fprintf(fl," points donate shifts to institutions with a deficit of points\n");
  }
  findDonors();
  while (findNextDonor()) findReceiver();

}

/*************************************************************************/
int seed[1000000];   // global 
/*************************************************************************/
void prepareRandomSeeds() {               // prepare 1,000,000 seeds 

  srand(271828183);

  for (int i = 0; i < 1000000; i++) seed[i] = rand();
  
  return;
}

/***********************************************************************/
void main(int argc, char *argv[]) {

  fl = fopen("AssignLog.txt", "w");  // open the log file 
  prepareRandomSeeds();
  
  bool scanMode;
  int seedIndex;   
  int nStop;
                   
  if (argc > 1) {
    scanMode = false;
    seedIndex = atoi(argv[1]);
    verbose = true;
    nStop = 0;                        /* run only once */
  }
  else {
    scanMode = true;
    verbose = false;
    nStop = 999999;                     // run 1,000,000 times 
  } 
 
  int openMin = NSHIFTS;;
  int chisqMin = 999;
  int chisqIndMin = 9999;

  for (int iloop = 0; iloop <= nStop; iloop++) {
    initialization();
    if (scanMode) srand(seed[iloop]);  // seed random number 
    else srand(seed[seedIndex]);
    parseInstFile();            // input institution file
    parseShiftFile();           // input shift file
    parsePriFile();             // input priority file
    parseIndFile();             // input shifter file from the questionnaire 
    algorithm();                // run on LoP-1 
    switchLoP();                // switch active file to LoP-2 
    algorithm();                // run on LoP-2 
    donationTime();             // wealthy groups donate to the poor 
    if (!scanMode) {
      shiftTable();                 // print shift table 
      shifterTable();               // print shifter table  
      institutionTable();           // print institution table 
    }
    else {
      report();
      //      if (openShifts < 0) printf("seed %d flag\n", iloop); 
      if (openShifts < openMin && openShifts >= 0) { // strange bug -- needs tracking down
        openMin = openShifts;
        chisqMin = chisq;           // reset chisq for new openMin
        chisqIndMin = chisqInd;     // reset chisqInd for new openMin
      }
      else if (openShifts == openMin && chisq < chisqMin) {
        chisqMin = chisq;
        chisqIndMin = chisqInd;     // reset chisqInd for new chisqMin
      }
      else if (openShifts == openMin && chisq == chisqMin && chisqInd < chisqIndMin) {
        chisqIndMin = chisqInd;
      }
      if ((openShifts == openMin && chisq == chisqMin && chisqInd == chisqIndMin) || 
        iloop % 10000 == 0) {
	printf ("seed %d open = %d chisq = %d %d\n", iloop, openShifts, chisq, chisqInd);
        fprintf (fl,"seed %d open = %d chisq = %d &d\n", iloop, openShifts, 
		 chisq, chisqInd);
      }
    }
  }    
  fclose(fl);
}
