#!/bin/sh

set -e

sudo apt-get update -qq
sudo apt-get install -qq ccache
sudo apt-get install -qq wine swig curl
sudo apt-get install -qq mingw32
