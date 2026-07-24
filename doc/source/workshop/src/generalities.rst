.. _generalities:

================================================================================
General principles of the new GDAL CLI
================================================================================

Why a new CLI ?
---------------

Webinar given on June 3, 2025 about GDAL Command Line Interface Modernization:

- `PDF slide deck <https://download.osgeo.org/gdal/presentations/GDAL%20CLI%20Modernization.pdf>`__
- `recording of the video <https://www.youtube.com/watch?v=ZKdrYm3TiBU>`__.

Reasons:

- Program naming inconsistencies:
    * ``gdal_translate`` vs ``gdalwarp``
    * ``gdal_merge`` vs ``ogrmerge``

- Inconsistent argument naming:
    * ``gdal_translate -projwin ulx uly lrx lry``
    * ``gdalwarp -te llx lly urx ury``

- Dataset order inconsistency:
    * ``gdal_translate in.tif out.tif``
    * ``gdalwarp out.tif in.tif``

- Some utilities silently overwriting output file (``gdal_translate``), while
  others silently update it (``gdalwarp``).

- "Jumbo" programs with tens of arguments like ``gdal_translate``, ``gdalwarp`` or ``ogr2ogr``.

Principles of new CLI
---------------------

``git`` style hierarchical commands
+++++++++++++++++++++++++++++++++++

    ::

        gdal
        |
        +-- raster              For all commands that accept a raster input (some may output vector)
            |
            +-- info
            +-- convert
            +-- reproject
            +-- calc
            +-- clip
            +-- contour         (outputs vector)
            +-- overview
            +-- mosaic
            +-- tile
            +-- index
            +-- ...
        |
        +-- vector              For all commands that accept a vector input (some may output raster)
            |
            +-- info
            +-- convert
            +-- reproject
            +-- clip
            +-- rasterize       (outputs raster)
            +-- mosaic
            +-- index
            +-- ...
        +-- mdim
            |
            +-- info
            +-- convert
            +-- mosaic
        |
        +-- pipeline
        |
        +-- dataset
            |
            +-- check
            +-- copy
            +-- delete
            +-- calc
            +-- identify
            +-- rename
        |
        +-- driver
            |
            +-- cog
            +-- gpkg
            +-- gti
            +-- openfilegdb
            +-- parquet
            +-- pdf
            +-- rpftoc
        |
        +-- vsi
            |
            +-- copy
            +-- delete
            +-- list
            +-- move
            +-- sozip
            +-- sync 
        |
        +-- info
        |
        +-- convert


Smaller scope programs
++++++++++++++++++++++

* ``gdal_translate`` ==>

  - :ref:`gdal raster convert <gdal_raster_convert>`
  - :ref:`gdal raster clip <gdal_raster_clip>`
  - :ref:`gdal raster edit <gdal_raster_edit>`

* ``gdalwarp`` ==>

  - :ref:`gdal raster reproject <gdal_raster_reproject>`
  - :ref:`gdal raster update <gdal_raster_update>`

* ``ogr2ogr`` ==>

  - :ref:`gdal vector convert <gdal_vector_convert>`
  - :ref:`gdal vector clip <gdal_vector_clip>`
  - :ref:`gdal vector edit <gdal_vector_edit>`
  - :ref:`gdal vector reproject <gdal_vector_reproject>`


Program syntax
++++++++++++++

* Positional arguments: 90% of positional arguments are for input dataset(s) and output dataset. Always in that order

  ::

      $ gdal raster convert input.png output.tif


* Positional arguments can also be specified as named arguments:

  - Long version

      ::

          $ gdal raster convert --input input.png --output output.tif


  - Short version

      ::

          $ gdal raster convert -i input.png -o output.tif


* Setting the value of a named argument:

  - space character separator as above:

  - or equal character separator:

      ::

          $ gdal raster convert --input=input.png --output=output.tif


* No more silent overwriting

  - ``--overwrite`` will be required and suggested if needed

      ::

          $ gdal raster convert in.png out.tif
          $ gdal raster convert in2.png out.tif
          ERROR 1: convert: Dataset 'out.tif' already exists. You may specify the --overwrite/--append option.

      .. warning::

          Overwrite destroys and recreates the whole dataset.

          Some programs with vector output can offer a more limited scope using ``--overwrite-layer``.

  - ``--update`` or ``--append`` may also be available


I need help !!!
+++++++++++++++

::

  $ gdal raster convert --help

::

    Usage: gdal raster convert [OPTIONS] <INPUT> <OUTPUT>

    Convert a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster datasets [required] [not available in pipelines]
      -o, --output <OUTPUT>                                Output raster dataset [required] [not available in pipelines]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      -q, --quiet                                          Quiet mode (no progress bar or warning message) [not available in pipelines]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format ("GDALG" allowed) [not available in pipelines]
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated] [not available in pipelines]
      --overwrite                                          Whether overwriting existing output dataset is allowed [not available in pipelines]
                                                           Mutually exclusive with --append
      --append                                             Append as a subdataset to existing output [not available in pipelines]
                                                           Mutually exclusive with --overwrite

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated] [not available in pipelines]
      --oo, --open-option <KEY>=<VALUE>                    Open options [may be repeated] [not available in pipelines]

    For more details, consult :ref:`gdal_raster_convert <gdal_raster_convert>`.

    WARNING: the gdal command is provisionally provided as an alternative interface to GDAL and OGR command line utilities.
    The project reserves the right to modify, rename, reorganize, and change the behavior of the utility
    until it is officially frozen in a future feature release of GDAL.


Smart auto-completion
+++++++++++++++++++++

Provided you use a Bash-compatible shell, suggestion of program name, options
and arguments are available by pressing <TAB> <TAB>

Discover sub-programs
*********************

::

  $ gdal raster <TAB><TAB>

::

    as-features      clip             create           index            overview         proximity        roughness        slope            unscale          
    aspect           color-map        edit             info             pansharpen       reclassify       scale            stack            update           
    blend            compare          fill-nodata      mosaic           pipeline         reproject        select           tile             viewshed         
    calc             contour          footprint        neighbors        pixel-info       resize           set-type         tpi              zonal-stats      
    clean-collar     convert          hillshade        nodata-to-alpha  polygonize       rgb-to-palette   sieve            tri              

Discover named arguments
************************

::

  $ gdal raster convert --<TAB><TAB>

::

    --append           --creation-option  --input            --input-format     --open-option      --output           --output-format    --overwrite        --quiet

Discover values of arguments
****************************

Which output formats are available?

::

    gdal raster convert --format=<TAB><TAB>

::

    GTiff            PNG              PCRaster         ISIS3            WMS              PDF              LCP              S102             HF2              NGW
    COG              JPEG             ILWIS            PDS4             RST              MBTiles          GTX              S104             ZMap             MiraMonRaster
    VRT              MEM              SRTMHGT          VICAR            GSAG             CALS             KRO              S111             SIGDEM           ENVI
    NITF             GIF              Leveller         ERS              GSBG             WMTS             ROI_PAC          NWT_GRD          JPEGXL           EHdr
    HFA              FITS             Terragen         JP2OpenJPEG      GS7BG            MRF              RRASTER          PostGISRaster    TileDB           ISCE
    AAIGrid          BMP              netCDF           GRIB             KMLSUPEROVERLAY  PNM              KEA              SAGA             GPKG             Zarr
    DTED             PCIDSK           HDF4Image        RMF              WEBP             BT               BAG              XYZ              OpenFileGDB      GDALG


Discover contextual values of arguments
***************************************

Which creation options are available for COG output?

::

    gdal raster convert --format=COG --creation-option <TAB><TAB>

::

    COMPRESS=              QUALITY=               JXL_EFFORT=            BLOCKSIZE=             WARP_RESAMPLING=       ZOOM_LEVEL_STRATEGY=   ADD_ALPHA=
    OVERVIEW_COMPRESS=     OVERVIEW_QUALITY=      JXL_DISTANCE=          INTERLEAVE=            OVERVIEWS=             TARGET_SRS=            GEOTIFF_VERSION=
    LEVEL=                 MAX_Z_ERROR=           JXL_ALPHA_DISTANCE=    BIGTIFF=               OVERVIEW_COUNT=        RES=                   SPARSE_OK=
    PREDICTOR=             MAX_Z_ERROR_OVERVIEW=  NUM_THREADS=           RESAMPLING=            TILING_SCHEME=         EXTENT=                STATISTICS=
    OVERVIEW_PREDICTOR=    JXL_LOSSLESS=          NBITS=                 OVERVIEW_RESAMPLING=   ZOOM_LEVEL=            ALIGNED_LEVELS=        

    

Which compression methods are available for COG output?

::

    gdal raster convert --format=COG --creation-option COMPRESS=<TAB><TAB>

::

    NONE          LZW           JPEG          DEFLATE       LZMA          ZSTD          WEBP          LERC          LERC_DEFLATE  LERC_ZSTD     JXL           
