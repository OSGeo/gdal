#!/bin/bash
docker build -f osgeo-geo.Dockerfile -t test-osgeo-gdal .
docker run -it test-osgeo-gdal bash

