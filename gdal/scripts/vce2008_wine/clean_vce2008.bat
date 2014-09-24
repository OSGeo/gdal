c:

set PATH=C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\IDE;C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\BIN;C:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\Tools;C:\windows\Microsoft.NET\Framework\v3.5;C:\windows\Microsoft.NET\Framework\v2.0.50727;C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\VCPackages;%PATH%
set INCLUDE=C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\INCLUDE;%INCLUDE%
set LIB=C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\LIB;%LIB%
set LIBPATH=C:\windows\Microsoft.NET\Framework\v3.5;C:\windows\Microsoft.NET\Framework\v2.0.50727;C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\LIB;%LIBPATH%

cd c:\gdal\gdal
set INCLUDE=%INCLUDE%c:\psdk
set LIB=%LIB%c:\psdk
set PATH=c:\psdk;%PATH%

nmake /f makefile.vc clean
