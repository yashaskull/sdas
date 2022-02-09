"""
import subprocess
command = ['ping', '-n', '1', 'www.google.com']
response = subprocess.call(command)

if response == 0:
	print('True')
else:
	print('False')
"""

from ping3 import ping
import time
import os
from datetime import datetime

ping_test_log = open('pingtest_log.txt', 'a')

# hostname to ping
hostname = 'www.google.com'

client = 'client21'

# hostname ping timeout in seconds
run_ping_test_time = 5 # 5 minutes

# how long to wait to ping after a previous ping fail in seconds
ping_again = 1

# counter to keep track of every ping fail
ping_fail_counter = 0

log_time = datetime.now()
ping_test_log.write('\n****************************************************\n')
ping_test_log.write('Running PING test at '+str(log_time)+'\n')
while True:
	
	if ping_fail_counter == run_ping_test_time:
		print('Cannot reach '+hostname+' after '+str(run_ping_test_time)+'s.')
		ping_test_log.write('Cannot reach '+hostname+' after '+str(run_ping_test_time)+'s.\n')
		#print('Restarting network interface or rebooting OS.')
		print('Restarting openvpn client service')
		ping_test_log.write('Restarting openvpn client service.\n')
		#os.system('systemctl restart openvpn@'+client+'.service')
		#print('systemctl restart openvpn@'+client+'.service')
		ping_test_log.write('****************************************************\n')
		ping_test_log.close()
		exit(0)
	
	resp = ping(hostname)

	if resp == False:
		ping_fail_counter = ping_fail_counter  + 1
		print('Ping test fail. Pinging '+hostname+' again after '+str(ping_again)+'s.')
		time.sleep(ping_again)
		continue
	else:
		print('Ping test pass. Running ping test again after '+str(run_ping_test_time)+'s.')
		ping_test_log.write('Ping test pass. Running ping test again after '+str(run_ping_test_time)+'s.\n')
		ping_test_log.write('****************************************************\n')
		ping_test_log.close()
		exit(0)
	
	#time.sleep(run_ping_test_time)
