@echo off
REM -- First make map file from Microsoft Visual C++ generated resource.h
echo // MAKEHELP.BAT generated Help Map file.  Used by VIDSEND.HPJ. >"hlp\vidsend.hm"
echo. >>"hlp\vidsend.hm"
echo // Commands (ID_* and IDM_*) >>"hlp\vidsend.hm"
makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\vidsend.hm"
echo. >>"hlp\vidsend.hm"
echo // Prompts (IDP_*) >>"hlp\vidsend.hm"
makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\vidsend.hm"
echo. >>"hlp\vidsend.hm"
echo // Resources (IDR_*) >>"hlp\vidsend.hm"
makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\vidsend.hm"
echo. >>"hlp\vidsend.hm"
echo // Dialogs (IDD_*) >>"hlp\vidsend.hm"
makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\vidsend.hm"
echo. >>"hlp\vidsend.hm"
echo // Frame Controls (IDW_*) >>"hlp\vidsend.hm"
makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\vidsend.hm"
REM -- Make help for Project VIDSEND


echo Building Win32 Help files
start /wait hcw /C /E /M "hlp\vidsend.hpj"
if errorlevel 1 goto :Error
if not exist "hlp\vidsend.hlp" goto :Error
if not exist "hlp\vidsend.cnt" goto :Error
echo.
if exist Debug\nul copy "hlp\vidsend.hlp" Debug
if exist Debug\nul copy "hlp\vidsend.cnt" Debug
if exist Release\nul copy "hlp\vidsend.hlp" Release
if exist Release\nul copy "hlp\vidsend.cnt" Release
echo.
goto :done

:Error
echo hlp\vidsend.hpj(1) : error: Problem encountered creating help file

:done
echo.
