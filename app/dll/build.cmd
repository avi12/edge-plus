@echo off
REM Build the native split with sighmake (github.com/CitroenGames/sighmake):
REM it reads edgeplus-split.buildscript and generates CMake, which the Build
REM Tools' bundled cmake + ninja compile. Outputs land in build\.
REM (sighmake's vcxproj generator rejects VS Build Tools, so we use its cmake
REM  generator, which the bundled cmake drives fine.)
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo Could not find vcvars64.bat at "%VCVARS%" - edit build.cmd to point at your MSVC.
  exit /b 1
)
call "%VCVARS%" >nul

REM Locate sighmake: PATH first, then the per-user install.
set "SIGHMAKE="
where sighmake >nul 2>&1 && set "SIGHMAKE=sighmake"
if not defined SIGHMAKE if exist "%USERPROFILE%\tools\sighmake\sighmake.exe" set "SIGHMAKE=%USERPROFILE%\tools\sighmake\sighmake.exe"
if not defined SIGHMAKE (
  echo sighmake not found. Install it from github.com/CitroenGames/sighmake
  echo or drop sighmake.exe in "%USERPROFILE%\tools\sighmake\".
  exit /b 1
)

set "ROOT=%~dp0"
set "VSCMAKE=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "CM=%VSCMAKE%\CMake\bin\cmake.exe"
set "NJ=%VSCMAKE%\Ninja\ninja.exe"
if not exist "%CM%" set "CM=cmake"
cd /d "%ROOT%"

echo [1/4] sighmake -^> CMake
"%SIGHMAKE%" edgeplus-split.buildscript -g cmake || goto :err
echo [2/4] cmake configure
"%CM%" -G Ninja -S . -B smninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NJ%" || goto :err
echo [3/4] cmake build
"%CM%" --build smninja || goto :err

echo [4/4] stage outputs
if not exist "%ROOT%build" mkdir "%ROOT%build"
copy /y "%ROOT%smninja\edgeplus-split\edgeplus-split.dll" "%ROOT%build\edgeplus-split.dll" >nul || goto :err
copy /y "%ROOT%smninja\inject\inject.exe" "%ROOT%build\inject.exe" >nul || goto :err

echo.
echo built: %ROOT%build\edgeplus-split.dll  and  %ROOT%build\inject.exe
exit /b 0
:err
echo BUILD FAILED
exit /b 1
