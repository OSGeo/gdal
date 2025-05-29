GDALG output (on-the-fly / streamed dataset)
--------------------------------------------

This program supports serializing the command line as a JSON file using the ``GDALG`` output format.
The resulting file can then be opened as a vector dataset using the
:ref:`vector.gdalg` driver, and apply the specified pipeline in a on-the-fly /
streamed way.

.. note::

    However this algorithm is not natively streaming compatible. Consequently a
    in-memory temporary dataset will be generated, which may cause significant processing
    time at opening.

