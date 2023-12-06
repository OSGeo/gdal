#!/bin/bash

conda info
conda list
conda config --show-sources

rm -f ~/.condarc

conda config --show-sources

conda config --show

conda install -c conda-forge conda-build -y

git clone  https://github.com/conda-forge/gdal-feedstock.git

cd gdal-feedstock

# Set -DOGR_REGISTER_DRIVER_ARROW_FOR_LATER_PLUGIN=ON and -DOGR_REGISTER_DRIVER_PARQUET_FOR_LATER_PLUGIN=ON
# To be dropped once that's upstreamed in conda-forge/gdal-feedstock.git
patch -p1 < ../ci/travis/conda/build.sh.patch
patch -p1 --binary < ../ci/travis/conda/bld.bat.patch

patch -p1 < ../ci/travis/conda/0001-Fix-build-of-Python-bindings-due-to-https-github.com.patch

cat > recipe/recipe_clobber.yaml <<EOL
source:
  path: ../../../gdal
  url:
  sha256:
  patches:

build:
  number: 2112
EOL


ls recipe
