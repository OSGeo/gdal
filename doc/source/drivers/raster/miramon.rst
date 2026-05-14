.. _raster.miramon:

MiraMon Raster
==============

.. versionadded:: 3.12

.. shortname:: MiraMonRaster

.. built_in_by_default::

This driver is capable of reading raster files in MiraMon format as well as create a copy to MiraMon format.

A `look-up table of MiraMon <https://www.miramon.cat/help/eng/mm32/AP6.htm>`__ and
`EPSG <https://epsg.org/home.html>`__ Spatial Reference Systems allows matching
identifiers in both systems.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Overview of MiraMon format
--------------------------

The MiraMon `.img` format is a binary raster format with rich metadata stored in a sidecar `I.rel` file.
More information is available in the `public specification <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__.

By specifying either the name of the `I.rel` metadata file or the name of any `.img` band file, the driver will automatically use the associated `I.rel` file.

- **REL file**: This metadata file governs how all bands are interpreted and accessed. It contains metadata including band names, number of rows and columns, data type, compression information (either global or per band) and others. So, a MiraMon dataset can include multiple bands, all linked through a single `.rel` file. Whether the ".img" file or the "I.rel" metadata file is provided, the result will be the same: all bands will be considered. If a layer contains an old *.rel* format file (used in legacy datasets), a warning will be issued explaining how to convert it into the modern *.rel 4* format. Next are the main characteristics of a MiraMon raster dataset band:

- **IMG file**: Stores the raw raster data. The data type may vary:
  - *Bit*: 1 bit. Range: 0 or 1. Converted to byte GDAL type.
  - *Byte*: 1 byte, unsigned. Range: 0 to 255
  - *Short*: 2 bytes, signed. Range: -32 768 to 32 767
  - *UShort*: 2 bytes, unsigned. Range: 0 to 65 535
  - *Int32*: 4 bytes, signed. Range: -2 147 483 648 to 2 147 483 647
  - *Real*: 4 bytes, floating-point
  - *Double*: 8 bytes, double precision floating-point

It's important that the specified filename for the dataset is "I.rel" instead of ".rel". Files ending with ".img" can be confused with the raw data files, and the chosen driver may not be the correct one. If the filename is "I.rel", the driver will automatically be MiraMonRaster. If ".img" is used, the user can also specify the driver with the open option "-if MiraMonRaster" or "-of MiraMonRaster".

Writing behavior
----------------

If the dataset has documented R, G, and B bands, the copy will also generate a map (in `.mmm format <https://www.miramon.cat/help/eng/mm32/ap3.htm>`__) to allow the file to be visualized in MiraMon as an RGB raster (24-bit).

Color Table
-----------

MiraMon raster bands may include a color table in dBASE (DBF) format, or in `extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__, if necessary.
If present, it will be exposed through the ``GetRasterColorTable()`` method.  
More information is available in the `public specification <https://www.miramon.cat/help/eng/mm32/ap4.htm>`__.

Attribute Table
---------------

MiraMon raster bands may also include an attribute table in the same format as color table. 
If available, it will be exposed as a GDAL Raster Attribute Table (RAT) via ``GetDefaultRAT()``.  
This allows applications to retrieve additional metadata associated with each raster value, such as class names  or descriptions.  

Metadata
--------

Metadata from MiraMon raster datasets are exposed through the "MIRAMON" domain.  
Only a subset of the metadata is used by the driver to interpret the dataset (e.g., georeferencing, data type, etc), but all other metadata entries are preserved and accessible via ``GetMetadata("MIRAMON")``.

This allows applications to access additional information embedded in the original dataset, even if it is not required for reading or displaying the data (use of -co SRC_MDD=MIRAMON will copy to destination dataset all this  MIRAMON metadata).

When creating a copy, some MIRAMON metadata items are copied to the comments part of REL file.
Also, when creating a copy, lineage metadata (if existent) is recovered from the source dataset and added to the MiraMon file to know how the file was created. The current process is added after all recovered processes to document that the file was created by GDAL.

Encoding
--------

When reading MiraMon DBF files, the code page setting in the `.dbf` header is used to translate string fields to UTF-8,
regardless of whether the original encoding is ANSI, OEM or UTF-8.

REL files are always encoded in ANSI.

Subdataset Generation
---------------------

The MiraMon format allows datasets to contain bands with heterogeneous characteristics. At present, the only strict restriction imposed by the format itself is that all bands must share the same spatial reference system (although different spatial extents and/or cell sizes are allowed).

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
      CT
            Expose the color table only, without the attribute table.


Creation Options
----------------

The following creation options are supported:

-  .. co:: COMPRESS
      :choices: YES, NO
      :default: YES

      Indicates whether the file will be compressed in RLE indexed mode.

-  .. co:: PATTERN
      
      Indicates the pattern used to create the names of the different bands. In the case of RGB, the suffixes “_R”, “_G”, and “_B” will be added to the base name.

-  .. co:: CATEGORICAL_BANDS
      :choices: <integer>
      
      Indicates which bands (1-based indexing) have to be treated as categorical (e.g., a land cover map). Comma (",") separators can be used to indicate multiple bands.

-  .. co:: CONTINUOUS_BANDS
      :choices: <integer>
      
      Indicates which bands (1-based indexing) have to be treated as continuous (e.g., temperatures map). Comma (",") separators can be used to indicate multiple bands. If a band is not indicated as categorical or continuous, it will be treated following an automatic criterion based on the presence of a color table and/or an attribute table, for instance.

Open examples
-------------
-  A MiraMon dataset with 3 bands that have different spatial extents and/or cell sizes will be exposed as 3 different subdatasets. This allows applications to read each band independently, without the need to resample them to a common grid. You can use ``-sds`` option to translate all datasets into, for instance, different TIFF files:
   .. code-block::

       gdal_translate multiband_input_datasetI.rel output_subdatasets.tiff -sds

   Output: output_subdatasets_1.tiff, output_subdatasets_2.tiff, output_subdatasets_3.tiff

-  A MiraMon dataset color table and RAT can be translated separately, by using the RAT_OR_CT open option:
   .. code-block::

       gdal_translate -oo RAT_OR_CT=RAT datasetI.rel output_only_with_rat.tiff (only RAT will be translated, without the color table)
       gdal_translate -oo RAT_OR_CT=CT datasetI.rel output_only_with_ct.tiff (only color table will be translated, without the attribute table)

-  A MiraMon dataset can be translated preserving it's own metadata, by using the SRC_MDD creation option:
   .. code-block::

       gdal_translate -oo SRC_MDD=MIRAMON datasetI.rel output_with_miramon_metadata.vrt

Creation examples
-----------------

-  A tiff file will be translated as a compressed file. If user does not want this behavior COMPRESS=NO can be specified in the creation options:
   .. code-block::

       gdal_translate -co COMPRESS=NO dataset.tiff output_uncompressed_datasetI.rel

   .. code-block::

       gdal_translate -co PATTERN="band_" input.tiff outputI.rel 

   Output: band_1I.rel, band_2I.rel, band_3I.rel

-  A tiff dataset with 3 bands that wants all bands to be treated as categorical will be translated as follows:
   .. code-block::

       gdal_translate -co Categorical=1,2,3 dataset.tiff outputI.rel

   Output: output_1I.rel, output_2I.rel, output_3I.rel

-  A tiff dataset with 3 bands that wants the first band to be treated as categorical will be translated as follows:
   .. code-block::
       
       gdal_translate -co Categorical=1 dataset.tiff outputI.rel

   Output: output_1I.rel, output_2I.rel, output_3I.rel
   Bands 2 and 3 will be treated with a heuristic criterion, for instance, based on the presence of a color table and/or an attribute table.

-  A tiff dataset with 3 bands that wants the first band to be treated as categorical and the second one as continuous will be translated as follows:
   .. code-block::
      
       gdal_translate -co Categorical=1 -co Continuous=2 dataset.tiff outputI.rel

   Output: output_1I.rel, output_2I.rel, output_3I.rel
   Band 1 will be treated as categorical, band 2 as continuous, and band 3 with a heuristic criterion, for instance, based on the presence of a color table and/or an attribute table.

-  A MiraMon dataset can be translated from a source and retrieving all its metadata, by using the SRC_MDD creation option:
   .. code-block::

       gdal_translate -oo SRC_MDD=MIRAMON input_with_miramon_metadata.vrt MiraMonDatasetI.rel

See Also
--------

-  `MiraMon's raster format specifications <https://www.miramon.cat/new_note/eng/notes/MiraMon_raster_file_format.pdf>`__
-  `MiraMon Extended DBF format <https://www.miramon.cat/new_note/eng/notes/DBF_estesa.pdf>`__
-  `MiraMon raster layer concepts <https://www.miramon.cat/help/eng/mm32/ap1.htm>`__.
-  `MiraMon main page <https://www.miramon.cat/Index_usa.htm>`__
-  `MiraMon help guide <https://www.miramon.cat/help/eng>`__
-  `Grumets research group, the people behind MiraMon <https://www.grumets.cat/index_eng.htm>`__
