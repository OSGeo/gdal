#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
mkdir -p "$chroot$PWD"
sudo apt-get update
sudo apt-get install -y debootstrap libcap2-bin
export LC_ALL=en_US.utf8
sudo i386 debootstrap --arch=i386 trusty "$chroot"
sudo mount --rbind /dev "$chroot/dev"
sudo mkdir -p "$chroot/run/shm"
sudo mount --rbind /run/shm "$chroot/run/shm"
sudo mount --rbind /proc "$chroot/proc"
sudo mount --rbind /home "$chroot/home"
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu trusty universe" >> buildroot.i386/etc/apt/sources.list'
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu trusty-updates main" >> buildroot.i386/etc/apt/sources.list'
sudo su -c 'echo "deb http://archive.ubuntu.com/ubuntu trusty-updates universe" >> buildroot.i386/etc/apt/sources.list'
sudo setcap cap_sys_chroot+ep /usr/sbin/chroot 
i386 chroot "$chroot" sh -c "echo 'Running as user'"
sudo i386 chroot "$chroot" locale-gen en_US.UTF-8
sudo i386 chroot "$chroot" update-locale LANG=en_US.UTF-8
sudo i386 chroot "$chroot" apt-get update
sudo i386 chroot "$chroot" apt-get install -y clang
sudo i386 chroot "$chroot" apt-get install -y software-properties-common  python-software-properties
sudo i386 chroot "$chroot" add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
#sudo i386 chroot "$chroot" add-apt-repository -y ppa:marlam/gta
sudo i386 chroot "$chroot" apt-get update
# Disable postgresql since it draws ssl-cert that doesn't install cleanly
# postgis postgresql-9.1 postgresql-client-9.1 postgresql-9.1-postgis-2.1 postgresql-9.1-postgis-2.1-scripts libpq-dev
sudo i386 chroot "$chroot" apt-get install -y --force-yes python3.5-dev libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev  liblcms2-2 libpcre3-dev libcrypto++-dev ccache autoconf automake sqlite3
# libgta-dev
sudo i386 chroot "$chroot" apt-get install -y make
sudo i386 chroot "$chroot" apt-get install -y g++
sudo i386 chroot "$chroot" apt-get install -y --force-yes curl
sudo i386 chroot "$chroot" sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python3.5"
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest && pip install -U -r ./requirements.txt"
