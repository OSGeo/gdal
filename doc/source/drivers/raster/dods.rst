.. _raster.dods:

================================================================================
DODS -- OPeNDAP Grid Client
================================================================================

.. shortname:: DODS

.. build_dependencies:: libdap

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_DODS

GDAL optionally includes read support for 2D grids and arrays via the
OPeNDAP (DODS) protocol.

Driver capabilities
-------------------

.. supports_georeferencing::

Dataset Naming
--------------

The full dataset name specification consists of the OPeNDAP dataset url,
the full path to the desired array or grid variable, and an indicator of
the array indices to be accessed.

For instance, if the url
http://maps.gdal.org/daac-bin/nph-hdf/3B42.HDF.dds returns a DDS
definition like this:

::

   Dataset {
     Structure {
       Structure {
         Float64 precipitate[scan = 5][longitude = 360][latitude = 80];
         Float64 relError[scan = 5][longitude = 360][latitude = 80];
       } PlanetaryGrid;
     } DATA_GRANULE;
   } 3B42.HDF;

then the precipitate grid can be accessed using the following GDAL
dataset name:

http://maps.gdal.org/daac-bin/nph-hdf/3B42.HDF?DATA_GRANULE.PlanetaryGrid.precipitate[0][x][y]

The full path to the grid or array to be accessed needs to be specified
(not counting the outer Dataset name). GDAL needs to know which indices
of the array to treat as x (longitude or easting) and y (latitude or
northing). Any other dimensions need to be restricted to a single value.

In cases of data servers with only 2D arrays and grids as immediate
children of the Dataset it may not be necessary to name the grid or
array variable.

In cases where there are a number of 2D arrays or grids at the dataset
level, they may be each automatically treated as separate bands.

Specialized AIS/DAS Metadata
----------------------------

A variety of information will be transported via the DAS describing the
dataset. Some DODS drivers (such as the GDAL based one!) already return
the following DAS information, but in other cases it can be supplied
locally using the AIX mechanism. See the DODS documentation for details
of how the AIS mechanism works.

::

   Attributes {

       GLOBAL {
           Float64 Northernmost_Northing 71.1722;
       Float64 Southernmost_Northing  4.8278;
       Float64 Easternmost_Easting  -27.8897;
       Float64 Westernmost_Easting -112.11;
           Float64 GeoTransform "71.1722 0.001 0.0 -112.11 0.0 -0.001";
       String spatial_ref "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
           Metadata {
             String TIFFTAG_XRESOLUTION "400";
             String TIFFTAG_YRESOLUTION "400";
             String TIFFTAG_RESOLUTIONUNIT "2 (pixels/inch)";
           }
       }

       band_1 {
           String Description "...";
           String
       }
   }

Dataset
~~~~~~~

There will be an object in the DAS named GLOBAL containing attributes of
the dataset as a whole.

It will have the following subitems:

-  **Northernmost_Northing**: The latitude or northing of the north edge
   of the image.
-  **Southernmost_Northing**: The latitude or northing of the south edge
   of the image.
-  **Easternmost_Easting**: The longitude or easting of the east edge of
   the image.
-  **Westernmost_Easting**: The longitude or easting of the west edge of
   the image.
-  **GeoTransform**: The six parameters defining the affine
   transformation between pixel/line space and georeferenced space if
   applicable. Stored as a single string with values separated by
   spaces. Note this allows for rotated or sheared images. (optional)
-  **SpatialRef**: The OpenGIS WKT description of the coordinate system.
   If not provided it will be assumed that the coordinate system is
   WGS84. (optional)
-  **Metadata**: a container with a list of string attributes for each
   available metadata item. The metadata item keyword name will be used
   as the attribute name. Metadata values will always be strings.
   (optional)
-  *address GCPs*

Note that the edge northing and easting values can be computed based on
the grid size and the geotransform. They are included primarily as extra
documentation that is easier to interpret by a user than the
GeoTransform. They will also be used to compute a GeoTransform
internally if one is note provided, but if both are provided the
GeoTransform will take precedence.

Band
~~~~

There will be an object in the DAS named after each band containing
attribute of the specific band.

It will have the following subitems:

-  **Metadata**: a container with a list of string attributes for each
   available metadata item. The metadata item keyword name will be used
   as the attribute name. Metadata values will always be strings.
   (optional)
-  **PhotometricInterpretation**: Will have a string value that is one
   of "Undefined", "GrayIndex", "PaletteIndex", "Red", "Green", "Blue",
   "Alpha", "Hue", "Saturation", "Lightness", "Cyan", "Magenta",
   "Yellow" or "Black". (optional)
-  **units**: name of units (one of "ft" or "m" for elevation data).
   (optional)
-  **add_offset**: Offset to be applied to pixel values (after
   scale_factor) to compute a "real" pixel value. Defaults to 0.0.
   (optional)
-  **scale_factor**: Scale to be applied to pixel values (before
   add_offset) to compute "real" pixel value. Defaults to 1.0.
   (optional)
-  **Description**: Descriptive text about the band. (optional)
-  **missing_value**: The nodata value for the raster. (optional)
-  **Colormap**: A container with a subcontainer for each color in the
   color table, looking like the following. The alpha component is
   optional and assumed to be 255 (opaque) if not provided.

   ::

          Colormap {
            Color_0 {
              Byte red 0;
              Byte green 0;
              Byte blue 0;
              Byte alpha 255;
            }
            Color_1 {
              Byte red 255;
              Byte green 255;
              Byte blue 255;
              Byte alpha 255;
            }
            ...
          }

See Also
--------

-  `OPeNDAP Website <http://www.opendap.org/>`__
