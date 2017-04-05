@echo off

:: %1 - The toolchain name (vc11 | vc12, vc14, etc)
:: %2 - The openssl version (1.1.0d, etc)

setlocal
pushd %~dp0

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

set TOOLCHAIN_NAME=%1
if "%TOOLCHAIN_NAME%" == "" set TOOLCHAIN_NAME=vc14

set OPENSSL_VERSION_NAME=%2
if "%OPENSSL_VERSION_NAME%" == "" set OPENSSL_VERSION_NAME=1.1.0d

:: Just invoke the openssl_build.bat file for the various build and arch types

call %CD%\openssl_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% debug
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\openssl_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% debug
  goto error
  )

call %CD%\openssl_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% release
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\openssl_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% release
  goto error
  )

call %CD%\openssl_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% debug
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\openssl_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% debug
  goto error
  )

call %CD%\openssl_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% release
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\openssl_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME% release
  goto error
  )

:okay
echo Done, all build types for openssl were built successfully!
exit /b 0

:error
exit /b 1
