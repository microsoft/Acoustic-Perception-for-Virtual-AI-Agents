:: Copyright (c) Microsoft Corporation. All rights reserved.
:: Licensed under the MIT License.
@echo off
cls

call :PatchFiles Private\AkAudioDevice.cpp AkAudioDevice.cpp.patch 1
call :PatchFiles Public\AkAudioDevice.h AkAudioDevice.h.patch 1

goto :Done

rem args:
rem %~1 file that needs patching
rem %~2 the patch to apply
rem %~3 number of lines from the end of file to start applying patch. All lines after this will be discarded
:PatchFiles
SETLOCAL enableextensions EnableDelayedExpansion
echo Patching %~1 ...

set "filepath=..\..\Wwise\Source\AkAudio\%~1"
set "cmd=findstr /R /N "^^" %filepath% | find /C ":""

rem Count lines in file and compute at what line we need to insert patch
for /f %%a in ('!cmd!') do set number=%%a
set /A number=number-%~3

set "output_file=_tempFile.txt"
set /a linecounter=0
SETLOCAL DisableDelayedExpansion
type nul > "%output_file%"

for /f "tokens=1,* delims=]" %%a in ('type %filepath% ^| find /V /N ""') do (
    set "line=%%b"
    (echo.%%b)>>%output_file%
    call :Increment
    SETLOCAL EnableDelayedExpansion

    if !linecounter!==!number! goto :break
    ENDLOCAL
)
ENDLOCAL

:break

if NOT %~2==NoFilePatch (
	echo Applying patch file %~2
	SETLOCAL DisableDelayedExpansion
	rem append our patch to the file
	for /f "tokens=1,* delims=]" %%a in ('type "%~2" ^| find /V /N ""') do (
	    set "line=%%b"
	    (echo.%%b)>>%output_file%
	)
	ENDLOCAL
)

rem replace the original
copy %output_file% %filepath%
del %output_file%

EXIT /B /0

:Increment
set /a linecounter+=1
EXIT /B /0

:Done