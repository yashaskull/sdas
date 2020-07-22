import sys
import os

# slarchive save command
slarchive_command = "~/Desktop/slarchive-2.2/./slarchive -tw ";
save_option = " -CSS ";
sl_host_port = " localhost:18000 ";

#print("number of cl arguments: ", len(sys.argv))
#print(sys.argv[2])

# timewindow for saving. Arguments read from command line
# starttime save
st_save_year = sys.argv[1]
st_save_month = sys.argv[2]
st_save_day = sys.argv[3]
st_save_hour = sys.argv[4]
st_save_mins = sys.argv[5]
st_save_secs = sys.argv[6]
# endtime save
et_save_year = sys.argv[7]
et_save_month = sys.argv[8]
et_save_day = sys.argv[9]
et_save_hour = sys.argv[10]
et_save_mins = sys.argv[11]
et_save_secs = sys.argv[12]

# save directory
save_dir = sys.argv[13]

# format time window
time_window = slarchive_command + st_save_year + ',' + st_save_month + ',' + st_save_day +',' + st_save_hour + ',' + st_save_mins + ',' + st_save_secs + ',' + ':'+ et_save_year +',' + et_save_month + ','+ et_save_day + ',' + et_save_hour + ',' + et_save_mins + ','+ et_save_secs
# format save command
save_command = time_window + save_option + save_dir + sl_host_port

# call save command
os.system(save_command)

