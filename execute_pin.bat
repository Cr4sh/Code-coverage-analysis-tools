@echo off

:: delete old files
if exist .\Logs rmdir .\Logs /S /Q
mkdir .\Logs

set PINPATH="E:\pin-2.8-37300-msvc9-ia32_intel64-windows"

:: start pin
"%PINPATH%\ia32\bin\pin.exe" -t "%PINPATH%\Coverager.dll" -d .\Logs -- %*
