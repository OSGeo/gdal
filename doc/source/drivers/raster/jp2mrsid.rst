.. _raster.jp2mrsid:

================================================================================
JP2MrSID -- JPEG2000 via MrSID SDK
================================================================================

.. shortname:: JP2MrSID

.. build_dependencies:: MrSID SDK

JPEG2000 file format is supported for reading with the MrSID DSDK. It is
also supported for writing with the MrSID ESDK.

JPEG2000 MrSID support is only available with the version 5.x or newer
DSDK and ESDK.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Georeferencing
--------------

Georeferencing information can come from different sources : internal
(GeoJP2 or GMLJP2 boxes), worldfile .j2w/.wld sidecar files, or PAM
(Persistent Auxiliary metadata) .aux.xml sidecar files. By default,
information is fetched in following order (first listed is the most
prioritary): PAM, GeoJP2, GMLJP2, WORLDFILE.

Starting with GDAL 2.2, the allowed sources and their priority order can
be changed with the GDAL_GEOREF_SOURCES configuration option (or
GEOREF_SOURCES open option) whose value is a comma-separated list of the
following keywords : PAM, GEOJP2, GMLJP2, INTERNAL (shortcut for
GEOJP2,GMLJP2), WORLDFILE, NONE. First mentioned sources are the most
prioritary over the next ones. A non mentioned source will be ignored.

For example setting it to "WORLDFILE,PAM,INTERNAL" will make a
geotransformation matrix from a potential worldfile prioritary over PAM
or internal JP2 boxes. Setting it to "PAM,WORLDFILE,GEOJP2" will use the
mentioned sources and ignore GMLJP2 boxes.

Creation Options
----------------

If you have the MrSID ESDK (5.x or newer), it can be used to write
JPEG2000 files. The following creation options are supported.

-  **WORLDFILE=YES**: to write an ESRI world file (with the extension
   .j2w).
-  **COMPRESSION=n**: Indicates the desired compression ratio. Zero
   indicates lossless compression. Twenty would indicate a 20:1
   compression ratio (the image would be compressed to 1/20 its original
   size).
-  **XMLPROFILE=[path to file]**: Indicates a path to an
   Extensis-specific XML profile that can be used to set JPEG2000
   encoding parameters. They can be created using the MrSID ESDK, or
   with GeoExpress, or by hand using the following example as a
   template:

   ::

      <?xml version="1.0"?>
      <Jp2Profile version="1.0">
        <Header>
          <name>Default</name>
          <description>Extensis preferred settings (20051216)</description>
        </Header>
        <Codestream>
          <layers>
            8
          </layers>
          <levels>
            99
          </levels>
          <tileSize>
            0 0
          </tileSize>
          <progressionOrder>
            RPCL
          </progressionOrder>
          <codeblockSize>
            64 64
          </codeblockSize>
          <pltMarkers>
            true
          </pltMarkers>
          <wavelet97>
            false
          </wavelet97>
          <precinctSize>
            256 256
          </precinctSize>
        </Codestream>
      </Jp2Profile>

See Also
--------

-  Implemented as ``gdal/frmts/mrsid/mrsiddataset.cpp``.
-  `Extensis web site <http://www.extensis.com/support/developers>`__
