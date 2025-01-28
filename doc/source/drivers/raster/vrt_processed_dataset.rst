.. _vrt_processed_dataset:

================================================================================
VRT processed dataset
================================================================================

.. versionadded:: 3.9

A VRT processed dataset is a specific variant of the :ref:`raster.vrt` format,
to apply chained processing steps that may apply to several bands at the same time.

The following built-in algorithms are available:

- Expression: evaluate a specified expression (e.g., "(B1+B2)/B3"). Available since GDAL 3.11.

- LocalScaleOffset: apply per-pixel gain and offset coming (typically subsampled)
  from auxiliary datasets. Can be used for dehazing processing.

- BandAffineCombination: perform an affine transformation combination of bands.

- Trimming: apply local thresholding of saturation

- LUT: apply a look-up table (band per band)

More algorithms can be registered at run-time with the :cpp:func:`GDALVRTRegisterProcessedDatasetFunc`
function`

Here's an example of such a file to apply various correction to a R,G,B,NIR dataset:

.. code-block:: xml

    <VRTDataset subClass="VRTProcessedDataset">
      <Input>
        <SourceFilename relativeToVRT="1">source.tif</SourceFilename>
      </Input>

      <ProcessingSteps>
        <Step name="Dehazing">
          <Algorithm>LocalScaleOffset</Algorithm>

          <Argument name="relativeToVRT">true</Argument>

          <Argument name="gain_dataset_filename_1">gains.tif</Argument>
          <Argument name="gain_dataset_filename_2">gains.tif</Argument>
          <Argument name="gain_dataset_filename_3">gains.tif</Argument>
          <Argument name="gain_dataset_filename_4">gains.tif</Argument>
          <Argument name="gain_dataset_band_1">1</Argument>
          <Argument name="gain_dataset_band_2">2</Argument>
          <Argument name="gain_dataset_band_3">3</Argument>
          <Argument name="gain_dataset_band_4">4</Argument>

          <Argument name="offset_dataset_filename_1">offsets.tif</Argument>
          <Argument name="offset_dataset_filename_2">offsets.tif</Argument>
          <Argument name="offset_dataset_filename_3">offsets.tif</Argument>
          <Argument name="offset_dataset_filename_4">offsets.tif</Argument>
          <Argument name="offset_dataset_band_1">1</Argument>
          <Argument name="offset_dataset_band_2">2</Argument>
          <Argument name="offset_dataset_band_3">3</Argument>
          <Argument name="offset_dataset_band_4">4</Argument>

          <Argument name="nodata">0</Argument>
          <Argument name="min">1</Argument>
          <Argument name="max">10000</Argument>
        </Step>

        <Step name="Linear combination">
          <Algorithm>BandAffineCombination</Algorithm>
          <Argument name="coefficients_1">0,1.2,-0.2,0.0,0.0</Argument>
          <Argument name="coefficients_2">0,-0.03,1.03,0.0,0.0</Argument>
          <Argument name="coefficients_3">0,0.0,0.0,1.0,0.0</Argument>
          <Argument name="coefficients_4">0,0.0,0.0,0.0,1.0</Argument>

          <Argument name="min">1</Argument>
          <Argument name="max">10000</Argument>
        </Step>

        <Step name="Trimming">
          <Algorithm>Trimming</Algorithm>
          <Argument name="relativeToVRT">true</Argument>
          <Argument name="trimming_dataset_filename">trimming.tif</Argument>
          <Argument name="tone_ceil">10000</Argument>
          <Argument name="top_margin">0</Argument>
          <Argument name="top_rgb">10000</Argument>
        </Step>

        <Step name="LUT">
          <Algorithm>LUT</Algorithm>
          <Argument name="lut_1">
              0:0,10000.0:255
          </Argument>
          <Argument name="lut_2">
              0:0,10000.0:255
          </Argument>
          <Argument name="lut_3">
              0:0,10000.0:255
          </Argument>
          <Argument name="lut_4">
              0:0,10000.0:255
          </Argument>
        </Step>
      </ProcessingSteps>

      <VRTRasterBand dataType="Byte" band="1" subClass="VRTProcessedRasterBand">
        <ColorInterp>Red</ColorInterp>
      </VRTRasterBand>
      <VRTRasterBand dataType="Byte" band="2" subClass="VRTProcessedRasterBand">
        <ColorInterp>Green</ColorInterp>
      </VRTRasterBand>
      <VRTRasterBand dataType="Byte" band="3" subClass="VRTProcessedRasterBand">
        <ColorInterp>Blue</ColorInterp>
      </VRTRasterBand>
      <VRTRasterBand dataType="Byte" band="4" subClass="VRTProcessedRasterBand">
      </VRTRasterBand>
    </VRTDataset>

.vrt format
-----------

The ``VRTDataset`` root element must have a ``subClass="VRTProcessedDataset"`` attribute.

The following child elements of ``VRTDataset`` may be defined: ``SRS``, ``GeoTransform``, ``Metadata``. If they are not explicitly set, they are inferred from the input dataset.

The ``VRTDataset`` root element must also have the 2 following child elements:

- ``Input``, which must have one and only one of the following ``SourceFilename`` or ``VRTDataset`` as child elements, to define the input dataset to which to apply the processing steps. Starting with GDAL 3.11, values from the input dataset will be automatically unscaled; this can be disabled by setting the ``unscale`` attribute of ``Input`` to ``false``.

- ``ProcessingSteps``, with at least one child ``Step`` element.

Each ``Step`` must have a ``Algorithm`` child element, and an optional ``name`` attribute.
The value of ``Algorithm`` must be a registered VRTProcessedDataset function. At time of writing, the following 4 algorithms are defined: ``LocalScaleOffset``, ``BandAffineCombination``, ``Trimming`` and ``LUT``.

A ``Step`` will generally have one or several ``Argument`` child elements, some of them being required, others optional. Consult the documentation of each algorithm.

Starting with GDAL 3.11, a ``OutputBands`` element can be
defined as a child element of ``VRTDataset``, with the following 2 attributes:

* ``count`` whose value can be ``FROM_SOURCE`` to indicate that the output band
  count must be the same as the number of bands of the input dataset,
  ``FROM_LAST_STEP`` to indicate that it must be the number of output bands
  returned by the initialization function of the last step, or an integer value.

* ``dataType`` whose value can be ``FROM_SOURCE`` to indicate that the output band
  data type must be the same as the one of the input dataset,
  ``FROM_LAST_STEP`` to indicate that it must be the one returned by the
  initialization function of the last step, or a value among
  Byte, Int8, UInt16, Int16, UInt32, Int32, UInt64, Int64, Float32, Float64, CInt16, CInt32, CFloat32 or CFloat64

Example:

.. code-block:: xml

    <VRTDataset subClass="VRTProcessedDataset">
      <Input>
        <SourceFilename relativeToVRT="1">source.tif</SourceFilename>
      </Input>
      <OutputBands count="FROM_LAST_STEP" dataType="FROM_LAST_STEP"/>
      <ProcessingSteps>...</ProcessingSteps>
    </VRTDataset>


If ``OutputBands`` is omitted,

* if there are explicit ``VRTRasterBand`` elements, they must have a
  ``subClass="VRTProcessedRasterBand"`` attribute

* it there are no explicit ``VRTRasterBand`` elements, the number and data types
  of input bands are used implicitly.

Both ``OutputBands`` and  ``VRTRasterBand`` elements may be defined. The information
specified by ``OutputBands`` will be used in priority, and ``VRTRasterBand`` elements
will be used only if they are compatible with the band count and data type specified
through ``OutputBands``. A situation where  ``OutputBands`` and  ``VRTRasterBand`` elements
are both found is for example when computing statistics on a .vrt file with only
``OutputBands`` initially set.

LocalScaleOffset algorithm
--------------------------

Apply per-pixel gain and offset coming (typically subsampled) from auxiliary
datasets. Can be used for dehazing processing.

The gain and offset auxiliary datasets must have a georeferencing consistent of
the input dataset, but may have a different resolution.

The formula applied by that algorithm is: ``output_value = clamp(input_value * gain - offset, min, max)``

The following required arguments must be specified:

- ``gain_dataset_filename_{band}``: Filename to the gain dataset, where {band} must be replaced by 1 to the number of input bands.

- ``gain_dataset_band_{band}``: Band number corresponding to ``gain_dataset_filename_{band}``, where {band} must be replaced by 1 to the number of input bands.

- ``offset_dataset_filename_{band}``: Filename to the offset dataset, where {band} must be replaced by 1 to the number of input bands.

- ``offset_dataset_band_{band}``: Band number corresponding to ``offset_dataset_filename_{band}``, where {band} must be replaced by 1 to the number of input bands.


The following optional arguments may be specified:

- ``relativeToVRT``: Whether gain and offset filenames are relative to the VRT. Allowed values are ``true`` and ``false``. Defaults to ``false``

- ``min``: Clamp minimum value, applied before writing the output value.

- ``max``: Clamp maximum value, applied before writing the output value.

- ``nodata``: Override the input nodata value coming from the previous step (or the input dataset for the first step).

- ``gain_nodata``: Override the nodata value coming from the gain dataset(s).

- ``offset_nodata``: Override the nodata value coming from the offset dataset(s).


BandAffineCombination algorithm
-------------------------------

Perform an affine transformation combination of bands.

The following required argument must be specified:

- ``coefficients_{band}``: Comma-separated coefficients for combining bands where {band} must be replaced by 1 to the number of output bands. The number of coefficients in each argument must be 1 + number_of_input_bands, where the first coefficient is a constant, the second coefficient is the weight of the first input band, the third coefficient is the weight of the second input band, etc.


The following optional arguments may be specified:

- ``src_nodata``: Override the input nodata value coming from the previous step (or the input dataset for the first step).

- ``dst_nodata``: Set the output nodata value.

- ``replacement_nodata``: Value to substitute to a valid computed value that would be equal to dst_nodata.

- ``dst_intended_datatype``: Intended datatype of output (which might be different than the working data type). Used to infer an appropriate value for replacement_nodata when it is not specified.

- ``min``: Clamp minimum value, applied before writing the output value.

- ``max``: Clamp maximum value, applied before writing the output value.


Trimming algorithm
------------------

Apply local thresholding of saturation, with a special processing of the R,G,B bands compared to other bands.

The pseudo algorithm used for each pixel is:

.. code-block::

    // Extract local saturation value from trimming image
    localMaxRGB = value from TrimmingImage
    reducedRGB = min ( (1-top_margin)*top_rgb/localMaxRGB ; 1)

    // RGB bands specific process
    RGB[] = get red, green, blue components of input buffer
    maxRGB = max(RGB[])
    toneMaxRGB = min ( toneCeil/maxRGB ; 1)
    toneBand[] = min ( toneCeil/RGB[] ; 1)

    output_value_RGB[] = min ( reducedRGB*RGB[]*toneBand[] / toneMaxRGB ; topRGB)

    // Other bands processing (NIR, ...): only apply RGB reduction factor
    Trimmed(OtherBands[]) = reducedRGB * OtherBands[]


The following required arguments must be specified:

- ``trimming_dataset_filename``: Filename of the trimming dataset. It must have one single band. It must have a georeferencing consistent of the input dataset, but may have a different resolution.

- ``top_rgb``: Maximum saturating RGB output value.

- ``tone_ceil``: Maximum threshold beyond which we give up saturation.

- ``top_margin``: Margin to allow for dynamics in brighest areas (between 0 and 1, should be close to 0)


The following optional arguments may be specified:

- ``relativeToVRT``: Whether the trimming dataset filename is relative to the VRT. Allowed values are ``true`` and ``false``. Defaults to ``false``

- ``red_band``: Index (one-based) of the red band. Defaults to 1.

- ``green_band``: Index (one-based) of the green band. Defaults to 1.

- ``blue_band``: Index (one-based) of the blue band. Defaults to 1.

- ``nodata``: Override the input nodata value coming from the previous step (or the input dataset for the first step).

- ``trimming_nodata``: Override the nodata value coming from the trimming dataset.


LUT
---

Apply a look-up table (band per band), typically to get from UInt16 to Byte data types.

The following required argument must be specified:

- ``lut_{band}``: List of the form ``[src value 1]:[dest value 1],[src value 2]:[dest value 2],....``. {band} must be replaced by 1 to the number of bands.

The intermediary values are calculated using a linear interpolation
between the bounding destination values of the corresponding range.
Source values should be monotonically non-decreasing. Clamping is performed for
input pixel values outside of the range specified by the LUT. That is, if an
input pixel value is lower than the minimum source value, then the destination
value corresponding to that minimum source value is used as the output pixel value.
And similarly for an input pixel value that is greater than the maximum source value.

The following optional arguments may be specified:

- ``src_nodata``: Override the input nodata value coming from the previous step (or the input dataset for the first step).

- ``dst_nodata``: Set the output nodata value.

Expression
----------

Evaluate an expression using `muparser <https://beltoforion.de/en/muparser/>`__ or, if enabled at compile-time, `ExprTk <https://www.partow.net/programming/exprtk/index.html>`__.

The following argument must be specified:

- ``expression``: An expression to be evaluated. Band values can be accessed by the variables ``B1``, ``B2``, etc.
  Although muparser does not support vector data types, the name ``BANDS`` will be expanded into a list of the
  available bands, allowing the use of expressions such as ``sum(BANDS)``. When using ExprTk, the name ``BANDS``
  will be a vector whose (0-indexed) elements can be accessed individually. Expressions may return more than one
  value; one output band will be created for each value. The expression must return the same number of values
  each time it is invoked.

The following optional arguments may be specified:

- ``dialect``: Indicates the library that should be used to evaluate the expression. Defaults to "muparser".

- ``batch_size``: When set to a value ``N``, the expression will be evaluated independently in groups of
  up to ``N`` bands. The result of the expression will be the concatenation of the results from each batch.

When using ExprTk, the following configuration options are available:

- .. config:: GDAL_EXPRTK_MAX_EXPRESSION_LENGTH
     :default: 100000

     Indicates the maximum number of characters expression that will be passed to the ExprTk parser.

- .. config:: GDAL_EXPRTK_MAX_VECTOR_LENGTH
     :default: 100000

     Indicates the maximum length of a vector variables declared within an expression.

- .. config:: GDAL_EXPRTK_ENABLE_LOOPS
     :choices: YES, NO
     :default: YES

     Indicates whether looping constructs (for, while, etc.) may be used within an expression.

- .. config:: GDAL_EXPRTK_TIMEOUT_SECONDS
     :default: 1

     Indicates the maximum per-pixel runtime of an ExprTk expression. ExprTk performs runtime
     checks only when loops are used.


