@echo off

:: CI_ENV_ROOT must point to the CI environment root
call %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat

set MSVC=%DIST_ROOT_DEPS3%\toolchain-msvc\vc12-update4\default
set WINSDK=%DIST_ROOT_DEPS3%\winsdk\8.1\default

set Path=%MSVC%\VC\redist\x86\Microsoft.VC120.CRT;%MSVC%\VC\bin\x86_amd64;%MSVC%\VC\bin;%MSVC%\Common7\ide;%WINSDK%\bin\x64;%Path%
set LIB=%MSVC%\VC\lib\amd64;%WINSDK%\lib\winv6.3\um\x64
set INCLUDE=%MSVC%\VC\include;%WINSDK%\include\shared;%WINSDK%\include\um
set LIBPATH=%LIB%
