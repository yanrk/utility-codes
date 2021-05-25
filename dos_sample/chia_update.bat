taskkill /t /im chia.exe
taskkill /t /im daemon.exe
taskkill /t /im sweeper.exe
taskkill /t /im scheduler_slave.exe

rmdir /s /q c:\client\slave\log\ > nul 2>&1
rmdir /s /q d:\chia_temp\ > nul 2>&1
rmdir /s /q e:\chia_temp\ > nul 2>&1
rmdir /s /q f:\chia_temp\ > nul 2>&1
rmdir /s /q g:\chia_temp\ > nul 2>&1
rmdir /s /q h:\chia_temp\ > nul 2>&1
rmdir /s /q i:\chia_temp\ > nul 2>&1

mkdir c:\client\
mkdir c:\client\daemon\
mkdir c:\client\daemon\cfg\
mkdir c:\client\slave\
mkdir c:\client\slave\cfg\
mkdir c:\client\sweeper\

copy /y \\10.40.96.251\shared\client\daemon\*.* c:\client\daemon\
copy /y \\10.40.96.251\shared\client\daemon\cfg\*.* c:\client\daemon\cfg\
echo "update daemon finished"

copy /y \\10.40.96.251\shared\client\sweeper\*.* c:\client\sweeper\
echo "update sweeper finished"

copy /y \\10.40.96.251\shared\client\slave\*.* c:\client\slave\
copy /y \\10.40.96.251\shared\client\slave\cfg\*.* c:\client\slave\cfg\
copy /y \\10.40.96.251\shared\config\worker\*.* c:\client\slave\cfg\
echo "update slave finished"

start c:\client\daemon\daemon.exe
echo "daemon.exe started"
