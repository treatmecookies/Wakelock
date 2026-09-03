@echo off

setlocal

rem -------------------------------------------------
rem
rem Usage:
rem
rem   build.bat llvm
rem   build.bat llvm -fsanitize=address
rem   build.bat msvc
rem   build.bat msvc /WX
rem
rem -------------------------------------------------

if "%~1"=="" (
    echo Usage: %~nx0 ^<llvm^|msvc^> [extra compiler options...]
    exit /b 1
)

set "TOOLCHAIN=%~1"
shift

rem -------------------------------------------------
rem Ensure build directory exists.
rem -------------------------------------------------

if not exist "build\" (
    mkdir "build"

    if errorlevel 1 (
        echo Failed to create build directory.
        exit /b 1
    )
)

rem -------------------------------------------------
rem Collect remaining command-line arguments.
rem -------------------------------------------------

set "EXTRA_ARGS="

:collect_args

if "%~1"=="" goto args_done

set "EXTRA_ARGS=%EXTRA_ARGS% %1"

shift
goto collect_args

:args_done

rem -------------------------------------------------
rem LLVM / Clang
rem -------------------------------------------------

if /i "%TOOLCHAIN%"=="llvm" goto build_llvm

rem -------------------------------------------------
rem MSVC
rem -------------------------------------------------

if /i "%TOOLCHAIN%"=="msvc" goto build_msvc

echo Unknown toolchain: %TOOLCHAIN%
echo Expected: llvm or msvc
exit /b 1


:build_llvm

llvm-rc res\main.rc -fo build\main.res

if errorlevel 1 (
    echo Resource compilation failed.
    exit /b 1
)

echo Building LLVM x86...

clang ^
    --target=i686-pc-windows-msvc ^
    src\main.c ^
    build\main.res ^
    -DUNICODE ^
    -D_UNICODE ^
    -O3 ^
    -g ^
    -nostdlib ^
    -Wall ^
    -flto ^
    -fuse-ld=lld ^
    -fstack-protector ^
    -Xlinker /SUBSYSTEM:WINDOWS ^
    -Xlinker /ENTRY:W_CRT_Entry ^
    -Xlinker /NODEFAULTLIB ^
    -Xlinker /DEBUG ^
    -Xlinker /PDB:build\Wakelock-x86.pdb ^
    -lkernel32 ^
    -llibvcruntime ^
    -llibcmt ^
    -llibucrt ^
    -luser32 ^
    -lshell32 ^
    -ladvapi32 ^
    %EXTRA_ARGS% ^
    -o build\Wakelock-x86.exe

if errorlevel 1 (
    echo LLVM x86 build failed.
    exit /b 1
)

echo LLVM x86 build succeeded.

echo Building LLVM x64...

clang ^
    --target=x86_64-pc-windows-msvc ^
    src\main.c ^
    build\main.res ^
    -DUNICODE ^
    -D_UNICODE ^
    -O3 ^
    -g ^
    -nostdlib ^
    -Wall ^
    -flto ^
    -fuse-ld=lld ^
    -fstack-protector ^
    -Xlinker /SUBSYSTEM:WINDOWS ^
    -Xlinker /ENTRY:W_CRT_Entry ^
    -Xlinker /NODEFAULTLIB ^
    -Xlinker /DEBUG ^
    -Xlinker /PDB:build\Wakelock-x64.pdb ^
    -lkernel32 ^
    -llibvcruntime ^
    -llibcmt ^
    -llibucrt ^
    -luser32 ^
    -lshell32 ^
    -ladvapi32 ^
    %EXTRA_ARGS% ^
    -o build\Wakelock-x64.exe

if errorlevel 1 (
    echo LLVM x64 build failed.
    exit /b 1
)

echo LLVM x64 build succeeded.

echo Building LLVM ARM64...

clang ^
    --target=aarch64-pc-windows-msvc ^
    src\main.c ^
    build\main.res ^
    -DUNICODE ^
    -D_UNICODE ^
    -O3 ^
    -g ^
    -nostdlib ^
    -Wall ^
    -flto ^
    -fuse-ld=lld ^
    -fstack-protector ^
    -Xlinker /SUBSYSTEM:WINDOWS ^
    -Xlinker /ENTRY:W_CRT_Entry ^
    -Xlinker /NODEFAULTLIB ^
    -Xlinker /DEBUG ^
    -Xlinker /PDB:build\Wakelock-arm64.pdb ^
    -lkernel32 ^
    -llibvcruntime ^
    -llibcmt ^
    -llibucrt ^
    -luser32 ^
    -lshell32 ^
    -ladvapi32 ^
    %EXTRA_ARGS% ^
    -o build\Wakelock-arm64.exe

if errorlevel 1 (
    echo LLVM ARM64 build failed.
    exit /b 1
)

echo LLVM ARM64 build succeeded.
exit /b 0

:build_msvc

rc /fo build\main.res res\main.rc

if errorlevel 1 (
    echo Resource compilation failed.
    exit /b 1
)

cl ^
    src\main.c ^
    build\main.res ^
    /DUNICODE ^
    /D_UNICODE ^
    /O2 ^
    /GL ^
    /GS ^
    /W4 ^
    /Zi ^
    /MT ^
    /Oi ^
    /Fo:build\main.obj ^
    /Fd:build\main.pdb ^
    %EXTRA_ARGS% ^
    /link ^
    /SUBSYSTEM:WINDOWS ^
    /NODEFAULTLIB ^
    kernel32.lib ^
    user32.lib ^
    shell32.lib ^
    advapi32.lib ^
    libvcruntime.lib ^
    libcmt.lib ^
    libucrt.lib ^
    /LTCG ^
    /DEBUG ^
    /OUT:build\Wakelock.exe ^
    /PDB:build\Wakelock.pdb

if errorlevel 1 (
    echo MSVC build failed.
    exit /b 1
)

echo MSVC build succeeded.
exit /b 0
