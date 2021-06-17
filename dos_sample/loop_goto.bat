@echo off

set count=0

:loop
set /a count+=1
echo loop %count% ...
timeout 5
goto loop
