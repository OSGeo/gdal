#!/bin/sh

set -e

cat /etc/apt/sources.list
ls -al /etc/apt/sources.list.d
sudo apt-get update -qq
sudo apt-get install -y \
    ccache automake \
    binutils-mingw-w64-x86-64 \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    g++-mingw-w64 \
    mingw-w64-tools \
    wine1.4-amd64 \
    curl
