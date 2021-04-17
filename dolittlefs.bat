set COM=com4
set ESPTOOL=D:\Users\owen\AppData\Local\Programs\Python\Python39\scripts\esptool.py.exe
echo ESPTOOL: %ESPTOOL%

mklittlefs -d 2 -c data -b 8192 -p 256 -s 0xfb000 littlefs.bin

echo 4MB (FS:1MB OTA:~1019KB)
"%ESPTOOL%" -p %COM% -b 921600 write_flash 0x300000 littlefs.bin

exit /b
