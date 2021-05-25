@echo off

set command="ipconfig | findstr IPv4"
for /f "tokens=2 delims=:" %%a in ('%command%') do (set ip=%%a)
set ip=%ip:~1%

rmdir /s /q C:\client\slave\log\

for %%i in (D E F G H I) do (
    if exist %%i:\chia_temp\ (
        rmdir /s /q %%i:\chia_temp\
    )
)

echo "" > \\10.40.96.251\tools\clean\%ip%
