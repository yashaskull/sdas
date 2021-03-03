#!/bin/sh

WORKING_DIR=.

cd /home/raspi/sdas/code
#cp bin/Debug/Version4 . not really necessary


if [ ! -e "${WORKING_DIR}/log" ]; then
        mkdir ${WORKING_DIR}/log
fi


echo "Starting main program..."

nohup ./sdas &
PID=$!
echo ${PID} > ${WORKING_DIR}/log/sdas.pid
#python3 /home/raspi/sdas/scripts/monitor_system/system_monitor.py ${PID} 3600 /home/raspi/sdas/code/log &


sleep 1

exit 0
