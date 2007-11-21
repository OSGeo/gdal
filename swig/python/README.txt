GDAL/OGR
========

This Python package and extensions are a number of tools for programming and 
manipulating the GDAL_ Geospatial Data Abstraction Library.  Actually, it is 
two libraries -- GDAL for manipulating geospatial raster data and OGR for 
manipulating geospatial vector data -- but we'll refer to the entire package 
as the GDAL library for the purposes of this document.

Dependencies
------------
 
 * libgdal (1.5.0 or greater)
 * numpy (1.0.1 or greater)

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
With the setup script::
  
  $ sudo python setup.py install

SWIG
----

The GDAL Python package is built using SWIG_. The earliest version of SWIG_ 
that is supported to generate the wrapper code is 1.3.31.  It is possible 
that usable bindings will build with a version earlier than 1.3.31, but no 
development efforts are targeted at versions below it.  

.. _GDAL: http://www.gdal.org
.. _SWIG: http://www.swig.org