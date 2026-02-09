.. _raster.miramon:

MiraMon Raster
==============

.. versionadded:: 3.12

.. shortname:: MiraMonRaster

.. built_in_by_default::

This driver is capable of reading raster files in the MiraMon format. Write support is expected soon.

A `look-up table of MiraMon <https://www.miramon.cat/help/eng/mm32/AP6.htm>`__ and
`EPSG <https://epsg.org/home.html>`__ Spatial Reference Systems allows matching
identifiers in both systems.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Overview of MiraMon format
--------------------------

The MiraMon `.img` format is a binary raster format with rich metadata stored in a sidecar `.rel` file.
More information is available in the `public specification <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__.

By specifying either the name of the `.rel` metadata file or the name of any `.img` band file, the driver will automatically use the associated `.rel` file.

- **REL file**: This metadata file governs how all bands are interpreted and accessed. It contains metadata including band names, number of rows and columns, data type, compression information (either global or per band) and others. So, a MiraMon dataset can include multiple bands, all linked through a single `.rel` file. Whether the name of one of the dataset's `.img` files or the `.rel` file is provided, the result will be the same: all bands will be considered. If a layer contains an old *.rel* format file (used in legacy datasets), a warning will be issued explaining how to convert it into the modern *.rel 4* format. Next are the main characteristics of a MiraMon raster dataset band:

- **IMG file**: Stores the raw raster data. The data type may vary:
  - *Bit*: 1 bit. Range: 0 or 1. Converted to byte GDAL type.
  - *Byte*: 1 byte, unsigned. Range: 0 to 255
  - *Short*: 2 bytes, signed. Range: -32 768 to 32 767
  - *UShort*: 2 bytes, unsigned. Range: 0 to 65 535
  - *Int32*: 4 bytes, signed. Range: -2 147 483 648 to 2 147 483 647
  - *UInt32*: 4 bytes, unsigned. Range: 0 to 4 294 967 295
  - *Int64*: 8 bytes, signed integer
  - *UInt64*: 8 bytes, unsigned integer
  - *Real*: 4 bytes, floating-point
  - *Double*: 8 bytes, double precision floating-point

Color Table
-----------

MiraMon raster bands may include a color table in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__, if necessary.
If present, it will be exposed through the ``GetRasterColorTable()`` method.  
More information is available in the `public specification <https://www.miramon.cat/help/eng/mm32/ap4.htm>`__.

Attribute Table
---------------

MiraMon raster bands may also include an attribute table in the same format as color table..  
If available, it will be exposed as a GDAL Raster Attribute Table (RAT) via ``GetDefaultRAT()``.  
This allows applications to retrieve additional metadata associated with each raster value, such as class names  or descriptions.  

Metadata
--------

Metadata from MiraMon raster datasets are exposed through the "MIRAMON" domain.  
Only a subset of the metadata is used by the driver to interpret the dataset (e.g., georeferencing, data type, etc), but all other metadata entries are preserved and accessible via ``GetMetadata("MIRAMON")``.

This allows applications to access additional information embedded in the original dataset, even if it is not required for reading or displaying the data.

Encoding
--------

When reading MiraMon DBF files, the code page setting in the `.dbf` header is used to translate string fields to UTF-8,
regardless of whether the original encoding is ANSI, OEM or UTF-8.

REL files are always encoded in ANSI.

Subdataset Generation
---------------------

The MiraMon format allows datasets to contain bands with heterogeneous characteristics. At present, the only strict restriction imposed by the format itself is that all bands must share the same spatial reference system (although different bounding boxes and/or cell sizes are allowed).

For interoperability with other GDAL formats, the MiraMonRaster driver applies, by default, a compatibility-based criterion when exposing multiband datasets. Bands are grouped into the same GDAL subdataset only when they are fully compatible. When this is not the case, bands are exposed as belonging to different subdatasets.

In practice, bands are separated into different subdatasets when there are differences in raster geometry, including raster dimensions or spatial extent, when the numeric data type differs, when symbolization semantics differ, when associated Raster Attribute Tables differ, or when the presence or value of NoData differs.

Open options
------------

The following open options are available:

-  .. oo:: RAT_OR_CT
      :choices: ALL, RAT, CT
      :default: ALL

      Controls whether the Raster Attribute Table (RAT) and/or the Color Table (CT) are exposed.
      
      ALL
            Expose both the attribute table and the color table. Note that in some software this option may cause visualization and/or legend issues.
      RAT
            Expose the attribute table only, without the color table.
      PER_BAND_ONLY
            Expose the color table only, without the attribute table.

Dataset creation options
------------------------

None.

See Also
--------

-  `MiraMon's raster format specifications <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__
-  `MiraMon Extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__
-  `MiraMon vector layer concepts <https://www.miramon.cat/help/eng/mm32/ap1.htm>`__.
-  `MiraMon page <https://www.miramon.cat/Index_usa.htm>`__
-  `MiraMon help guide <https://www.miramon.cat/help/eng>`__
-  `Grumets research group, the people behind MiraMon <https://www.grumets.cat/index_eng.htm>`__