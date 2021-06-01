timeout /t 5 /nobreak > nul 2>&1

set gpuid=%7
set script_task=%1
set script_file="c:\task\%6\script\%script_task:~0,8%\script_%1_%2.py"
set python_exec="c:\python27\python.exe"
set output_file="c:\task\%6\user\%script_task:~0,8%\%1\run_%2.log"

md "c:\task\%6\user\%script_task:~0,8%\%1\" > nul 2>&1

echo ---------------------------- start ---------------------------- >> %output_file% 2>&1

echo %date:~0,4%-%date:~5,2%-%date:~8,2% %time:~0,2%:%time:~3,2%:%time:~6,2% >> %output_file% 2>&1

echo run.bat stop resource_monitor.exe first time begin >> %output_file% 2>&1
taskkill /f /im resource_monitor.exe > nul 2>&1
echo run.bat stop resource_monitor.exe first time end >> %output_file% 2>&1

if not exist %script_file% (
    echo error: %script_file% is not exist >> %output_file% 2>&1
    exit 101
)

for %%a in (%script_file%) do (
    if 0 equ %%~za (
        echo error: %script_file% is empty >> %output_file% 2>&1
        exit 102
    )
)

echo %python_exec% %script_file% "%1" "%2" "%3" "%4" "%5" "%8" >> %output_file% 2>&1

set date_now=%date:~0,10%
set /a date_beg=%date_now:~0,4%%date_now:~5,2%%date_now:~8,2%
set /a date_beg=%date_beg% / 10000 * 365 + %date_beg% %% 10000 / 100 * 30 + %date_beg% %% 100

set time_now=%time:~0,8%
set /a time_beg=%time_now:~0,2%%time_now:~3,2%%time_now:~6,2%
set /a time_beg=%time_beg% / 10000 * 3600 + %time_beg% %% 10000 / 100 * 60 + %time_beg% %% 100

%python_exec% %script_file% "%1" "%2" "%3" "%4" "%5" "%8" >> %output_file% 2>&1

set result=%errorlevel%

set date_now=%date:~0,10%
set /a date_end=%date_now:~0,4%%date_now:~5,2%%date_now:~8,2%
set /a date_end=%date_end% / 10000 * 365 + %date_end% %% 10000 / 100 * 30 + %date_end% %% 100

set time_now=%time:~0,8%
set /a time_end=%time_now:~0,2%%time_now:~3,2%%time_now:~6,2%
set /a time_end=%time_end% / 10000 * 3600 + %time_end% %% 10000 / 100 * 60 + %time_end% %% 100

set /a min_delta_time=5
set /a delta_time=(%date_end%-%date_beg%) * 86400 + (%time_end% - %time_beg%)

echo result is %result% >> %output_file% 2>&1

echo run.bat stop resource_monitor.exe last time begin >> %output_file% 2>&1
taskkill /f /im resource_monitor.exe > nul 2>&1
echo run.bat stop resource_monitor.exe last time end >> %output_file% 2>&1

if not exist %output_file% (
    echo error: %output_file% is not exist >> %output_file% 2>&1
    exit 103
)

if %delta_time% lss %min_delta_time% (
    echo error: script execute time %delta_time% is too short >> %output_file% 2>&1
    exit 104
)

echo exit with %result% >> %output_file% 2>&1

exit %result%
