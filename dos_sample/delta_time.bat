@echo off

set date_now=%date:~0,10%
set /a date_beg=%date_now:~0,4%%date_now:~5,2%%date_now:~8,2%
set /a date_beg=%date_beg% / 10000 * 365 + %date_beg% %% 10000 / 100 * 30 + %date_beg% %% 100

set time_now=%time:~0,8%
set /a time_beg=%time_now:~0,2%%time_now:~3,2%%time_now:~6,2%
set /a time_beg=%time_beg% / 10000 * 3600 + %time_beg% %% 10000 / 100 * 60 + %time_beg% %% 100

timeout %1 > nul

set date_now=%date:~0,10%
set /a date_end=%date_now:~0,4%%date_now:~5,2%%date_now:~8,2%
set /a date_end=%date_end% / 10000 * 365 + %date_end% %% 10000 / 100 * 30 + %date_end% %% 100

set time_now=%time:~0,8%
set /a time_end=%time_now:~0,2%%time_now:~3,2%%time_now:~6,2%
set /a time_end=%time_end% / 10000 * 3600 + %time_end% %% 10000 / 100 * 60 + %time_end% %% 100

set /a time_delta=(%date_end%-%date_beg%) * 86400 + (%time_end% - %time_beg%)
echo %time_delta% seconds
