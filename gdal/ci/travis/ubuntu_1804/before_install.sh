#!/bin/sh

set -e

export chroot="$PWD"/bionic
mkdir -p "$chroot$PWD"
sudo apt-get update
sudo apt-get install -y debootstrap libcap2-bin dpkg
export LC_ALL=en_US.utf8
sudo debootstrap bionic "$chroot"
sudo mount --rbind /dev "$chroot/dev"
sudo mount --rbind /proc "$chroot/proc"
sudo mount --rbind /home "$chroot/home"
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu bionic universe" >> bionic/etc/apt/sources.list'
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu bionic-updates universe" >> bionic/etc/apt/sources.list'
sudo su -c 'echo "en_US.UTF-8 UTF-8" >> bionic/etc/locale.gen'
sudo setcap cap_sys_chroot+ep /usr/sbin/chroot 
chroot "$chroot" sh -c "echo 'Running as user'"
sudo chroot "$chroot" locale-gen
sudo chroot "$chroot" apt-get update
#sudo chroot "$chroot" apt-get install -y clang
sudo chroot "$chroot" apt-get install -y software-properties-common
#sudo chroot "$chroot" add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
sudo chroot "$chroot" apt-get update
# Disable postgresql since it draws ssl-cert that doesn't install cleanly
# postgis postgresql-9.1 postgresql-client-9.1 postgresql-9.1-postgis-2.1 postgresql-9.1-postgis-2.1-scripts libpq-dev
sudo chroot "$chroot" apt-get install -y --allow-unauthenticated python-numpy libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev libkml-dev libmysqlclient-dev libogdi3.2-dev libcfitsio-dev openjdk-8-jdk libzstd1-dev ccache bash zip
# libpodofo-dev : FIXME incompatibilities at runtime with that version
sudo chroot "$chroot" apt-get install -y doxygen texlive-latex-base
sudo chroot "$chroot" apt-get install -y make
sudo chroot "$chroot" apt-get install -y python-dev
sudo chroot "$chroot" apt-get install -y g++
#sudo chroot "$chroot" apt-get install -y --allow-unauthenticated libsfcgal-dev
sudo chroot "$chroot" apt-get install -y --allow-unauthenticated fossil libgeotiff-dev libcharls-dev libopenjp2-7-dev libcairo2-dev

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp FileGDB_API-64gcc51/lib/* "$chroot/usr/lib"
sudo chroot "$chroot" ldconfig
