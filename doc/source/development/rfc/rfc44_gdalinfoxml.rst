.. _rfc-44:

=======================================================================================
RFC 44: Add Parseable Output Formats for ogrinfo and gdalinfo
=======================================================================================

Authors: Dan "Ducky" Little, Faza Mahamood

Contacts: danlittle at yahoo dot com, fazamhd at live dot com

Status: Development. gdalinfo -json implemented in GDAL 2.0

Summary
-------

Add XML and JSON output to the ogrinfo and gdalinfo utilities.

Background
----------

ogrinfo and gdalinfo are incredibly useful metadata gathering tools.
Their native text-based output formats, however, are not easily
parseable by common external tools. Both XML and JSON are easily parsed
and adding those output formats would substantially increase the utility
for those looking to add the ogrinfo and gdalinfo utilities to a
scripting stack.

Implementation
--------------

An example implementation can be seen at the following github fork
`https://github.com/theduckylittle/gdal/blob/trunk/gdal/apps/ogrinfo.cpp <https://github.com/theduckylittle/gdal/blob/trunk/gdal/apps/ogrinfo.cpp>`__

To add the XML output to each utility will require "breaking up" the
main loop into contingent chunks. All diagnostic messages will also need
to be moved to STDERR to ensure that output on STDOUT is always
parseable. The XML representation will be constructed using the MiniXML
library built into GDAL.

Proposed json format for gdalinfo
---------------------------------

::

   {
       "description": "...",
       "driverShortName": "GTiff",
       "driverLongName": "GeoTIFF",
       "files": [ (if -nofl not specified)
           "../gcore/data/byte.tif",
           "../gcore/data/byte.tif.aux.xml"
       ],
       "size": [
           20,
           20
       ],
       "coordinateSystem": {
           "proj": "+proj=.......", (if -proj4 specified)
           "wkt": "PROJCS[....]"
       },
       "gcps": { (if -nogcp not specified)
           "coordinateSystem": {
               "proj": "+proj=.......", (if -proj4 specified)
               "wkt": "PROJCS[....]"
           },
           "gcpList": [
               {
                   "id": "1",
                   "info": "a",
                   "pixel": 0.5,
                   "line": 0.5,
                   "X": 0.0,
                   "Y": 0.0,
                   "Z": 0.0
               },
               {
                   "id": "2",
                   "info": "b",
                   .
                   .
                   .
               }
           ]
       },
       "geoTransform": [
           440720.000000000000000,
           60.000000000000000,
           0.0,
           3751320.000000000000000,
           0.0,
           -60.000000000000000
       ],
       "cornerCoordinates":{
         "upperLeft":[
           440720.0,
           3751320.0
         ],
         "lowerLeft":[
           440720.0,
           3750120.0
         ],
         "upperRight":[
           441920.0,
           3751320.0
         ],
         "lowerRight":[
           441920.0,
           3750120.0
         ],
         "center":[
           441320.0,
           3750720.0
         ]
       },
       "wgs84Extent":{
         "type":"Polygon",
         "coordinates":[
         [
           [
             -117.642054,
             33.9023677
           ],
           [
             -117.6419729,
             33.8915454
           ],
           [
             -117.6290752,
             33.9024346
           ],
           [
             -117.6289957,
             33.8916123
           ],
           [
             -117.642054,
             33.9023677
           ]
          ]
         ]
       },
       "rat": { (if -norat not specified)
           "row0Min": 40918,
           "binSize": 1,
           "fieldDefn": [
               {
                   "index": 0,
                   "name": "Histogram",
                   "type": "integer",
                   "usage": "PixelCount"
               },
               {
                   "index": 1,
                   "name": "fieldName2",
                   "type": 2,
                   "usage": 2
               },
           ],
           "rows": [
               {
                   "index": 0,
                   "f": [
                       1,
                       4
                   ]
               },
               {
                   "index": 1,
                   "f": [
                       5,
                       4
                   ]
               },
               .
               .
               .
           ]
       },
       "metadata": { (if -nomd not specified)
           "": {
               "key1": "value1"
           },
           "IMAGE_STRUCTURE": {
               "key1": "value1"
           },
           "OTHER_DOMAIN": {
               "key1": "value1"
           },
       },
       "cornerCoordinates": {
           "upperLeft": [
               440720.000,
               3751320.000
           ],
           "lowerLeft": [
               440720.000,
               3750120.000
           ],
           "upperRight": [
               441920.000,
               3751320.000
           ],
           "lowerRight": [
               441920.000,
               3750120.000
           ],
           "center": [
               441320.000,
               3750720.000
           ]
       },
       "bands": [
           {
               "description": "...",
               "band": 1,
               "block": [
                   20,
                   20
               ],
               "type": "Byte",
               "colorInterp": "Gray",
               "min": 74.000,
               "max": 255.000,
               
               "computedMin": 74.000, (if -mm specified)
               "computedMax": 255.000,
               
               "minimum": 74.000, (if -stats specified)
               "maximum": 255.000,
               "mean": 126.765,
               "stdDev": 22.928,
               
               "unit": "....",
               "offset": X,
               "scale": X,
               "noDataValue": X,
               "overviews": [
                   {
                       "size": [
                           400,
                           400 ],
                       "checksum": X (if -checksum specified)
                   }, 
                   {
                       "size": [
                           200,
                           200 ],
                       "checksum": X (if -checksum specified)
                   }
               ],
               "mask": {
                   "flags": [
                       "PER_DATASET",
                       "ALPHA"
                   ],
                   "overviews": [
                       {
                           "size": [
                               400,
                               400 ]
                       }, 
                       { 
                           "size": [
                               200,
                               200 ],
                       }
                   ]
               },
               "metadata": { (if -nomd not specified)
                   "__default__": {
                       "key1": "value1"
                   },
                   "IMAGE_STRUCTURE": {
                       "key1": "value1"
                   },
                   "OTHER_DOMAIN": {
                       "key1": "value1"
                   },
               },
               "histogram": { (if -hist specified)
                   "count": 25,
                   "min": -0.5,
                   "max": 255.5,
                   "buckets": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
               },
               "checksum": 4672, (if -checksum specified)
               "colorTable": { (if -noct not specified)
                   "palette": "RGB",
                   "count": 6,
                   "entries": [
                       [255,255,255,255],
                       [255,255,208,255],
                       [255,255,204,255],
                       [153,204,255,255],
                       [0,153,255,255],
                       [102,102,102,255]
                   ]
               }
           },
           {
               "band": 2,
               "block": [
                   20,
                   20
               ],
               .
               .
               .
           }
       ]
   }

Impacted drivers
----------------

None.

Impacted utilities
------------------

gdalinfo

-  Adds a "-xml" output option.
-  Adds a "-json" output option.

ogrinfo

-  Adds a "-xml" output option.
-  Adds a "-json" output option.

Backward Compatibility
----------------------

This change has no impact on backward compatibility at the C API/ABI and
C++ API/ABI levels. Default output will remain the same. The new XML
output will only effect users who specify "-xml" or "-json" on the
command line.

Testing
-------

The Python autotest suite will be extended to test the new XML/JSON
outputs and existing tests will be modified to check STDERR for
diagnostic messages.

Ticket
------

No tickets.

Voting history
--------------

Proposed.
