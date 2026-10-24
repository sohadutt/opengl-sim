@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%ROOT_DIR%\build"
set "PROJECT_NAME=opengl-sim"
set "COPY_SINGLE=0"
set "AUTO_SETUP=0"
set "CMAKE_TOOLCHAIN_FILE="
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

if not defined CMAKE_EXE set "CMAKE_EXE=cmake"
if defined CMAKE_TOOLCHAIN_FILE (
    "%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -A %CMAKE_GENERATOR_PLATFORM% -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE%" "-DVCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET%"
) else (
    "%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
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

:auto_setup
echo Checking Windows build tools and installing project dependencies...

where winget >nul 2>nul
if errorlevel 1 (
    echo Error: --auto requires Windows Package Manager winget.
    echo Install winget, or install CMake, Visual Studio C++ Build Tools, GLFW, GLM, and GLEW manually.
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo Installing CMake...
    winget install --id Kitware.CMake --exact --accept-package-agreements --accept-source-agreements
    if errorlevel 1 exit /b 1
    if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
    if defined CMAKE_EXE set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
)

if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo Installing Visual Studio 2022 C++ Build Tools. This download may be large...
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --accept-package-agreements --accept-source-agreements --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if errorlevel 1 exit /b 1
)

if defined VCPKG_ROOT if exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" goto :vcpkg_ready
set "VCPKG_ROOT=%LOCALAPPDATA%\opengl-sim\vcpkg"
if exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" goto :vcpkg_ready

echo Downloading vcpkg into "%VCPKG_ROOT%"...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $base=Split-Path -Parent $env:VCPKG_ROOT; New-Item -ItemType Directory -Force -Path $base | Out-Null; $zip=Join-Path $base 'vcpkg-source.zip'; Invoke-WebRequest -Uri 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip' -OutFile $zip; Expand-Archive -LiteralPath $zip -DestinationPath $base -Force; if (Test-Path $env:VCPKG_ROOT) { Remove-Item -LiteralPath $env:VCPKG_ROOT -Recurse -Force }; Move-Item -LiteralPath (Join-Path $base 'vcpkg-master') -Destination $env:VCPKG_ROOT; Remove-Item -LiteralPath $zip -Force"
if errorlevel 1 (
    echo Error: vcpkg download failed. Check the network connection and retry.
    exit /b 1
)

:vcpkg_ready
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat" -disableMetrics
    if errorlevel 1 exit /b 1
)

set "VCPKG_TRIPLET=x64-windows"
set "CMAKE_GENERATOR_PLATFORM=x64"
if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set "VCPKG_TRIPLET=arm64-windows"
    set "CMAKE_GENERATOR_PLATFORM=ARM64"
)
if /I "%PROCESSOR_ARCHITECTURE%"=="x86" (
    set "VCPKG_TRIPLET=x86-windows"
    set "CMAKE_GENERATOR_PLATFORM=Win32"
)

echo Installing GLFW, GLM, and GLEW for %VCPKG_TRIPLET%...
"%VCPKG_ROOT%\vcpkg.exe" install glfw3 glm glew --triplet %VCPKG_TRIPLET%
if errorlevel 1 exit /b 1

set "CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
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
echo   build    Configure and compile the project.
echo   run      Build and run the project.
echo   test     Build and run the unit tests.
echo   clean    Remove the build directory.
echo.
echo Options:
echo   --single Copy the built executable into the project root (build/run only).
echo   --auto   Install CMake, Visual Studio C++ Build Tools, and GLFW/GLM/GLEW with winget and vcpkg.
exit /b 0

:usage_error
echo Usage: scripts\manage.bat build [--single] [--auto] ^| run [--single] [--auto] ^| test [--auto] ^| clean ^| --help
exit /b 2
