.. _gdal_sieve:

================================================================================
gdal_sieve.py
================================================================================

.. only:: html

    Removes small raster polygons.

.. Index:: gdal_sieve

Synopsis
--------

.. code-block::

    gdal_sieve.py [-q] [-st threshold] [-4] [-8] [-o name=value]
            srcfile [-nomask] [-mask filename] [-of format] [dstfile]

Description
-----------

:program:`gdal_sieve.py` script removes raster polygons smaller than
a provided threshold size (in pixels) and replaces them with the
pixel value of the largest neighbour polygon. The result can be written
back to the existing raster band, or copied into a new file.

The input dataset is read as integer data which means that floating point
values are rounded to integers. Re-scaling source data may be necessary in
some cases (e.g. 32-bit floating point data with min=0 and max=1).

Additional details on the algorithm are available in the :cpp:func:`GDALSieveFilter` docs.
