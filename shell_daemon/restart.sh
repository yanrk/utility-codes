#!/bin/bash

dir=$(cd "$(dirname "$0")"; pwd)

echo stop...
$dir/stop.sh > /dev/null 2>&1 &
echo start...
$dir/start.sh > /dev/null 2>&1 &
echo done...
