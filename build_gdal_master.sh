
# build gdal thir_party_libraries from source code
cd third_party_app

BUILD_FOLDER="build"
if [ ! -d $BUILD_FOLDER ]; then
	mkdir build
fi

cd build

LIB_FOLDER="./third-party/install"
if [ ! -d $LIB_FOLDER ]; then
   cmake .. -DWITH_ZLIB=ON -DWITH_ZLIB_EXTERNAL=ON -DWITH_EXPAT=ON -DWITH_EXPAT_EXTERNAL=ON -DWITH_JSONC=ON -DWITH_JSONC_EXTERNAL=ON -DWITH_ICONV=ON -DWITH_CURL=ON -DWITH_CURL_EXTERNAL=ON -DWITH_LibXml2=OFF -DWITH_LibXml2_EXTERNAL=OFF -DWITH_GEOS=OFF -DWITH_GEOS_EXTERNAL=OFF -DWITH_JPEG=ON -DWITH_JPEG_EXTERNAL=ON -DWITH_JPEG12=OFF -DWITH_JPEG12_EXTERNAL=OFF -DWITH_TIFF=ON -DWITH_TIFF_EXTERNAL=ON -DWITH_GeoTIFF=ON -DWITH_GeoTIFF_EXTERNAL=ON -DWITH_JBIG=ON -DWITH_JBIG_EXTERNAL=ON -DWITH_GIF=ON -DWITH_GIF_EXTERNAL=ON -DWITH_OpenCAD=OFF -DWITH_OpenCAD_EXTERNAL=OFF -DWITH_PNG=ON -DWITH_PNG_EXTERNAL=ON -DWITH_PROJ=ON -DWITH_PROJ_EXTERNAL=ON -DWITH_OpenJPEG=ON -DWITH_OpenJPEG_EXTERNAL=ON -DENABLE_OPENJPEG=ON -DWITH_OpenSSL=OFF -DWITH_OpenSSL_EXTERNAL=OFF -DWITH_LibLZMA=ON -DWITH_LibLZMA_EXTERNAL=ON -DWITH_PYTHON=ON -DWITH_PYTHON3=ON -DENABLE_OZI=ON -DENABLE_NITF_RPFTOC_ECRGTOC=ON -DGDAL_ENABLE_GNM=ON -DWITH_OCI=OFF -DWITH_OCI_EXTERNAL=OFF -DENABLE_OCI=OFF -DENABLE_GEORASTER=ON -DWITH_SQLite3=ON -DWITH_SQLite3_EXTERNAL=ON -DWITH_PostgreSQL=OFF -DWITH_PostgreSQL_EXTERNAL=OFF -WITH_Boost=ON -DWITH_Boost_EXTERNAL=ON -DWITH_KML=OFF -DWITH_KML_EXTERNAL=OFF -DGDAL_BUILD_APPS=ON -DWITH_HDF4=OFF -DWITH_HDF4_EXTERNAL=OFF -DENABLE_HDF4=OFF -DWITH_QHULL=OFF -DWITH_QHULL_EXTERNAL=OFF -DWITH_Spatialite=OFF -DWITH_Spatialite_EXTERNAL=OFF -DWITH_SZIP=ON -DWITH_SZIP_EXTERNAL=ON -DWITH_UriParser=ON -DWITH_UriParser_EXTERNAL=ON -DWITH_NUMPY=ON -DENABLE_WEBP=ON -DWITH_WEBP=OFF -DWITH_WEBP_EXTERNAL=OFF -DBUILD_TESTING=ON -DSKIP_PYTHON_TESTS=ON -DWITH_GTest=ON -DWITH_GTest_EXTERNAL=ON -DWITH_NUMPY=OFF -DWITH_NUMPY_EXTERNAL=OFF -DWITH_ICONV=ON -DWITH_ICONV_EXTERNAL=ON  -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
fi

# change directory and build gdal from source code
cd ..
cd ..
if [ ! -d $BUILD_FOLDER ]; then
	mkdir build
fi

cd build

script_dir=$(cd $(dirname $0);pwd)
echo "current path: $script_dir"

cmake .. -DPROJ_LIBRARY="$script_dir/../third_party_app/build/third-party/install/lib/libproj.so" -DPROJ_INCLUDE_DIR="$script_dir/../third_party_app/build/third-party/install/include" -DSQLite3_LIBRARY="$script_dir/../third_party_app/build/third-party/install/lib/libsqlite3.so" -DSQLite3_INCLUDE_DIR="$script_dir/../third_party_app/build/third-party/install/include" -DGDAL_USE_HDF5=OFF -DGDAL_USE_BLOSC=OFF -DGDAL_USE_ODBC=OFF -DCMAKE_INSTALL_PREFIX="$script_dir/../installed"
cmake --build . --config Release --target install

# echo the python version
U_V1=`python -V 2>&1|awk '{print $2}'|awk -F '.' '{print $1}'`
U_V2=`python -V 2>&1|awk '{print $2}'|awk -F '.' '{print $2}'`
U_V3=`python -V 2>&1|awk '{print $2}'|awk -F '.' '{print $3}'`

SRC_DIR="$script_dir"
DST_DIR="$script_dir/../installed/lib/python$U_V1.$U_V2/site-packages/osgeo"
LINUX_SRC_DIR="$script_dir/../installed"
LINUX_DST_DIR="$script_dir/../../GDAL_linux"

cp $DST_DIR/_* $DST_DIR/..
cp $SRC_DIR/libgdal* $DST_DIR/..

if [ ! -d $LINUX_DST_DIR ]; then
	mkdir $LINUX_DST_DIR
fi

cp $LINUX_SRC_DIR/* -r $LINUX_DST_DIR

echo "success building gdal project!"