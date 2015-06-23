gdal-pixfun-plugin
==================

A gdal plugin that provides a small set of "pixel functions" (see
http://www.gdal.org/gdal_vrttut.html) that can be used to create derived
raster bands.

The package provides:

* the implementation of a set of GDALDerivedPixelFunc(s) to be used with
  source raster band of virtual GDAL datasets
* a fake GDAL driver to register pixel functions

.. note::

    using the plugin mechanism is a hack aimed to enable python users
    to use pixel functions without C++ coding


List of pixel functions
-----------------------

:"real":
    extract real part from a single raster band (just a copy if the
    input is non-complex)
:"imag":
    extract imaginary part from a single raster band (0 for
    non-complex)
:"makecomplex":
    make a complex band merging two bands used as real and imag values
:"mod":
    extract module from a single raster band (real or complex)
:"phase":
    extract phase from a single raster band (0 for non-complex)
:"conj":
    computes the complex conjugate of a single raster band (just a
         copy if the input is non-complex)
:"sum":
    sum 2 or more raster bands
:"diff":
    computes the difference between 2 raster bands (b1 - b2)
:"mul":
    multilpy 2 or more raster bands
:"cmul":
    multiply the first band for the complex conjugate of the second
:"inv":
    inverse (1./x). Note: no check is performed on zero division
:"intensity":
    computes the intensity Re(x*conj(x)) of a single raster band
    (real or complex)
:"sqrt":
    perform the square root of a single raster band (real only)
:"log10":
    compute the logarithm (base 10) of the abs of a single raster
    band (real or complex): log10( abs( x ) )
:"dB2amp":
    perform scale conversion from logarithmic to linear
    (amplitude) (i.e. 10 ^ ( x / 20 ) ) of a single raster band (real only)
:"dB2pow":
    perform scale conversion from logarithmic to linear
    (power) (i.e. 10 ^ ( x / 10 ) ) of a single raster band (real only)


How to get it
-------------

The project home page is at https://github.com/avalentino/gdal-pixfun-plugin.
A copy of the latest version of the sources can be obtained ising git_::

  $ git clone https://github.com/avalentino/gdal-pixfun-plugin.git

.. _git: http://git-scm.com


How to build, test and install
------------------------------

The gdal-pixfun-plugin can be built using the following command::

  $ make

in the root folder of the source distribution.
It assumes that GDAL is correctly installed on your system.

To run the unit test suite::

  $ make check

To install the plugin just copy the generated shared object (gdal_PIXFUN.so)
into the GDAL plugin directory (/usr/lib/gdalplugins/1.XX/ on unix).

The plugin can also be used without installing it.
The user just needs to set the GDAL_DRIVER_PATH environment variable::

    export GDAL_DRIVER_PATH=<PATH_TO_GDAL_PIXFUN_SO>:$GDAL_DRIVER_PATH


License
-------

See the LICENSE.txt file.
