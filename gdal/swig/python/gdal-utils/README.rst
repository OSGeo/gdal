gdal-utils
=============

This gdal-utils Python package includes modules and functions that use and extend the
GDAL_ Geospatial Data Abstraction Library.

The GDAL project (primarily Even Rouault) maintains SWIG generated Python
bindings for GDAL and OGR, which are included in the gdal package.

Please also refer to the osgeo documentation and to the `GDAL API Tutorial`_.

Currently, The the gdal package also includes the utils package,
but in the future they might be available only in this gdal-utils package.

Because we don't test the utils against a different versions of gdal -
If you want to get the latest version of the utils, we do recommend you to upgrade
to the latest version of gdal and get the latest version of both.

But if you are unable to upgrade the gdal package for whatever reason,
You may still be able to upgrade the utils by installing this package.

Dependencies
------------

 * gdal (the osgeo package)
 * numpy (1.0.0 or greater) and header files (numpy-devel) (not explicitly
   required, but many examples and utilities will not work without it)

Installation
------------

gdal-utils can be installed from pypi.org::

  $ python -m pip install gdal-utils

In case you want to run GDAL scripts on Windows,
If don't have `python.exe` associated with py file extension
(For instance, because you associate them with your IDE)
you might want to create batch wrappers for the gdal python scripts,
Otherwise you won't be able to run the scripts easily.
To create the batch files, Run the following command (after installing `gdal-utils`):

  $ python -m osgeo_utils.auxiliary.batch_creator

.. _GDAL API Tutorial: https://gdal.org/tutorials/
.. _GDAL: http://www.gdal.org
