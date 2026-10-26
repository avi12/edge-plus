@echo off
REM Build + run every pure unit test (*_test.cpp; host, no Edge deps).
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (echo missing vcvars64.bat & exit /b 1)
call "%VCVARS%" >nul
set "ROOT=%~dp0"
pushd "%ROOT%"
set "RC=0"
for /r "%ROOT%" %%T in (*_test.cpp) do (
  echo [%%~nT]
  cl /nologo /EHsc /std:c++20 /W4 /I "%ROOT%..\src" /Fe:%%~nT.exe "%%T" >build.log 2>&1 || (type build.log & set "RC=1")
  if exist %%~nT.exe (call "%ROOT%%%~nT.exe" || set "RC=1")
  del /q %%~nT.exe %%~nT.obj build.log 2>nul
)
popd
exit /b %RC%
