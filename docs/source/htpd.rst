.. _htpd:

================================================================================
HTPD
================================================================================

.. contents:: Contents
   :depth: 2
   :backlinks: none

This page documents use of the `crs2crs2grid.py` script and the HTDP
(Horizontal Time Dependent Positioning) grid shift modelling program from
NGS/NOAA to produce PROJ.4 compatible grid shift files for fine grade
conversions between various NAD83 epochs and WGS84.  Traditionally PROJ.4 has
treated NAD83 and WGS84 as equivalent and failed to distinguish between
different epochs or realizations of those datums.  At the scales of much
mapping this is adequate but as interest grows in high resolution imagery and
other high resolution mapping this is inadequate.  Also, as the North American
crust drifts over time the displacement between NAD83 and WGS84 grows (more
than one foot over the last two decades).

Getting and building HTDP
--------------------------------------------------------------------------------

The HTDP modelling program is in written FORTRAN.  The source and documentation
can be found on the HTDP page at http://www.ngs.noaa.gov/TOOLS/Htdp/Htdp.shtml

On linux systems it will be necessary to install `gfortran` or some FORTRAN
compiler.  For ubuntu something like the following should work.

::

    apt-get install gfortran

To compile the program do something like the following to produce the binary "htdp" from the source code.

::

    gfortran htdp.for -o htdp

Getting crs2crs2grid.py
--------------------------------------------------------------------------------

The `crs2crs2grid.py` script can be found at
https://github.com/OSGeo/gdal/tree/trunk/gdal/swig/python/samples/crs2crs2grid.py

It depends on having the GDAL Python bindings operational.  If they are not
available you will get an error something like the following:


::

    Traceback (most recent call last):
      File "./crs2crs2grid.py", line 37, in <module>
        from osgeo import gdal, gdal_array, osr
    ImportError: No module named osgeo

Usage
--------------------------------------------------------------------------------

::

    crs2crs2grid.py
            <src_crs_id> <src_crs_date> <dst_crs_id> <dst_crs_year>
            [-griddef <ul_lon> <ul_lat> <ll_lon> <ll_lat> <lon_count> <lat_count>]
            [-htdp <path_to_exe>] [-wrkdir <dirpath>] [-kwf]
            -o <output_grid_name>

 -griddef: by default the following values for roughly the continental USA
           at a six minute step size are used:
           -127 50 -66 25 251 611
 -kwf: keep working files in the working directory for review.

::

    crs2crs2grid.py 29 2002.0 8 2002.0 -o nad83_2002.ct2

The goal of `crs2crs2grid.py` is to produce a grid shift file for a designated
region.  The region is defined using the `-griddef` switch.  When missing a
continental US region is used.  The script creates a set of sample points for
the grid definition, runs the "htdp" program against it and then parses the
resulting points and computes a point by point shift to encode into the final
grid shift file.  By default it is assumed the `htdp` program will be in the
executable path.  If not, please provide the path to the executable using the
`-htdp` switch.

The `htdp` program supports transformations between many CRSes and for each (or
most?) of them you need to provide a date at which the CRS is fixed.  The full
set of CRS Ids available in the HTDP program are:

::

  1...NAD_83(2011) (North America tectonic plate fixed)
  29...NAD_83(CORS96)  (NAD_83(2011) will be used)
  30...NAD_83(2007)    (NAD_83(2011) will be used)
  2...NAD_83(PA11) (Pacific tectonic plate fixed)
  31...NAD_83(PACP00)  (NAD 83(PA11) will be used)
  3...NAD_83(MA11) (Mariana tectonic plate fixed)
  32...NAD_83(MARP00)  (NAD_83(MA11) will be used)

  4...WGS_72                             16...ITRF92
  5...WGS_84(transit) = NAD_83(2011)     17...ITRF93
  6...WGS_84(G730) = ITRF92              18...ITRF94 = ITRF96
  7...WGS_84(G873) = ITRF96              19...ITRF96
  8...WGS_84(G1150) = ITRF2000           20...ITRF97
  9...PNEOS_90 = ITRF90                  21...IGS97 = ITRF97
 10...NEOS_90 = ITRF90                   22...ITRF2000
 11...SIO/MIT_92 = ITRF91                23...IGS00 = ITRF2000
 12...ITRF88                             24...IGb00 = ITRF2000
 13...ITRF89                             25...ITRF2005
 14...ITRF90                             26...IGS05 = ITRF2005
 15...ITRF91                             27...ITRF2008
                                         28...IGS08 = ITRF2008

The typical use case is mapping from NAD83 on a particular date to WGS84 on
some date.  In this case the source CRS Id "29" (NAD_83(CORS96)) and the
destination CRS Id is "8 (WGS_84(G1150)).  It is also necessary to select the
source and destination date (epoch).  For example:

::

    crs2crs2grid.py 29 2002.0 8 2002.0 -o nad83_2002.ct2

The output is a CTable2 format grid shift file suitable for use with PROJ.4
(4.8.0 or newer).  It might be utilized something like:


::

    cs2cs +proj=latlong +ellps=GRS80 +nadgrids=./nad83_2002.ct2 +to +proj=latlong +datum=WGS84

See Also
--------------------------------------------------------------------------------

* http://www.ngs.noaa.gov/TOOLS/Htdp/Htdp.shtml - NGS/NOAA page about the HTDP
  model and program.  Source for the HTDP program can be downloaded from here.
