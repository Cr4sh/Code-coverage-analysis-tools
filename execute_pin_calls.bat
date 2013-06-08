@echo off

:: delete old files
if exist .\logs rmdir .\logs /S /Q
mkdir .\logs

set PINPATH="D:\pin-2.12-56759-msvc9-windows"

if exist .\coverager.dll copy .\coverager.dll %PINPATH%\coverager.dll

:: start pin
"%PINPATH%\ia32\bin\pin.exe" -t "%PINPATH%\coverager.dll" -d .\logs -c -- %*
