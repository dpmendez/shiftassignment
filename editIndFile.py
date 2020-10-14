# =========================================== #
#                                             #
# ICARUS shift assigment 2020		      #
#                                             #
# Author: Diana Mendez                        #
# Contact: dmendezme@bnl.gov 		      #
#                                             #
# =========================================== #

import itertools
import sys

import csv as csv 
import pandas as pd
import utilities as myutils

#####
#to use input from users, first import sys
#sys.argv gives a list of the input arguments
#so running
#python3.7 editIndFile.py rawInd.csv
#sys.argv = ['editIndFile.py', 'rawInd.csv']
#sys.argv[1] = 'rawInd.csv'
######

# Read data from csv file
fileName=sys.argv[1]
data=pd.read_csv(fileName)
column_names = list(data.columns)
ncolumns = len(column_names)-1

weeks = 15 # number of weeks
shifttypes = 2 # weekday and weekend shifts
shifttimes = 3 # night, day and swing schedules


with open('Ind.csv', 'w', newline='') as file:
    writer = csv.writer(file)

    for index, row in data.iterrows():

        this_person_list = [] # Define empty person's list

        name    = str(row['Name'])
        ecl     = str(row['ECL'])
        email   = str(row['Email'])
        points  = int(row['Points'])

        inst_no         = myutils.editInstitution(row['Institution'])
        over_id         = myutils.setExceedRequest(row['ExceedRequest'])
        consec_id       = myutils.setConsecShifts(row['ConsecutiveShifts'])
        rest_id         = myutils.setRest(row['Rest'])
        strict_id       = myutils.setStrictPairing(row['StrictPairing'])
        nonconsec_id    = myutils.setNonConsecShifts(row['NonConsecutiveShifts'])
        brake_id        = myutils.setBrake(row['Brake'])
        previous_id     = myutils.setPreviousRequest(row['PreviousRequest'])
        special_id      = myutils.setSpecialPriority(row['Priority'])

        response_id = str(row['ResponseID'])

    # Explain why you need more points
        expPoints=''
        if row['MorePoints']=='NULL' or row['MorePoints']=='nan':
            expPoints=''
        else:
            expPoints=row['MorePoints']

    # Explain constraints for priority
        expConstraints=''
        if row['ExplainConstraints']=='NULL' or row['ExplainConstraints']=='nan':
            expConstraints=''
        else:
            expConstraints=row['ExplainConstraints']        


        this_person_list.append( name )
        this_person_list.append( ecl )
        this_person_list.append( email )
        this_person_list.append( inst_no )
        this_person_list.append( points )
        this_person_list.append( str(expPoints))
        this_person_list.append( over_id )
        this_person_list.append( consec_id )
        this_person_list.append( rest_id )
        this_person_list.append( strict_id )
        this_person_list.append( nonconsec_id )
        this_person_list.append( brake_id )
        this_person_list.append( previous_id )
        this_person_list.append( special_id )
        this_person_list.append( str(expConstraints))

        # Shift dates start from column 15
        for column_index in range(15,ncolumns):

            this_date = row[column_names[column_index]]
            selected_blocks = myutils.selectBlocks(this_date)
            this_person_list.extend(selected_blocks)

        this_person_list.append (response_id)
    #    print(this_person_list)
        writer.writerow(this_person_list)
