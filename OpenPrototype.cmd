@echo off
setlocal
if not defined NAVAL_PREVIEW set "NAVAL_PREVIEW=C:\CProj\perevia-osgkit\integrations\InfraredPreview"
if not exist "%NAVAL_PREVIEW%\OpenNavalEffects.cmd" (
  echo Cannot find OpenNavalEffects.cmd under NAVAL_PREVIEW.
  exit /b 1
)
call "%NAVAL_PREVIEW%\OpenNavalEffects.cmd" %*
exit /b %ERRORLEVEL%
