@echo Setting environment variables for MinGw build of Inkscape
IF "%DEVLIBS_PATH%"=="" set DEVLIBS_PATH=c:\devlibs
IF "%MINGW_PATH%"=="" set MINGW_PATH=c:\mingw
set MINGW_BIN=%MINGW_PATH%\bin
set PKG_CONFIG_PATH=%DEVLIBS_PATH%\lib\pkgconfig
set PATH=%MINGW_BIN%;%DEVLIBS_PATH%\bin;%DEVLIBS_PATH%\python;%PATH%
