@echo off

set command="ipconfig | findstr IPv4"
for /f "tokens=2 delims=:" %%a in ('%command%') do (set ip=%%a)
set ip=%ip:~1%

if not exist "G:" (
    exit
)

set file_count=0
for %%i in (D E F G H I) do (
    if exist %%i: (
        for /f %%j in ('dir /b %%i:\*.plot ^| findstr /r /n "^" ^| find /c ":"') do (
            set /a file_count+=%%j
        )
    )
)
echo %file_count% > \\10.40.96.251\tools\find\%ip%
