#!/usr/bin/evn python

import os
from subprocess import Popen, PIPE
import time
import timeout_decorator
import datetime


time_threshold = 50 # ms

def startup_routine():
	
	# start ntp server
	startup_log.write("Attempting to start ntp server...\n")
	os.system("rm /usr/sbin/ntpd")
	os.system("cp /home/raspi/sdas/ntp/ntpd /usr/sbin/ntpd")
	proc = Popen("service ntp restart", shell=True,stdout=PIPE, stderr=PIPE)
	out,err = proc.communicate()
	exitcode=proc.returncode
	if (exitcode != 0):
		startup_log.write("out: "+str(out)+"\n")			
		startup_log.write("err: "+str(err)+"\n")
		startup_log.write("Failed to start ntp server...\n")
		return -1
	
	startup_log.write("NTP server started!\n")
	
	startup_log.write("\n")
	
	# start ring buffer
	startup_log.write("Attempting to start ringserver...\n")
	startup_log.flush()
	proc = Popen("/home/raspi/sdas/code/ringserver/start_ringserver.sh", shell=True, stdout=PIPE, stderr=PIPE)
	out,err = proc.communicate()
	exitcode=proc.returncode
	#print(exitcode)
	if (exitcode != 0):
		startup_log.write("out: "+str(out)+"\n")			
		startup_log.write("err: "+str(err)+"\n")
		startup_log.write("Failed to start ringserver...\n")
		startup_log.flush()
		return -1
		
	startup_log.write("ringserver started!\n")
	startup_log.write("\n")
	
	
	# compile arduino code
	startup_log.write("Attempting to compile arduino code...\n")
	startup_log.flush()
	Popen("arduino-cli core update-index", shell=True,stdout=PIPE,stderr=PIPE)
	proc = Popen("arduino-cli compile --fqbn arduino:avr:mega /home/raspi/sdas/arduino/3_channel", shell=True,stdout=PIPE, stderr=PIPE)
	out,err = proc.communicate()
	if (proc.returncode != 0):
		startup_log.write("out: "+str(out)+"\n")			
		startup_log.write("err: "+str(err)+"\n")
		startup_log.write("Failed to compile arduino code...\n")
		startup_log.flush()
		return -1
			
	startup_log.write("Arduino code compiled!\n")
	
	startup_log.write("\n")
	time.sleep(1)
	
	# upload arduino code
	startup_log.write("Attempting to upload arduino code...\n")
	startup_log.flush()
	try:
		list = run_arduino()
		exitcode = list[2]
		#print(exitcode)
		startup_log.write(list[3] + "\n")
		if (exitcode != 0):
			startup_log.write("err: "+str(list[1])+" \n")
			startup_log.write("out: "+str(list[0])+" \n")
			startup_log.write("Failed to upload arduino code...\n")
			startup_log.flush()
			return -1
		
		startup_log.write("Arduino code uploaded!\n")
	
	except():
		startup_log.write("Uploading arduino code timed out...\n")
		return -1
	

	startup_log.write("\n")
	# Start acquisition system
	# may need to add timeout in case it never syncs ?
	startup_log.write("Waiting for time synchronization to be correct within "+str(time_threshold)+" ms before beginning program.\n")	
	startup_log.flush()
	# if after 5 minutes time doesnt not sync, the program will still start in hopes that time sync happens later.
	# program updates time periodically, so data will eventually have correct timestamps.
	timeout_counter = 0
	while True:
		if (timeout_counter == 60):
			startup_log.write("ntp did not sync within 5 minutes of startup. Starting program anyway.\n")
			break
		
		time_accuracy = ntp_status()
		if (time_accuracy == -1):
			return -1
	
		if (time_accuracy <= time_threshold):
			break
		
		time.sleep(5)
		timeout_counter = timeout_counter + 1
		
	# begin acquisition system	 
	startup_log.write("Beginning main program at "+str(datetime.datetime.now())+" UTC\n")
	startup_log.flush()
	proc = Popen("/home/raspi/sdas/code/start_temp.sh", shell=True)
	out,err = proc.communicate()
	if (proc.returncode != 0):
		startup_log.write("err: "+str(err)+" \n")
		startup_log.write("out: "+str(out)+" \n")
		startup_log.write("Failed to start main program...\n")
		startup_log.flush()
		return -1


	
	startup_log.write("Main program started!\n")
	startup_log.flush()
	return 1

@timeout_decorator.timeout(30, use_signals=False)
def run_arduino():
	#os.system("env USER=arvid")
	proc = Popen("echo $HOME", shell=True, stdout=PIPE, stderr=PIPE)
	out, err = proc.communicate()
	#print(out)
	strs = str(out)
	
	proc = Popen("arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega /home/raspi/sdas/arduino/3_channel", shell=True, stdout=PIPE, stderr=PIPE)
	out,err = proc.communicate()
	return [out, err, proc.returncode, strs]
	



def ntp_status():

	proc = Popen(['ntpstat', '-q'], shell=True, stdout=PIPE, stderr=PIPE)
	out,err = proc.communicate()
	if (proc.returncode != 0):
		startup_log.write("err: "+str(err)+" \n")
		startup_log.write("out: "+str(out)+" \n")
		startup_log.write("Failed to get ntp status...\n")
		startup_log.flush()
		return -1

	stdout_value = out
	stdout_value_str = str(stdout_value)
	
	time_index = stdout_value_str.find("time")
	ms_index = stdout_value_str.find("ms")

	i = time_index
	temp = ""

	while i <= ms_index+1:
		temp = temp + stdout_value_str[i]
		i = i + 1	
	
	i = 0
	time_accuracy=""
	for i in range(len(temp)):
		if (temp[i].isdigit()):
 			time_accuracy = time_accuracy + temp[i]
 	
		i = i + 1

	print(int(time_accuracy))

	return int(time_accuracy) 

# Main routine
if __name__ == '__main__':
	
	# log file to log output of startup commands
	startup_log = open("/home/raspi/sdas/scripts/startup/startup_log.log", "w+")
	
	# beging startup routine
	rv = startup_routine()
	if (rv == -1):
		startup_log.write("Error in start up configuration. Fix errors then reboot system.\n")
	
	startup_log.close()	
	
	
	
	
	
	

