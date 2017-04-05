@echo off

:: CI_ENV_ROOT must point to the CI environment root
call %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat

set MSVC=%DIST_ROOT_DEPS3%\toolchain-msvc\vc14-update3\default
set WINSDK=%DIST_ROOT_DEPS3%\winsdk\8.1\default
set WINSDK10=%DIST_ROOT_DEPS3%\winsdk\10\default

set Path=%MSVC%\VC\redist\x64\Microsoft.VC140.CRT;%MSVC%\VC\bin\amd64;%MSVC%\VC\bin;%MSVC%\Common7\ide;%WINSDK%\bin\x64;%WINSDK10%\bin\x64\ucrt;%Path%
set LIB=%MSVC%\VC\lib\amd64;%WINSDK%\lib\winv6.3\um\x64;%WINSDK10%\Lib\10.0.10240.0\ucrt\x64
set INCLUDE=%MSVC%\VC\include;%WINSDK%\include\shared;%WINSDK%\include\um;%WINSDK10%\Include\10.0.10240.0\ucrt
set LIBPATH=%LIB%
