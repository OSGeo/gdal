.. _rfc-32:

================================================================================
RFC 32: gdallocationinfo utility
================================================================================

Authors: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

This document proposes the addition of a new standard commandline
utility for GDAL to report details about a location (pixel) in a raster.

Rationale
---------

1) A user has a use case where they would like to be able to identify
   the VRT file used to satisfy requests for a particular pixel /
   location.

2) Many users have requested a tool to find the value of a location,
   often expressed in a coordinate system different than that of the
   image. For instance, "what is the elevation at a given lat/long
   location?".

The gdallocationinfo utility is intended to address both sorts of
requests, and hopefully in a way that will have some general value as a
"raster point query" tool.

gdallocationinfo
----------------

Full docs are available at :ref:`gdallocationinfo`

::

   Usage: gdallocationinfo [--help-general] [-xml] [-lifonly] [-valonlyl]
                           [-b band]* [-l_srs srs_def] [-geoloc] [-wgs84]
                           srcfile x y

The key aspects of the utility are control over the coordinate system of
the location (-s_srs, -geoloc, -wgs84) and various controls over the
output format (-xml, -lifonly, -valonly). An example of full output in
xml might be:

::

   $ gdallocationinfo -xml -wgs84 utm.vrt -117.5 33.75
   <Report pixel="217" line="282">
     <BandReport band="1">
       <LocationInfo>
         <File>utm.tif</File>
       </LocationInfo>
       <Value>16</Value>
     </BandReport>
   </Report>

LocationInfo Metadata Domain
----------------------------

The pixel values and location transformation logic is all built into
gdallocationinfo and doesn't require much elaboration. The more exotic
portion is reporting of "LocationInfo" queried from the datasource.

For our immediate needs the requirement is to have the
VRTSourcedRasterBand return information on the file(s) overlapping the
target pixel. But, in theory different drivers might return different
sorts of information about a location. For instance, a WMS driver might
issue a GetFeatureInfo for the location and return the result.

The mechanism to query the datasource is a specially named
GetMetadataItem() request against the "LocationInfo" domain of the
target band(s). The following requested item name is of the form
"Pixel_x_y" where x and y are the pixel and line of the pixel being
queried.

The returned value from this item should either be NULL, or an XML
documented with the root element "". The contents of the document are
otherwise undefined as long as they are well formed XML. The VRT driver
returns a series of xxx entries for each of the files at that location.

eg.

::

       GDALGetMetadataItem( hBand, "Pixel_100_200", "LocationInfo" );

might return:

::

       <LocationInfo>
         <File>utm.tif</File>
       </LocationInfo>

Test Suite
----------

A test will be introduced in the gdal utilities suite, and the
gdrivers/vrt.py script for the utility and VRT behavior respectively.

Documentation
-------------

Documentation for the utility has already been prepared and is
referenced above.

Implementation
--------------

Implementation is already complete, and in trunk. Adjustments can be
made by Frank Warmerdam as needed due to RFC revisions.
