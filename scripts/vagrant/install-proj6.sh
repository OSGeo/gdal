#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit

echo "Cloning proj sources..."
# git clone --depth 1 https://github.com/osgeo/proj.4.git proj
curl https://download.osgeo.org/proj/proj-6.0.0RC1.tar.gz > proj-6.0.0RC1.tar.gz
tar xzf proj-6.0.0RC1.tar.gz
mv proj-6.0.0 proj
echo "#!/bin/sh" > proj/autogen.sh
chmod +x proj/autogen.sh
(cd proj/data && curl http://download.osgeo.org/proj/proj-datumgrid-1.8.tar.gz > proj-datumgrid-1.8.tar.gz && tar xvzf proj-datumgrid-1.8.tar.gz)

echo "Build and install proj6..."
mkdir proj/build-x86_64
(cd proj; ./autogen.sh)
(cd proj/build-x86_64; CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ../configure --prefix=/usr/local/ && make -j3 && sudo make install && sudo mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && sudo rm /usr/local/lib/libproj.so* && sudo rm /usr/local/lib/libproj.a && sudo rm /usr/local/lib/libproj.la && sudo ln -s libinternalproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15 && sudo ln -s libinternalproj.so.15.0.0 /usr/local/lib/libinternalproj.so)
