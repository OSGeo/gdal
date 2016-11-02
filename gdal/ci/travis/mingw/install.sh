#!/bin/sh

set -e

export CCACHE_CPP2=yes

curl http://download.osgeo.org/proj/proj-4.9.2.tar.gz > proj-4.9.2.tar.gz
tar xvzf proj-4.9.2.tar.gz
cd proj-4.9.2/nad
curl http://download.osgeo.org/proj/proj-datumgrid-1.5.tar.gz > proj-datumgrid-1.5.tar.gz
tar xvzf proj-datumgrid-1.5.tar.gz
cd ..
CC="ccache i586-mingw32msvc-gcc" CXX="ccache i586-mingw32msvc-g++" LD=i586-mingw32msvc-ld ./configure --host=i586-mingw32msvc
make -j3
cd ..
# build GDAL
cd gdal
CC="ccache i586-mingw32msvc-gcc" CXX="ccache i586-mingw32msvc-g++" ./configure --enable-debug --host=i586-mingw32msvc
LD=i586-mingw32msvc-ld make USER_DEFS="-Wextra -Werror" -j3
cd apps
LD=i586-mingw32msvc-ld make USER_DEFS="-Wextra -Werror" test_ogrsf.exe
cd ..
ln -sf $PWD/.libs/libgdal-20.dll $HOME/.wine/drive_c/windows
#ln -sf /usr/lib/gcc/i686-w64-mingw32/4.6/libgcc_s_sjlj-1.dll $HOME/.wine/drive_c/windows
#ln -sf /usr/lib/gcc/i686-w64-mingw32/4.6/libstdc++-6.dll $HOME/.wine/drive_c/windows
ln -sf $PWD/../proj-4.9.2/src/.libs/libproj-9.dll $HOME/.wine/drive_c/windows
wine apps/.libs/gdalinfo.exe  --version
wget http://www.python.org/ftp/python/2.7.3/python-2.7.3.msi
wine msiexec /i python-2.7.3.msi
cd swig/python
#sed "s/i586-mingw32msvc/i686-w64-mingw32/" < fallback_build_mingw32_under_unix.sh > tmp.sh
#mv tmp.sh fallback_build_mingw32_under_unix.sh
CXX=i586-mingw32msvc-g++ bash fallback_build_mingw32_under_unix.sh 
cd ../..
