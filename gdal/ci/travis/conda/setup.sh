#!/bin/bash

if grep -q "windows" <<< "$PLATFORM"; then
  conda install -c conda-forge conda-build anaconda-client python=3.8 -y
else
  conda install -c conda-forge conda-build automake pkgconfig anaconda-client python=3.8 -y
  # Generating ./configure #
  (cd gdal && ./autogen.sh && cd ../ )
fi

git clone  https://github.com/conda-forge/gdal-feedstock.git

cd gdal-feedstock
cat > recipe/recipe_clobber.yaml <<EOL
source:
  path: ../../../gdal/gdal
  url:
  sha256:
  patches:

build:
  number: 2112
EOL


ls recipe
