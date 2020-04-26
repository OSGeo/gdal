.. _mg4lidar_view_point_cloud:

================================================
Specification for MrSID/MG4 LiDAR View Documents
================================================

Version 1.0

Introduction
------------

This document specifies the contents of an XML document used as a "view" into a LiDAR point cloud.  It is intended to be a rigorous definition of an XML-based format for specifying rasterization of point cloud data.  If you are looking for "something to get me started very quickly", please see the examples below.

Document Structure (informative)
--------------------------------

The overall element structure of a View document is informally shown below.  Indentation and regular expression syntax are used to intuitively indicate parent-child nesting and the number of occurrences of elements.

::

      PointCloudView
            InputFile +
            Datatype ?
            Band *
                  Channel ?
                  ClassificationFilter ?
                  ReturnNumberFilter ?
                  AggregationMethod ?
                  InterpolationMethod ?
            ClassificationFilter ?
            ReturnNumberFilter ?
            AggregationMethod ?
            InterpolationMethod ?
            ClipBox ?
            CellSize ?
            GeoReference ?

Elements
--------

Each element is specified as follows:

::

    ElementName
        Cardinality:  number of occurrences allowed
        Parents:  what element(s) may contain this element
        Contents: what may be placed inside the element
        Attributes:  what attributes are allowed, if any
        Notes:  additional usage information or restriction

PointClouldView
+++++++++++++++

Description:  the root element for the document

Cardinality:  1

Parents:  none (must be root element)

Contents:  child elements as specified below:

- InputFile
- Datatype
- Band
- ClassificationFilter
- ReturnNumberFilter
- AggregationMethod
- InterpolationMethod
- ClipBox
- CellSize
- GeoReference

Attributes:

- version - this attribute must be present and set to the value 1.0

Notes:  (none)

InputFile
+++++++++

Description:  specifies an input file containing point cloud data

Cardinality:  1..n

Parents:  PointClouldView

Contents:  string (corresponding to a filename)

Attributes:  (none)

Notes:

- Typically the file will be a MrSID/MG4 LiDAR file, but may also be a LAS file.
-  The file name given may have a relative or absolute path.  If relative, the path is to be expanded relative to the directory containing this View document.

Datatype
++++++++

Description:  specifies the datatype to which channel data should be coerced

Cardinality:  0 or 1

Parents:  PointClouldView

Contents:  string (corresponding to a datatype name)

Attributes:  (none)

Notes:

- If this element is not present, the native datatype of the channel is used.
- Legal values are derived from those returned by GDALGetDataTypeByName, as follows:

    - Byte
    - UInt16
    - Int16
    - UInt32
    - Int32
    - Float32
    - Float64

- Channel data will be coerced via a c-style cast, truncating data as necessary.

Band
++++

Description:  list of which band(s) to expose and in what manner to process the band data

Cardinality:  0, 1 or 3

Parents:  PointClouldView

Contents:  child elements as follows:

- 0 or 1 Channel element
- 0 or 1 ClassificationFilter element
- 0 or 1 ReturnNumberFilter element
- 0 or 1 InterpolationMethod element
- 0 or 1 AggregationMethod element

Attributes:  (none)

Notes:

- Not specifying any bands is the same as specifying only one with all default values.

Channel
+++++++

Description:  the name of the channel in the input file

Cardinality:  0 or 1 per Band element

Parents:  Band

Contents:  we use the following canonical names of channels

- X
- Y
- Z
- Intensity
- ReturnNum
- NumReturns
- ScanDir
- EdgeFlightLine
- ClassId
- ScanAngle
- UserData
- SourceId
- GPSTime
- Red
- Green
- Blue

Attributes:  (none)

Notes:

- Custom channels have non-canonical names, are supported, and may be specified.
- If this element is omitted, the Channel for the Band shall default to Z.
- The channel names are derived from PointData.h of the MG4 Decode SDK.

ClassificationFilter
++++++++++++++++++++

Description:  A filter for points whose classification code is one of the specified values.

Cardinality:  0 or 1 per Band element

Parents:  Band or PointCloudView

Contents:  space-separated "Classification Values" (0-31) as defined by ASPRS Standard LIDAR Point Classes in the LAS 1.3 Specification.

Attributes:  (none)

Notes:

- If this element is omitted, the band shall have no classification filter applied.
- If this element is a child of the PointCloudView element, it applies to all bands (unless overridden for a specific band)
- If this element is a child of a Band element, it applies to this band only and overrides any other setting
- Note that numbers are used to represent the filters, rather than strings.  This is because there is no canonical, simple naming convention for them, and is also in keeping with existing practice in certain existing applications.

ReturnNumberFilter
++++++++++++++++++

Description:  A filter for points whose return number is one of the specified values.

Cardinality:  0 or 1 per Band element

Parents:  Band or PointCloudView

Contents:  space-separated numbers (1, 2, ...) or the string LAST

Attributes:  (none)

Notes:

- If this element is omitted, the band shall have no return number filter applied
- If this element is a child of the PointCloudView element, it applies to all bands (unless overridden for a specific band)
- If this element is a child of a Band element, it applies to this band only and overrides any other setting

AggregationMethod
+++++++++++++++++

Description:  Each cell (pixel) can expose a single value.  When 2 or more points fall on a single cell, this method determines what value to expose.

Cardinality:  0 or 1 per Band element

Parents:  Band or PointCloudView

Contents:  a string, one of Min, Max, or Mean

Attributes:  (none)

Notes:

- If this element is omitted, the band shall have the "Mean" aggregation method applied
- If this element is a child of the PointCloudView element, it applies to all bands (unless overridden for a specific band)
- If this element is a child of a Band element, it applies to this band only and overrides any other setting

InterpolationMethod
+++++++++++++++++++

Description:  Method and parameter to interpolate NODATA values.  Also specifies what the NODATA value is.

Cardinality:  0 or 1 per Band element

Parents:  Band or PointCloudView

Contents:   exactly one of the following elements:

- None

- InverseDistanceToAPower

- MovingAverage

- NearestNeighbor

- Minimum

- Maximum

- Range

Attributes:  (none)

Notes

- Each of the interpolation methods (MovingAverage, etc.) is an element whose content is a text string corresponding to the parameter(s) for that method.  See :ref:`gdal_grid_tut` for a description of the methods and their parameter strings.
- In the parameter descriptions, MAX is used to indicate the value defined by libc which is the largest supportable value for the output datatype.  If you choose to override this default be sure that the number you specify will fit in the datatype you specify.
- If this element is omitted, the band shall have the "None" interpolation method applied.
- If this element is a child of the PointCloudView element, it applies to all bands (unless overridden for a specific band)
- If this element is a child of a Band element, it applies to this band only and overrides any other setting

ClipBox
+++++++

Description:  geographic extent of region to be viewed

Cardinality:  0 or 1

Parents:  PointClouldView

Contents:  4 or 6 doubles; the string NOFILTER may be specified in place of a double value

Attributes:  (none)

Notes:

- The full 6 values are (in order): xmin, xmax, ymin, ymax, zmin, zmax.
- The string NOFILTER means to use the corresponding value of the Minimum Bounding Rectangle (MBR) of the input files.  The point is not filtered by that value.
- If only 4 double are present, the zmin and zmax are assumed to be NOFILTER.
- If this element is not present, the clip box is assumed to be the MBR of the input files.

CellSize
++++++++

Description:  Side length of a (square) pixel in ground units

Cardinality:  0 or 1

Parents:  PointClouldView

Contents:  1 double

Attributes:  (none)

Notes:

- This element is used to determine the size of the resulting raster.
- If this element is omitted, the default cell size is the average (linear) point spacing (assuming a uniform distribution over the entire extent).

GeoReference
++++++++++++

Description:  the coordinate reference system of the view

Cardinality:  0 or 1

Parents:  PointClouldView

Contents:  a string (corresponding to a WKT)

Attributes:  (none)

Notes:

- If this element is omitted, the WKT of the input files is used.  If two or more files have different WKTs, then no GeoReference is defined.
- A typical use of this element is for when the MG4 file was created without adequate GeoReference information: cases where some combination of UOM, HorizCS and VertCS are missing are quite common.

Additional Requirements
-----------------------

Any element not recognized should be treated as an error.

Any attribute not recognized should be treated as an error.

This specification does not mandate the lexical ordering of the child elements within a given parent.

Examples
--------

Simplest possible .view file
++++++++++++++++++++++++++++

The simplest way to view an MG4 file is to wrap it in a View (.view) file like this.  Here, the relative reference to the MG4 file means that the file must exist in the same directory as the .view file.  Since we're not mapping any bands explicitly, we get the default, which is elevation only.  By default, we aggregate based on mean.  That is, if two (or more) points land on a single cell, we will expose the average of the two.  There's no filtering here so we'll get all the points regardless of classification code or return number.  Since the native datatype of elevation is "Float64", that is the datatype of the band we will expose.

.. code-block:: xml

    <PointCloudView>
        <InputFile>Tetons.sid</InputFile>
    </PointCloudView>

 
Crop the data
+++++++++++++

This is similar to the example above but we are using the optional ClipBox tag to select a 300 meter North-South swatch through the cloud.  If we wanted to crop in the East-West directions, we could have specified that explicitly instead of using NOFITLER for those.  Similarly, we could also have cropped in the Z direction as well.

.. code-block:: xml

    <PointCloudView>
    <InputFile>Tetons.sid</InputFile>
    <ClipBox>505500 505800 NOFILTER NOFILTER</ClipBox>
    </PointCloudView>

 
Expose as a bare earth (Max) DEM
++++++++++++++++++++++++++++++++

Here, we expose a single band (elevation) but we want only those points that have been classified as "Ground."  The ClassificationFilter specifies a value of 2 - the ASPRS Point Class code that stipulates "Ground" points. Additionally, instead of the default "Mean" aggregation method, we specify "Max."  This means that if two (or more) points land on a single cell, we expose the larger of the two elevation values.

.. code-block:: xml

    <PointCloudView>
        <InputFile>E:\ESRIDevSummit2010\Tetons.sid</InputFile>
        <Band> <!-- Max Bare Earth-->
            <Channel>Z</Channel>
            <AggregationMethod>Max</AggregationMethod>
            <ClassificationFilter>2</ClassificationFilter>
        </Band>
    </PointCloudView>

Intensity image
+++++++++++++++

Here we expose an intensity image from the point cloud.

.. code-block:: xml

    <PointCloudView>
        <InputFile>Tetons.sid</InputFile>
        <Band>
            <!-- All intensities -->
            <Channel>Intensity</Channel>
        </Band>
    </PointCloudView>

RGB image
+++++++++

Some point cloud images include RGB data.  If that's the case, you can use a .view file like this to expose that data.

.. code-block:: xml

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
