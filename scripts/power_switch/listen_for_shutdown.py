#!/usr/bin/env python


import datetime
import RPi.GPIO as GPIO
import subprocess

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(3, GPIO.IN, pull_up_down=GPIO.PUD_UP)

GPIO.setup(23, GPIO.OUT)
GPIO.output(23, GPIO.HIGH)

GPIO.wait_for_edge(3, GPIO.FALLING)

#date_time = datetime.datetime.now()
#f = open("/home/arvid/software/scripts/power_switch/shutdown.log", "w+")
#f.write("Shutdown initiated via swtich at " +str(date_time))

GPIO.output(23, GPIO.LOW)

#subprocess.call(['shutdown', '-h', 'now'], shell=False)
