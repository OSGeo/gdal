gdal-utils
=============

This gdal-utils Python package includes modules and functions that use and
extend the GDAL_ Geospatial Data Abstraction Library.

The GDAL project maintains SWIG generated Python bindings for GDAL and OGR,
which are included in the gdal package.

Please also refer to the osgeo documentation and to the `GDAL API Tutorial`_.

Currently, The the gdal package also includes the utils package,
but in the future they might be available only in this gdal-utils package.

Versioning of GDAL and gdal-utils are independent from each other, e.g.
gdal-utils v3.0 works just fine with GDAL 3.4. We do recommend upgrading to
the latest versions of each as general practice.


Dependencies
------------

 * gdal (the osgeo package)
 * numpy (1.0.0 or greater) and header files (numpy-devel) (not explicitly
   required, but many examples and utilities will not work without it)


Installation
------------

gdal-utils can be installed from pypi.org::

  $ python -m pip install gdal-utils

After install the utilities are in ``PYTHYONHOME\Scripts`` and can be
invoked like regular programs, ``gdal_edit`` instead of ``gdal_edit.py`` or
``python path/to/gdal_edit.py`` for example.


Important note for Packagers
----------------------------

Starting March 2022 installing gdal-utils with pip will use Setuptools'
_console_scripts_, which turn the the scripts into native platform
executables that call the script using the appropriate platform interpreter.
This means you no longer need to something similar as a post-install step.
If this causes problems with your distribution please file an issue on
Github.


.. _GDAL API Tutorial: https://gdal.org/tutorials/
.. _GDAL: http://www.gdal.org
