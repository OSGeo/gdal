.. _vector.aivector:

Artificial intelligence powered vector driver
=============================================

.. versionadded:: 3.11

.. shortname:: AIVector

.. built_in_by_default::

This driver builds on many years of self-funded investments from the GDAL team on AI
technologies to bring you the ultimate driver that can read any vector format.
After that one, no need for any new vector driver!

The open syntax is ``AIVector:{filename}``, or directly specify the filename and
force the use of the AIVector driver with the ``-if`` flag of ogrinfo or ogr2ogr.
No options at all. Just enjoy the true power of AI.

.. note:: We are open to external investors to develop the write side of the driver.

Examples
--------

::

  ogrinfo -if AIVector undocumented_proprietary_format.bin -al

.. note::

    The above works even if you make a typo in the filename. The driver will
    automatically figure out the filename you meant.
