How GDAL overviews are modeled in TileDB
----------------------------------------

Overviews can only be created on a dataset with the default CREATE_GROUP=YES
creation option.

The dataset name passed to Create() or CreateCopy() is used
to create a TileDB group, with the full resolution dataset being created
as a TileDB array whose name is ``l_0``

The first overview level created is a TileDB array, placed within that group,
and whose name is ``l_1``. And so on.

.. code-block:: bash

    $ ls -al byte_group.tiledb

    drwxr-xr-x  6 even even  4096 mai    2 02:13 ./
    drwxrwxr-x 57 even even 86016 mai    2 01:42 ../
    drwxr-xr-x  8 even even  4096 mai    1 17:17 l_0/      <== Full resolution dataset
    drwxr-xr-x  8 even even  4096 mai    2 02:13 l_1/      <== First overview level
    drwxr-xr-x  8 even even  4096 mai    2 02:13 l_2/      <== Second overview level
    drwxr-xr-x  2 even even  4096 mai    2 02:13 __group/
    drwxr-xr-x  2 even even  4096 mai    2 02:13 __meta/
    -rw-r--r--  1 even even     0 mai    1 17:17 __tiledb_group.tdb


The group has the following TileDB metadata items:

* ``dataset_type``: set to ``raster``

* ``_gdal``: XML serialized string whose root element is ``PAMDataset``.
  This XML string is the same as the one stored in the full resolution TileDB
  array under the ``_gdal`` TileDB metadata item.
  In addition to traditional GDAL PAM elements, the additional ``tiledb:OverviewCount``
  element is added with the number of overviews (omitted if there are none)

For example:

.. code-block:: xml

    <PAMDataset>
      <SRS dataAxisToSRSAxisMapping="1,2">PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.978698213898,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</SRS>
      <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
      <Metadata>
        <MDI key="AREA_OR_POINT">Area</MDI>
      </Metadata>
      <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="DATASET_TYPE">raster</MDI>
        <MDI key="DATA_TYPE">Byte</MDI>
        <MDI key="INTERLEAVE">BAND</MDI>
        <MDI key="NBITS">8</MDI>
        <MDI key="X_SIZE">20</MDI>
        <MDI key="Y_SIZE">20</MDI>
      </Metadata>
      <tiledb:OverviewCount>1</tiledb:OverviewCount>
    </PAMDataset>
