gdal-utils
=============

This gdal-utils Python package is a sub-set of the GDAL Osgeo Python package.

If you've installed the GDAL package you have these utils. However you
may wish to get a newer version of the utilities without changing GDAL,
that's what this package is for.

For using and developing the utils please refer to the osgeo documentation
and to the `GDAL API Tutorial`_.

We don't systematically test the utils against a different versions of gdal.
Versioning of GDAL and gdal-utils are independent from each other, today's
gdal-utils v3.3 works with GDAL v3.4 but it might not with v3.2. We
recommend upgrading to the latest versions of each as general practice. See
'Packaging' below for how to test compatibility.


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


Packaging
---------

Starting March 2022 installing gdal-utils with pip will use Setuptools'
_console_scripts_, which turn the the scripts into native platform
executables that call the script using the appropriate platform interpreter.
This means you no longer need to something similar as a post-install step.
If this causes problems with your distribution please file an issue on
Github.

Recipe for testing gdal-utils compatibility against your installed GDAL
binaries::

    # Get installed GDAL version
    export _GDALVER=`gdal-config --version``

    # verify python osgeo is present
    python -c "from osgeo import gdal;print(f'Python Osgeo version: {gdal.__version__}')"

    git clone https://github.com/OSGeo/gdal.git --depth=50
    git remote set-branches origin "*"
    git fetch -v --depth=50

    # Install current version of gdal-utils
    cd swig/python/gdal-utils
    pip install .
    # OR, to use published PyPi.org version:
    # pip install gdal-utils

    # set source code tree to match binary gdal version
    git checkout v$_GDALVER

    cd gdal/autotest
    python install -r requirements.txt
    pytest ./pyscripts


.. _GDAL API Tutorial: https://gdal.org/tutorials/
.. _GDAL: http://www.gdal.org
