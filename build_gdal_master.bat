
REM build gdal thir_party_libraries from source code
cd third_party_app

SET BUILD_FOLDER="build"
if not exist %BUILD_FOLDER% (
    mkdir build
)

cd build

REM SET LIB_FOLDER='%cd%\third-party\install'
if not exist "%cd%\third-party\install" (
   cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DWITH_GTest=ON -DWITH_GTest_EXTERNAL=ON -G "CodeBlocks - MinGW Makefiles"
   cmake --build . --config Release
)

REM change directory and build gdal from source code
cd ..
cd ..
if not exist %BUILD_FOLDER% (
    mkdir build
)

cd build

cmake .. -DPROJ_LIBRARY="%cd%/../third_party_app/build/third-party/install/lib/libproj.dll.a" -DPROJ_INCLUDE_DIR="%cd%/../third_party_app/build/third-party/install/include" -DSQLite3_LIBRARY="%cd%/../third_party_app/build/third-party/install/lib/libsqlite3.dll.a" -DSQLite3_INCLUDE_DIR="%cd%/../third_party_app/build/third-party/install/include" -DGDAL_USE_HDF5=OFF -DGDAL_USE_BLOSC=OFF -DGDAL_USE_ODBC=OFF -DCMAKE_INSTALL_PREFIX="%cd%\..\installed" -G "CodeBlocks - MinGW Makefiles"
cmake --build . --config Release --target install

echo "current dir: %cd%"
SET SRC_DIR="%cd%"
SET DST_DIR="%cd%/../installed/Lib/site-packages/osgeo"
SET GWIN_SRC_DIR="%cd%/../installed"
SET GWIN_DST_DIR="%cd%/../../GDAL_win"

for /f "delims=" %%f in ('dir %SRC_DIR%\libgdal* /b') do (
   REM echo "%%f"
   @copy "%cd%\%%f" %DST_DIR%
)

if not exist %GWIN_DST_DIR% (
	mkdir %GWIN_DST_DIR%
)
xcopy %GWIN_SRC_DIR% %GWIN_DST_DIR% /s