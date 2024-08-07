#!/bin/sh

set -e

conda update -n base -c defaults conda
conda install -y compilers automake pkgconfig cmake

conda config --set channel_priority strict
conda install --yes --quiet proj python=3.12 swig lxml jsonschema numpy setuptools
conda install --yes --quiet libgdal libgdal-arrow-parquet
# Now remove all libgdal* packages, but not their dependencies
conda remove --yes --force $(conda list libgdal | grep libgdal | awk '{print $1}')
