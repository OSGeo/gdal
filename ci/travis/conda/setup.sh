#!/bin/bash

conda info
conda list
conda config --show-sources

rm -f ~/.condarc

# Cf https://github.com/conda-forge/gdal-feedstock/pull/939
conda config --add channels conda-forge/label/numpy_rc

conda config --show-sources

conda config --show

conda install -c conda-forge conda-build -y

git clone  https://github.com/conda-forge/gdal-feedstock.git

cd gdal-feedstock

patch -p1 < ../ci/travis/conda/install_python.sh.patch

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
