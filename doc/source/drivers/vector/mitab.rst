.. _vector.mitab:

MapInfo TAB and MIF/MID
=======================

.. shortname:: MapInfo File

.. built_in_by_default::

MapInfo datasets in native (TAB) format and in interchange (MIF/MID)
format are supported for reading and writing.
Update of existing TAB files is supported (append of new features,
modifications and deletions of existing features,
adding/renaming/deleting fields, ...). Update of existing MIF/MID files
is not supported.

Note: In the rest of this document "MIF/MID File" is used to refer to a
pair of .MIF + .MID files, and "TAB file" refers to the set of files for
a MapInfo table in binary form (usually with extensions .TAB, .DAT,
.MAP, .ID, .IND).

The MapInfo driver treats a whole directory of files as a dataset, and a
single file within that directory as a layer. In this case the directory
name should be used as the dataset name.

However, it is also possible to use one of the files (.tab or .mif) in a
MapInfo set as the dataset name, and then it will be treated as a
dataset with one single layer.

MapInfo coordinate system information is supported for reading and
writing.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

The TAB File format requires that the bounds (geographical extents) of a
new file be set before writing the first feature.

There is currently no automated setting of valid default bounds for each
spatial reference system, so for the time being, the MapInfo driver sets
the following default bounds when a new layer is created:

-  For a file in LAT/LON (geographic) coordinates: BOUNDS (-180, -90)
   (180, 90)
-  For any other projection: BOUNDS (-30000000 + false_easting,
   -15000000 + false_northing) (30000000 + false_easting, 15000000 +
   false_northing)

It is possible to override those bounds through two mechanisms.

-  specify a user-defined file that contain projection definitions with
   bounds. The name of this file must be specified with the
   MITAB_BOUNDS_FILE configuration option. This allows users to override
   the default bounds for existing projections, and to define bounds for
   new projections not listed in the hard-coded table in the driver. The
   format of the file is a simple text file with one CoordSys string per
   line. The CoordSys lines should follow the MIF specs, and MUST
   include the optional Bounds definition at the end of the line, e.g.

   ::

      # Lambert 93 French bounds
      CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000002, 700000, 6600000 Bounds (75000, 6000000) (1275000, 7200000)

   It is also possible to establish a mapping between a source CoordSys
   and a target CoordSys with bounds. Such a mapping is specified by
   adding a line starting with "Source = " followed by a CoordSys
   (spaces before or after the equal sign do not matter). The following
   line should start with "Destination = " followed by a CoordSys with
   bounds, e.g.

   ::

      # Map generic Lambert 93 to French Lambert 93, Europe bounds
      Source      = CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49, 700000, 6600000
      Destination = CoordSys Earth Projection 3, 33, "m", 3, 46.5, 44, 49.00000000001, 700000, 6600000 Bounds (-792421, 5278231) (3520778, 9741029)

-  use the BOUNDS layer creation option (see below)

If no coordinate system is provided when creating a layer, the
projection case is used, not geographic, which can result in very low
precision if the coordinates really are geographic. You can add "-a_srs
WGS84" to the **ogr2ogr** commandline during a translation to force
geographic mode.

MapInfo feature attributes suffer a number of limitations:

-  Only Integer, Real and String field types can be created. The various
   list, and binary field types cannot be created.
-  For String fields, the field width is used to establish storage size
   in the .dat file. This means that strings longer than the field width
   will be truncated
-  String fields without an assigned width are treated as 254
   characters.

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

-  **FORMAT=MIF**: To create MIF/MID instead of TAB files (TAB is the
   default).
-  **SPATIAL_INDEX_MODE=QUICK/OPTIMIZED**: The default is QUICK force
   "quick spatial index mode". In this mode writing files can be about 5
   times faster, but spatial queries can be up to 30 times slower. This
   can be set to OPTIMIZED to generate optimized spatial index.
-  **BLOCKSIZE=[512,1024,...,32256]** (multiples of 512):
   Block size for .map files. Defaults to 512.
   MapInfo 15.2 and above creates .tab files with a blocksize of 16384
   bytes. Any MapInfo version should be able to handle block sizes from
   512 to 32256.

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **BOUNDS=xmin,ymin,xmax,ymax**: Define custom layer
   bounds to increase the accuracy of the coordinates. Note: the
   geometry of written features must be within the defined box.
-  **ENCODING=**\ *value*: (GDAL >=2.3) Define the encoding for field
   names and field values. The encoding name is specified in the format
   supported by CPLRecode (e.g. ISO-8859-1, CP1251, CP1252 ...) and
   internally converted to MapInfo charsets names. Default value is ''
   that equals to 'Neutral' MapInfo charset.
-  **DESCRIPTION=**\ *value*: (GDAL >= 3.1.0) Friendly layer name (only for
   TAB format). Friendly names can be up to 256 characters long and can include
   most ASCII characters. Supported by MapInfo Pro v15.0 or higher.

Configuration options
~~~~~~~~~~~~~~~~~~~~~

The following :ref:`configuration options <configoptions>` are
available:

-  :decl_configoption:`MITAB_SET_TOWGS84_ON_KNOWN_DATUM` =YES/NO:
   (GDAL >= 3.0.3). The default behavior, starting with GDAL 3.0.3, is NO.
   That is, the TOWGS84 parameters read from the .tab header will *not* be set
   on the Datum object of the CRS, when the datum can be inferred.

See Also
~~~~~~~~

-  `MITAB Page <http://mitab.maptools.org/>`__
-  `About friendly layer names <https://support.pitneybowes.com/SearchArticles/VFP05_KnowledgeWithSidebarHowTo?id=kA180000000CtuHCAS&popup=false&lang=en_US>`__
