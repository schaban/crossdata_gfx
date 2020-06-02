@echo off
set GLSL=D:\Tools\Prog\VulkanSDK\1.0.54.0\Bin\glslangValidator.exe
if not exist %GLSL% (set GLSL=D:\Tools\Prog\VulkanSDK\1.0.21.1\Bin\glslangValidator.exe)
