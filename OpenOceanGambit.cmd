@echo off
setlocal
if not exist "%~dp0bin\OceanGambit.exe" (
  echo Build Release first. See README.md.
  exit /b 1
)
"%~dp0bin\OceanGambit.exe" %*
exit /b %ERRORLEVEL%
