.. _gdalogrin_python:

================================================================================
GDAL/OGR in Python
================================================================================

This Python package and extensions are a number of tools for programming and manipulating the `GDAL <http://www.gdal.org/>`__ Geospatial Data Abstraction Library. Actually, it is two libraries -- GDAL for manipulating geospatial raster data and 
OGR for manipulating geospatial vector data -- but we'll refer to the entire package as the GDAL library for the purposes of this document.

The GDAL project maintains SWIG generated Python bindings for GDAL and OGR.


API
---

Generally speaking the classes and methods mostly match those of the GDAL and OGR C++ classes. An automatically generated API documentation is available on `http://gdal.org/python <http://gdal.org/python>`__.
It is far from being ideal however : you will essentially get the names of available classes and functions, but few descriptions (except for OGR classes where the OGR C documentation is copied). For more details,
look at the `GDAL API Tutorial <http://www.gdal.org/gdal_tutorial.html>`__ includes Python examples.

Tutorials
---------

Chris Garrard has given courses at Utah State University on "Geoprocessing with Python using Open Source GIS" (`http://www.gis.usu.edu/~chrisg/python <http://www.gis.usu.edu/~chrisg/python>`__). There are many slides, examples, test data... and homework ;-) that can
be greatly helpful for beginners with GDAL/OGR in Python.

Greg Petrie has an OGR related tutorial available at `http://cosmicproject.org/OGR <http://cosmicproject.org/OGR>`__.

A cookbook full of recipes for using the Python GDAL/OGR bindings : `http://pcjericks.github.io/py-gdalogr-cookbook/index.html <http://pcjericks.github.io/py-gdalogr-cookbook/index.html>`__

Examples
--------

* One example of GDAL/numpy integration is found in the val_repl.py script.
* An assortment of other samples are available in the Python SVN samples directory at `http://svn.osgeo.org/gdal/trunk/gdal/swig/python/samples/ <http://svn.osgeo.org/gdal/trunk/gdal/swig/python/samples/>`__ with some description in the `samples README <http://svn.osgeo.org/gdal/trunk/gdal/swig/python/samples/README>`__.
* Several `GDAL utilities <http://svn.osgeo.org/gdal/trunk/gdal/swig/python/scripts/>`__ are implemented in Python and can be useful examples.
* The majority of GDAL regression tests are written in Python. They are available at `http://svn.osgeo.org/gdal/trunk/autotest <http://svn.osgeo.org/gdal/trunk/autotest>`__

Gotchas
-------

Although GDAL's and OGR's Python bindings provide a fairly "Pythonic" wrapper around the underlying C++ code, there are several ways in which the Python bindings differ from typical Python libraries.
These differences can catch Python programmers by surprise and lead to unexpected results. These differences result from the complexity of developing a large, long-lived library while continuing to maintain
backward compatibility. They are being addressed over time, but until they are all gone, please review this list of :ref:`python_gotchas`.


Dependencies
------------

* Python 2.X (and 3.X since GDAL 1.7.0). Python 2 versions known to work range from 2.3 to 2.7.
* libgdal (1.5.0 or greater) and header files (gdal-devel)
* numpy (1.0.0 or greater) and header files (numpy-devel) (not explicitly required, but many examples and utilities will not work without it).


Installation
------------

Unix
~~~~

The GDAL Python bindings support both distutils and setuptools, with a preference for using setuptools. If setuptools can be imported,
setup will use that to build an egg by default. If setuptools cannot be imported, a simple distutils root install of the GDAL package (and no dependency chaining for numpy) will be made.


easy_install
~~~~~~~~~~~~

GDAL can be installed from the Python CheeseShop:

.. code-block:: Bash

    $ sudo easy_install GDAL

It may be necessary to have libgdal and its development headers installed if easy_install is expected to do a source build because no egg is available for your specified platform and Python version.

setup.py
~~~~~~~~

Most of setup.py's important variables are controlled with the setup.cfg file. In setup.cfg, you can modify pointers to include files and libraries.
The most important option that will likely need to be modified is the gdal_config parameter. If you installed GDAL from a package, the location of this program is likely /usr/bin/gdal-config,
but it may be in another place depending on how your packager arranged things.

After modifying the location of gdal-config, you can build and install with the setup script:

.. code-block:: Bash

    $ python setup.py build
    $ python setup.py install

If you have setuptools installed, you can also generate an egg:

.. code-block:: Bash

    $ python setup.py bdist_egg




Building as part of the GDAL library source tree
------------------------------------------------

You can also have the GDAL Python bindings built as part of a source build by specifying --with-python as part of your configure line:



Use the typical make and make install commands to complete the installation:

.. note::
    A note about setuptools, ./configure attempts to detect if you have setuptools installed in the tree of the Python binary it was given (or detected on the execution path),
    and it will use an egg build by default in that instance. If you have a need to use a distutils-only install, you will have to edit setup.py to ensure that the HAVE_SETUPTOOLS variable
    is ultimately set to False and proceed with a typical 'python setup.py install' command.


Windows
~~~~~~~

You will need the following items to complete an install of the GDAL Python bindings on Windows:

* `GDAL Windows Binaries <http://download.osgeo.org/gdal/win32/1.6/>`__ The basic install requires the gdalwin32exe160.zip distribution file. Other files you see in the directory are for various optional plugins
  and development headers/include files. After downloading the zip file, extract it to the directory of your choosing.
* GDAL Python Bindings for GDAL 1.6.1 are available at the `Python Cheeseshop <http://pypi.python.org/pypi/GDAL/1.6.1>`__. Executable installers (MSI or egg) are available for Python 2.4, 2.5 or 2.6.

As explained in the README_EXE.txt file, after unzipping the GDAL binaries you will need to modify your system path and variables. If you're not sure how to do this, read the `Microsoft KnowledgeBase doc <http://support.microsoft.com/kb/310519>`__

1. Add the installation directory bin folder to your system PATH, remember to put a semicolon in front of it before you add to the existing path.

.. code-block:: bat

    C:\gdalwin32-1.6\bin

2. Create a new user or system variable with the data folder from your installation.

.. code-block:: bat

    Name : GDAL_DATA
    Path : C:\gdalwin32-1.6\data


Skip down to the `Usage <https://trac.osgeo.org/gdal/wiki/GdalOgrInPython#usage>`__ section to test your install. Note, a reboot may be required.

SWIG
----

The GDAL Python package is built using `SWIG <http://www.swig.org/>`__. The earliest version of `SWIG <http://www.swig.org/>`__ that is supported to generate the wrapper code is 1.3.31. It is possible that usable bindings will
build with a version earlier than 1.3.31, but no development efforts are targeted at versions below it. You should not have to run SWIG in your development tree to generate the binding code, as it is
usually included with the source. However, if you do need to regenerate, you can do so with the following make command from within the ./swig/python directory:


Usage
-----

Imports
~~~~~~~~

There are five major modules that are included with the `GDAL <http://www.gdal.org/>`__ Python bindings.:

.. code-block:: python

    >>> from osgeo import gdal
    >>> from osgeo import ogr
    >>> from osgeo import osr
    >>> from osgeo import gdal_array
    >>> from osgeo import gdalconst


Additionally, there are five compatibility modules that are included but provide notices to state that they are deprecated and will be going away. If you are using GDAL 1.5 bindings,
you should update your imports to utilize the usage above, but the following will work until at least GDAL 2.0.

.. code-block:: python

    >>> import gdal
    >>> import ogr
    >>> import osr
    >>> import gdalnumeric
    >>> import gdalconst

If you have previous code that imported the global module and still need to support the old import, a simple try...except import can silence the deprecation warning and keep things named essentially the same as before:

.. code-block:: python

    >>> try:
    ...     from osgeo import gdal
    ... except ImportError:
    ...     import gdal

Docstrings
~~~~~~~~~~

Currently, only the OGR module has docstrings which are generated from the C/C++ API doxygen materials. Some of the arguments and types might not match up exactly with what you are seeing from Python,
but they should be enough to get you going. Docstrings for GDAL and OSR are planned for a future release.

The History of Using GDAL/OGR in Python
---------------------------------------

Python was the first set of bindings supported by GDAL/OGR and though the bindings were generated with SWIG (1.1 series), the process was very Python specific and contained a significant
amount of hand written wrapper code. In 2005, Kevin Ruland launched an effort for a set of next generation bindings generated with SWIG (1.3 series) and supported by a variety of languages.
With GDAL 1.4.0 the various bindings became fairly mature, and for GDAL 1.5.0, the "next-generation" bindings become the default bindings. The previous, "old-generation," bindings will continue to be available
, but they will not be widely supported and no new development will be targeted at them. From the viewpoint of a user, with GDAL 1.5.0 and above, you should not have to worry very much about the distinction
between these two development efforts.

Usage of Old-Generation Python Bindings
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For certain legacy applications (most notably OpenEV 1.x), it may be necessary to continue to use the old-generation Python bindings. These can be built and installed as part of a source build from ./configure:

.. code-block:: Bash

   ./configure --with-ogpython=/usr/bin/python


Numpy/Numeric
-------------

One advanced feature of the GDAL Python bindings not found in the other language bindings (C#, Perl) is integration with the Python numerical array facilities. The gdal.Dataset.ReadAsArray() method can
be used to read raster data as numerical arrays, ready to use with the Python numerical array capabilities.

These facilities have evolved somewhat over time. In the past the package was known as "Numeric" and imported using "import Numeric". A new generation is imported using "import numpy". Currently the old
generation bindings only support the older Numeric package, and the new generation bindings only support the new generation numpy package. They are mostly compatible, and by importing gdalnumeric (or osgeo.gdal_array)
you will get whichever is appropriate to the current bindings type.

Examples
~~~~~~~~

One example of GDAL/numpy integration is found in the `val_repl.py <http://trac.osgeo.org/gdal/browser/trunk/gdal/swig/python/samples/val_repl.py>`__ script.

.. note::
   **Perfomance Notes**

   ReadAsArray expects to make an entire copy of a raster band or dataset
   unless the data are explicitly subsetted as part of the function call. For
   large data, this approach is expected to be prohibitively memory intensive.
