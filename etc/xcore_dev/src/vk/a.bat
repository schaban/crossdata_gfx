@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
set GLSL=D:\Tools\Prog\VulkanSDK\1.0.54.0\Bin\glslangValidator.exe

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


