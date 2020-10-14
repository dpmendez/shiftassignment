###date, shift, identifier, points, weekday or end, date in mm/dd/yy'
import calendar
from datetime import date
year=2021
month_range=[1]
shift_order = ['Night', 'Day', 'Swing']
filename='Shift_TEST.csv'
lines=[]
####may to september
for month in month_range:
    month_c = calendar.monthcalendar(year, month)
    for week in month_c:
        for day in week:
            if day==0:
                pass
            else:
                d = date(year, month, day)
                if week.index(day)<4:
                    daytype='Weekday'
                    points = [10,10,10] # 6 4 4 
                else:
                    daytype='Weekend'
                    points=[10,10,10] # 3.5 3 3
                for i in range(3):
                    s = d.strftime('%d-%b')+','+shift_order[i]+','+str(i)+','+str(points[i])+','+daytype+' '+str(shift_order[i])+','+d.strftime(''"%m/%d/%y")+'\n'
                    print(s)
                    if week.index(day)==0:
                        lines.append(s)
                    elif week.index(day)==4:
                        lines.append(s)
                    else:
                        pass
f=open(filename,'w')
f.writelines(lines)
f.close()
