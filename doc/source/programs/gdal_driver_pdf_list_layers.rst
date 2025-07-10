.. _gdal_driver_pdf_list_layers:

================================================================================
``gdal driver pdf list-layer``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Return the list of layers of a PDF file.

.. Index:: gdal driver pdf list-layer

Synopsis
--------

.. We are not using 'program-output:: gdal driver pdf list-layer --help-doc' on purpose,
   because the PDF driver needs to have read support for that operation.

.. code-block::

    Usage: gdal driver pdf list-layers [OPTIONS] <INPUT>

    List layers of a PDF dataset

    Positional arguments:
      -i, --input <INPUT>                                  Input raster or vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format. OUTPUT-FORMAT=json|text (default: json)


Description
-----------

Return the list of layers of a PDF file, as a JSON array or a text list.

Examples
--------

.. example::
   :title: Return the list of layers of a PDF file as a JSON array

   .. code-block:: bash

       gdal driver pdf list-layers autotest/gdrivers/data/pdf/adobe_style_geospatial.pdf

   returns:

   .. code-block:: json

        [
          "New_Data_Frame",
          "New_Data_Frame.Graticule",
          "Layers",
          "Layers.Measured_Grid",
          "Layers.Graticule"
        ]


.. example::
   :title: Return the list of layers of a PDF file as as a text list

   .. code-block:: bash

       gdal driver pdf list-layers --of=text autotest/gdrivers/data/pdf/adobe_style_geospatial.pdf

   returns:

   .. code-block::

        New_Data_Frame
        New_Data_Frame.Graticule
        Layers
        Layers.Measured_Grid
        Layers.Graticule
