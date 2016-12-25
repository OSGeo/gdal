#!/bin/bash

cd docs
echo "building docs for $TRAVIS_BUILD_DIR/docs"
docker run -v $TRAVIS_BUILD_DIR:/data -w /data/docs osgeo/proj.4 make html


