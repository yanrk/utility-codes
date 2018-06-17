set username=cust
set password=test
net user %username% %password% /add /y /fullname:"%username%" /comment:"special aspera user" /passwordchg:no /expires:never 
WMIC USERACCOUNT WHERE "Name='%username%'" SET PasswordExpires=FALSE

pause
