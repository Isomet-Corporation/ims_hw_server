@echo off
setlocal enabledelayedexpansion

REM =======================================================
REM Input Arguments
REM =======================================================
set "options=-c:Release -o:dist -s: -t: -p: -a:both"
:: Set the default option values
for %%O in (%options%) do for /f "tokens=1,* delims=:" %%A in ("%%O") do set "%%A=%%~B"
:loop
:: Validate and store the options, one at a time, using a loop.
if not "%~1"=="" (
  set "test=!options:*%~1:=! "
  if "!test!"=="!options! " (
    rem No substitution was made so this is an invalid option.
    rem Error handling goes here.
    rem I will simply echo an error message.
    echo Error: Invalid option %~1
  ) else if "!test:~0,1!"==" " (
    rem Set the flag option using the option name.
    rem The value doesn't matter, it just needs to be defined.
    set "%~1=1"
  ) else (
    rem Set the option value using the option as the name.
    rem and the next arg as the value
    set "%~1=%~2"
    shift
  )
  shift
  goto :loop
)
set OUT_DIR=%-o%

set BUILD_HW_SERVER=OFF
set BUILD_TEST_CLIENT=OFF
set GENERATE_PROTO_ALL=OFF
if "%-s%"=="1" ( set BUILD_HW_SERVER=ON)
if "%-t%"=="1" ( set BUILD_TEST_CLIENT=ON)
if "%-p%"=="1" ( set GENERATE_PROTO_ALL=ON)

if "%BUILD_HW_SERVER%"=="OFF" if "%BUILD_TEST_CLIENT%"=="OFF" if "%GENERATE_PROTO_ALL%"=="OFF" (
    echo ERROR: Must specify a build option
    echo Usage: build.bat [-c Release^|Debug] [-o ^<outdir^>] [-s] [-t] [-p]
    echo        ^-c Release^|Debug =^> Specify Build Type (default: Release^)
    echo        -o ^<outdir^>      =^> Location to copy output products
    echo        -s               =^> Build H/W Server
    echo        -t               =^> Build Test BUILD_TEST_CLIENT
    echo        -p               =^> Generate Multiple Language Protobuf definitions and gRPC Services
    exit /b 1
)

set BUILD32=0
if "%-a%"=="both" ( set BUILD32=1)
if "%-a%"=="32"   ( set BUILD32=1)
if "%-a%"=="all"  ( set BUILD32=1)

set BUILD64=0
if "%-a%"=="both" ( set BUILD64=1)
if "%-a%"=="64"   ( set BUILD64=1)
if "%-a%"=="all"  ( set BUILD64=1)

if "%BUILD32%"=="0" if "%BUILD64%"=="0" (
    echo ERROR: You must specify a machine architecture! [Use -a 32, -a 64, -a all]
    exit /b 1
)

if NOT EXIST %OUT_DIR% (
    mkdir "%OUT_DIR%"
    if ERRORLEVEL 1 (
        exit /b 1
    )
)

set BUILD_TYPE=%-c%
if NOT "%BUILD_TYPE%"=="Debug" if NOT "%BUILD_TYPE%"=="Release" (
    echo Invalid Build Type. Choose Debug or Release
    exit /b 1
)

REM =======================================================
REM Paths
REM =======================================================
set BUILD32_DIR=build32
set BUILD64_DIR=build64
set GEN_DIR=generated

REM -------------------------------------------------------
REM Step 1: Check Conan profile
REM -------------------------------------------------------
conan profile list | findstr /C:"default" >nul
IF ERRORLEVEL 1 (
    echo Conan default profile not found. Detecting...
    conan profile detect -f
) ELSE (
    echo Conan default profile already exists
)

REM -------------------------------------------------------
REM Step 2: Increment Patch Number
REM -------------------------------------------------------
set /p Patch= < src\patch.txt
set /a Patch=%Patch%+1
echo %Patch% > src\patch.txt
     

REM -------------------------------------------------------
REM Step 3: Clean previous builds
REM -------------------------------------------------------
rmdir /S /Q "%BUILD32_DIR%" "%BUILD64_DIR%" "%GEN_DIR%"
mkdir "%BUILD32_DIR%"
mkdir "%BUILD64_DIR%"
mkdir "%GEN_DIR%"
REM =======================================================
REM Step 4: Build 32-bit HW Server
REM =======================================================
if "%BUILD32%"=="1" (
    echo.
    echo Building 32-bit Applications...
    conan install . --profile default -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 -s:b compiler.cppstd=17 -s arch=x86 --build=missing -of %BUILD32_DIR%
    cd %BUILD32_DIR%
    cmake -S .. -B .^
    -DCMAKE_CXX_FLAGS_DEBUG="/MDd /Zi /Od /Ob0 /RTC1 /DPATCH_VERSION=%Patch%"^
    -DCMAKE_CXX_FLAGS_RELEASE="/MD /Ox /Ob2 /DNDEBUG /DPATCH_VERSION=%Patch%"^
    -DBUILD_HW_SERVER=%BUILD_HW_SERVER%^
    -DBUILD_TEST_CLIENT=%BUILD_TEST_CLIENT%^
    -DGENERATE_PROTO_ALL=%GENERATE_PROTO_ALL%^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A Win32
    cmake --build . --config %BUILD_TYPE%

    IF NOT EXIST %BUILD_TYPE%\ims_hw_server.exe (
        echo ERROR: 32-bit H/W Server not found!
        exit /b 1
    )
    cd ..
)

REM =======================================================
REM Step 5: Build 64-bit HW Server
REM =======================================================
if "%BUILD64%"=="1" (
    echo.
    echo Building 64-bit Applications...
    conan install . --profile default -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 -s arch=x86_64 --build=missing -of %BUILD64_DIR%
    cd %BUILD64_DIR%
    cmake -S .. -B .^
    -DCMAKE_CXX_FLAGS_DEBUG="/MDd /Zi /Od /Ob0 /RTC1 /DPATCH_VERSION=%Patch%"^
    -DCMAKE_CXX_FLAGS_RELEASE="/MD /Ox /Ob2 /DNDEBUG /DPATCH_VERSION=%Patch%"^
    -DBUILD_HW_SERVER=%BUILD_HW_SERVER%^
    -DBUILD_TEST_CLIENT=%BUILD_TEST_CLIENT%^
    -DGENERATE_PROTO_ALL=%GENERATE_PROTO_ALL%^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A x64
    cmake --build . --config %BUILD_TYPE%

    IF NOT EXIST %BUILD_TYPE%\ims_hw_server.exe (
        echo ERROR: 64-bit H/W Server not found!
        exit /b 1
    )
    cd ..
)

REM -------------------------------------------------------
REM Step 6: Copy Artifacts to output folder
REM -------------------------------------------------------
if "%BUILD32%"=="1" (
    xcopy /S /Q /Y "%BUILD32_DIR%\%BUILD_TYPE%\*.exe" "%OUT_DIR%"\bin\Win32\
    xcopy /S /Q /Y "protos" "%OUT_DIR%\protos\"
    xcopy /S /Q /Y "generated" "%OUT_DIR%\grpc\" 
)
if "%BUILD64%"=="1" (
    xcopy /S /Q /Y "%BUILD64_DIR%\%BUILD_TYPE%\*.exe" "%OUT_DIR%"\bin\x64\
    xcopy /S /Q /Y "protos" "%OUT_DIR%\protos\"
    xcopy /S /Q /Y "generated" "%OUT_DIR%\grpc\" 
)


echo ==============================================================
echo Build complete!
echo  - Protobuf files are in %OUT_DIR%\protos
echo  - Generated Language-specific gRPC/Proto files in %OUT_DIR%\grpc
if "%BUILD32%"=="1" (
    if "%BUILD_HW_SERVER%"=="ON" (
        echo  - 32-bit H/W server in %OUT_DIR%\bin\Win32
    )
    if "%BUILD_TEST_CLIENT%"=="ON" (
        echo  - 32-bit test client in %OUT_DIR%\bin\Win32
    )
)
if "%BUILD64%"=="1" (
    if "%BUILD_HW_SERVER%"=="ON" (
        echo  - 64-bit H/W server in %OUT_DIR%\bin\x64
    )
    if "%BUILD_TEST_CLIENT%"=="ON" (
        echo  - 64-bit test client in %OUT_DIR%\bin\x64
    )
)
echo ==============================================================
echo on