.. _vrt_multidimensional:

================================================================================
Multidimensional VRT
================================================================================

.. versionadded:: 3.1

Multidimensional VRT is a specific variant of the :ref:`raster.vrt` format,
dedicated to represent Multidimensional arrays, according to the
:ref:`multidim_raster_data_model`.

Here's an example of such a file:

.. code-block:: xml

    <VRTDataset>
        <Group name="/">
            <Dimension name="Y" size="4"/>
            <Dimension name="X" size="3"/>

            <Array name="temperature">
                <DataType>Float64</DataType>
                <DimensionRef ref="Y"/>
                <DimensionRef ref="X"/>
                <Source>
                    <SourceFilename>my.nc</SourceFilename>
                    <SourceArray>temperature</SourceArray>
                    <SourceSlab offset="1,1" count="2,2" step="2,1"/>
                    <DestSlab offset="2,1"/>
                </Source>
            </Array>
        </Group>
    </VRTDataset>

.vrt Format
-----------

A `XML schema of the GDAL VRT format <https://raw.githubusercontent.com/OSGeo/gdal/master/data/gdalvrt.xsd>`_
is available.

Virtual files stored on disk are kept in an XML format with the following
elements.

**VRTDataset**: This is the root element for the whole GDAL dataset. It
has no attributes, and must have a single Group child element with an attribute
name set to "/"

.. code-block:: xml

    <VRTDataset>
        <Group name="/">


**Group**: This represents a :cpp:class:`GDALGroup`. There is at least one root
group of name "/" immediately under the VRTDataset element. A Group must have
a *name* attribute, and may have the following child elements, with 0:n
multiplicity: Dimension, Attribute, Array, Group

**Dimension**: This represents a :cpp:class:`GDALDimension`. It has the following
attributes: *name* (required), *size* (required), *type* and *direction*

.. code-block:: xml

    <Dimension name="X" size="30" type="HORIZONTAL_X" direction="EAST"/>


**Attribute**: This represents a :cpp:class:`GDALAttribute`. It must have a
*name* attribute and a child *DataType* element. Attribute values are stored in
one or several child *Value* element(s)

The value of *DataType* may be: String, Byte, UInt16, Int16, UInt32, Int32,
Float32, Float64, CInt16, CInt32, CFloat32 or CFloat64.

.. code-block:: xml

    <Attribute name="foo">
        <DataType>String</DataType>
        <Value>bar</Value>
    </Attribute>


**Array**: This represents a :cpp:class:`GDALMDArray`. It must have a
*name* attribute and a child *DataType* element. It may have 0 or more
*DimensionRef* or *Dimension* child elements to define its dimensions. And
the following elements may be optionally specified to define its properties.
*SRS, *Unit*, *NoDataValue*, *Offset* and *Scale*.
To define its values, it may have one *RegularlySpacedValues* element,
or zero, one or several elements among *ConstantValue*, *InlineValues*, *InlineValuesWithValueElement* or
*Source*.

.. code-block:: xml

    <Array name="longitude">
        <DataType>Float64</DataType>
        <DimensionRef ref="longitude"/>
        <RegularlySpacedValues start="-180" step="0.5"/>
    </Array>

.. code-block:: xml

    <Array name="time">
        <DataType>String</DataType>
        <DimensionRef ref="time"/>
        <InlineValuesWithValueElement>
            <Value>2010-01-01</Value>
            <Value>2011-01-01</Value>
            <Value>2012-01-01</Value>
        </InlineValuesWithValueElement>
    </Array>

.. code-block:: xml

    <Array name="temperature">
        <DataType>Float64</DataType>
        <DimensionRef ref="Y"/>
        <Dimension name="X" size="3"/>
        <SRS dataAxisToSRSAxisMapping="2,1">EPSG:32631</SRS>
        <Unit>Kelvin</Unit>
        <NoDataValue>-999</NoDataValue>
        <Offset>0</Offset>
        <Scale>1</Scale>
        <Source>
            <SourceFilename>my.nc</SourceFilename>
            <SourceArray>temperature</SourceArray>
        </Source>
    </Array>

**Source**: This indicates that raster data should be read from a separate dataset.
A Source must have a *SourceFilename*, and either a *SourceArray* (when the source
is a Multidimensional dataset), or a *SourceBand* (when the source is a classic
2D dataset) child element. It may have a *SourceTranspose* child element to apply
a :cpp:func:`GDALMDArray::Transpose` operation and a *SourceView* to apply
slicing/trimming operations or extraction of a component of a compound data
type (see :cpp:func:`GDALMDArray::GetView`). It may have a *SourceSlab* element
with attributes *offset*, *count* and *step* defining respectively the starting
offset of the source, the number of values along each dimension and the step
between source elements. It may have a *DestSlab* element with an *offset*
attribute to define where the source data is placed into the target array.
SourceSlab operates on the output of SourceView if specified, which operates
itself on the output of SourceTranspose if specified.

.. code-block:: xml

        <Source>
            <SourceFilename>my.nc</SourceFilename>
            <SourceArray>temperature</SourceArray>
            <SourceTranspose>1,0</SourceTranspose>
            <SourceView>[...]</SourceView>
            <SourceSlab offset="1,1" count="2,2" step="2,1"/>
            <DestSlab offset="2,1"/>
        </Source>
