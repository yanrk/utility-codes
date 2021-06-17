@echo off

powershell -command "(type c:\src.bat) -replace 'template.exe', 'app.exe' | out-file -encoding ascii c:\dst.bat"
powershell -command "(gc c:\src.txt) -replace 'xxxxxx', $env:computername | out-file -encoding ascii c:\dst.txt"
