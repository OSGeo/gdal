.. _vector.vfk:

VFK - Czech Cadastral Exchange Data Format
==========================================

.. shortname:: VFK

.. build_dependencies:: libsqlite3

This driver reads VFK files, i.e. data in the *Czech cadastral exchange
data format*. The VFK file is recognized as an datasource with multiple
layers.

The driver is compiled only if GDAL is *built with SQLite support*.

Points are represented as wkbPoints, lines and boundaries as
wkbLineStrings and areas as wkbPolygons. wkbMulti\* features are not
used. Feature types cannot be mixed in one layer.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

|about-open-options|
Starting with GDAL 2.3, the following open options can be specified:

-  .. oo:: SUPPRESS_GEOMETRY
      :choices: YES, NO
      :default: NO

      Setting it to YES
      will skip resolving geometry. All layers will be recognized with no
      geometry type. Mostly useful when user is interested at attributes
      only. Note that suppressing geometry can cause significant
      performance gain when reading input VFK data by the driver.

-  .. oo:: FILE_FIELD
      :choices: YES, NO
      :default: NO

      Setting it to YES will
      append new field *VFK_FILENAME* containing name of source VFK file to
      all layers.

Configuration options
---------------------

|about-config-options|
The following configuration options are available:

-  .. config:: OGR_VFK_DB_NAME

      Determine the name of the SQLite backend database used when reading VFK
      data. By default, SQLite database is created in a directory of input VFK
      file (with file extension '.db').

-  .. config:: OGR_VFK_DB_OVERWRITE
      :choices: YES, NO

      Determines whether the driver should overwrite an existing SQLite
      database and stores data read from input VFK file into newly created DB.

-  .. config:: OGR_VFK_DB_DELETE
      :choices: YES, NO

      Determines whether the driver should delete the backend SQLite database
      when closing the datasource.

-  .. config:: OGR_VFK_DB_SPATIAL
      :choices: YES, NO

      Determines whether the driver should store resolved geometries in the
      backend SQLite database. If ``YES``, geometries are resolved only once
      when building SQLite database from VFK data. Geometries are stored in WKB
      format. Note that GDAL doesn't need to be built with SpatiaLite support.
      If ``NO``, geometries are not stored in the DB and are resolved when
      reading data from DB on the fly.

-  .. config:: OGR_VFK_DB_READ_ALL_BLOCKS
      :choices: YES, NO

      Determines whether all data blocks should be read, or only the
      data blocks requested by the used.

-  .. config:: OGR_VFK_DB_READ
      :choices: YES, NO

      If ``YES``, opening a VFK backend SQLite database will cause
      the VFK driver to be used instead of the SQLite driver.

Internal working and performance tweaking
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If backend SQLite database already exists then the driver reads features
directly from the database and not from input VFK file given as an input
datasource. This causes significant performance gain when reading
features by the driver.

The driver reads by default all data blocks from VFK
file when building backend SQLite database. When configuration option
:config:`OGR_VFK_DB_READ_ALL_BLOCKS` =NO is given, the driver
reads only data blocks which are requested by the user. This can be
useful when the user want to process only part of VFK data.

Examples
~~~~~~~~

Data related to a single cadastral area is typically distributed in
multiple VFK files. Example below is based on `sample VFK files
<https://services.cuzk.cz/vfk/anonym/>`__ provided by the Czech State
Administration of Land Surveying and Cadastre. In order to process all
VFK files related to a single cadastral area (in example below with ID
602515), the configuration option :config:`OGR_VFK_DB_NAME` has to be
defined.

   ::

      # load first file mapa/602515.vfk
      ogrinfo --config OGR_VFK_DB_NAME 602515.db mapa/602515.vfk
      # load second file spi_s_jpv/602515.vfk
      ogrinfo --config OGR_VFK_DB_NAME 602515.db spi_s_jpv/602515.vfk
      # now we can access eg. geometry of parcels
      ogrinfo 602515.db PAR -fid 1
      ...

Datasource name
---------------

Datasource name is a full path to the VFK file.

The driver supports reading files managed by VSI Virtual File System
API, which include "regular" files, as well as files in the /vsizip/,
/vsigzip/, and /vsicurl/ read-only domains.

Since GDAL 2.2 also a full path to the backend SQLite database can be
used as an datasource. By default, such datasource is read by SQLite
driver. If configuration option :config:`OGR_VFK_DB_READ` =YES
is given, such datasource is opened by VFK driver instead.

Layer names
-----------

VFK data blocks are used as layer names.

Filters
-------

Attribute filter
~~~~~~~~~~~~~~~~

An internal SQL engine is used to evaluate the expression. Evaluation is
done once when the attribute filter is set.

Spatial filter
~~~~~~~~~~~~~~

Bounding boxes of features stored in topology structure are used to
evaluate if a features matches current spatial filter. Evaluation is
done once when the spatial filter is set.

References
----------

-  `OGR VFK Driver Implementation
   Issues <https://www.researchgate.net/publication/238067945_OGR_VFK_Driver_Implementation_Issues>`__
-  `Czech cadastral exchange data format
   documentation <http://www.cuzk.cz/Dokument.aspx?PRARESKOD=998&MENUID=0&AKCE=DOC:10-VF_ISKNTEXT>`__
   (in Czech)
