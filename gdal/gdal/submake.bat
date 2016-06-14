@echo off
cd %1
nmake /NOLOGO /f makefile.vc %2
if ERRORLEVEL 1 exit 1
cd ..
