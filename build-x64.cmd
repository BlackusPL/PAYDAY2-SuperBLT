@echo off
rem Build x64 Release WSOCK32.dll for PAYDAY 2 (Diesel 3.0)
rem Requires: VS 2022 Build Tools with C++ workload
setlocal
call "E:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
cd build
cmake .. -A x64 -G "Visual Studio 17 2022" || exit /b 1
msbuild SuperBLT.sln /t:Build /p:Configuration=Release /m /v:m || exit /b 1
echo.
echo Build finished: %~dp0build\Release\WSOCK32.dll
