.. _gdalmanage:

================================================================================
gdalmanage
================================================================================

.. only:: html

    Identify, delete, rename and copy raster data files.

.. Index:: gdalmanage

Synopsis
--------

.. code-block::

    Usage: gdalmanage mode [-r] [-u] [-f format]
                      datasetname [newdatasetname]

Description
-----------

The :program:`gdalmanage` program can perform various operations on raster data
files, depending on the chosen *mode*. This includes identifying raster
data types and deleting, renaming or copying the files.

.. option:: <mode>

    Mode of operation

    **identify** *<datasetname>*:
        List data format of file.
    **copy** *<datasetname>* *<newdatasetname>*:
        Create a copy of the raster file with a new name.
    **rename** *<datasetname>* *<newdatasetname>*:
        Change the name of the raster file.
    **delete** *<datasetname>*:
        Delete raster file.

.. option:: -r

    Recursively scan files/folders for raster files.

.. option:: -u

    Report failures if file type is unidentified.

.. option:: -f <format>

    Specify format of raster file if unknown by the application. Uses
    short data format name (e.g. *GTiff*).

.. option:: <datasetname>

    Raster file to operate on.

.. option:: <newdatasetname>

    For copy and rename modes, you provide a *source* filename and a
    *target* filename, just like copy and move commands in an operating
    system.

Examples
--------

Using identify mode
~~~~~~~~~~~~~~~~~~~

Report the data format of the raster file by using the *identify* mode
and specifying a data file name:

.. code-block::

    $ gdalmanage identify NE1_50M_SR_W.tif

    NE1_50M_SR_W.tif: GTiff

Recursive mode will scan subfolders and report the data format:

.. code-block::

    $ gdalmanage identify -r 50m_raster/

    NE1_50M_SR_W/ne1_50m.jpg: JPEG
    NE1_50M_SR_W/ne1_50m.png: PNG
    NE1_50M_SR_W/ne1_50m_20pct.tif: GTiff
    NE1_50M_SR_W/ne1_50m_band1.tif: GTiff
    NE1_50M_SR_W/ne1_50m_print.png: PNG
    NE1_50M_SR_W/NE1_50M_SR_W.aux: HFA
    NE1_50M_SR_W/NE1_50M_SR_W.tif: GTiff
    NE1_50M_SR_W/ne1_50m_sub.tif: GTiff
    NE1_50M_SR_W/ne1_50m_sub2.tif: GTiff

Using copy mode
~~~~~~~~~~~~~~~

Copy the raster data:

.. code-block::

    $ gdalmanage copy NE1_50M_SR_W.tif ne1_copy.tif

Using rename mode
~~~~~~~~~~~~~~~~~

Rename raster data:

.. code-block::

    $ gdalmanage rename NE1_50M_SR_W.tif ne1_rename.tif

Using delete mode
~~~~~~~~~~~~~~~~~

Delete the raster data:

.. code-block::

    gdalmanage delete NE1_50M_SR_W.tif
