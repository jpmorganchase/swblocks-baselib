@echo off

:: CI_ENV_ROOT must point to the CI environment root
call %CI_ENV_ROOT%\scripts\ci\ci-init-env.bat

::
:: note, Boost.Python depends on the python install, so we need to make sure we put the right
:: python library on the path (%GFSDEV_ROOT%\3rd\python\2.7.5\win7-x86-vc11)
::
set MSVC=%DIST_ROOT_DEPS3%\toolchain-msvc\vc12-update4\default
set WINSDK=%DIST_ROOT_DEPS3%\winsdk\8.1\default

set Path=%MSVC%\VC\redist\x86\Microsoft.VC120.CRT;%MSVC%\VC\bin;%MSVC%\Common7\ide;%WINSDK%\bin\x86;%GFSDEV_ROOT%\3rd\python\2.7.5\win7-x86-vc11;%Path%
set LIB=%MSVC%\VC\lib;%WINSDK%\lib\winv6.3\um\x86
set INCLUDE=%MSVC%\VC\include;%WINSDK%\include\shared;%WINSDK%\include\um
set LIBPATH=%LIB%
