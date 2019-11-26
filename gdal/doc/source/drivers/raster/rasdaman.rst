.. _raster.rasdaman:

================================================================================
Rasdaman GDAL driver
================================================================================

.. shortname:: Rasdaman

.. build_dependencies:: raslib

Rasdaman is a raster database middleware offering an SQL-style query
language on multi-dimensional arrays of unlimited size, stored in a
relational database. See `www.rasdaman.org <http://www.rasdaman.org>`__
for the open-source code, documentation, etc. Currently rasdaman is
under consideration for OSGeo incubation.

In our driver implementation, GDAL connects to rasdaman by defining a
query template which is instantiated with the concrete subsetting box
upon every access. This allows delivering 2-D cutouts from n-D data sets
(such as hyperspectral satellite time series, multi-variable climate
simulation data, ocean model data, etc.). In particular, virtual imagery
can be offered which is derived on demand from ground truth data. Some
more technical details are given below.

| The connect string syntax follows the WKT Raster pattern and goes like
  this:
| rasdaman: query='select a[$x_lo:$x_hi,$y_lo:$y_hi] from MyImages as a'
  [tileXSize=1024] [tileYSize=1024] [host='localhost'] [port=7001]
  [database='RASBASE'] [user='rasguest'] [password='rasguest']

The rasdaman query language (rasql) string in this case only performs
subsetting. Upon image access by GDAL, the $ parameters are substituted
by the concrete bounding box computed from the input tile coordinates.

| However, the query provided can include any kind of processing, as
  long as it returns something 2-D. For example, this determines the
  average of red and near-infrared pixels from the oldest image time
  series:
| query='select ( a.red+a.nir ) /2 [$x_lo:$x_hi,$y_lo:$y_hi, 0 ] from
  SatStack as a'

Currently there is no support for reading or writing georeferencing
information.

See Also
--------

-  `Rasdaman Project <http://www.rasdaman.org/>`__
