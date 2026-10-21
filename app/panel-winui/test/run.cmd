@echo off
REM Build + run the panel's pure-logic tests (no WinUI, no Edge, no Win32).
dotnet run --project "%~dp0EdgePlus.Tests.csproj" -c Release
exit /b %ERRORLEVEL%
