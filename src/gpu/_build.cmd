@echo off
setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set _FXC_="%ProgramFiles(x86)%\Windows Kits\8.1\bin\x86\fxc.exe" /nologo
set _OUTDIR_=code
if not exist %_OUTDIR_% mkdir %_OUTDIR_%

goto :start

:build_vtx
set NAME=%1
%_FXC_% /O3 /Gfp /Tvs_5_0 /Vn%NAME% /Fh%_OUTDIR_%\%NAME%.h %NAME%.hlsl
goto :EOF

:build_pix
set NAME=%1
%_FXC_% /O3 /Gfp /Tps_5_0 /Vn%NAME% /Fh%_OUTDIR_%\%NAME%.h %NAME%.hlsl
goto :EOF

:build_hul
set NAME=%1
%_FXC_% /O3 /Gfp /Ths_5_0 /Vn%NAME% /Fh%_OUTDIR_%\%NAME%.h %NAME%.hlsl
goto :EOF

:build_dom
set NAME=%1
%_FXC_% /O3 /Gfp /Tds_5_0 /Vn%NAME% /Fh%_OUTDIR_%\%NAME%.h %NAME%.hlsl
goto :EOF

:start
call :build_vtx vs_obj
call :build_pix ps_mtl
call :build_vtx vs_sdw
call :build_pix ps_sdw

call :build_vtx vs_env
call :build_pix ps_env

call :build_hul hs_pntri
call :build_dom ds_pntri
