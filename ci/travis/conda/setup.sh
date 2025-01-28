#!/bin/bash

conda info
conda list
conda config --show-sources

rm -f ~/.condarc

# For Python 3.13
conda config --add channels conda-forge/label/python_rc

conda config --show-sources

conda config --show

conda install -c conda-forge conda-build -y

git clone  https://github.com/conda-forge/gdal-feedstock.git

cd gdal-feedstock

patch -p1 < ../ci/travis/conda/libgdal-adbc.patch
patch -p1 < ../ci/travis/conda/muparser.patch

# Patch version = "X.Y.Z" to "X.Y.99"
sed 's/version = "\([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)"/version = "\1.\2.99"/' < recipe/meta.yaml > meta.yaml
mv meta.yaml recipe/meta.yaml

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
