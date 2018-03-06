#!/bin/sh

set -e

cat /etc/apt/sources.list
ls -al /etc/apt/sources.list.d
sudo apt-get update -qq
sudo apt-get install ccache
sudo apt-get install binutils-mingw-w64-x86-64
sudo apt-get install gcc-mingw-w64-x86-64
sudo apt-get install g++-mingw-w64-x86-64
sudo apt-get install g++-mingw-w64
sudo apt-get install mingw-w64-tools
sudo apt-get install -y wine1.4-amd64
