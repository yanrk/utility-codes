@echo off

set file_count=0
for /f %%i in ('dir /b c:\tools\find') do (
    for /f "delims=\n" %%j in (c:\tools\find\%%i) do (
        set /a file_count+=%%j
    )
)
echo %file_count%

pause
