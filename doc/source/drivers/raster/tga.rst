.. _raster.tga:

================================================================================
TGA -- TARGA Image File Format
================================================================================

.. versionadded:: 3.2

.. shortname:: TGA

.. built_in_by_default::

The TGA driver currently supports reading TGA 2.0 files.

The driver supports reading 1 (grey-level or paletted), 3 (RGB), and 4
(RGBA, or RGB-undefined) band images. The driver supports both uncompressed or
RLE-compressed images. Top-left or bottom-left origins are supported (if the
later, the lines are re-order to expose a top-left order). 16-bit RGB encoded
images are also supported.

The following metadata items may be reported: IMAGE_ID, AUTHOR_NAME and
COMMENTS. The extended area is used to determine if the fourth band is an alpha
channel or not.

Driver capabilities
-------------------

.. supports_virtualio::

Links
-----

- `Format specification <http://www.dca.fee.unicamp.br/~martino/disciplinas/ea978/tgaffs.pdf>`_
