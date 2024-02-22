.. _vector.miramon:

MiraMon Vectors
====================

.. shortname:: MiraMonVector

This driver is capable of translating (reading and writing) structured vectors
of point, arc (*linestrings*), and polygon types from MiraMon. Structured vectors is
the binary format of MiraMon for vector layer data, linked to one or more database tables,
with or without topology and with reach metadata. More information about the structured MiraMon
vector format is available `on the public specifications <https://www.miramon.cat/new_note/usa/notes/FormatFitxersTopologicsMiraMon.pdf>`__.

It's important to keep in mind that a MiraMon vector layer is composed by several files as follows:

Previous note: *FileName* is, in the following explanations, the first part of the name
of the layer file.   

- **Point layers**: These layers contain *point* type features. Each layer is composed by 3 files:

    - *FileName.pnt* file: Contains the graphic database with the coordinates that define the
      point vector features.

    - *FileNameT.dbf* file (note the 'T' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if necessary. It contains the information (usually alphanumeric, but also file or web links, etc)
      of the every feature. The Feature Identifier (FID) field is a field called *ID_GRAFIC* and relates
      every graphical feature to one or more records in the main table.

    - *FileNameT.rel* file (note the 'T' before the '.'): Contains the layer metadata,
      the relational structure of the database (links between the main table and other
      tables [thesauruses, etc] if needed, and the cardinality of the link) and the default
      symbolization description. In the GDAL environment
      only some aspects are documented: the spatial reference system, the language of the
      metadata (English), the extension and a description of the fields.

- **Arc layers**: These layers contain *linestring* type features. Each layer is composed by 6 files:

    - *FileName.arc* file: Contains the graphic database with the coordinates that define the
      linestring (arc) vector features.

    - *FileNameA.dbf* file (note the 'A' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if necessary. It contains the information (usually alphanumeric, but also file or web links, etc)
      of the every feature. The Feature Identifier (FID) field is a field called *ID_GRAFIC* and relates
      every graphical feature to one or more records in the main table.

    - *FileNameA.rel* file (note the 'A' before the '.'): Contains the layer metadata,
      the relational structure of the database (links between the main table and other
      tables [thesauruses, etc] if needed, and the cardinality of the link) and the default
      symbolization description. In the GDAL environment
      only some aspects are documented: the spatial reference system, the language of the
      metadata (English), the extension and a description of the fields.

    - *FileName.nod* file: Contains the graphic database with the coordinates that define the
      vector features. It is necessary in the MiraMon vector format but not read by
      the GDAL MiraMon driver because nodes contain topological information that is not
      transferred to other formats.

    - *FileNameN.dbf* file (note the 'N' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if necessary. This table contains information about
      the relationships between arcs and nodes, and other attributes of the nodes, if needed.
      It is necessary in the MiraMon vector format but not read by the GDAL MiraMon driver because
      nodes contain topological information that is not transferred to other formats.

    - *FileNameN.rel* file (note the 'N' before the '.'): Contains the layer metadata,
      the relational structure of the database (links between the main table and other
      tables [thesauruses, etc] if needed, and the cardinality of the link) and the default
      symbolization description. It is necessary in the MiraMon vector format but not read by
      the GDAL MiraMon driver because nodes contain topological information that is not
      transferred to other formats.

- **Polygon layers**: These layers contain *polygons* or *multipolygons* type features. 
  Each layer is composed by 9 files:

    - *FileName.pol* file: Contains the graphic database with the coordinates that define the
      polygonal (or multipolygonal) vector features. In fact, this file contains the list of arcs
      that compose every polygon (or polypolygon). 

    - *FileNameP.dbf* file (note the 'P' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if necessary. It contains the information (usually alphanumeric, but also file or web links, etc)
      of the every feature. The Feature Identifier (FID) field is a field called *ID_GRAFIC* and relates
      every graphical feature to one or more records in the main table.

    - *FileNameP.rel* file (note the 'P' before the '.'): Contains the layer metadata,
      the relational structure of the database (links between the main table and other
      tables [thesauruses, etc] if needed, and the cardinality of the link) and the default
      symbolization description. In the GDAL environment
      only some aspects are documented: the spatial reference system, the language of the
      metadata (English), the extension and a description of the fields.

    - *FileName.arc* file: Contains the graphic database with the coordinates that define the
      arc vector features. The polygons within the polygon file reference the arcs in this file by their index.

    - *FileNameA.dbf* file (note the 'A' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if necessary. This table contains information about
      the relationship between arcs and polygons, not the main features information. It's necessary in
      MiraMon but not read directly by the GDAL MiraMon driver because
      it is redundant to the information on the linestring part.

    - *FileNameA.rel* file (note the 'A' before the '.'): Contains additional data about the data,
      the relations of the database and the symbolization description. It's necessary in
      MiraMon but not read directly by the GDAL MiraMon driver.

    - *FileName.nod* file: Contains the graphic database with the coordinates that define the
      vector features. It is necessary in the MiraMon vector format but not read by
      the GDAL MiraMon driver because nodes contain topological information that is not
      transferred to other formats.  

    - *FileNameN.dbf* file (note the 'N' before the '.'): Contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if necessary. This table contains information about
      the relationships between arcs and nodes, and other attributes of the nodes, if needed.
      It is necessary in the MiraMon vector format but not read by the GDAL MiraMon driver because
      nodes contain topological information that is not transferred to other formats.

    - *FileNameN.rel* file (note the 'N' before the '.'): Contains additional data about the data,
      the relations of the database and the symbolization description. It's necessary in
      MiraMon but not read directly by the GDAL MiraMon driver.

In MiraMon the concepts of multipoints and multistrings are not supported but the driver translates a
multipoint into N points and a multistring into N arcs. The concept of multipolygon is translated as
polypolygon (described above).

Note that when reading a MiraMon file of type *.pol*, the corresponding
layer will be reported as of type wkbPolygon, but depending on the
number of parts of each geometry, the actual type of the geometry for
each feature can be either OGRPolygon or OGRMultiPolygon. This does not 
apply for ARC and PNT MiraMon files because the concept of 
OGRMultiLineString or OGRMultiPoint does not exist.

The reading driver verifies if multipart polygons adhere to the 
specification (that is to say, the vertices of outer rings should be
oriented clockwise on the X/Y plane, and those of inner rings
counterclockwise) otherwise, the driver corrects the orientation.

Measures (M coordinate) are not supported.
Symbolization is neither read nor generated by this driver.

A `look-up-table of MiraMon <https://www.miramon.cat/help/eng/mm32/AP6.htm>`__ and
`EPSG <https://epsg.org/home.html>`__ Spatial Reference Systems allows matching
identifiers in both systems.

If a layer contains an old *.rel* format file (used some decades ago),
a warning message will appear explaining how to convert it into a modern *.rel 4* file.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Encoding
--------

When writing, the codepage of *.dbf* files can be ANSI or UTF8
depending on the creation option DBFEncoding.

Creation Issues
---------------

MiraMon can only store one kind of geometry per layer
(points, arcs or polygons). Mixing different kinds of layers
(including raster and geoservices as WMS or WMTS) is possible through MiraMon maps (.mmm).
During creation, the driver generates the necessary files to
accommodate each of the three possible types of geometries.
For instance, if a layer or a dataset contains points and arcs,
a set of point files and a set of arc files will be created.

Consequently, during creation the MiraMon driver output can be a
folder or a file with the appropriate extension (*.pnt*, etc):

- If the output is a **folder** it will contain all the translated layers with the original name in the origin dataset.

  - In this case a *.mmm* file will be created referencing all layers in the origin dataset to make an
    easy open of the dataset using the MiraMon software.
  - In this case, please specify the MiraMon file output format name using the -f option (**-f MiraMon**).

- If it the output is a **file** with extension all the translated layers in the origin dataset will be created with the specified name.
  Use this option only when you know that there is only one layer in the origin dataset.

When translating from a MiraMon format, the MiraMon driver input needs a file with one of the
described extensions: *.pnt*, *.arc* or *.pol*. The extension *.nod* is not valid for translation.

The attributes of the MiraMon feature are stored in an associated *.dbf*.
If a classical DBF IV table could not be used (too many fields or records,
large text fields, etc) a file type called extended DBF is used.
This is an improvement of dBASE IV DBF files. The specification
of this format can be found in this file.

The specification of this format can be found in `this file
<https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__.

Note that extended *.dbf* files cannot be opened with Excel or
other typical programs. If the complete MiraMon Professional software
is not installed on the computer, the free and standalone
MiraD application can be downloaded from
`this page <https://www.miramon.cat/USA/Prod-MiraD.htm>`__ to open them.

Field sizes
-----------

The driver knows to auto-extend string and integer fields to
dynamically accommodate for the length of the data to be inserted.

Size Issues
-----------

Geometry: The MiraMon vector format explicitly uses 32-bit offsets in the 1.1 version
and 64-bit offsets in the 2.0 version. It is better to produce 1.1 version files if 2.0
version is not really necessary than always use 2.0 version. Version 1.x files are smaller.

Attributes: The dbf format does not have any offsets in it, so it can be
arbitrarily large.

Open options
------------

The following open options are available.

-  .. oo:: Height
      :choices: First, Lowest, Highest

      Sets which of the possible heights for each vertex is read: 
      the *first*, the *lowest* or the *highest* one. It only applies to
      MiraMon multi-height layers, where the same X,Y vertex can have more than one Z.

-  .. oo:: iMultiRecord
      :choices: 1, 2, ..., Last, JSON

      In case of fields of type List, if the output driver can not support them,
      user can select which one wants to keep: *iMultiRecord=1* for first, *iMultiRecord=2* for second, etc
      and *iMultiRecord=last* for the last element of the list.
      *iMultiRecord=JSON* option converts the list in a single value in JSON format.
      If not specified, all elements of the list will be translated by default.

-  .. oo:: MemoryRatio
      :choices: 0.5, 1, 2, ...
      :default: 1

      It is a ratio used to enhance certain aspects of memory.
      In some memory allocations a block of either 256 or 512 bytes is used.
      This parameter can be adjusted to achieve
      nMemoryRatio*256 or nMemoryRatio*512.
      By way of example, please use nMemoryRatio=2 in powerful computers and
      nMemoryRatio=0.5 in less powerful computers.
      By increasing this parameter, more memory will be required,
      but there will be fewer read/write operations to the (network and) disk.


Dataset creation options
------------------------

None

Layer creation options
----------------------

-  .. lco:: Version
      :choices: V1.1, V2.0, last_version
      :default: V1.1
      :since: 3.9

      Version of the file.
      Version 1.1 is limited to an unsigned 32-bit integer for FID, for internal
      offsets and for the number of entities the layer can handle. 
      It's the default option.
      Version 2.0 is the 64-bit version. It is practically unlimited
      (unsigned 64-bit integer for FID and internal offsets).      
      last_version selects to the last existing version ever.

-  .. lco:: DBFEncoding
      :choices: UTF8, ANSI
      :default: ANSI
      :since: 3.9

      Encoding of the *.dbf* files.
      The MiraMon driver can write *.dbf* files in UTF-8 or ANSI charsets.

      As at the moment of this release UTF-8 tables are not editable in the
      `MiraD application <https://www.miramon.cat/USA/Prod-MiraD.htm>`__, it is recommended
      to use ANSI instead, if there are no coding problems.

Examples
--------

-  A translation from an *Example.dxf* file with one layer but some different geometric types in the layer,
   will result 'file1.dxf' into into a new MiraMon set of layers in the 'output_folder'.

   ::

      ogr2ogr output_folder Example.dxf -f MiraMonVector -lco Version=V1.1


-  A translation from a *Example2.dxf* file with one polygon type layer 'file1.dxf' into a new MiraMon layer
   'territories.pol' (with UTF-8 encoding at the *.dbf* files) is performed like this:

   ::

      ogr2ogr territories.pol Example2.dxf -lco DBFEncoding=UTF8 (no needed to include **-f MiraMonVector** because the output layer is not a directory)


-  A translation from an arc's MiraMon layer 'rivers.arc' into a new *.gml* file (taking only the first element of
   the multirecords in the attributes table) is performed like this:

   ::

      ogr2ogr rivers.gml rivers.arc -oo iMultiRecord=1

-  A translation from a MiraMon layer 'tracks.arc' into a new *.gml* file taking the first height of
   every point is performed like this:

   ::

      ogr2ogr tracks.gml tracks.arc -oo Height=First


See Also
--------

-  `MiraMon page <https://www.miramon.cat/Index_usa.htm>`__
-  `MiraMon's vector format specifications <https://www.miramon.cat/new_note/usa/notes/FormatFitxersTopologicsMiraMon.pdf>`__
-  `MiraMon Extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
-  `MiraMon vector layer concepts <https://www.miramon.cat/help/eng/mm32/ap2.htm#structured_vector>`__.
-  `MiraMon help guide <https://www.miramon.cat/help/eng>`__
-  `Grumets research group, the people behind MiraMon <https://www.grumets.cat/index_eng.htm>`__
