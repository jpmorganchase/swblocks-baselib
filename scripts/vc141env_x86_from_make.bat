@echo off

if "%CI_ENV_ROOT%" == "" (
  echo CI_ENV_ROOT must be defined
  goto error
  )

:: CI_ENV_ROOT must point to the CI environment root
call %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat

set MSVC=%DIST_ROOT_DEPS3%\toolchain-msvc\vc14.1\BuildTools
set WINSDK=%DIST_ROOT_DEPS3%\winsdk\8.1\default
set WINSDK10=%DIST_ROOT_DEPS3%\winsdk\10\default

set Path=%MSVC%\VC\Redist\MSVC\14.16.27012\x86\Microsoft.VC141.CRT;%MSVC%\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x86;%MSVC%\VC\Tools\MSVC\14.16.27023\bin\Hostx64\x64;%MSVC%\DIA SDK\bin;%WINSDK%\bin\x86;%WINSDK10%\bin\x86\ucrt;%Path%
set LIB=%MSVC%\VC\Tools\MSVC\14.16.27023\lib\x86;%WINSDK%\lib\winv6.3\um\x86;%WINSDK10%\Lib\10.0.10240.0\ucrt\x86
set INCLUDE=%MSVC%\VC\Tools\MSVC\14.16.27023\include;%WINSDK%\include\shared;%WINSDK%\include\um;%WINSDK10%\Include\10.0.10240.0\ucrt
set LIBPATH=%LIB%

:okay
exit /b 0

:error
exit /b 1
