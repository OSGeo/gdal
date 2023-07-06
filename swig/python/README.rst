
GDAL/OGR in Python
==================

This Python package and extensions are a number of tools for programming and
manipulating the GDAL_ Geospatial Data Abstraction Library.  Actually, it is
two libraries -- GDAL for manipulating geospatial raster data and OGR for
manipulating geospatial vector data -- but we'll refer to the entire package
as the GDAL library for the purposes of this document.

The GDAL project (primarily Even Rouault) maintains SWIG generated Python
bindings for GDAL and OGR. Generally speaking the classes and methods mostly
match those of the GDAL and OGR C++ classes. There is no Python specific
reference documentation, but the `GDAL API Tutorial`_ includes Python examples.

Dependencies
------------

 * libgdal (3.7.1 or greater) and header files (gdal-devel)
 * numpy (1.0.0 or greater) and header files (numpy-devel) (not explicitly
   required, but many examples and utilities will not work without it)

Installation
------------

Conda
~~~~~

GDAL can be quite complex to build and install, particularly on Windows and MacOS.
Pre built binaries are provided for the conda system:

https://docs.conda.io/en/latest/

By the conda-forge project:

https://conda-forge.org/

Once you have Anaconda or Miniconda installed, you should be able to install GDAL with:

``conda install -c conda-forge gdal``

Unix
~~~~

The GDAL Python bindings requires setuptools.

pip
~~~

GDAL can be installed from the Python Package Index:

  $ pip install GDAL

It will be necessary to have libgdal and its development headers installed
if pip is expected to do a source build because no wheel is available
for your specified platform and Python version.

To install the version of the Python bindings matching your native GDAL library:

  $ pip install GDAL=="$(gdal-config --version).*"

Building as part of the GDAL library source tree
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also have the GDAL Python bindings built as part of a source
build::

  $ cmake ..

Use the typical cmake build and install commands to complete the installation::

  $ cmake --build .
  $ cmake --build . --target install

Windows
~~~~~~~

You will need the following items to complete an install of the GDAL Python
bindings on Windows:

* `GDAL Windows Binaries`_ Download the package that best matches your environment.

As explained in the README_EXE.txt file, after unzipping the GDAL binaries you
will need to modify your system path and variables. If you're not sure how to
do this, read the `Microsoft Knowledge Base doc`_

1. Add the installation directory bin folder to your system PATH, remember
   to put a semicolon in front of it before you add to the existing path.

   ::

     C:\gdalwin32-1.7\bin

2. Create a new user or system variable with the data folder from
   your installation.

   ::

     Name : GDAL_DATA
     Path : C:\gdalwin32-1.7\data

Skip down to the `Usage`_ section to test your install. Note, a reboot
may be required.

SWIG
----

The GDAL Python package is built using SWIG_. The currently supported version
is SWIG >= 4

Usage
-----

Imports
~~~~~~~

There are five major modules that are included with the GDAL_ Python bindings.::

  >>> from osgeo import gdal
  >>> from osgeo import ogr
  >>> from osgeo import osr
  >>> from osgeo import gdal_array
  >>> from osgeo import gdalconst

Additionally, there are five compatibility modules that are included but
provide notices to state that they are deprecated and will be going away.
If you are using GDAL 1.7 bindings, you should update your imports to utilize
the usage above, but the following will work until GDAL 3.1. ::

  >>> import gdal
  >>> import ogr
  >>> import osr
  >>> import gdalnumeric
  >>> import gdalconst

If you have previous code that imported the global module and still need to
support the old import, a simple try...except import can silence the
deprecation warning and keep things named essentially the same as before::

  >>> try:
  ...     from osgeo import gdal
  ... except ImportError:
  ...     import gdal

Docstrings
~~~~~~~~~~

Currently, only the OGR module has docstrings which are generated from the
C/C++ API doxygen materials.  Some of the arguments and types might not
match up exactly with what you are seeing from Python, but they should be
enough to get you going.  Docstrings for GDAL and OSR are planned for a future
release.

Numpy
-----

One advanced feature of the GDAL Python bindings not found in the other
language bindings is integration with the Python numerical array
facilities. The gdal.Dataset.ReadAsArray() method can be used to read raster
data as numerical arrays, ready to use with the Python numerical array
capabilities.

Examples
~~~~~~~~

One example of GDAL/numpy integration is found in the `val_repl.py`_ script.

Performance Notes
~~~~~~~~~~~~~~~~~

ReadAsArray expects to make an entire copy of a raster band or dataset unless
the data are explicitly subsetted as part of the function call. For large
data, this approach is expected to be prohibitively memory intensive.

.. _GDAL API Tutorial: https://gdal.org/tutorials/
.. _GDAL Windows Binaries: http://gisinternals.com/sdk/
.. _Microsoft Knowledge Base doc: http://support.microsoft.com/kb/310519
.. _Python Package Index: https://pypi.org/project/GDAL/
.. _val_repl.py: http://trac.osgeo.org/gdal/browser/trunk/gdal/swig/python/gdal-utils/osgeo_utils/samples/val_repl.py
.. _GDAL: http://www.gdal.org
.. _SWIG: http://www.swig.org
