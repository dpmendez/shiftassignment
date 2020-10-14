# =========================================== #
#                                             #
# ICARUS shift assigment 2020                 #
#                                             #
# Author: Diana Mendez                        #
# Contact: dmendezme@bnl.gov                  #
#                                             #
# =========================================== #

def setDefaultOptions():

    institutionNo   = 0     # institution id helps track institutional quota
    institutionId   = ''    # institution short name
    overId          = 1     # default not exceed number of points
    consecId        = 1     # default to need consecutive shifts
    restId          = 2     # default rest 16 hours
    strictId        = 2     # default abandom if not consecutive shifts
    nonConsecId     = 0     # default not requested
    brakeId         = 1     # default no break
    previousId      = 0     # default no previously requested points
    specialId       = 2     # default no priority


def editInstitution(this_input):

    inst_no = 999
    inst_short = 'NOINST_ERROR'
    
    tempInst = str(this_input)
    if 'Brookhaven' in tempInst:
    	inst_no     = 1
    	inst_short  ='BNL'
    elif 'CERN' in tempInst:
    	inst_no     =2
    	inst_short  ='CERN'
    elif 'CINVESTAV' in tempInst:
    	inst_no     = 3
    	inst_short  ='CINVESTAV'
    elif 'Colorado' in tempInst:
    	inst_no     = 4
    	inst_short  ='CSU'
    elif 'Fermi' in tempInst:
    	inst_no     = 5
    	inst_short  ='FNAL'
    elif 'Aquila' in tempInst:
    	inst_no     = 6
    	inst_short  ='INFNAquila'
    elif 'Assergi' in tempInst:
    	inst_no     = 7
    	inst_short  ='INFNAssergi'
    elif 'Bicocca' in tempInst:
        inst_no     = 10
        inst_short  ='MilanoBicocca'
    elif 'Milano' in tempInst:
    	inst_no     = 8
    	inst_short  ='INFNMilano'
    elif 'Catania' in tempInst:
    	inst_no     = 9
    	inst_short  ='INFNCatania'
    elif 'Napoli' in tempInst:
    	inst_no     = 11
    	inst_short  ='INFNNapoli'
    elif 'Padova' in tempInst:
    	inst_no     = 12
    	inst_short  ='INFNPadova'
    elif 'Pavia' in tempInst:
    	inst_no     = 13
    	inst_short  ='INFNPavia'
    elif 'LNS' in tempInst:
    	inst_no     = 14
    	inst_short  ='INFNLNS'
    elif 'SLAC' in tempInst:
    	inst_no     = 15
    	inst_short  ='SLAC'
    elif 'Methodist' in tempInst:
    	inst_no     = 16
    	inst_short  ='SMU'
    elif 'Tufts' in tempInst:
    	inst_no     = 17
    	inst_short  ='Tufts'
    elif 'Bologna' in tempInst:
    	inst_no     = 18
    	inst_short  ='Bologna'
    elif 'Genova' in tempInst:
    	inst_no     = 19
    	inst_short  ='Genova'
    elif 'Houston' in tempInst:
    	inst_no     = 20
    	inst_short  ='Houston'
    elif 'Pitts' in tempInst:
    	inst_no     = 21
    	inst_short  ='Pittsburgh'
    elif 'Rochester' in tempInst:
    	inst_no     = 22
    	inst_short  ='Rochester'
    elif 'Texas' in tempInst:
    	inst_no     = 23
    	inst_short  ='UTA'

    return inst_no
    #return inst_short


def setExceedRequest(this_input):
    # exceed or not number of points
    # default not exceed number of points
    over_id = 1
    if 'Go' in str(this_input):
        over_id=2
        
    return int(over_id)


def setConsecShifts(this_input):
    # requested consecutive shifts
    # default is yes
    consec_id = 1
    if str(this_input)=='No':
        consec_id = 2

    return int(consec_id)


def setRest(this_input):
    # requested rest hours
    # default to 16 hrs
    rest_id = 2
    if '8' in str(this_input):
        rest_id = 1

    return int(rest_id)


def setStrictPairing(this_input):
    # strictly consecutive shifts requirement
    # default abandom if not consecutive shifts
    strict_id = 2
    if 'give' in str(this_input):
        strict_id = 1

    return int(strict_id)


def setNonConsecShifts(this_input):
    # request for non-consecutive shifts
    # default not requested
    nonconsec_id = 0
    if str(this_input)=='Yes':
        nonconsec_id = 1

    return int(nonconsec_id)


def setBrake(this_input):
    # requested rest between non consecutive shifts
    # default no break
    brake_id = 1
    if '3' in str(this_input):
        brake_id = 2 # one shift rest
    elif 'week' in str(this_input):
        brake_id = 3 # one week rest
    elif 'more' in str(this_input):
        brake_id = 4 # more than 1 week rest

    return int(brake_id)

def setPreviousRequest(this_input):
    # requested shifts in previous period without full success
    # default no previously requested points
    previous_id = 0
    if 'meet' in str(this_input):
        previous_id = 1

    return int(previous_id)


def setSpecialPriority(this_input):
    # requested special priority
    # default is no priority
    special_id = 2
    if str(this_input)=='Yes':
        special_id = 1

    return int(special_id)


def selectBlocks(selection):

    selection = str(selection).lower()
    # default 0 means block not requested
    shift_block = ['','' ,'' ,'' ,'' ,'' ]
    if 'weekday night' in selection:
        shift_block[0]=1
    if 'weekday day' in selection:
        shift_block[1]=1
    if 'weekday swing' in selection:
        shift_block[2]=1
    if 'weekend night' in selection:
        shift_block[3]=1
    if 'weekend day' in selection:
        shift_block[4]=1
    if 'weekend swing' in selection:
        shift_block[5]=1

    return shift_block
