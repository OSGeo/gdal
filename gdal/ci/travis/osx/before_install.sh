#!/bin/sh

set -e

conda update -n base -c defaults conda
conda install compilers -y
conda install automake -y
conda install pkgconfig -y

conda config --set channel_priority strict
conda install --yes --quiet proj=7.1.1=h45baca5_3 python=3.8  -y
conda install --yes --quiet libgdal=3.1.4=hd7bf8dc_0  --only-deps -y
