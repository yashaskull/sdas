from subprocess import Popen, PIPE
import json
import re

proc = Popen(['ntpstat', '-q'], shell=True, stdout=PIPE, stderr=PIPE)
out,err = proc.communicate()
print(proc.returncode)
print(str(out))
print(str(err))
#stdout_value = proc.communicate()[0]
stdout_value = out
stdout_value_str = str(stdout_value)
print(stdout_value_str + "\n")

time_index = stdout_value_str.find("time")
ms_index = stdout_value_str.find("ms")

i = time_index
temp = ""


while i <= ms_index+1:
	temp = temp + stdout_value_str[i]
	i = i + 1	
	
print(temp)

i = 0
time_accuracy=""
for i in range(len(temp)):
	if (temp[i].isdigit()):
 		time_accuracy = time_accuracy + temp[i]
 	
	i = i + 1

print(int(time_accuracy))

# 10 ms
if (int(time_accuracy) <= 10):
	print ("start!") 
#print(stdout_value_str.find("ms"))

#print(stdout_value_str[ms_index-2])
#start = stdout_value.find(b"===\n")

