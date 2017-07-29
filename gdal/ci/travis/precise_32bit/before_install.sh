#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
mkdir -p "$chroot$PWD"
sudo apt-get update
sudo apt-get install -y debootstrap
export LC_ALL=en_US
sudo i386 debootstrap --arch=i386 precise "$chroot"
sudo mount --rbind "$PWD" "$chroot$PWD"
sudo mount --rbind /dev/pts "$chroot/dev/pts"
sudo mount --rbind /proc "$chroot/proc"
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu precise universe" >> buildroot.i386/etc/apt/sources.list'
sudo i386 chroot "$chroot" apt-get update
sudo i386 chroot "$chroot" apt-get install -y clang
sudo i386 chroot "$chroot" apt-get install -y python-software-properties
sudo i386 chroot "$chroot" add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
#sudo i386 chroot "$chroot" add-apt-repository -y ppa:marlam/gta
sudo i386 chroot "$chroot" apt-get update
# Disable postgresql since it draws ssl-cert that doesn't install cleanly
# postgis postgresql-9.1 postgresql-client-9.1 postgresql-9.1-postgis-2.1 postgresql-9.1-postgis-2.1-scripts libpq-dev
sudo i386 chroot "$chroot" apt-get install -y python-numpy libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev  liblcms2-2 libpcre3-dev libcrypto++-dev
# libgta-dev
sudo i386 chroot "$chroot" apt-get install -y make
sudo i386 chroot "$chroot" apt-get install -y python-dev
sudo i386 chroot "$chroot" apt-get install -y g++
