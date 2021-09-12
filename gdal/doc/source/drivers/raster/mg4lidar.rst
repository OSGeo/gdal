.. _raster.mg4lidar:

================================================================
MG4Lidar -- MrSID/MG4 LiDAR Compression / Point Cloud View files
================================================================

.. shortname:: MG4Lidar

.. build_dependencies:: LIDAR SDK

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_MG4LIDAR

This driver provides a way to view MrSID/MG4 compressed LiDAR file as a
raster DEM. The specifics of the conversion depend on the desired
cellsize, filter criteria, aggregation methods and possibly several
other parameters. For this reason, **the best way to read a MrSID/MG4
compressed LiDAR file is by referencing it in a View (.view) file, which
also parametrizes its raster-conversion. The driver will read an MG4
file directly, however it uses default rasterization parameters that may
not produce a desirable output.** The contents of the View file are
described in the specification :ref:`MrSID/MG4 LiDAR View
Documents <mg4lidar_view_point_cloud>`.

MrSID/MG4 is a wavelet-based point-cloud compression technology. You may
think of it like a LAS file, only smaller and with a built in spatial
index. It is developed and distributed by Extensis. This driver supports
reading of MG4 LiDAR files using Extensis' decoding software development
kit (DSDK). **This DSDK is freely distributed; but, it is not open
source software. You should contact Extensis to obtain it (see link at
end of this page).**

Example View files (from View Document specification)
-----------------------------------------------------

Simplest possible .view file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to view an MG4 file is to wrap it in a View (.view)
file like this. Here, the relative reference to the MG4 file means that
the file must exist in the same directory as the .view file. Since we're
not mapping any bands explicitly, we get the default, which is elevation
only. By default, we aggregate based on mean. That is, if two (or more)
points land on a single cell, we will expose the average of the two.
There's no filtering here so we'll get all the points regardless of
classification code or return number. Since the native datatype of
elevation is "Float64", that is the datatype of the band we will expose.

::

   <PointCloudView>
      <InputFile>Tetons.sid</InputFile>
   </PointCloudView>

Crop the data
~~~~~~~~~~~~~

This is similar to the example above but we are using the optional
ClipBox tag to select a 300 meter North-South swatch through the cloud.
If we wanted to crop in the East-West directions, we could have
specified that explicitly instead of using NOFITLER for those.
Similarly, we could also have cropped in the Z direction as well.

::

   <PointCloudView>
      <InputFile>Tetons.sid</InputFile>
      <ClipBox>505500 505800 NOFILTER NOFILTER</ClipBox>
   </PointCloudView>

Expose as a bare earth (Max) DEM
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here, we expose a single band (elevation) but we want only those points
that have been classified as "Ground". The ClassificationFilter
specifies a value of 2 - the ASPRS Point Class code that stipulates
"Ground" points. Additionally, instead of the default "Mean" aggregation
method, we specify "Max". This means that if two (or more) points land
on a single cell, we expose the larger of the two elevation values.

::

   <PointCloudView>
      <InputFile>E:\ESRIDevSummit2010\Tetons.sid</InputFile>
      <Band> <!-- Max Bare Earth-->
         <Channel>Z</Channel>
         <AggregationMethod>Max</AggregationMethod>
         <ClassificationFilter>2</ClassificationFilter>
      </Band>
   </PointCloudView>

Intensity image
~~~~~~~~~~~~~~~

Here we expose an intensity image from the point cloud.

::

   <PointCloudView>
      <InputFile>Tetons.sid</InputFile>
      <Band>
         <!-- All intensities -->
         <Channel>Intensity</Channel>
      </Band>
   </PointCloudView>

RGB image
~~~~~~~~~

Some point cloud images include RGB data. If that's the case, you can
use a .view file like this to expose that data.

::

   <PointCloudView>
      <InputFile>Grass Lake Small.xyzRGB.sid</InputFile>
      <Band>
         <Channel>Red</Channel>
      </Band>
      <Band>
         <Channel>Green</Channel>
      </Band>
      <Band>
         <Channel>Blue</Channel>
      </Band>
   </PointCloudView>

Writing not supported
---------------------

This driver does not support writing MG4 files.

Limitations of current implementation
-------------------------------------

Only one *<InputFile>* tag is supported. It must reference an MG4 file.

The only *<InterpolationMethod>* that is supported is *<None>*
(default). Use this to specify a NODATA value if the default (maximum
value of the datatype) is not what you want. See View Specification for
details.

There is insufficient error checking for format errors and invalid
parameters. Many invalid entries will likely fail silently.

See Also:
---------

-  Implemented as *gdal/frmts/mrsid_lidar/gdal_MG4Lidar.cpp*
-  :ref:`MrSID/MG4 LiDAR View Document
   Specification <mg4lidar_view_point_cloud>`
-  `Extensis web site <http://www.extensis.com/support/developers>`__

.. toctree::
   :maxdepth: 1
   :hidden:

   mg4lidar_view_point_cloud
