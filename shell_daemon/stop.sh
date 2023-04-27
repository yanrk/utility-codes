#!/bin/bash

ps -eo pid,cmd | grep scheduler_daemon | grep -v grep | awk '{print $1}' | xargs kill -9
ps -eo pid,cmd | grep scheduler_server | grep -v grep | awk '{print $1}' | xargs kill -15

exit 0
