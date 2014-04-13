gdal-pixfun-plugin
==================

A small gdal plugin that provides a small set of "pixel functions" (see
http://www.gdal.org/gdal_vrttut.html) that can be used to create derived
raster bands.


List of pixel functions
-----------------------

:"real":
    extract real part from a single raster band (just a copy if the
    input is non-complex)
:"imag":
    extract imaginary part from a single raster band (0 for
    non-complex)
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
-----------------------------

The gdal-pixfun-plugin can be built using the following command::

  $ make

in the root folder of the source distribution.
It assumes that GDAL is correctly installed on your system.

To run the unit test suite::

  $ make check

To install the plugin just copy the generated shared object (gdal_PIXFUN.so)
into the GDAL plugin directory (/usr/lib/gdalplugins/1.XX/ on unix).


License
-------

See the LICENSE.txt file.
