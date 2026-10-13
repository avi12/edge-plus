@echo off
REM Build fixedge-split.dll + inject.exe with MSVC. Output in build\.
REM Self-contained: no sighmake, just vcvars64 + cl, since this is one DLL + minhook.
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo Could not find vcvars64.bat at "%VCVARS%" - edit build.cmd to point at your MSVC.
  exit /b 1
)
call "%VCVARS%" >nul
set "ROOT=%~dp0"
set "OUT=%ROOT%build"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"

set "MH=%ROOT%external\minhook"
set "CFLAGS=/nologo /O2 /MT /GS- /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I"%MH%\include""

echo [1/3] minhook
cl %CFLAGS% /c "%MH%\src\buffer.c" "%MH%\src\hde\hde64.c" "%MH%\src\hook.c" "%MH%\src\trampoline.c" || goto :err

echo [2/3] fixedge-split.dll
cl %CFLAGS% /EHsc /std:c++20 /I"%ROOT%src" /LD "%ROOT%src\dllmain.cpp" ^
   buffer.obj hde64.obj hook.obj trampoline.obj version.lib ^
   /Fe:fixedge-split.dll /link /OUT:fixedge-split.dll || goto :err

echo [3/3] inject.exe
cl %CFLAGS% "%ROOT%src\inject.cpp" /Fe:inject.exe || goto :err

popd
echo.
echo built: %OUT%\fixedge-split.dll  and  %OUT%\inject.exe
exit /b 0
:err
popd
echo BUILD FAILED
exit /b 1
