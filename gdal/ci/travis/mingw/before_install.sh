#!/bin/sh

set -e

sudo apt-get update -qq
sudo apt-get install -qq ccache wine swig curl mingw32
#sudo apt-get install -y gcc-mingw-w64
#sudo apt-get install -y g++-mingw-w64
#sudo apt-get install -y mingw-w64
