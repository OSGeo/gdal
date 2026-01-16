.. _gdal_cli_from_python:

================================================================================
How to use "gdal" CLI algorithms from Python
================================================================================

Principles
----------

"gdal" CLI algorithms are available as :py:class:`osgeo.gdal.Algorithm` instances.

A convenient way to access an algorithm and run it is to use the :py:func:`osgeo.gdal.Run`
function.

.. py:function:: Run(*alg, arguments={}, progress=None, **kwargs)

   Run a GDAL algorithm and return it.

   .. versionadded:: 3.11

   :param alg: Path to the algorithm or algorithm instance itself. For example "raster info", or ["raster", "info"] or "raster", "info".
   :type alg: str, list[str], tuple[str], Algorithm
   :param arguments: Input arguments of the algorithm. For example {"format": "json", "input": "byte.tif"}
   :type arguments: dict
   :param progress: Progress function whose arguments are a progress ratio, a string and a user data
   :type progress: callable
   :param kwargs: Instead of using the ``arguments`` parameter, it is possible to pass
                  algorithm arguments directly as named parameters of gdal.Run().
                  If the named argument has dash characters in it, the corresponding
                  parameter must replace them with an underscore character.
                  For example ``dst_crs`` as a a parameter of gdal.Run(), instead of
                  ``dst-crs`` which is the name to use on the command line.
   :rtype: a :py:class:`osgeo.gdal.Algorithm` instance


If you do not need to access output value(s) of the algorithm, you can call
``Run`` directly:
``gdal.Run(["raster", "convert"], input="in.tif", output="out.tif")``

The name of algorithm arguments are the long names as documented in the
documentation page of each algorithm. They can also be obtained with
:py:meth:`osgeo.gdal.Algorithm.GetArgNames`.

.. code-block:: python

    >>> gdal.Algorithm("raster", "convert").GetArgNames()
    ['help', 'help-doc', 'version', 'json-usage', 'drivers', 'config', 'progress', 'output-format', 'open-option', 'input-format', 'input', 'output', 'creation-option', 'overwrite', 'append']

For a command such as :ref:`gdal_raster_info` that returns a JSON
output, you can get the return value of :py:func:`osgeo.gdal.Run` and call the
:py:meth:`osgeo.gdal.Algorithm.Output` method.

.. code-block:: python

    alg = gdal.Run("raster", "info", input="byte.tif")
    info_as_dict = alg.Output()

If the return value is a dataset, :py:func:`osgeo.gdal.Run` can be used
within a context manager, in which case :py:meth:`osgeo.gdal.Algorithm.Finalize`
will be called at the exit of the context manager.

.. code-block:: python

    with gdal.Run("raster reproject", input=src_ds, output_format="MEM",
                  dst_crs="EPSG:4326") as alg:
        values = alg.Output().ReadAsArray()

When not using a context manager, :py:meth:`osgeo.gdal.Algorithm.Finalize` can be called directly to
release the algorithm's references to the output dataset and free associated resources. If a variable
still references the dataset, it will not be closed and may still be accessed:

.. code-block:: python

    >>> alg = gdal.Run("raster reproject", input=src_ds, output_format="MEM",
                   dst_crs="EPSG:4326")
    >>> out_ds = alg.Output()
    >>> print(out_ds.GetRefCount())
    2
    >>> alg.Finalize()
    >>> print(out_ds.GetRefCount())
    1
    >>> alg.Output().ReadAsArray() # no longer available from the algorithm
    AttributeError: 'NoneType' object has no attribute 'ReadAsMaskedArray'
    >>> out_ds.ReadAsArray() # still accessible via the existing reference
    array([[-9999., -9999., -9999., ...,

``gdal.alg`` module
-------------------

.. versionadded:: 3.12

The ``gdal.alg`` module is also available with submodules and
functions reflecting the hierarchy of algorithms. Functions return an
:py:class:`osgeo.gdal.Algorithm` instance similarly to :py:func:`osgeo.gdal.Run`

The above example running "gdal raster convert" can also be rewritten as:

.. code-block:: python

    >>> from osgeo import gdal
    >>> gdal.alg.raster.convert(input="in.tif", output="out.tif")

The documentation of the function lists argument names, types and semantics:

.. code-block:: python

    >>> from osgeo import gdal
    >>> help(gdal.alg.raster.convert)

        Help on function convert:

        convert(input: Union[List[osgeo.gdal.Dataset], List[str], List[os.PathLike[str]]], output: Union[osgeo.gdal.Dataset, str, os.PathLike[str]], input_format: Union[List[str], str, NoneType] = None, open_option: Union[List[str], str, NoneType] = None, output_format: Optional[str] = None, creation_option: Union[List[str], str, NoneType] = None, overwrite: Optional[bool] = None, append: Optional[bool] = None)
            Convert a raster dataset.

            Consult https://gdal.org/programs/gdal_raster_convert.html for more details.

            Parameters
            ----------
            input: Union[List[gdal.Dataset], List[str], List[os.PathLike[str]]]
                Input raster datasets
            output: Union[gdal.Dataset, str, os.PathLike[str]]
                Output raster dataset
            input_format: Optional[Union[List[str], str]]=None
                Input formats
            open_option: Optional[Union[List[str], dict, str]]=None
                Open options
            output_format: Optional[str]=None
                Output format ("GDALG" allowed)
            creation_option: Optional[Union[List[str], dict, str]]=None
                Creation option
            overwrite: Optional[bool]=None
                Whether overwriting existing output is allowed
            append: Optional[bool]=None
                Append as a subdataset to existing output


            Output parameters
            -----------------
            output: gdal.ArgDatasetValue
                Output raster dataset


Raster commands examples
------------------------

.. example::
   :title: Getting information on a raster dataset as JSON

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        info_as_dict = gdal.alg.raster.info(input="byte.tif").Output()


.. example::
   :title: Converting a georeferenced netCDF file to cloud-optimized GeoTIFF


   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        gdal.alg.raster.convert(input="in.tif", output="out.tif", output_format="COG", overwrite=True)

   or

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        gdal.Run("raster", "convert", input="in.tif", output="out.tif",
                 output_format="COG", overwrite=True)

   or

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        gdal.Run(["raster", "convert"], {"input": "in.tif", "output": "out.tif", "output-format": "COG", "overwrite": True})


.. example::
   :title: Reprojecting a GeoTIFF file to a Deflate-compressed tiled GeoTIFF file

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        gdal.alg.raster.reproject(
                  input="in.tif", output="out.tif",
                  dst_crs="EPSG:4326",
                  creation_options={ "TILED": "YES", "COMPRESS": "DEFLATE"})


.. example::
   :title: Reprojecting a (possibly in-memory) dataset to a in-memory dataset

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        with gdal.alg.raster.reproject(input=src_ds, output_format="MEM",
                      dst_crs="EPSG:4326") as alg:
            values = alg.Output().ReadAsArray()


Vector commands examples
------------------------


.. example::
   :title: Getting information on a vector dataset as JSON

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        info_as_dict = gdal.alg.vector.info(input="poly.gpkg").Output()


.. example::
   :title: Converting a shapefile to a GeoPackage

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        gdal.alg.vector.convert(input="in.shp", output="out.gpkg", overwrite=True)


Pipeline examples
-----------------

.. example::
   :title: Perform raster reprojection and gets the result as a streamed dataset.

   .. code-block:: python

        from osgeo import gdal

        gdal.UseExceptions()
        with gdal.alg.pipeline(pipeline="read byte.tif ! reproject --dst-crs EPSG:4326 --resampling cubic") as alg:
            ds = alg.Output()
            # do something with the dataset
