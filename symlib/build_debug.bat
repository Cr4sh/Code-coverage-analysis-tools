@echo off
nmake /f makefile_i386_debug
nmake /f makefile_i386_debug clean
nmake /f makefile_i386_debug-2.5
nmake /f makefile_i386_debug-2.5 clean
