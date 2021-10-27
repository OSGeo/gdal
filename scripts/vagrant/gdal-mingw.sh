#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

echo "Configure for mingw-w64"

NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

rsync -a /vagrant/gdal/ /home/vagrant/gnumake-build-mingw-w64
rsync -a --exclude='__pycache__' /vagrant/autotest/ /home/vagrant/gnumake-build-mingw-w64/autotest
echo rsync -a /vagrant/gdal/ /home/vagrant/gnumake-build-mingw-w64/ > /home/vagrant/gnumake-build-mingw-w64/resync.sh
echo rsync -a --exclude='__pycache__' /vagrant/autotest/ /home/vagrant/gnumake-build-mingw-w64/autotest >> /home/vagrant/gnumake-build-mingw-w64/resync.sh

chmod +x /home/vagrant/gnumake-build-mingw-w64/resync.sh

export CCACHE_CPP2=yes

wine64 cmd /c dir
ln -sf /usr/lib/gcc/x86_64-w64-mingw32/4.8/libstdc++-6.dll  "$HOME/.wine/drive_c/windows"
ln -sf /usr/lib/gcc/x86_64-w64-mingw32/4.8/libgcc_s_sjlj-1.dll  "$HOME/.wine/drive_c/windows"
ln -sf /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll  "$HOME/.wine/drive_c/windows"
ln -sf /usr/local/x86_64-w64-mingw32/bin/libsqlite3-0.dll "$HOME/.wine/drive_c/windows"
ln -sf /usr/local/x86_64-w64-mingw32/bin/libproj-15.dll "$HOME/.wine/drive_c/windows"
ln -sf /usr/x86_64-w64-mingw32/lib/libgeos_c-1.dll "$HOME/.wine/drive_c/windows"
ln -sf /usr/x86_64-w64-mingw32/lib/libgeos-3-5-0.dll "$HOME/.wine/drive_c/windows"

(cd /home/vagrant/gnumake-build-mingw-w64
    CC="ccache x86_64-w64-mingw32-gcc" CXX="ccache x86_64-w64-mingw32-g++" LD=x86_64-w64-mingw32-ld \
    ./configure --prefix=/usr/x86_64-w64-mingw32  --host=x86_64-w64-mingw32  --with-geos \
     --with-sqlite3=/usr/local/x86_64-w64-mingw32 --with-proj=/usr/local/x86_64-w64-mingw32 \
    ln -sf "$PWD/.libs/libgdal-20.dll" "$HOME/.wine/drive_c/windows"

    # Python bindings
    sudo wget -N -nv -P /var/cache/wget/ http://www.python.org/ftp/python/2.7.15/python-2.7.15.amd64.msi
    wine64 msiexec /i /var/cache/wget/python-2.7.15.amd64.msi
    cd swig/python
    gendef "$HOME/.wine/drive_c/Python27/python27.dll"
    x86_64-w64-mingw32-dlltool --dllname "$HOME/.wine/drive_c/Python27/python27.dll" --input-def python27.def --output-lib "$HOME/.wine/drive_c/Python27/libs/libpython27.a"
)

#
echo "------------------------------------------------------"
echo "You now can run cross building with mingw-w64, please run on $PWD"
echo "make -j $NUMTHREADS"
echo "cd swig/python; CXX=x86_64-w64-mingw32-g++ bash fallback_build_mingw32_under_unix.sh"
echo "------------------------------------------------------"
