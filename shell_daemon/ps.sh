#!/bin/bash

ps -eo pid,cmd | grep scheduler_daemon | grep -v grep
ps -eo pid,cmd | grep scheduler_server | grep -v grep
