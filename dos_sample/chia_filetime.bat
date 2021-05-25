@echo off

set command="ipconfig | findstr IPv4"
for /f "tokens=2 delims=:" %%a in ('%command%') do (set ip=%%a)
set ip=%ip:~1%

set file_time=
for %%a in ("c:\client\slave\scheduler_slave.exe") do (
    set file_time=%%~ta
)

if "2021/05/24 16:50" equ "%file_time%" (
    echo %file_time% > \\10.40.96.251\tools\check2\equ\%ip%
) else (
    echo %file_time% > \\10.40.96.251\tools\check2\neq\%ip%
)
