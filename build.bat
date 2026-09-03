@echo off
setlocal

rem -------------------------------------------------
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

rem Collect remaining command-line arguments.
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto args_done
set "EXTRA_ARGS=%EXTRA_ARGS% "%~1""
shift
goto collect_args
:args_done


rem -------------------------------------------------
rem Resource
rem -------------------------------------------------

llvm-rc res\main.rc -fo build\main.res

if errorlevel 1 (
    echo Resource compilation failed.
    exit /b 1
)


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

clang ^
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
    -Xlinker /NODEFAULTLIB ^
    -lkernel32 ^
    -llibvcruntime ^
    -llibcmt ^
    -llibucrt ^
    -luser32 ^
    -lshell32 ^
    -ladvapi32 ^
    %EXTRA_ARGS% ^
    -o build\Wakelock.exe

if errorlevel 1 (
    echo LLVM build failed.
    exit /b 1
)

echo LLVM build succeeded.
exit /b 0


:build_msvc

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
    %EXTRA_ARGS%

if errorlevel 1 (
    echo MSVC build failed.
    exit /b 1
)

echo MSVC build succeeded.
exit /b 0
