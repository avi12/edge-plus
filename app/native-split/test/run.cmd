@echo off
REM Build + run the pure pane-tree unit tests (host, no Edge deps).
setlocal
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (echo missing vcvars64.bat & exit /b 1)
call "%VCVARS%" >nul
set "ROOT=%~dp0"
pushd "%ROOT%"
cl /nologo /EHsc /std:c++20 /W4 /Fe:pane_tree_test.exe "%ROOT%pane_tree_test.cpp" >build.log 2>&1 || (type build.log & popd & exit /b 1)
call "%ROOT%pane_tree_test.exe"
set "RC=%ERRORLEVEL%"
del /q pane_tree_test.exe pane_tree_test.obj build.log 2>nul
popd
exit /b %RC%
