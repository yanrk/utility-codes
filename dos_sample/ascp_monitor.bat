@echo off
setlocal enabledelayedexpansion
set /a cpu_min=85
set /a ascp_max=15
set /a check_seconds=30
set /a cpu_max=99
set /a ascp_min=%ascp_max%/3
set /a check_ms=%check_seconds%*1000
for /l %%i in (0, 0, 1) do (
    set ascp_count=
    for /f %%j in ('tasklist ^| findstr ascp.exe ^| findstr /r /n "^" ^| find /c ":"') do (set /a ascp_count=%%j)
    echo ascp count is !ascp_count!
    set use_cpu=
    for /f "tokens=2 delims==" %%k in (
        'wmic path Win32_PerfFormattedData_PerfOS_Processor get PercentProcessorTime /value ^| findstr "PercentProcessorTime" ^| findstr /R /N "^" ^| findstr "1:"'
    ) do (set /a use_cpu=%%k)
    echo use cpu is !use_cpu!
    if !ascp_count! geq %ascp_max% (
        echo "ascp count: (!ascp_count! > %ascp_max%)"
        taskkill /f /im ascp.exe
    ) else (
        if !ascp_count! geq %ascp_min% (
            if !use_cpu! geq %cpu_min% (
                echo "ascp count: (!ascp_count! > %ascp_min%) and use cpu: (!use_cpu! > %cpu_min%)"
                taskkill /f /im ascp.exe
            )
        ) else (
            if !use_cpu! geq %cpu_max% (
                echo "ascp count: (!ascp_count! <= %ascp_min%) and use cpu: (!use_cpu! > %cpu_max%)"
                taskkill /f /im ascp.exe
            ) else (
                echo "all is ok"
            )
        )
    )
    echo "check once..."
    ping 1.1.1.1 -n 1 -w %check_ms% > nul
)
pause

:: equ(==), neq(!=), lss(<), gtr(>), leq(<=), geq(>=)
