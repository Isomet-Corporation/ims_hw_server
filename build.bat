@echo off
setlocal enabledelayedexpansion

REM =======================================================
REM Input Arguments
REM =======================================================
set "options=-c:Release -o:dist"
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
rmdir /S /Q "%BUILD32_DIR%" "%BUILD64_DIR%"
mkdir "%BUILD32_DIR%"
mkdir "%BUILD64_DIR%"

REM =======================================================
REM Step 4: Build 32-bit HW Server
REM =======================================================
echo.
echo Building 32-bit H/W Server...
conan install . --profile default -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 -s:b compiler.cppstd=17 -s arch=x86 --build=missing -of %BUILD32_DIR%
cd %BUILD32_DIR%
cmake -S .. -B .^
  -DCMAKE_CXX_FLAGS_DEBUG="/MDd /Zi /Od /Ob0 /RTC1 /DPATCH_VERSION=%Patch%"^
  -DCMAKE_CXX_FLAGS_RELEASE="/MD /Ox /Ob2 /DNDEBUG /DPATCH_VERSION=%Patch%"^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A Win32
cmake --build . --config %BUILD_TYPE%

IF NOT EXIST %BUILD_TYPE%\ims_hw_server.exe (
    echo ERROR: 32-bit H/W Server not found!
    exit /b 1
)
cd ..

REM =======================================================
REM Step 5: Build 64-bit DLL
REM =======================================================
echo.
echo Building 64-bit H/W Server...
conan install . --profile default -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 -s arch=x86_64 --build=missing -of %BUILD64_DIR%
cd %BUILD64_DIR%
cmake -S .. -B .^
  -DCMAKE_C_FLAGS_DEBUG="/MDd /Zi /Od /Ob0 /RTC1 /D_STATIC_IMS /DPATCH_VERSION=%Patch%"^
  -DCMAKE_CXX_FLAGS_DEBUG="/MDd /Zi /Od /Ob0 /RTC1 /D_STATIC_IMS /DPATCH_VERSION=%Patch%"^
  -DCMAKE_C_FLAGS_RELEASE="/MD /Ox /Ob2 /DNDEBUG /D_STATIC_IMS /DPATCH_VERSION=%Patch%"^
  -DCMAKE_CXX_FLAGS_RELEASE="/MD /Ox /Ob2 /DNDEBUG /D_STATIC_IMS /DPATCH_VERSION=%Patch%"^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -A x64
cmake --build . --config %BUILD_TYPE%

IF NOT EXIST %BUILD_TYPE%\ims_hw_server.exe (
    echo ERROR: 64-bit H/W Server not found!
    exit /b 1
)
cd ..
exit 0

REM -------------------------------------------------------
REM Step 6: Copy DLLs to resource folder
REM -------------------------------------------------------
copy "%BUILD32_DIR%\%BUILD_TYPE%\%CLIBNAME%32.dll" "%BUILDDOTNET_DIR%\%RESOURCE_DIR%\%CLIBNAME%32.dll"
copy "%BUILD64_DIR%\%BUILD_TYPE%\%CLIBNAME%64.dll" "%BUILDDOTNET_DIR%\%RESOURCE_DIR%\%CLIBNAME%64.dll"


REM -------------------------------------------------------
REM Step 7: Create temporary project
REM -------------------------------------------------------
echo Creating temporary C# project...

set CSPROJ=%BUILDDOTNET_DIR%\iMSNET.csproj

echo ^<Project Sdk="Microsoft.NET.Sdk"^> > %CSPROJ%
echo   ^<PropertyGroup^> >> %CSPROJ%
echo     ^<TargetFramework^>netstandard2.0^</TargetFramework^> >> %CSPROJ%
echo     ^<GenerateAssemblyInfo^>false^</GenerateAssemblyInfo^> >> %CSPROJ%
echo     ^<AssemblyName^>%CSLIBNAME%^</AssemblyName^> >> %CSPROJ%
echo   ^</PropertyGroup^> >> %CSPROJ%
echo   ^<PropertyGroup Condition=" '$(Configuration)' == 'Debug' "^> >> %CSPROJ%
echo     ^<DebugSymbols^>true^</DebugSymbols^> >> %CSPROJ%
echo     ^<DebugType^>full^</DebugType^> >> %CSPROJ%
echo     ^<Optimize^>false^</Optimize^> >> %CSPROJ%
echo     ^<DefineConstants^>DEBUG^</DefineConstants^> >> %CSPROJ%
echo   ^</PropertyGroup^> >> %CSPROJ%
echo   ^<PropertyGroup Condition=" '$(Configuration)' == 'Release' "^> >> %CSPROJ%
echo     ^<DebugType^>none^</DebugType^> >> %CSPROJ%
echo     ^<Optimize^>true^</Optimize^> >> %CSPROJ%
echo   ^</PropertyGroup^> >> %CSPROJ%
echo   ^<ItemGroup^> >> %CSPROJ%
echo     ^<Compile Include="%IMSC_FILE%" /^> >> %CSPROJ%
echo     ^<EmbeddedResource Include="%RESOURCE_DIR%\%CLIBNAME%32.dll" LogicalName="iMSNETlib32" /^> >> %CSPROJ%
echo     ^<EmbeddedResource Include="%RESOURCE_DIR%\%CLIBNAME%64.dll" LogicalName="iMSNETlib64" /^> >> %CSPROJ%
echo     ^<Compile Include="%EDL_FILE%" /^> >> %CSPROJ%
echo     ^<Compile Include="%ASSY_FILE%" /^> >> %CSPROJ%
echo   ^</ItemGroup^> >> %CSPROJ%
echo ^</Project^> >> %CSPROJ%

REM -------------------------------------------------------
REM Step 8: Compile C# wrapper with embedded DLLs
REM -------------------------------------------------------
echo Building managed wrapper...

dotnet build %CSPROJ% -c %BUILD_TYPE% -o %OUT_DIR%\%BUILD_TYPE%

IF ERRORLEVEL 1 (
    echo ERROR: Failed to compile %CSLIBNAME%.dll
    exit /b 1
)

echo =======================================================
echo Build complete!
echo %CSLIBNAME%.dll created in %OUT_DIR%\%BUILD_TYPE%
echo Embedded resources: iMSNETlib32, iMSNETlib64
echo =======================================================
echo on