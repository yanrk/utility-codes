:: install service
sc create test_service binpath="%~dp0test_service.exe"
sc config test_service start=auto
sc start test_service

:: uninstall service
sc stop test_service
sc delete test_service
