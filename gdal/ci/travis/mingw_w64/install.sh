#!/bin/sh

set -e

export CCACHE_CPP2=yes

wine64 cmd /c dir
ln -s /usr/lib/gcc/x86_64-w64-mingw32/4.8/libstdc++-6.dll  $HOME/.wine/drive_c/windows
ln -s /usr/lib/gcc/x86_64-w64-mingw32/4.8/libgcc_s_sjlj-1.dll  $HOME/.wine/drive_c/windows
ln -s /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll  $HOME/.wine/drive_c/windows
# build proj
curl http://download.osgeo.org/proj/proj-4.9.2.tar.gz > proj-4.9.2.tar.gz
tar xvzf proj-4.9.2.tar.gz
cd proj-4.9.2/nad
curl http://download.osgeo.org/proj/proj-datumgrid-1.5.tar.gz > proj-datumgrid-1.5.tar.gz
tar xvzf proj-datumgrid-1.5.tar.gz
cd ..
CC="ccache x86_64-w64-mingw32-gcc" CXX="ccache x86_64-w64-mingw32-g++" LD=x86_64-w64-mingw32-ld ./configure --host=x86_64-w64-mingw32
make -j3
cd ..
# build GDAL
cd gdal
CC="ccache x86_64-w64-mingw32-gcc" CXX="ccache x86_64-w64-mingw32-g++" ./configure --host=x86_64-w64-mingw32
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf.exe
cd ..
ln -sf $PWD/.libs/libgdal-20.dll $HOME/.wine/drive_c/windows
ln -sf $PWD/../proj-4.9.2/src/.libs/libproj-9.dll $HOME/.wine/drive_c/windows
# Python bindings
wget http://www.python.org/ftp/python/2.7.3/python-2.7.3.amd64.msi
wine64 msiexec /i python-2.7.3.amd64.msi
cd swig/python
gendef $HOME/.wine/drive_c/Python27/python27.dll
x86_64-w64-mingw32-dlltool --dllname $HOME/.wine/drive_c/Python27/python27.dll --input-def python27.def --output-lib $HOME/.wine/drive_c/Python27/libs/libpython27.a
CXX=x86_64-w64-mingw32-g++ bash fallback_build_mingw32_under_unix.sh 
cd ../..
