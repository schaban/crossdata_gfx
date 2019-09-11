@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
set GLSL=D:\Tools\Prog\VulkanSDK\1.0.54.0\Bin\glslangValidator.exe

set DST_DIR=..\..\bin\data\ogl_x
if not exist %DST_DIR% mkdir %DST_DIR%

goto :start

:cpy
	rem %GLSL% -V %1
	echo %1
	%GLSL% %1
	copy /BY %1 %DST_DIR%\%1 > nul
	goto :EOF

:start
call :cpy vtx.vert
call :cpy pix.frag


