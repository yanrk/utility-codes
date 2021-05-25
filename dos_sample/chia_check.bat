@echo off

set command="ipconfig | findstr IPv4"
for /f "tokens=2 delims=:" %%a in ('%command%') do (set ip=%%a)
set ip=%ip:~1%

set disk_count=0
for %%i in (D E F G H I) do (
    if exist %%i: (
        set /a disk_count+=1
    )
)

set process_count=0
for /f %%i in ('tasklist ^| findstr chia.exe ^| findstr /r /n "^" ^| find /c ":"') do (
	set /a process_count=%%i
)

set /a min_check_count=0
set /a max_check_count=%disk_count%*20
if %process_count% gtr %min_check_count% (
    if %process_count% gtr %max_check_count% (
        echo %process_count% > \\10.40.96.251\tools\check\over\%ip%
    ) else (
        echo %process_count% > \\10.40.96.251\tools\check\less\%ip%
    )
)
