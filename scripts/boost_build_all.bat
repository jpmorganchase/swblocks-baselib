@echo off

:: %1 - The architecture tag (x64 | x86)
:: %2 - The toolchain name (vc11 | vc12, vc14, vc141, etc)
:: %3 - The boost version (1.72.0, etc)
:: %4 - The # of boost.build threads (8 by default)

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

set ARCH_NAME=%1
if "%ARCH_NAME%" == "" set ARCH_NAME=x64

set ADDRESS_MODEL=64
if "%ARCH_NAME%" == "x86" set ADDRESS_MODEL=32

set TOOLCHAIN_NAME=%2
if "%TOOLCHAIN_NAME%" == "" set TOOLCHAIN_NAME=vc141

set BOOST_VERSION_NAME=%3
if "%BOOST_VERSION_NAME%" == "" set BOOST_VERSION_NAME=1.72.0

set BOOST_BUILD_THREADS=%4
if "%BOOST_BUILD_THREADS%" == "" set BOOST_BUILD_THREADS=8

:: Just invoke the boost_build.bat file for the various build and arch types

call %CD%\boost_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME%
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\boost_build.bat x64 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME%
  goto error
  )

call %CD%\boost_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME%
if %ERRORLEVEL% NEQ 0 (
  echo error invoking the build %CD%\boost_build.bat x86 %TOOLCHAIN_NAME% %OPENSSL_VERSION_NAME%
  goto error
  )

:okay
echo Done, all build types for boost were built successfully!
popd
endlocal
exit /b 0

:error
popd
endlocal
exit /b 1
