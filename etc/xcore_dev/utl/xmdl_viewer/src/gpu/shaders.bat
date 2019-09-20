@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set SED=D:\Tools\Prog\cygwin64\bin\sed.exe
set EXPR='s:\r::g; s:\\:\\\\:g; s:":\\":g; s:\t:\\t:g; s:.*:\t"&\\n":'

set INC_FILE=..\shaders.inc

set FILE_LST='dir /b *.vert *.frag'


echo /* shaders */ > %INC_FILE%
for /f %%i in (%FILE_LST%) do (
	echo %%i
	set NAME=%%~ni
	set KIND=%%~xi
	set ID=!NAME!_!KIND:~1!
	echo static const char* s_!ID! = >> %INC_FILE%
	%SED% !EXPR! %%i >> %INC_FILE%
	echo ; >> %INC_FILE%
)
