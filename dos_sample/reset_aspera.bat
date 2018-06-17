taskkill /f /im sshd.exe
taskkill /f /im ascp.exe
net stop sshd
taskkill /f /im sshd.exe
net start sshd
net start sshd