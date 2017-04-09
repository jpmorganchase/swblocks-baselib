@echo off

:: %1 - The architecture tag (x64 | x86)
:: %2 - The toolchain name (vc11 | vc12, vc14, etc)
:: %3 - The boost version (1.63.0, etc)
:: %4 - The # of boost.build threads (8 by default)

setlocal
pushd %~dp0..

if "%CI_ENV_ROOT%" == "" (
  echo CI_ENV_ROOT must be defined
  goto error
  )

call %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat
if %ERRORLEVEL% NEQ 0 goto error

if "%DIST_ROOT_DEPS1%" == "" (
  echo DIST_ROOT_DEPS1 must be defined by %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat
  goto error
  )

set ARCH_NAME=%1
if "%ARCH_NAME%" == "" set ARCH_NAME=x64

set ADDRESS_MODEL=64
if "%ARCH_NAME%" == "x86" set ADDRESS_MODEL=32

set TOOLCHAIN_NAME=%2
if "%TOOLCHAIN_NAME%" == "" set TOOLCHAIN_NAME=vc14

set BOOST_VERSION_NAME=%3
if "%BOOST_VERSION_NAME%" == "" set BOOST_VERSION_NAME=1.63.0

set BOOST_BUILD_THREADS=%4
if "%BOOST_BUILD_THREADS%" == "" set BOOST_BUILD_THREADS=8

call .\scripts\%TOOLCHAIN_NAME%env_%ARCH_NAME%_from_make.bat
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the toolchain configuration file scripts\%TOOLCHAIN_NAME%env_%ARCH_NAME%_from_make.bat
  goto error
  )

set BOOST_SOURCE_PATH=%DIST_ROOT_DEPS1%\boost\%BOOST_VERSION_NAME%\source-windows

if not exist %BOOST_SOURCE_PATH% (
    echo Path %BOOST_SOURCE_PATH% does not exist
    goto error
)

set BOOST_ROOT_PATH=.\bld\swblocks\boost\%BOOST_VERSION_NAME%\win7-%ARCH_NAME%-%TOOLCHAIN_NAME%

if exist %BOOST_ROOT_PATH% (
    echo Deleting existing boost build in %BOOST_ROOT_PATH% ...
    rmdir /s/q %BOOST_ROOT_PATH%
    if %ERRORLEVEL% NEQ 0 (
        echo error deleting existing boost build in %BOOST_ROOT_PATH% ...
        goto error
        )
)

mkdir %BOOST_ROOT_PATH%
if %ERRORLEVEL% NEQ 0 goto error

cd /d %BOOST_ROOT_PATH%
if %ERRORLEVEL% NEQ 0 goto error

echo Copying files from %BOOST_SOURCE_PATH% to %BOOST_ROOT_PATH% ...
xcopy /s /e /f /h %BOOST_SOURCE_PATH% > log_src_filecopy.log 2>&1
if %ERRORLEVEL% NEQ 0 (
  echo error while copying files, see %BOOST_ROOT_PATH%\log_src_filecopy.log
  goto error
  )

echo Bootstrapping the toolchain (log file is %BOOST_ROOT_PATH%\log_bootstrap.log) ...
call .\bootstrap.bat %TOOLCHAIN_NAME% > log_bootstrap.log 2>&1
if %ERRORLEVEL% NEQ 0 goto error

::
:: http://www.boost.org/build/doc/html/bbv2/overview/invocation.html
:: http://www.boost.org/build/doc/html/bbv2/overview/builtins/features.html
:: http://www.boost.org/build/doc/html/bbv2/overview/configuration.html
::
:: cflags="-bigobj -GS -Oy- -D_SECURE_SCL=0 -D_WIN32_WINNT=0x0601"
:: linkflags="-subsystem:console -incremental:no -opt:icf -opt:ref -release"
::
:: debug-symbols=on : /Z7
:: exception-handling=on asynch-exceptions=off extern-c-nothrow=off : -EHs
::
:: Unfortunately debug store 'database' is not supported well for some reason, so we should default to /Z7
:: debug-store=database : /Zi
::
:: Unfortunately lots of libraries still use asynch-exceptions so we can't force this,
:: but we can set the default to 'off'
:: (findstr /s /c:"asynch-exceptions" Jamfile*)
::

echo Building boost (log file is %BOOST_ROOT_PATH%\log_boost_build.log) ...
.\b2 --prefix=.\bld --stagedir=.\bld\stage --build-dir=.\bld\int -j%BOOST_BUILD_THREADS% --build-type=complete --layout=tagged exception-handling=on asynch-exceptions=off extern-c-nothrow=off debug-symbols=on runtime-debugging=off embed-manifest=on cflags="-bigobj -GS -Oy- -D_SECURE_SCL=0 -D_WIN32_WINNT=0x0601" linkflags="-subsystem:console -incremental:no -opt:icf -opt:ref -release" address-model=%ADDRESS_MODEL% install > log_boost_build.log 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Building boost failed, see log file here: %BOOST_ROOT_PATH%\log_boost_build.log
    goto error
    )

echo Copying PDB files and deleting intermediate folders ...

cd .\bld\int
if %ERRORLEVEL% NEQ 0 goto error

for /r %%i in (*.pdb) do move %%i ..\lib\
if %ERRORLEVEL% NEQ 0 goto error

cd ..
if %ERRORLEVEL% NEQ 0 goto error

rd /s/q .\int
if %ERRORLEVEL% NEQ 0 goto error

:okay
echo Done, boost was built successfully!
exit /b 0

:error
exit /b 1
