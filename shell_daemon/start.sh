#!/bin/bash

dir=$(cd "$(dirname "$0")"; pwd)

nohup $dir/scheduler_daemon.sh > /dev/null 2>&1 &

exit 0
