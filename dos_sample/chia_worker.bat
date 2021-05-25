if "startup" == "%1" (
    mkdir %2:\chia_temp\
    mkdir %2:\chia_temp\%3\
    c:\daemon\chia.exe plots create -k 32 -r 2 -n 1 -b 3500 -t %2:\chia_temp\%3\ -d \\%4\%5\ -f 977f8b54ae6c9bea43a5998a71973e9aabb165f3287ec3506989c4ef3220d0f17d605270f6e35774f111006fc360ca51 -p a193ea9ff76264b08407b6bbf8fa7110ae6d4fa28b0074c57d2c861f82b1886bd9c3ade84235f69263c6576bc155c814 >> %2:\chia_temp\%3.log
) else (
    rmdir /s /q %2:\chia_temp\%3\
)
