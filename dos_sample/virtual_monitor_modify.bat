@echo off

cd /d "%~dp0"

if "%1" == "" (
    set /a count=1
) else (
    set /a count=%1
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
    for /l %%i in (1, 1, 4) do (
        %installer% enableidd 0
    )
)

