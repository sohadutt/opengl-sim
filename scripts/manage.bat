@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"
set "PROJECT_NAME=opengl-sim"
set "COPY_SINGLE=0"
set "AUTO_SETUP=0"
set "COMMAND_NAME=%~1"

if /I "%COMMAND_NAME%"=="--help" goto :help
if /I "%COMMAND_NAME%"=="-h" goto :help
if not defined COMMAND_NAME goto :usage_error
shift

:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="--single" (
    if /I "%COMMAND_NAME%"=="clean" (
        echo Error: --single cannot be used with clean.
        exit /b 2
    )
    set "COPY_SINGLE=1"
    shift
    goto :parse_args
)
if /I "%~1"=="--auto" (
    if /I "%COMMAND_NAME%"=="clean" (
        echo Error: --auto cannot be used with clean.
        exit /b 2
    )
    set "AUTO_SETUP=1"
    shift
    goto :parse_args
)
if /I "%~1"=="--help" goto :help
if /I "%~1"=="-h" goto :help
echo Error: unknown option "%~1".
goto :usage_error

:args_done
if /I "%COMMAND_NAME%"=="clean" goto :clean
if /I not "%COMMAND_NAME%"=="build" if /I not "%COMMAND_NAME%"=="run" if /I not "%COMMAND_NAME%"=="test" goto :usage_error

if "%AUTO_SETUP%"=="1" call :auto_setup
if errorlevel 1 exit /b 1
call :prepare_gcc
if errorlevel 1 exit /b 1

echo Configuring with GCC and Ninja...
if defined MSYS2_ROOT (
    "%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ "-DCMAKE_PREFIX_PATH=%MSYS2_ROOT%\ucrt64"
) else (
    "%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
)
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b %errorlevel%

set "EXECUTABLE=%BUILD_DIR%\%PROJECT_NAME%.exe"
if not exist "%EXECUTABLE%" (
    echo Error: expected executable was not created at "%EXECUTABLE%".
    exit /b 1
)

if "%COPY_SINGLE%"=="1" (
    copy /Y "%EXECUTABLE%" "%ROOT_DIR%\%PROJECT_NAME%.exe" >nul
    if errorlevel 1 exit /b %errorlevel%
    echo Executable copied to "%ROOT_DIR%\%PROJECT_NAME%.exe"
)

if /I "%COMMAND_NAME%"=="test" (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
    if errorlevel 1 exit /b 1
    exit /b 0
)

if /I "%COMMAND_NAME%"=="run" (
    pushd "%ROOT_DIR%"
    "%EXECUTABLE%"
    set "RUN_RESULT=%ERRORLEVEL%"
    popd
    exit /b %RUN_RESULT%
)
exit /b 0

:prepare_gcc
if not defined CMAKE_EXE set "CMAKE_EXE=cmake"

rem Derive the MSYS2 root from g++ when its UCRT64 bin directory is on PATH.
if defined MSYS2_ROOT goto :root_ready
set "GCC_EXE="
for /f "delims=" %%G in ('where g++.exe 2^>nul') do if not defined GCC_EXE set "GCC_EXE=%%G"
if not defined GCC_EXE goto :root_ready
for %%G in ("%GCC_EXE%") do set "GCC_BIN=%%~dpG"
for %%I in ("%GCC_BIN%..\..") do set "MSYS2_ROOT=%%~fI"
:root_ready
if not defined MSYS2_ROOT if exist "C:\msys64\ucrt64\bin\g++.exe" set "MSYS2_ROOT=C:\msys64"

if defined MSYS2_ROOT (
    set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"
)

where "%CMAKE_EXE%" >nul 2>nul
if errorlevel 1 (
    echo Error: CMake was not found. Install it or use --auto.
    exit /b 1
)
where g++ >nul 2>nul
if errorlevel 1 (
    echo Error: g++ was not found. Install GCC in MSYS2 UCRT64 and add its ucrt64\bin folder to PATH, or use --auto.
    exit /b 1
)
where ninja >nul 2>nul
if errorlevel 1 (
    echo Error: Ninja was not found. Install mingw-w64-ucrt-x86_64-ninja in MSYS2 UCRT64, or use --auto.
    exit /b 1
)
exit /b 0

:auto_setup
echo Checking for MSYS2 and installing the UCRT64 GCC toolchain and libraries...

where winget >nul 2>nul
if errorlevel 1 (
    echo Error: --auto requires Windows Package Manager winget.
    echo Install MSYS2 and its UCRT64 GCC packages manually, or install winget.
    exit /b 1
)

if not defined MSYS2_ROOT if exist "C:\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=C:\msys64"
if not defined MSYS2_ROOT if exist "%ProgramFiles%\MSYS2\usr\bin\bash.exe" set "MSYS2_ROOT=%ProgramFiles%\MSYS2"
if not defined MSYS2_ROOT if exist "%LOCALAPPDATA%\Programs\MSYS2\usr\bin\bash.exe" set "MSYS2_ROOT=%LOCALAPPDATA%\Programs\MSYS2"

if not defined MSYS2_ROOT (
    echo Installing MSYS2 with winget. The installer may ask for confirmation...
    winget install --id MSYS2.MSYS2 --exact --accept-package-agreements --accept-source-agreements
    if errorlevel 1 exit /b 1
    if exist "C:\msys64\usr\bin\bash.exe" set "MSYS2_ROOT=C:\msys64"
    if not defined MSYS2_ROOT if exist "%ProgramFiles%\MSYS2\usr\bin\bash.exe" set "MSYS2_ROOT=%ProgramFiles%\MSYS2"
    if not defined MSYS2_ROOT if exist "%LOCALAPPDATA%\Programs\MSYS2\usr\bin\bash.exe" set "MSYS2_ROOT=%LOCALAPPDATA%\Programs\MSYS2"
)

if not defined MSYS2_ROOT (
    echo Error: Could not find MSYS2. Set MSYS2_ROOT to its installation folder and retry.
    exit /b 1
)

"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "pacman -Syu --noconfirm"
if errorlevel 1 exit /b 1
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-glew"
if errorlevel 1 exit /b 1

set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"
set "CMAKE_EXE=%MSYS2_ROOT%\ucrt64\bin\cmake.exe"
exit /b 0

:clean
if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
echo Removed "%BUILD_DIR%"
exit /b 0

:help
echo Usage:
echo   scripts\manage.bat build [--single] [--auto]
echo   scripts\manage.bat run [--single] [--auto]
echo   scripts\manage.bat test [--auto]
echo   scripts\manage.bat clean
echo.
echo Commands:
echo   build    Configure and compile with GCC and Ninja.
echo   run      Build and run the project.
echo   test     Build and run the unit tests.
echo   clean    Remove the build directory.
echo.
echo Options:
echo   --single Copy the built executable into the project root (build/run only).
echo   --auto   Install MSYS2 and the UCRT64 GCC/CMake/Ninja/GLFW/GLM/GLEW packages with winget and pacman.
exit /b 0

:usage_error
echo Usage: scripts\manage.bat build [--single] [--auto] ^| run [--single] [--auto] ^| test [--auto] ^| clean ^| --help
exit /b 2
