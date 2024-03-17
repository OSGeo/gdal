#!/bin/sh

set -e

conda update -n base -c defaults conda
conda install -y compilers automake pkgconfig cmake

conda config --set channel_priority strict
conda install --yes --quiet proj python=3.12 swig lxml jsonschema numpy -y
conda install --yes --quiet libgdal libgdal-arrow-parquet --only-deps -y
