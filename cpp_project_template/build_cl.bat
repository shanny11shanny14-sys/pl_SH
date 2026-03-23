@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b 1

cl /EHsc /nologo /Fe:program_build.exe program.cpp
