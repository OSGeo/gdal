WORK-IN-PROGRESS
----------------

Plan is to merge back to the GDAL copy:
* Degrib Version 2.14 (10/16/2017)
   http://www.mdl.nws.noaa.gov/~degrib/download/archive/degrib-20171016.tar.gz
   Note: Degrib 2.14 is originally bundled with g2clib 1.0.5
* g2clib:
   http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/g2clib-1.6.0.tar (Last updated: 02/18/2016)

Status:
    degrib/
        Merged:
            src/degrib/clock.c
            src/degrib/clock.h
            src/degrib/tendian.c
            src/degrib/tendian.h
            src/degrib/degrib1.c
            src/degrib/degrib1.h
            src/degrib/grib1tab.c
            src/degrib/degrib2.h
            src/degrib/degrib2.c
            src/degrib/weather.c
            src/degrib/weather.h
            src/degrib/meta.h
            src/mdl_g2c/engribapi.c
            src/mdl_g2c/engribapi.h
            src/mdl_g2c/mdl_g2c.h -> grib2api.h
            src/mdl_g2c/grib2api.c

        TODO:
            src/degrib/inventory.c
            src/degrib/inventory.h
            src/degrib/metaname.c
            src/degrib/metaname.h
            src/degrib/metaparse.c
            src/degrib/metaprint.c
            src/degrib/myassert.c
            src/degrib/myassert.h
            src/degrib/myerror.c
            src/degrib/myerror.h
            src/degrib/myutil.c
            src/degrib/myutil.h
            src/degrib/scan.c
            src/degrib/scan.h
            src/degrib/tdlpack.c
            src/degrib/tdlpack.h
            src/degrib/type.h


PROVENANCE (at r40728)
----------

This directory contains a *modified* version of - presumably - degrib 1.80 (given
the original subdirectory name degrib18), with partial merges of degrib-2.0.3
per r35437

https://sats.nws.noaa.gov/~degrib/release/degrib1.80.txt

The original 1.80 version is apparently no longer available on the web. The closest
one is degrib 1.81:
https://sats.nws.noaa.gov/~degrib/download/archive/degrib-20060831.tar.gz

The degrib-XXXX.tar.gz includes a bundled version of the g2clib library,
at version 1.0.4 in the case of degrib 1.80

This internal g2clib has also been modified for GDAL needs.

Up-to-date versions of degrib can be found at
    https://www.weather.gov/mdl/degrib_archive
    https://sats.nws.noaa.gov/~degrib/download/archive

Up-to-date versions of g2clib can be found at
    http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/
