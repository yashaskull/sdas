#!/bin/sh

WORKING_DIR=.

cd /home/arvid/software/Version4
cp bin/Debug/Version4 .


if [ ! -e "${WORKING_DIR}/log" ]; then
        mkdir ${WORKING_DIR}/log
fi


echo "Starting main program..."

nohup ./Version4 &
PID=$!
echo ${PID} > ${WORKING_DIR}/log/Version4.pid
python3 /home/arvid/software/scripts/monitor_system/system_monitor.py ${PID} 3600 /home/arvid/software/Version4/log &


sleep 1

exit 0
