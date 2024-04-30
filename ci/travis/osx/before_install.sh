#!/bin/sh

set -e

conda update -n base -c defaults conda
conda install -y compilers automake pkgconfig cmake

conda config --set channel_priority strict
conda install --yes --quiet proj python=3.12 swig lxml jsonschema numpy
conda install --yes --quiet --only-deps libgdal libgdal-arrow-parquet
# Remove libgdal as above installation of libgdal-arrow-parquet installed it
conda remove --yes libgdal
