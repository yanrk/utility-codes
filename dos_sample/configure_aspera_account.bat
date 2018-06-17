set username=cust
set password=test
set docroot=C:\
net user %username% %password% /add /y /fullname:"%username%" /comment:"special aspera user" /passwordchg:no /expires:never
WMIC USERACCOUNT WHERE "Name='%username%'" SET PasswordExpires=FALSE
"C:\Program Files (x86)\Aspera\Enterprise Server\bin\asconfigurator.exe" -x set_user_data;user_name,%username%;absolute,%docroot%
"C:\Program Files (x86)\Aspera\Enterprise Server\bin\asconfigurator.exe" -x set_user_data;user_name,%username%;docroot_mask,%docroot%;read_allowed,false
"C:\Program Files (x86)\Aspera\Enterprise Server\bin\asconfigurator.exe" -x set_user_data;user_name,%username%;docroot_mask,%docroot%;write_allowed,false

pause
