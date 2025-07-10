.. _gdal_dataset_identify:

================================================================================
``gdal dataset identify``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Identify driver opening dataset(s).

.. Index:: gdal dataset identify

:program:`gdal manage-dataset identify` reports the name of drivers that can open one or
several dataset(s).

Synopsis
--------

.. program-output:: gdal dataset identify --help-doc

Options
-------

.. option:: --filename <FILENAME>

    Any file name or directory name. Required. May be repeated

.. option:: -f, --of, --format, --output-format json|text

    Which output format to use. Default is JSON, or text when invoked from command line.

.. option:: -r, --recursive

    Recursively scan files/folders for datasets.

.. option:: --force-recursive

    Recursively scan folders for datasets, forcing recursion in folders
    recognized as valid formats.

.. option:: --report-failures

    Report failures if file type is unidentified.

Examples
--------

.. example::
   :title: Identifying a single file

   .. code-block:: console

       $ gdal dataset identify --of=text NE1_50M_SR_W.tif

       NE1_50M_SR_W.tif: GTiff

.. example::
   :title: Identifying a single file with JSON output

   .. code-block:: console

       $ gdal dataset identify NE1_50M_SR_W.tif

   .. code-block:: json

       [
          {
            "name": "NE1_50M_SR_W.tif",
            "driver": "GTiff"
          }
       ]

.. example::
   :title: Recursive mode will scan subfolders and report the data format

    .. code-block::

        $ gdal dataset identify --of=text -r 50m_raster/

        NE1_50M_SR_W/ne1_50m.jpg: JPEG
        NE1_50M_SR_W/ne1_50m.png: PNG
        NE1_50M_SR_W/ne1_50m_20pct.tif: GTiff
        NE1_50M_SR_W/ne1_50m_band1.tif: GTiff
        NE1_50M_SR_W/ne1_50m_print.png: PNG
        NE1_50M_SR_W/NE1_50M_SR_W.aux: HFA
        NE1_50M_SR_W/NE1_50M_SR_W.tif: GTiff
        NE1_50M_SR_W/ne1_50m_sub.tif: GTiff
        NE1_50M_SR_W/ne1_50m_sub2.tif: GTiff
