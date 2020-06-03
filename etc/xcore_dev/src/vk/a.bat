@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
call ..\..\gpu_paths.bat

set DST_DIR=..\..\bin\data\vk
if not exist %DST_DIR% mkdir %DST_DIR%

goto :start

:cpy
	%GLSL% -V %1 -o %DST_DIR%\%1.spv
	rem copy /BY %1 %DST_DIR%\%1 > nul
	goto :EOF

:start
call :cpy vtx.vert
call :cpy pix.frag


