@echo off

cd /d "%~dp0"

set /a count=%1
if %count% equ 0 (
    goto end
)

if "AMD64" == "%processor_architecture%" (
    set installer=deviceinstaller64
) else (
    set installer=deviceinstaller
)

for /f "tokens=1" %%k in (
    '%installer% find usbmmidd ^| findstr ":" ^| findstr /R /N "^" ^| find /C ":"'
) do (set /a installed=%%k)

if %installed% equ 0 (
    %installer% install usbmmidd.inf usbmmidd
)

if %count% gtr 0 (
    for /l %%i in (1, 1, %count%) do (
        %installer% enableidd 1
    )
) else (
    for /l %%i in (-1, -1, %count%) do (
        %installer% enableidd 0
    )
)

:end
