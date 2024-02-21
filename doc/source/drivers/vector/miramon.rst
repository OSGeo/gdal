.. _vector.miramon:

MiraMon
====================

.. shortname:: MiraMon

All varieties of MiraMon structured vector files should be available for reading and creation.

More information about the structured Miramon vector format is available `on the public
specifications <https://www.miramon.cat/new_note/usa/notes/FormatFitxersTopologicsMiraMon.pdf>`__.

It's important to keep in mind that a MiraMon layer is composed by several files as follows:

- **Point layer**: these layers contain point type entities which are described by a
  single coordinate (x,y) or (x, y, z). They are composed by 3 files:

    - *.pnt* file: contains the graphic database with the coordinates that define the
      point vectorial entities.

    - *T.dbf* file (note the 'T' before the '.'): contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if needed. Contains the information of the feature attributes. The feature id (FID) field is
      a field called *ID_GRAFIC*.

    - *T.rel* file (note the 'T' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). In GDAL environment
      only some aspects are documented: the spatial reference system, the language of the metadata (english),
      the extension and a description of the fields.

- **Arc layer**: these layers contain arc type entities which are lines described by a series of segments,
  each one defined by coordinates (x, y) or (x, y, z). They are called *linestring* in GDAL terminology. 
  Each coordinate is called a vertex, and the line, which is always straight, that joins
  every pair of vertices is called a segment. Both extreme vertices of each arc are called nodes.
  Each arc has to contain a minimum of 2 vertices (and in this case, they have to be different
  and form a single segment). They are composed by 6 files:

    - *.arc* file: contains the graphic database with the coordinates that define the
      linestring (arc) vectorial entities.

    - *A.dbf* file (note the 'A' before the '.'): contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if needed. Contains the information of the feature attributes. The feature id (FID) field is
      a field called *ID_GRAFIC*.

    - *A.rel* file (note the 'A' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). In GDAL environment
      only some aspects are documented: the spatial reference system, the language of the metadata (english),
      the extension and a description of the fields.

    - *.nod* file: contains the graphic database with the coordinates that define the
      node vectorial entities. It's needed in MiraMon but not read directly for the GDAL MiraMon driver because
      it's redundant to the information on the linestring part.

    - *N.dbf* file (note the 'N' before the '.'): contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if needed. This table contains information about
      the relationship between arcs and nodes, not the main features information. It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver because
      it's redundant to the information on the linestring part.

    - *N.rel* file (note the 'N' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver.

- **Polygon layer**: these layers contain polygon or polypolygon type entities, which are closed shapes described by one or more arcs.
  Sometimes a polygon has holes inside it; in this case, it is termed a polypolygon and all the polygons that
  form it (the outside polygon and each of the polygons that outline the inside holes) are termed elemental
  polygon (or "ring" or "outline"). They are composed by 9 files:

    - *.pol* file: contains the graphic database with the coordinates that define the
      polygonal (or multipolygonal) vectorial entities. In fact, this file contains the list of arcs
      that compose every polygon (or polypolygon). 

    - *P.dbf* file (note the 'P' before the '.'):contains the main table of the database
      in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__
      if needed. Contains the information of the feature attributes. The feature id (FID) field is
      a field called *ID_GRAFIC*.

    - *P.rel* file (note the 'P' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). In GDAL environment
      only some aspects are documented: the spatial reference system, the language of the metadata (english),
      the extension and a description of the fields.

    - *.arc* file: contains the graphic database with the coordinates that define the
      arc vectorial entities. The polygons in the polygon file reference the arcs in this file by their index.

    - *A.dbf* file (note the 'A' before the '.'): contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if needed. This table contains information about
      the relationship between arcs and polygons, not the main features information. It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver because
      it's redundant to the information on the linestring part.

    - *A.rel* file (note the 'A' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver.

    - *.nod* file: contains the graphic database with the coordinates that define the
      vectorial entities.

    - *N.dbf* file (note the 'N' before the '.'): contains the main table of the database
      in dBASE (DBF) format, or in extended DBF if needed. This table contains information about
      the relationship between arcs and nodes, not the main features information. It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver because
      it's redundant to the information on the linestring part.

    - *N.rel* file (note the 'N' before the '.'): contains additional data about the data,
      the relations of the database and the visualization description (symbolization). It's needed in
      MiraMon but not read directly for the GDAL MiraMon driver.

In MiraMon the concepts of multipoints and multistrings are not supported but the driver translates a
multipoint into N points and a multistring into N arcs. The concept of multipolygon is translated as
polypolygon (described above).

For more informatation about the MiraMon vector layers concept visit
`this appendice MiraMon help <https://www.miramon.cat/help/eng/mm32/ap2.htm#structured_vector>`__.

Note that when reading a MiraMon file of type *.pol*, the corresponding
layer will be reported as of type wkbPolygon, but depending on the
number of parts of each geometry, the actual type of the geometry for
each feature can be either OGRPolygon or OGRMultiPolygon. This not 
applies for ARC and PNT MiraMon files because the concept of 
OGRMultiLineString or OGRMultiPoint doesn't exist.

The reading driver verifies if multipart polygons adhere to the 
specification (that is to say the vertices of outer rings should be
oriented clockwise on the X/Y plane, and those of inner rings
counterclockwise) and if not, the driver corrects the orientation.

Measures (M coordinate) are not supported.
Symbolization is neither read nor generated by this driver.
Only `REL 4 <https://www.miramon.cat/help/eng/GeMPlus/REL1_4.htm>`__ format
is read or generated.

If a *.REL* (in REL 4 format and not in any other) MiraMon metadata file is
present, it will be read and used to associate a projection with
features. A match will be attempted with the
EPSG databases to identify the SRS of the .prj with an entry in the
catalog. If a *.REL* (REL 1 format) or other old MiraMon metadata file
is present, a warning message will appear explaining how to convert it 
in a REL 4 file (using a MiraMon Support Application
`MSA <https://www.miramon.cat/help/eng/mm32/ap7.htm>`__ called
`ConvRel <https://www.miramon.cat/help/eng/msa/convrel.htm>`__).

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Encoding
--------

An attempt is made to read the code page setting in the codepage 
setting from the *.dbf* file, and use it
to translate string fields to UTF-8 on read, and back when writing. 
At the moment of the first version of the driver the codepage of *.dbf* files is ANSI 
which may not be appropriate. The
:config:`DBF_ENCODING` configuration option may be used to
override the encoding interpretation of the *.dbf* file with any encoding
supported by CPLRecode or to "" to avoid any recoding.

Creation Issues
---------------

MiraMon can only store one kind of geometry per layer
(points, arcs or polygons). During creation, the driver generates
the necessary files to accommodate each of the three possible types of geometries.
For instance, if a layer or a dataset contains points and arcs,
a set of point files and a set of arc files will be created.

When creating the MiraMon driver output can be a whole
folder or a file with extension (*.pnt*, *.arc* or *.pol*):

- If it's a **folder** this will contain all the translated layers with the original name in the origin dataset.

  - In this case a *.mmm* file will be created referencing all layers in the origin dataset to make easy open it with MiraMon software.
  - In this case the **-f MiraMon** file output file format name has to be specified.

- If it's a **file** with extension all the translated layers in the origin dataset will be created with the specified name.
  Use this option only when you know that there is only one layer in the origin dataset.

When translating from a MiraMon format the MiraMon driver input needs to be a file with one of the
described extensions: *.pnt*, *.arc* or *.pol*. The extension *.nod* is not valid for translation.

MiraMon feature attributes are stored in an associated *.dbf* file called
extended MiraMon *.dbf* file. This is an improvement of DBASEIV dbf files.
The specification of this format can be found in `this file
<https://www.miramon.cat/new_note/usa/notes/DBF_estesa.pdf>`__.

This extended *.dbf* files cannot be opened with Excel or other typical programs.
Only the free MiraD program can open them. It can be downloaded from `the page <https://www.miramon.cat/USA/Prod-MiraD.htm>`__.

Field sizes
-----------

The driver knows to auto-extend string and integer fields to
dynamically accommodate for the length of the data to be inserted.

Size Issues
-----------

Geometry: The MiraMon format explicitly uses 32bit offsets in the 1.0 version
and 64bit offsets in the 2.0 version. It's better to produce V1.0 version files if 2.0
version is not really needed than always use 2.0 version. A shorter file will be created.

Attributes: The dbf format does not have any offsets in it, so it can be
arbitrarily large.

Open options
------------

The following open options are available.

-  .. oo:: Height
      :choices: First, Lower, Highest

      Sets which of the possible heights for each point is read: 
      the *first*, the *highest* or the *lowest* one. It only applies to
      MiraMon multi-height layers.

-  .. oo:: iMultiRecord
      :choices: 1, 2, ..., Last, JSON

      In case of type List fields type, if output driver can not support them,
      user can select which one wants to keep: *Height=1* for first, *Height=2* for second, etc
      and *Height=last* for the last element of the list.
      *Height=JSON* option converts the list in a single value in JSON format.
      If not specified all elements of the list will be translated.

-  .. oo:: MemoryRatio
      :choices: 0.5, 1, 2, ...
      :default: 1

      It is a ratio used to enhance certain aspects of memory.
      In some memory allocations a block of 256 or 512 bytes is used.
      This parameter can be adjusted to achieve
      nMemoryRatio*256 or nMemoryRatio*512.
      For example, nMemoryRatio=2 in powerful computers and
      nMemoryRatio=0.5 in less powerful computers.
      By increasing this parameter, more memory will be required,
      but there will be fewer read/write operations to the disk.


Dataset creation options
------------------------

None

Layer creation options
----------------------

-  .. lco:: Version
      :choices: V11, V20, last_version
      :default: V11
      :since: 3.9

      Version of the file.
      V11 is a limited 32 bits for FID and internal offsets and the number
      of features the layer can handle. It's the default option.
      V20 is the 64 bits version for FID and internal offsets.
      last_version forces to the last existing version ever.

-  .. lco:: DBFEncoding
      :choices: UTF8, ANSI
      :default: ANSI
      :since: 3.9

      Encoding of the *.dbf* files.
      MiraMon can write *.dbf* files in these two charsets.
      At the moment of this release the UTF8 is not editable
      in `MiraD application <https://www.miramon.cat/USA/Prod-MiraD.htm>`__.
      Use ANSI instead if there are no codification problems.

Examples
--------

-  A translation from a *.dxf* file with one layer but some diferent types in the layer 'file1.dxf' into a new MiraMon bunch of layers
   'output_folder' in Version 2.0 is performed like this:

   ::

      ogr2ogr output_folder file1.dxf -f MiraMon -lco Version=V11


-  A translation from a *.dxf* file with one polygons type layer 'file1.dxf' into a new MiraMon of layer
   'territories.pol' (with UTF-8 encoding at the *.dbf* files) is performed like this:

   ::

      ogr2ogr territories.pol file1.dxf -lco DBFEncoding=UTF8 (you do not need to add **-f MiraMon**)


-  A translation from a arcs MiraMon layer 'rivers.arc' into a new gml file (taking only the first element of
   multirecords) is performed like this:

   ::

      ogr2ogr rivers.gml rivers.arc -oo iMultiRecord=1

-  A translation from a arcs MiraMon layer 'tracks.arc' into a new gml file taking the first height of
   every point is performed like this:

   ::

      ogr2ogr tracks.gml tracks.arc -oo Height=First


See Also
--------

-  `Miramon page <https://www.miramon.cat/Index_usa.htm>`__
-  `CREAF page <https://www.creaf.cat/>`__
-  `Miramon help guide <https://www.miramon.cat/help/eng>`__
-  `Miramon video tutorials <https://www.miramon.cat/USA/Videos.htm>`__
-  `Miramon users forum <https://www.miramon.cat/Fum/viewforum.php?f=16>`__
-  `Miramon technical notes <https://www.miramon.cat/USA/NotesTecniques.htm>`__
-  `Research group strongly linked to Miramon <https://www.grumets.cat/index_eng.htm>`__
