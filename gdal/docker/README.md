GDAL Docker images
==================

This directory contains a number of Dockerfile for different configurations.
Each directory contains a `./build.sh` for convenient building of the image.

Note: the mention of the overall licensing terms of the GDAL build is to the
best of our knowledge and not guaranteed. Users should check by themselves.

# Alpine based

## Ultra small: `osgeo/gdal:alpine-ultrasmall-latest`

* Image size: < 40 MB
* Raster drivers: VRT, GTiff, HFA, PNG, JPEG, MEM, JP2OpenJPEG, WEB, GPKG
* Vector drivers: Shapefile, MapInfo, VRT, Memory, GeoJSON, GPKG, SQLite
* External libraries enabled: libsqlite3, libproj, libcurl, libjpeg, libpng, libwebp, libzstd
* No GDAL Python
* Base PROJ grid package
* Overall licensing terms of the GDAL build: permissive (X/MIT, BSD style, Apache, etc..)

See [alpine-ultrasmall/Dockerfile](alpine-ultrasmall/Dockerfile)

## Small: `osgeo/gdal:alpine-small-latest`

* Image size: ~ 52 MB
* Raster drivers: ultrasmall + built-in + SQLite-based ones + network-based ones
* Vector drivers: ultrasmall + built-in + most XML-based ones + network-based ones + PostgreSQL
* External libraries enabled: ultrasmall + libexpat, libpq, libssl
* No GDAL Python
* Base PROJ grid package
* Overall licensing terms of the GDAL build: permissive (X/MIT, BSD style, Apache, etc..)

See [alpine-small/Dockerfile](alpine-small/Dockerfile)

## Nomal: `osgeo/gdal:alpine-normal-latest`

* Image size: ~ 155 MB
* Raster drivers: small + netCDF, HDF5, BAG
* Vector drivers: small + Spatialite, XLS
* External libraries enabled: small + libgeos, libhdf5, libnetcdf, libfreexl,
  libspatialite, libxml2, libpoppler
* GDAL Python (Python 2.7)
* Base PROJ grid package
* Overall licensing terms of the GDAL build: copy-left (GPL) + LGPL + permissive

See [alpine-normal/Dockerfile](alpine-normal/Dockerfile)

# Ubuntu based (18:04 / bionic)

## Small: `osgeo/gdal:ubuntu-small-latest`

* Image size: ~ 270 MB
* Raster drivers: all built-in + JPEG + PNG + JP2OpenJPEG + WEBP +SQLite-based ones + network-based ones
* Vector drivers: all built-in + XML based ones + SQLite-based ones + network-based ones + PostgreSQL
* External libraries enabled: libsqlite3, libproj, libcurl, libjpeg, libpng, libwebp,
  libzstd, libexpat, libxerces-c, libpq, libssl, libgeos
* GDAL Python (Python 2.7)
* Base PROJ grid package
* Overall licensing terms of the GDAL build: LGPL + permissive (X/MIT, BSD style, Apache, etc..)

See [ubuntu-small/Dockerfile](ubuntu-small/Dockerfile)

## Full: `osgeo/gdal:ubuntu-full-latest`

* Image size: ~ 1.8 GB
* Raster drivers: all based on almost all possible free and open-source dependencies
* Vector drivers: all based on almost all possible free and open-source dependencies
* External libraries enabled: small + libnetcdf, libhdf4, libhdf5, libtiledb, libkea,
  mongocxx 3.4, libspatialite, unixodbc, libxml2, libcfitsio, libmysqlclient,
  libkml, libpoppler
* GDAL Python (Python 2.7)
* *All* PROJ grid packages
* Overall licensing terms of the GDAL build: copy-left (GPL) + LGPL + permissive

See [ubuntu-full/Dockerfile](ubuntu-full/Dockerfile)


# Usage example

```shell
docker pull osgeo/gdal:alpine-small-latest
docker run --rm -v /home:/home osgeo/gdal:alpine-small-latest gdalinfo $PWD/my.tif
```
