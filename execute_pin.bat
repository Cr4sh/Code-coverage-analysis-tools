@echo off

:: delete old files
del CoverageData.* callgrind.out.*

set PINPATH="E:\pin-2.8-37300-msvc9-ia32_intel64-windows"

:: start pin
"%PINPATH%\ia32\bin\pin.exe" -t "%PINPATH%\Coverager.dll" -o "%~dp0\CoverageData.log" -- %*
