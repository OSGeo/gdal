GDAL/OGR
========

This Python package and extensions are a number of tools for programming and 
manipulating the GDAL_ Geospatial Data Abstraction Library.  Actually, it is 
two libraries -- GDAL for manipulating geospatial raster data and OGR for 
manipulating geospatial vector data -- but we'll refer to the entire package 
as the GDAL library for the purposes of this document.

Dependencies
------------
 
 * libgdal (1.5.0 or greater) and header files (gdal-devel)
 * numpy (1.0.0 or greater) and header files (numpy-devel)

Installation
------------

The GDAL Python bindings support both distutils and setuptools, with a 
preference for using setuptools.  If setuptools can be imported, setup will 
use that to build an egg by default.  If setuptools cannot be imported, a 
simple distutils root install of the GDAL package (and no dependency 
chaining for numpy) will be made.  

easy_install
~~~~~~~~~~~~

GDAL can be installed from the Python CheeseShop::

  $ sudo easy_install GDAL

It may be necessary to have libgdal and its development headers installed 
if easy_install is expected to do a source build because no egg is available 
for your specified platform and Python version.

setup.py
~~~~~~~~~

Most of setup.py's important variables are controlled with the setup.cfg 
file.  In setup.cfg, you can modify pointers to include files and libraries.  
The most important option that will likely need to be modified is the 
gdal_config parameter.  If you installed GDAL from a package, the location 
of this program is likely /usr/bin/gdal-config, but it may be in another place 
depending on how your packager arranged things.  

After modifying the location of gdal-config, you can build and install 
with the setup script::
  
  $ python setup.py build
  $ python setup.py install

If you have setuptools installed, you can also generate an egg::
  
  $ python setup.py bdist_egg


SWIG
----

The GDAL Python package is built using SWIG_. The earliest version of SWIG_ 
that is supported to generate the wrapper code is 1.3.31.  It is possible 
that usable bindings will build with a version earlier than 1.3.31, but no 
development efforts are targeted at versions below it.  You should not have 
to run SWIG in your development tree to generate the binding code, as it 
is usually included with the source.  However, if you do need to regenerate, 
you can do so with the following make command from within the ./swig/python
directory::

  $ make generate

To ensure that all of the bindings are regenerated, you can clean the 
bindings code out before the generate command by issuing::

  $ make veryclean

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
If you are using GDAL 1.5 bindings, you should update your imports to utilize 
the usage above, but the following will work until at least GDAL 2.0. ::

  >>> import gdal
  >>> import ogr
  >>> import osr
  >>> import gdalnumeric
  >>> import gdalconst

Docstrings
~~~~~~~~~~

Currently, only the OGR module has docstrings which are generated from the 
C/C++ API doxygen materials.  Some of the arguments and types might not 
match up exactly with what you are seeing from Python, but they should be 
enough to get you going.  Docstrings for GDAL and OSR are planned for another 
release.


.. _GDAL: http://www.gdal.org
.. _SWIG: http://www.swig.org