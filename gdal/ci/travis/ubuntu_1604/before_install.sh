#!/bin/sh

set -e

export chroot="$PWD"/xenial
mkdir -p "$chroot$PWD"
sudo apt-get update
sudo apt-get install -y debootstrap
export LC_ALL=en_US
sudo debootstrap xenial "$chroot"
sudo mount --rbind "$PWD" "$chroot$PWD"
sudo mount --rbind /dev/pts "$chroot/dev/pts"
sudo mount --rbind /proc "$chroot/proc"
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu xenial universe" >> xenial/etc/apt/sources.list'
sudo chroot "$chroot" apt-get update
#sudo chroot "$chroot" apt-get install -y clang
sudo chroot "$chroot" apt-get install -y software-properties-common python-software-properties
sudo chroot "$chroot" add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
sudo chroot "$chroot" apt-get update
# Disable postgresql since it draws ssl-cert that doesn't install cleanly
# postgis postgresql-9.1 postgresql-client-9.1 postgresql-9.1-postgis-2.1 postgresql-9.1-postgis-2.1-scripts libpq-dev
sudo chroot "$chroot" apt-get install -y python-numpy libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev
# libkml-dev
sudo chroot "$chroot" apt-get install -y doxygen texlive-latex-base
sudo chroot "$chroot" apt-get install -y make
sudo chroot "$chroot" apt-get install -y python-dev
sudo chroot "$chroot" apt-get install -y g++
wget http://llvm.org/releases/3.9.0/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
tar xJf clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
