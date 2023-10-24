.. _python_gotchas:

================================================================================
Python Gotchas in the GDAL and OGR Python Bindings
================================================================================

This page lists aspects of GDAL's and OGR's Python bindings that may catch Python programmers by surprise.
If you find something new, feel free to open a pull request adding it to the list. Consider discussing it on the `gdal-dev mailing list <https://lists.osgeo.org/mailman/listinfo/gdal-dev>`__  first,
to make sure you fully understand the issue and that others agree that it is unexpected, "non-Pythonic",
or something that would catch many Python programmers by surprise.
Be sure to reference email threads, GitHub tickets, and other sources of additional information.

This list is not the place to report bugs. If you believe something is a bug, please `open a ticket <https://github.com/OSGeo/gdal/issues>`__ and report the problem to gdal-dev.
Then consider listing it here if it is something related to Python specifically. Do not list it here if it relates to GDAL or OGR generally, and not the Python bindings specifically.

Not all items listed here are bugs. Some of these are just how GDAL and OGR work and cannot be fixed easily without breaking existing code.
If you don't like how something works and think it should be changed, feel free to discuss it on gdal-dev and see what can be done.


Gotchas that are by design... or per history
--------------------------------------------

These are unexpected behaviors that are not considered by the GDAL and OGR teams to be bugs and are unlikely to be changed due to effort required, or whose fixing might affect backward compatibility, etc.


Python bindings do not raise exceptions unless you explicitly call ``UseExceptions()``
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

By default, the GDAL and OGR Python bindings do not raise exceptions when errors occur.
Instead they return an error value such as ``None`` and write an error message to ``sys.stdout``. For example, when you try to open a non-existing dataset with GDAL:

.. code-block::

    >>> from osgeo import gdal
    >>> gdal.Open('C:\\foo.img')
    ERROR 4: 'C:\foo.img does not exist in the file system,
    and is not recognized as a supported dataset name.

    >>>

In Python, it is traditional to report errors by raising exceptions. You can enable this behavior in GDAL and OGR by calling the ``UseExceptions()`` function:

.. code-block::

   >>> from osgeo import gdal
   >>> gdal.UseExceptions()    # Enable exceptions
   >>> gdal.open('C:\\foo.img')
   Traceback (most recent call last):
     File "<stdin>", line 1, in <module>
   RuntimeError: 'C:\foo.img' does not exist in the file system,
   and is not recognized as a supported dataset name.

   >>>


.. warning::

   It is planned that exceptions will be enabled by default in GDAL 4.0. Code that does not want exceptions to be raised in a future version of GDAL should explicitly disable them with ``gdal.DontUseExceptions()``.


Python crashes or throws an exception if you use an object after deleting a related object
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Consider this example:

.. code-block::

   >>> from osgeo import gdal
   >>> dataset = gdal.Open('C:\\RandomData.img')
   >>> band = dataset.GetRasterBand(1)
   >>> print(band.Checksum())
   31212

In this example, ``band`` has a relationship with ``dataset`` that requires ``dataset`` to remain allocated in order for ``band`` to work.
If we delete ``dataset`` and then try to use ``band``, Python will throw a confusing exception:

.. code-block::

   >>> from osgeo import gdal
   >>> dataset = gdal.Open('C:\\RandomData.img')
   >>> band = dataset.GetRasterBand(1)
   >>> del dataset           # This will cause the Python garbage collector to deallocate dataset
   >>> band.Checksum()       # This will throw an exception because the band's dataset is gone
   # TypeError: in method 'Band_Checksum', argument 1 of type 'GDALRasterBandShadow *'

.. note::

   In GDAL 3.7 and earlier, using a band after the dataset has been destroyed will cause a crash instead of an exception.

This problem can manifest itself in subtle ways. For example, it can occur if you try to instantiate a temporary dataset instance within a single line of code:

.. code-block::

   >>> from osgeo import gdal
   >>> print(gdal.Open('C:\\RandomData.img').GetRasterBand(1).Checksum())
   # TypeError: in method 'Band_Checksum', argument 1 of type 'GDALRasterBandShadow *'

In this example, the dataset instance was no longer needed after the call to ``GetRasterBand()`` so Python deallocated it *before* calling ``Checksum()``.

.. code-block::

   >>> from osgeo import gdal
   >>>
   >>> def load_band(data_filename):
   >>>     dataset = gdal.Open(data_filename)
   >>>     return dataset.GetRasterBand(1)
   >>>
   >>> band = load_band('c:\\RandomData.img')
   >>> print(band.Checksum())
   # TypeError: in method 'Band_Checksum', argument 1 of type 'GDALRasterBandShadow *'

This example is the same case as above but it looks different. The dataset
object is only available in the ``load_band`` function and will be deleted
right after leaving the function.

The problem is not restricted to GDAL band and dataset objects and happens in
other areas where objects have relationships with each other. The issue occurs
because deleting an object in Python causes not only the C++ object behind it
to be deallocated, but also other objects for which that C++ object maintains
ownership (e.g., a Dataset owning a Band, a Feature owning a Geometry.) If the
Python object associated with one of these child objects retains a reference to
that object, Python will crash when the object is accessed. In common cases
such as the Band/Dataset relationship above, the GDAL bindings invalidate
references to objects that no longer exist so that an exception is thrown
instead of a crash, but the work is not complete.

Unfortunately there is no complete list of such relationships, so you have to watch for it yourself.

Python crashes if you add a new field to an OGR layer when features deriving from this layer definition are still active
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

For example:

.. code-block::

   >>> feature = lyr.GetNextFeature()
   >>> field_defn = ogr.FieldDefn("foo", ogr.OFTString)
   >>> lyr.CreateField(field_defn)                       # now, existing features deriving from this layer are invalid
   >>> feature.DumpReadable()                            # segfault
   < Python crashes >

For more information, please see `#3552 <https://trac.osgeo.org/gdal/ticket/3552>`__.

Layers with attribute filters (``SetAttributeFilter()``) will only return filtered features when using ``GetNextFeature()``
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

If you read the documentation for ``SetAttributeFilter()`` carefully you will see the caveat about ``OGR_L_GetNextFeature()``.
This means that if you use ``GetFeature()``, instead of ``GetNextFeature()``, then you can still access and work with features from the layer that are not covered by the filter.
``GetFeatureCount()`` will respect the filter and show the correct number of features filtered. However, working with ``GetFeatureCount()`` in a loop can lead to some subtle confusion.
Iterating over the Layer object or using ``GetNextFeature()`` should be the default method for accessing features:

.. code-block::

   >>> lyr = inDataSource.GetLayer()
   >>> lyr.SetAttributeFilter("PIN = '0000200001'")      # this is a unique filter for only one record
   >>> for i in range( 0, lyr.GetFeatureCount() ):
   ...    feat = lyr.GetFeature( i )
   ...    print(feat)                                    # this will print one feat, but it's the first feat in the Layer and not the filtered feat
   ...

Certain objects contain a ``Destroy()`` method, but you should never use it
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

You may come across examples that call the ``Destroy()`` method. `This tutorial <https://www.gis.usu.edu/~chrisg/python/2009/lectures/ospy_slides2.pdf>`__ even gives specific advice on page 12 about when to call ``Destroy``.

Calling ``Destroy`` forces the underlying native object to be destroyed.  This
is typically unnecessary because these objects are automatically destroyed
during garbage collection when no references to the Python object remain.

In most situations, it is not necessary to force the object to be destroyed
at a specific point in time. However, because the contents of ``gdal.Dataset`` and
``ogr.DataSource`` objects are only guaranteed to be written to disk when
the backing native object is destroyed, it may be necessary to explicitly destroy
these objects. In these cases, a context manager (``with`` block) is often a
good solution, e.g.:

.. code-block::

   from osgeo import ogr
   with ogr.GetDriverByName("ESRI Shapefile").CreateDataSource("/tmp/test.shp") as ds:
       lyr = ds.CreateLayer("test")
       feat = ogr.Feature(lyr.GetLayerDefn())
       feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)')
       lyr.CreateFeature(feat)
   # contents of ds are written to disk

If this is not possible, for example if the object needs to be destroyed within a
function, then the ``Close()`` method may be called.

.. note::

   Context managers and the ``Close()`` method are available beginning in GDAL 3.8.
   In earlier versions, ``Destroy()`` can be used for ``ogr.DataSource`` objects,
   or garbage collection may be forced by destroying reference using ``del`` or setting
   variables to ``None``.

With some drivers, raster datasets can be intermittently saved without closing
using ``FlushCache()``. Similarly, vector datasets can be saved using
``SyncToDisk()``.  However, neither of these methods guarantee that the data
are written to disk, so the preferred method is to use a context manager
or call ``Close()``.


Exceptions raised in custom error handlers do not get caught
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

The python bindings allow you to specify a python callable as an error handler (`#4993 <https://trac.osgeo.org/gdal/ticket/4993>`__).
However, these error handlers appear to be called in a separate thread and any exceptions raised do not propagate back to the main thread (`#5186 <https://trac.osgeo.org/gdal/ticket/5186>`__).

So if you want to  `catch warnings as well as errors <https://gis.stackexchange.com/questions/43404/how-to-detect-a-gdal-ogr-warning/68042>`__, something like this won't work:

.. code-block::

    from osgeo import gdal

    def error_handler(err_level, err_no, err_msg):
        if err_level >= gdal.CE_Warning:
            raise RuntimeError(err_level, err_no, err_msg)  # this exception does not propagate back to main thread!

    if __name__ == '__main__':
        # Test custom error handler
        gdal.PushErrorHandler(error_handler)
        gdal.Error(gdal.CE_Warning, 2, 'test warning message')
        gdal.PopErrorHandler()



But you can do something like this instead:


.. code-block::

    from osgeo import gdal

    class GdalErrorHandler(object):
        def __init__(self):
            self.err_level = gdal.CE_None
            self.err_no = 0
            self.err_msg = ''

        def handler(self, err_level, err_no, err_msg):
            self.err_level = err_level
            self.err_no = err_no
            self.err_msg = err_msg

    if __name__ == '__main__':
        err = GdalErrorHandler()
        gdal.PushErrorHandler(err.handler)
        gdal.UseExceptions()  # Exceptions will get raised on anything >= gdal.CE_Failure

        assert err.err_level == gdal.CE_None, 'the error level starts at 0'

        try:
            # Demonstrate handling of a warning message
            try:
                gdal.Error(gdal.CE_Warning, 8675309, 'Test warning message')
            except Exception:
                raise AssertionError('Operation raised an exception, this should not happen')
            else:
                assert err.err_level == gdal.CE_Warning, (
                    'The handler error level should now be at warning')
                print('Handled error: level={}, no={}, msg={}'.format(
                    err.err_level, err.err_no, err.err_msg))

            # Demonstrate handling of an error message
            try:
                gdal.Error(gdal.CE_Failure, 42, 'Test error message')
            except Exception as e:
                assert err.err_level == gdal.CE_Failure, (
                    'The handler error level should now be at failure')
                assert err.err_msg == e.args[0], 'raised exception should contain the message'
                print('Handled warning: level={}, no={}, msg={}'.format(
                    err.err_level, err.err_no, err.err_msg))
            else:
                raise AssertionError('Error message was not raised, this should not happen')

        finally:
            gdal.PopErrorHandler()



Gotchas that result from bugs or behaviors of other software
------------------------------------------------------------

Python crashes in GDAL functions when you upgrade or downgrade numpy
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Much of GDAL's Python bindings are implemented in C++. Much of the core of numpy is implemented in C. The C++ part of GDAL's Python bindings interacts with the C part of numpy through numpy's ABI (application binary interface).
This requires GDAL's Python bindings to be compiled using numpy header files that define numpy C data structures. Those data structures sometimes change between numpy versions. When this happens, the new version of numpy is not be compatible at the binary level with the old version, and the GDAL Python bindings must be recompiled before they will work with the new version of numpy.
And when they are recompiled, they probably won't work with the old version.

If you obtained a precompiled version of GDAL's Python bindings, such as the Windows packages from `http://gisinternals.com/sdk.php <http://gisinternals.com/sdk.php>`__ be sure you look up what version of numpy was used to compile them, and install that version of numpy on your machine.

Python bindings cannot be used successfully from ArcGIS in-process geoprocessing tools (ArcGIS 9.3 and later)
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

ArcGIS allows the creation of custom, Python-based geoprocessing tools. Until ArcGIS 10, there was no easy way to read raster data into memory. GDAL provides such a mechanism.

Starting with ArcGIS 9.3, geoprocessing tools can either run in the ArcGIS process itself (ArcCatalog.exe or ArcMap.exe) or run in a separate python.exe worker process. Unfortunately ArcGIS contains a bug in how it runs in-process tools. Thus, if you use GDAL from an in-process tool, it will run fine the first time but after that it may fail with ``TypeError`` exceptions until you restart the ArcGIS process. For example, band.ReadAsArray() fails with:

``TypeError: in method 'BandRasterIONumpy', argument 1 of type 'GDALRasterBandShadow *``'

This is a bug in ArcGIS. Please see `#3672 <https://trac.osgeo.org/gdal/ticket/3672>`__ for complete details and advice on workarounds.
