#!/usr/bin/env python

import time
import datetime
import RPi.GPIO as GPIO
import subprocess

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(3, GPIO.IN, pull_up_down=GPIO.PUD_UP)

loop_counter = 0
#GPIO.wait_for_edge(3, GPIO.FALLING)

while True:
	GPIO.wait_for_edge(3, GPIO.FALLING)
	while True:
		#print(loop_counter)
		if loop_counter == 5: # 5 sec
			date_time = datetime.datetime.now()
			f = open("/home/arvid/projects/sdas/scripts/power_switch/shutdown.log", "w+")
			f.write("Shutdown initiated via swtich at " +str(date_time))
			f.close()
			subprocess.call(['shutdown', '-h', 'now'], shell=False)
			#print("shutdown initiated")
			#loop_counter = 0
			#break
		time.sleep(1)
		if GPIO.input(3) == 0:
			loop_counter = loop_counter + 1
			continue
		else:
			loop_counter = 0
			break


#print("shutdown initiated")
#date_time = datetime.datetime.now()
#f = open("/home/arvid/software/scripts/power_switch/shutdown.log", "w+")
#f.write("Shutdown initiated via swtich at " +str(date_time))

#GPIO.output(23, GPIO.LOW)

#subprocess.call(['shutdown', '-h', 'now'], shell=False)
