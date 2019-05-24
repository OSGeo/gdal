.. _rfc-17:

================================================================================
RFC 17: Python Namespaces
================================================================================

Author: Howard Butler

Contact: hobu.inc@gmail.com

Status: Adopted

Summary
-------

| GDAL bindings for Python have historically dodged the normal Python
  practices of using packages and namespaces to provide organization.
| This RFC implements a new namespace for Python, called *osgeo*, where
  the GDAL Python bindings henceforth will reside. Backward
  compatibility is provided, so that current code will continue to run
  unchanged, but new developments should utilize the namespace for code
  organization and global namespace pollution reasons. As of 10/1/2007,
  the changes described here in RFC 17 only pertain to the "next-gen"
  Python bindings. It is expected that these bindings will be the
  default bindings for GDAL 1.5.

Objective
---------

To provide the GDAL Python bindings in a Python package that is properly
namespaced, eliminating pollution of Python's global namespace.

Past Usage
----------

GDAL's Python bindings previously used globally-aware Python modules:

::

   import gdal
   import osr
   import ogr
   import gdalconst
   import gdalnumeric

New Usage
---------

RFC 17 now provides these modules under the *osgeo* namespace:

::

   from osgeo import gdal
   from osgeo import osr
   from osgeo import ogr
   from osgeo import gdalconst
   from osgeo import gdal_array

Additionally, the old module-style imports continue to work with a
deprecation warning:

::

   >>> import gdal
   /Users/hobu/svn/gdal/swig/python/gdal.py:3: DeprecationWarning: gdal.py was placed in a namespace, it is now available as osgeo.gdal
     warn('gdal.py was placed in a namespace, it is now available as osgeo.gdal', DeprecationWarning)

It is planned that we will remove the GDAL-specific global modules at
some point in the future.

Other Sprint Updates
--------------------

The work for this RFC was done at the FOSS4G2007 GDAL code sprint by
Howard Butler and Chris Barker. In addition to the Python namespacing,
some minor issues were dealt with respect to building the GDAL bindings.

1. The next-gen Python bindings now use setuptools by default if it is
   available.
2. The ./swig/python directory was slightly reorganized to separate
   extension building from pure python modules.
3. gdal2tiles, a Google Summer of Code project by Petr Klokan, was
   integrated into the next-gen bindings

Voting History
--------------

A voice vote (our first ever!) commenced at the FOSS4G2007 sprint.

-  Frank Warmerdam +1
-  Howard Butler +1
-  Daniel Morissette +1
-  Tamas Szekerest +1
