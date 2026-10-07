@echo off
if exist C:\COMMAND.COM goto start
echo ERROR: C: is unavailable. Check the FAT16 partition and VM drive geometry.
echo Boot remains at A: for recovery; Freeciv has not been started.
goto end
:start
C:
cd \
call C:\AUTOEXEC.BAT
:end
