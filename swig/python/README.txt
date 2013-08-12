
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
 
 * libgdal (1.10.0 or greater) and header files (gdal-devel)
 * numpy (1.0.0 or greater) and header files (numpy-devel) (not explicitly 
   required, but many examples and utilities will not work without it)

Installation
------------

Unix
~~~~~~~~~~~~~

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

Building as part of the GDAL library source tree
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can also have the GDAL Python bindings built as part of a source 
build by specifying --with-python as part of your configure line::

  $ ./configure --with-python

Use the typical make and make install commands to complete the installation:: 
  
  $ make
  $ make install

A note about setuptools
.......................

./configure attempts to detect if you have setuptools installed in the tree 
of the Python binary it was given (or detected on the execution path), and it 
will use an egg build by default in that instance.  If you have a need to 
use a distutils-only install, you will have to edit setup.py to ensure that 
the HAVE_SETUPTOOLS variable is ultimately set to False and proceed with a 
typical 'python setup.py install' command.

Windows
~~~~~~~~~~~~

You will need the following items to complete an install of the GDAL Python
bindings on Windows:

* `GDAL Windows Binaries`_ Download the package that best matches your environment. 

As explained in the README_EXE.txt file, after unzipping the GDAL binaries you 
will need to modify your system path and variables. If you're not sure how to 
do this, read the `Microsoft KnowledgeBase doc`_ 

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

The GDAL Python package is built using SWIG_. The earliest version of SWIG_ 
that is supported to generate the wrapper code is 1.3.40.  It is possible 
that usable bindings will build with a version earlier than 1.3.40, but no 
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
If you are using GDAL 1.7 bindings, you should update your imports to utilize 
the usage above, but the following will work until at least GDAL 2.0. ::

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

Numpy/Numeric
-------------

One advanced feature of the GDAL Python bindings not found in the other 
language bindings (C#, Perl) is integration with the Python numerical array 
facilities. The gdal.Dataset.ReadAsArray() method can be used to read raster 
data as numerical arrays, ready to use with the Python numerical array 
capabilities.

These facilities have evolved somewhat over time. In the past the package was 
known as "Numeric" and imported using "import Numeric". A new generation is 
imported using "import numpy". Currently the old generation bindings only 
support the older Numeric package, and the new generation bindings only 
support the new generation numpy package. They are mostly compatible, and 
by importing gdalnumeric (or osgeo.gdal_array) you will get whichever is
appropriate to the current bindings type.

Examples
~~~~~~~~

One example of GDAL/numpy integration is found in the `val_repl.py`_ script.

Performance Notes
~~~~~~~~~~~~~~~~~

ReadAsArray expects to make an entire copy of a raster band or dataset unless 
the data are explicitly subsetted as part of the function call. For large 
data, this approach is expected to be prohibitively memory intensive.

.. _GDAL API Tutorial: http://www.gdal.org/gdal_tutorial.html
.. _GDAL Windows Binaries: http://vbkto.dyndns.org/sdk/
.. _Microsoft KnowledgeBase doc: http://support.microsoft.com/kb/310519
.. _Python Cheeseshop: http://pypi.python.org/pypi/GDAL/
.. _val_repl.py: http://trac.osgeo.org/gdal/browser/trunk/gdal/swig/python/samples/val_repl.py
.. _GDAL: http://www.gdal.org
.. _SWIG: http://www.swig.org
