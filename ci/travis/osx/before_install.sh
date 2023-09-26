#!/bin/sh

set -e

conda update -n base -c defaults conda
conda install -y compilers automake pkgconfig cmake

conda config --set channel_priority strict
conda install --yes --quiet proj python=3.8 swig lxml jsonschema -y
conda install --yes --quiet libgdal=3.7.0=hc13fe4b_4  --only-deps -y
