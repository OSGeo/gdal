.. _gdalmanage:

================================================================================
gdalmanage
================================================================================

.. only:: html

    Identify, delete, rename and copy dataset files.

.. Index:: gdalmanage

Synopsis
--------

.. code-block::

    Usage: gdalmanage [--help] [--help-general]
                      <mode> [-r] [-fr] [-u] [-f <format>]
                      <datasetname> [<newdatasetname>]

Description
-----------

The :program:`gdalmanage` program can perform various operations on dataset
files, depending on the chosen *mode*. This includes identifying dataset
types and deleting, renaming or copying the files.

.. option:: <mode>

    Mode of operation

    **identify** *<datasetname>*:
        List data format of file(s).
    **copy** *<datasetname>* *<newdatasetname>*:
        Create a copy of the dataset file with a new name.
    **rename** *<datasetname>* *<newdatasetname>*:
        Change the name of the dataset file.
    **delete** *<datasetname>*:
        Delete dataset file(s).

.. option:: -r

    Recursively scan files/folders for dataset files.

.. option:: -fr

    Recursively scan folders for dataset files, forcing recursion in folders recognized as valid formats.

.. option:: -u

    Report failures if file type is unidentified.

.. option:: -f <format>

    Specify format of dataset file if unknown by the application. Uses
    short data format name (e.g. *GTiff*).

.. option:: <datasetname>

    Raster file to operate on. With **identify** may be repeated for multiple files.

.. option:: <newdatasetname>

    For copy and rename modes, you provide a *source* filename and a
    *target* filename, just like copy and move commands in an operating
    system.

Examples
--------

Using identify mode
~~~~~~~~~~~~~~~~~~~

Report the data format of the dataset file by using the *identify* mode
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

Copy the dataset:

.. code-block::

    $ gdalmanage copy NE1_50M_SR_W.tif ne1_copy.tif

Using rename mode
~~~~~~~~~~~~~~~~~

Rename dataset:

.. code-block::

    $ gdalmanage rename NE1_50M_SR_W.tif ne1_rename.tif

Using delete mode
~~~~~~~~~~~~~~~~~

Delete the dataset:

.. code-block::

    gdalmanage delete NE1_50M_SR_W.tif
