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
