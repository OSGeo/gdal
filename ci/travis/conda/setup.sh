#!/bin/bash

conda info
conda list
conda config --show-sources

rm -f ~/.condarc

# For Python 3.13
conda config --add channels conda-forge/label/python_rc

conda config --show-sources

conda config --show

conda install -c conda-forge rattler-build yq -y

git clone  https://github.com/conda-forge/gdal-feedstock.git

cd gdal-feedstock

# Patch version: "X.Y.Z" to "X.Y.99"
sed -E 's/version: "([0-9]+)\.([0-9]+)\.([0-9]+)"/version: "\1.\2.99"/' < recipe/recipe.yaml > recipe.yaml
mv recipe.yaml recipe/recipe.yaml

cat > recipe/recipe_clobber.yaml <<EOL
source:
  path: ../../../gdal

build:
  number: 2112
EOL

# single quote intended to avoid $base expansion
# shellcheck disable=SC2016
yq -y -s '.[0] as $base | .[1] as $patch | ($base * $patch) | .source = $patch.source' recipe/recipe.yaml recipe/recipe_clobber.yaml > recipe/recipe_patched.yaml

# Update installation messages of plugins to reflect the appropriate channel
sed "s/-c conda-forge/-c gdal-master/" < recipe/build_core.sh >  recipe/build_core.sh.new
mv recipe/build_core.sh.new  recipe/build_core.sh
sed "s/-c conda-forge/-c gdal-master/" < recipe/build_core.bat > recipe/build_core.bat.new
mv recipe/build_core.bat.new  recipe/build_core.bat

ls recipe
