.. _raster.hf2:

================================================================================
HF2 -- HF2/HFZ heightfield raster
================================================================================

.. shortname:: HF2

.. built_in_by_default::

GDAL supports reading and writing HF2/HFZ/HF2.GZ heightfield raster
datasets.

HF2 is a heightfield format that records difference between consecutive
cell values. HF2 files can also optionally be compressed by the gzip
algorithm, and so the HF2.GZ files (or HFZ, equivalently) may be
significantly smaller than the uncompressed data. The file format
enables the user to have control on the desired accuracy through the
vertical precision parameter.

GDAL can read and write georeferencing information through extended
header blocks.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Creation options
----------------

-  .. co:: COMPRESS
      :choices: YES, NO
      :default: NO

      Whether the file must be compressed with GZip.

-  .. co:: BLOCKSIZE
      :default: 256

      Internal tile size. Must be >= 8.

-  .. co:: VERTICAL_PRECISION
      :default: 0.01

      Vertical precision. Must be > 0.
      Increasing the vertical precision decreases the file size, especially
      with :co:`COMPRESS=YES`, but at the loss of accuracy.

See also
--------

-  `Specification of HF2/HFZ
   format <http://www.bundysoft.com/docs/doku.php?id=l3dt:formats:specs:hf2>`__
-  `Specification of HF2 extended header
   blocks <http://www.bundysoft.com/docs/doku.php?id=l3dt:formats:specs:hf2#extended_header>`__
