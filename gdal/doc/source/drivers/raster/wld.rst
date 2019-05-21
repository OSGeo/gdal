.. _raster.wld:

================================================================================
WLD -- ESRI World File
================================================================================

A world file file is a plain ASCII text file consisting of six values
separated by newlines. The format is:

.. code-block::

    pixel X size
    rotation about the Y axis (usually 0.0)
    rotation about the X axis (usually 0.0)
    negative pixel Y size
    X coordinate of upper left pixel center
    Y coordinate of upper left pixel center

For example:

.. code-block::

   60.0000000000
   0.0000000000
   0.0000000000
   -60.0000000000
   440750.0000000000
   3751290.0000000000

You can construct that file simply by using your favorite text editor.

World file usually has suffix ``.wld``, but sometimes it may has ``.tfw``,
``.tifw``, ``.jgw`` or other suffixes depending on the image file it comes with.
