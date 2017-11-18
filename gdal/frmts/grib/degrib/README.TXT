WORK-IN-PROGRESS
----------------

Plan is to merge back to the GDAL copy:
* g2clib:
   http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/g2clib-1.6.0.tar (Last updated: 02/18/2016)

   Merged:

   TODO:
        cmplxpack.c
        compack.c
        comunpack.c
        dec_jpeg2000.cpp
        dec_png.c
        drstemplates.c
        drstemplates.h
        enc_jpeg2000.c
        enc_png.c
        g2_addfield.c
        g2_addgrid.c
        g2_addlocal.c
        g2_create.c
        g2_free.c
        g2_getfld.c
        g2_gribend.c
        g2_info.c
        g2_miss.c
        g2_unpack1.c
        g2_unpack2.c
        g2_unpack3.c
        g2_unpack4.c
        g2_unpack5.c
        g2_unpack6.c
        g2_unpack7.c
        gbits.c
        getdim.c
        getpoly.c
        grib2c.doc
        grib2.h
        gridtemplates.c
        gridtemplates.h
        int_power.c
        jpcpack.c
        jpcunpack.c
        misspack.c
        mkieee.c
        pack_gp.c
        pdstemplates.c
        pdstemplates.h
        pngpack.c
        pngunpack.c
        rdieee.c
        reduce.c
        seekgb.c
        simpack.c
        simunpack.c
        specpack.c
        specunpack.c


PROVENANCE (at r40749)
----------

This directory contains a *modified* version of degrib 2.14
   http://www.mdl.nws.noaa.gov/~degrib/download/archive/degrib-20171016.tar.gz

The degrib-20171016.tar.gz includes a bundled version of the g2clib library,
at version 1.0.5, while GDAL g2clib is a *modified* g2clib 1.0.4.

Up-to-date versions of degrib can be found at
    https://www.weather.gov/mdl/degrib_archive
    https://sats.nws.noaa.gov/~degrib/download/archive

Up-to-date versions of g2clib can be found at
    http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/
