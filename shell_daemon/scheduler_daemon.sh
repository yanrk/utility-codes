#!/bin/bash

dir=$(cd "$(dirname "$0")/.."; pwd)

log="$dir/daemon/daemon.log"

run_program()
{
	if [ $2 == "scheduler_server" ]; then
		lsof -i :$3 | awk '{print $2}' | xargs kill -15
	fi

	$1/$2 >/dev/null 2>&1 &

	date >> $log
	echo $1/$2 >> $log
	echo "     start" >> $log
}

kill_program()
{
	ps -eo pid,cmd | grep $1/$2 | grep -v grep | awk '{print $1}' | xargs kill -15
	date >> $log
	echo $1/$2 >> $log
	echo "     stop" >> $log
}

program_start()
{
	program_find_count=`ps aux | grep $1/$2 | grep -v grep | wc -l`
	if [ $program_find_count -lt 1 ] ; then
		run_program $1 $2 $3
		return 1
	fi

	program_stop_count=`ps aux | grep $1/$2 | grep -v grep | awk '{print $8}' | grep T | wc -l`
	if [ $program_stop_count -gt 0 ] ; then
		kill_program $1 $2
		sleep 1
		run_program $1 $2 $3
		return 2
	fi

	return 0
}

program_stop()
{
	kill_program $1 $2
}

scheduler_server_start()
{
	program_start "$dir/server" "scheduler_server" "9999"
}

scheduler_server_stop()
{
	program_stop "$dir/server" "scheduler_server"
}

ulimit -n 65535

while true ; do
	scheduler_server_start
	sleep 10
done

exit 0

