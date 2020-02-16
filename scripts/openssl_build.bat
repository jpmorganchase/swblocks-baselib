@echo off

:: %1 - The architecture tag (x64 | x86)
:: %2 - The toolchain name (vc11 | vc12, vc14, vc141, etc)
:: %3 - The openssl version (1.1.1d, etc)
:: %4 - The build type (debug vs. release)

setlocal

set SCRIPTS_DIR_WITH_TRAILING_SLASH=%~dp0
pushd %SCRIPTS_DIR_WITH_TRAILING_SLASH%..\..

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

set TOOLCHAIN_NAME=%2
if "%TOOLCHAIN_NAME%" == "" set TOOLCHAIN_NAME=vc141

set OPENSSL_VERSION_NAME=%3
if "%OPENSSL_VERSION_NAME%" == "" set OPENSSL_VERSION_NAME=1.1.1d

set BUILD_TYPE=%4
if "%BUILD_TYPE%" == "" set BUILD_TYPE=debug

call %SCRIPTS_DIR_WITH_TRAILING_SLASH%%TOOLCHAIN_NAME%env_%ARCH_NAME%_from_make.bat
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the toolchain configuration file %SCRIPTS_DIR_WITH_TRAILING_SLASH%%TOOLCHAIN_NAME%env_%ARCH_NAME%_from_make.bat
  goto error
  )

set OPENSSL_SOURCE_PATH=%DIST_ROOT_DEPS1%\openssl\%OPENSSL_VERSION_NAME%\source-windows

if not exist %OPENSSL_SOURCE_PATH% (
    echo Path %OPENSSL_SOURCE_PATH% does not exist
    goto error
)

set OPENSSL_ROOT_PATH=%CD%\bld\swblocks\openssl\%OPENSSL_VERSION_NAME%\win7-%ARCH_NAME%-%TOOLCHAIN_NAME%-%BUILD_TYPE%

if exist %OPENSSL_ROOT_PATH% (
    echo Deleting existing openssl build in %OPENSSL_ROOT_PATH% ...
    rm -rf %OPENSSL_ROOT_PATH%
    if %ERRORLEVEL% NEQ 0 (
        echo error deleting existing openssl build in %OPENSSL_ROOT_PATH% ...
        goto error
        )
)

mkdir %OPENSSL_ROOT_PATH%
if %ERRORLEVEL% NEQ 0 goto error

pushd %OPENSSL_ROOT_PATH%
if %ERRORLEVEL% NEQ 0 goto error

echo Copying files from %OPENSSL_SOURCE_PATH% to %OPENSSL_ROOT_PATH% ...
robocopy /E %OPENSSL_SOURCE_PATH% . > ..\log_src_filecopy.log 2>&1
if %ERRORLEVEL% GTR 1 (
  echo error while copying files, see %OPENSSL_ROOT_PATH%\log_src_filecopy.log
  goto error
  )
move ..\log_src_filecopy.log .

echo Bootstrapping the makefile (log file is %OPENSSL_ROOT_PATH%\log_bootstrap.log) ...

set Path=%DIST_ROOT_DEPS1%\strawberry-perl\latest\default\perl\bin;%Path%

if "%ARCH_NAME%" == "x64" (
  set BUILD_CONFIG_NAME=VC-WIN64A
  ) else (
  set BUILD_CONFIG_NAME=VC-WIN32
  )

if "%BUILD_TYPE%" == "debug" (
  perl Configure %BUILD_CONFIG_NAME% no-asm no-shared --debug --prefix=%OPENSSL_ROOT_PATH%\out --openssldir=%OPENSSL_ROOT_PATH%\out\ssl -Od -Ob0 -Oy- -EHs -Zi -GS -bigobj > log_bootstrap.log 2>&1
  :: do some replacements to turn off the bad CRT debugging stuff
  bash -c "sed -ie 's/\/MDd/\/MD/g' ./makefile"
  bash -c "sed -ie 's/\-DDEBUG//g' ./makefile"
  bash -c "sed -ie 's/\-D_DEBUG//g' ./makefile"
  ) else (
  perl Configure %BUILD_CONFIG_NAME% no-asm no-shared --release --prefix=%OPENSSL_ROOT_PATH%\out --openssldir=%OPENSSL_ROOT_PATH%\out\ssl -O2 -Ob1 -Ot -Oi -Oy- -EHs -Zi -GS -bigobj -Zo -DNDEBUG > log_bootstrap.log 2>&1
  )
if %ERRORLEVEL% NEQ 0 goto error

echo Building openssl (log file is %OPENSSL_ROOT_PATH%\log_openssl_build.log) ...
nmake > log_openssl_build.log 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Building openssl failed, see log file here: %OPENSSL_ROOT_PATH%\log_openssl_build.log
    goto error
    )

echo Testing openssl (log file is %OPENSSL_ROOT_PATH%\log_openssl_test.log) ...
(nmake test) > log_openssl_test.log 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Testing openssl failed, see log file here: %OPENSSL_ROOT_PATH%\log_openssl_test.log
    :: TODO: release tests are failing die to some issue that seems benign
    :: TODO: more details here: https://github.com/openssl/openssl/issues/8694
    if "%BUILD_TYPE%" NEQ "release" (
        goto error
        )
    )

echo Installing openssl (log file is %OPENSSL_ROOT_PATH%\log_openssl_install.log) ...
(nmake install) > log_openssl_install.log 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Installing openssl failed, see log file here: %OPENSSL_ROOT_PATH%\log_openssl_install.log
    goto error
    )

set OPENSSL_TARGET_PATH=%DIST_ROOT_DEPS1%\openssl\%OPENSSL_VERSION_NAME%\win7-%ARCH_NAME%-%TOOLCHAIN_NAME%-%BUILD_TYPE%

if exist %OPENSSL_TARGET_PATH% (
    echo Deleting existing openssl build in %OPENSSL_TARGET_PATH% ...
    rm -rf %OPENSSL_TARGET_PATH%
    if %ERRORLEVEL% NEQ 0 (
        echo error deleting existing openssl build in %OPENSSL_TARGET_PATH% ...
        goto error
        )
)

mkdir %OPENSSL_TARGET_PATH%
if %ERRORLEVEL% NEQ 0 goto error

robocopy /E %OPENSSL_ROOT_PATH%\out %OPENSSL_TARGET_PATH% > ..\log_dst_filecopy.log 2>&1
if %ERRORLEVEL% GTR 1 (
  echo error while copying files to %OPENSSL_TARGET_PATH%, see %OPENSSL_ROOT_PATH%\log_dst_filecopy.log
  goto error
  )
move ..\log_dst_filecopy.log .

move log_* %OPENSSL_TARGET_PATH%
if %ERRORLEVEL% NEQ 0 (
  echo error while moving the log files to %OPENSSL_TARGET_PATH%
  goto error
  )

move makefile %OPENSSL_TARGET_PATH%
if %ERRORLEVEL% NEQ 0 (
  echo error while moving the makefile to %OPENSSL_TARGET_PATH%
  goto error
  )

popd
rm -rf %OPENSSL_ROOT_PATH%
if %ERRORLEVEL% NEQ 0 (
  echo error while deleting the root directory %OPENSSL_ROOT_PATH%
  goto error
  )

:okay
echo Done, openssl was built successfully!
popd
endlocal
exit /b 0

:error
popd
endlocal
exit /b 1
