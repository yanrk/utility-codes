for /l %%i in (1, 1, 55) do ffmpeg.exe -i "D:\phone\big\big_%%i.jpg" -vf scale=iw*0.2:ih*0.2 D:\phone\small\small_%%i.jpg
