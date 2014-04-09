NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

svn checkout https://svn.osgeo.org/gdal/trunk gdal
cd gdal/gdal
#  --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local
./configure  --prefix=/usr --without-libtool --enable-debug --with-jpeg12 \
            --with-python --with-poppler \
            --with-podofo --with-spatialite --with-java --with-mdb \
            --with-jvm-lib-add-rpath --with-epsilon --with-gta \
            --with-mysql --with-liblzma --with-webp --with-libkml \
            --with-openjpeg=/usr/local --with-armadillo

make -j $NUMTHREADS
cd apps
make test_ogrsf
cd ..

# A previous version of GDAL has been installed by PostGIS
sudo rm -f /usr/lib/libgdal.so*
sudo make install
sudo ldconfig
# not sure why we need to do that
sudo cp -r /usr/lib/python2.7/site-packages/*  /usr/lib/python2.7/dist-packages/

cd ../..
