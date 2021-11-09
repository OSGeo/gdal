.. _rfc-76:

================================================================================
RFC 76: OGR Python drivers
================================================================================

============== ============================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2019-Nov-5
Last updated:  2019-Nov-15
Status:        Adopted, implemented in GDAL 3.1
============== ============================

Summary
-------

This RFC adds the capability to write OGR/vector drivers in Python.

Motivation
----------

For some use cases that do not require lighting speed, or to deal with very
niche formats (possibly in house format), it might be faster and more efficient
to write a vector driver in Python rather than a GDAL C++ driver as currently required,
or an ad-hoc converter.

.. note::

    QGIS has now a way to create Python-based providers such as
    in https://github.com/qgis/QGIS/blob/master/tests/src/python/provider_python.py
    Having a way to do in GDAL itself also allows the rest of GDAL/OGR based
    tools to use the OGR Python driver.

How does that work ?
--------------------

Driver registration
+++++++++++++++++++

The driver registration mechanism is extended to look for .py scripts in a
dedicated directory:

* the directory pointed by the ``GDAL_PYTHON_DRIVER_PATH`` configuration option
  (there may be several paths separated by `:` on Unix or `;` on Windows)
* if not defined, the directory pointed by the ``GDAL_DRIVER_PATH`` configuration
  option.
* if not defined, in the directory (hardcoded at compilation time on Unix builds)
  where native plugins are located.

Those Python script must set in their first lines at least 2 directives:

- ``# gdal: DRIVER_NAME = "short_name"``
- ``# gdal: DRIVER_SUPPORTED_API_VERSION = 1`` . Currently only 1 supported. If the
  interface changed in a backward incompatible way, we would increment internally
  the supported API version number. This item enables us to check if we are able
  to "safely" load a Python driver. If a Python driver would support several API
  versions (not clear if that's really possible at that point), it might use an
  array syntax to indicate that, like ``[1,2]``
- ``# gdal: DRIVER_DCAP_VECTOR = "YES"``
- ``# gdal: DRIVER_DMD_LONGNAME = "my super plugin"``

Optional metadata such as ``# gdal: DRIVER_DMD_EXTENSIONS`` or
``# gdal: DRIVER_DMD_HELPTOPIC`` can be defined (basically, any driver metadata key string prefixed by
``# gdal: DRIVER_``

These directives will be parsed in a pure textual way, without invocation of the Python
interpreter, both for efficiency consideration and also because we want to
delay the research or launch of the Python interpreter as much as possible
(the typical use case if GDAL used by QGIS: we want to make sure that QGIS
has itself started Python, to reuse that Python interpreter)

From the short metadata, the driver registration code can instantiate GDALDriver
C++ objects. When the Identify() or Open() method is invoked on that object,
the C++ code will:

* if not already done, find Python symbols, or start Python (see below paragraph
  for more details)
* if not already done, load the .py file as a Python module
* if not already done, instantiate an instance of the Python class of the module
  deriving from ``gdal_python_driver.BaseDriver``
* call the  ``identify`` and ``open`` method depending on the originated API call.

The ``open`` method will return a Python ``BaseDataset`` object with required and
optional methods that will be invoked by the corresponding GDAL API calls. And
likewise for the ``BaseLayer`` object. See the example_.

Connection with the Python interpreter
++++++++++++++++++++++++++++++++++++++

The logic will be shared with the VRT pixel functions written in Python functionality
It relies on runtime linking to the Python symbols already available in the process (for
example the python executable or a binary embedding Python and using GDAL, such
as QGIS), or loading of the Python library in case no Python symbols are found,
rather than compile time linking.
The reason is that we do not know in advance with which Python version GDAL can
potentially be linked, and we do not want gdal.so/gdal.dll to be explicitly linked
with a particular Python library.

This is both embedding and extending Python.

The steps are:

1. through dlopen() + dlsym() on Unix and EnumProcessModules()+GetProcAddress()
   on Windows, look for Python symbols. If found, use it. This is for example
   the case if GDAL is used from a Python module (GDAL Python bindings, rasterio, etc.)
   or an application like QGIS that starts a Python interpreter.
2. otherwise, look for the PYTHONSO environment variable that should point to
   a pythonX.Y[...].so/.dll
3. otherwise, look for the python binary in the path and try to identify the
   correspond Python .so/.dll
4. otherwise, try to load with dlopen()/LoadLibrary() well-known names of
   Python .so/.dll

Impacts on GDAL core
--------------------

They are minimal. The GDALAllRegister() method has an added call to
GDALDriverManager::AutoLoadPythonDrivers() that implements the above mentioned
logic. The GDALDriver class has been extended to support a new function
pointer, IdentifyEx(), which is used by the C++ shim that loads the Python code.

.. code-block:: c++

    int                 (*pfnIdentifyEx)( GDALDriver*, GDALOpenInfo * );

This extended IdentifyEx() function pointer, which adds the GDALDriver* argument,
is used in priority by GDALIdentify() and GDALOpen() methods. The need for that
is purely boring. For normal C++ drivers, there is no need to pass the driver,
as there is a one-to-one correspondence between a driver and the function that
implements the driver. But for the Python driver, there is a single C++ method
that does the interface with the Python Identify() method of several Python drivers,
hence the need of a GDALDriver* argument to forward the call to the appropriate
driver.

.. _example:

Example of such a driver
------------------------

Note that the prefixing by the driver name in the connection string is absolutely
not a requirement, but something specific to this particular driver which is a
bit artificial. The CityJSON driver mentioned below does not need it.

.. code-block:: python

    #!/usr/bin/env python
    # -*- coding: utf-8 -*-
    # This code is in the public domain, so as to serve as a template for
    # real-world plugins.
    # or, at the choice of the licensee,
    # Copyright 2019 Even Rouault
    # SPDX-License-Identifier: MIT

    # Metadata parsed by GDAL C++ code at driver pre-loading, starting with '# gdal: '
    # Required and with that exact syntax since it is parsed by non-Python
    # aware code. So just literal values, no expressions, etc.
    # gdal: DRIVER_NAME = "DUMMY"
    # API version(s) supported. Must include 1 currently
    # gdal: DRIVER_SUPPORTED_API_VERSION = [1]
    # gdal: DRIVER_DCAP_VECTOR = "YES"
    # gdal: DRIVER_DMD_LONGNAME = "my super plugin"

    # Optional driver metadata items.
    # # gdal: DRIVER_DMD_EXTENSIONS = "ext1 est2"
    # # gdal: DRIVER_DMD_HELPTOPIC = "http://example.com/my_help.html"

    # The gdal_python_driver module is defined by the GDAL library at runtime
    from gdal_python_driver import BaseDriver, BaseDataset, BaseLayer

    class Layer(BaseLayer):
        def __init__(self):

            # Reserved attribute names. Either those or the corresponding method
            # must be defined
            self.name = 'my_layer'  # Required, or name() method

            self.fid_name = 'my_fid'  # Optional

            self.fields = [{'name': 'boolField', 'type': 'Boolean'},
                        {'name': 'int16Field', 'type': 'Integer16'},
                        {'name': 'int32Field', 'type': 'Integer'},
                        {'name': 'int64Field', 'type': 'Integer64'},
                        {'name': 'realField', 'type': 'Real'},
                        {'name': 'floatField', 'type': 'Float'},
                        {'name': 'strField', 'type': 'String'},
                        {'name': 'strNullField', 'type': 'String'},
                        {'name': 'strUnsetField', 'type': 'String'},
                        {'name': 'binaryField', 'type': 'Binary'},
                        {'name': 'timeField', 'type': 'Time'},
                        {'name': 'dateField', 'type': 'Date'},
                        {'name': 'datetimeField', 'type': 'DateTime'}]  # Required, or fields() method

            self.geometry_fields = [{'name': 'geomField',
                                    'type': 'Point',  # optional
                                    'srs': 'EPSG:4326'  # optional
                                    }]  # Required, or geometry_fields() method

            self.metadata = {'foo': 'bar'}  # optional

            # uncomment if __iter__() honour self.attribute_filter
            #self.iterator_honour_attribute_filter = True

            # uncomment if __iter__() honour self.spatial_filter
            #self.iterator_honour_spatial_filter = True

            # uncomment if feature_count() honour self.attribute_filter
            #self.feature_count_honour_attribute_filter = True

            # uncomment if feature_count() honour self.spatial_filter
            #self.feature_count_honour_spatial_filter = True

            # End of reserved attribute names

            self.count = 5

        # Required, unless self.name attribute is defined
        # def name(self):
        #    return 'my_layer'

        # Optional. If not defined, fid name is 'fid'
        # def fid_name(self):
        #    return 'my_fid'

        # Required, unless self.geometry_fields attribute is defined
        # def geometry_fields(self):
        #    return [...]

        # Required, unless self.required attribute is defined
        # def fields(self):
        #    return [...]

        # Optional. Only to be usd if self.metadata field is not defined
        # def metadata(self, domain):
        #    if domain is None:
        #        return {'foo': 'bar'}
        #    return None

        # Optional. Called when self.attribute_filter is changed by GDAL
        # def attribute_filter_changed(self):
        #     # You may change self.iterator_honour_attribute_filter
        #     # or feature_count_honour_attribute_filter
        #     pass

        # Optional. Called when self.spatial_filter is changed by GDAL
        # def spatial_filter_changed(self):
        #     # You may change self.iterator_honour_spatial_filter
        #     # or feature_count_honour_spatial_filter
        #     pass

        # Optional
        def test_capability(self, cap):
            if cap == BaseLayer.FastGetExtent:
                return True
            if cap == BaseLayer.StringsAsUTF8:
                return True
            # if cap == BaseLayer.FastSpatialFilter:
            #    return False
            # if cap == BaseLayer.RandomRead:
            #    return False
            if cap == BaseLayer.FastFeatureCount:
                return self.attribute_filter is None and self.spatial_filter is None
            return False

        # Optional
        def extent(self, force_computation):
            return [2.1, 49, 3, 50]  # minx, miny, maxx, maxy

        # Optional.
        def feature_count(self, force_computation):
            # As we did not declare feature_count_honour_attribute_filter and
            # feature_count_honour_spatial_filter, the below case cannot happen
            # But this is to illustrate that you can callback the default implementation
            # if needed
            # if self.attribute_filter is not None or \
            #   self.spatial_filter is not None:
            #    return super(Layer, self).feature_count(force_computation)

            return self.count

        # Required. You do not need to handle the case of simultaneous iterators on
        # the same Layer object.
        def __iter__(self):
            for i in range(self.count):
                properties = {
                    'boolField': True,
                    'int16Field': 32767,
                    'int32Field': i + 2,
                    'int64Field': 1234567890123,
                    'realField': 1.23,
                    'floatField': 1.2,
                    'strField': 'foo',
                    'strNullField': None,
                    'binaryField': b'\x01\x00\x02',
                    'timeField': '12:34:56.789',
                    'dateField': '2017-04-26',
                    'datetimeField': '2017-04-26T12:34:56.789Z'}

                yield {"type": "OGRFeature",
                    "id": i + 1,
                    "fields": properties,
                    "geometry_fields": {"geomField": "POINT(2 49)"},
                    "style": "SYMBOL(a:0)" if i % 2 == 0 else None,
                    }

        # Optional
        # def feature_by_id(self, fid):
        #    return {}


    class Dataset(BaseDataset):

        # Optional, but implementations will generally need it
        def __init__(self, filename):
            # If the layers member is set, layer_count() and layer() will not be used
            self.layers = [Layer()]
            self.metadata = {'foo': 'bar'}

        # Optional, called on native object destruction
        def __del__(self):
            pass

        # Optional. Only to be usd if self.metadata field is not defined
        # def metadata(self, domain):
        #    if domain is None:
        #        return {'foo': 'bar'}
        #    return None

        # Required, unless a layers attribute is set in __init__
        # def layer_count(self):
        #    return len(self.layers)

        # Required, unless a layers attribute is set in __init__
        # def layer(self, idx):
        #    return self.layers[idx]


    # Required: class deriving from BaseDriver
    class Driver(BaseDriver):

        # Optional. Called the first time the driver is loaded
        def __init__(self):
            pass

        # Required
        def identify(self, filename, first_bytes, open_flags, open_options={}):
            return filename == 'DUMMY:'

        # Required
        def open(self, filename, first_bytes, open_flags, open_options={}):
            if not self.identify(filename, first_bytes, open_flags):
                return None
            return Dataset(filename)


Other examples:

* a PASSTHROUGH driver that forwards calls to the GDAL SWIG Python API:
  https://github.com/OSGeo/gdal/blob/master/examples/pydrivers/ogr_PASSTHROUGH.py
* a driver implemented a simple parsing of `CityJSON <https://www.cityjson.org/>`_:
  https://github.com/OSGeo/gdal/blob/master/examples/pydrivers/ogr_CityJSON.py

Limitations and scope
---------------------

- Vector and read-only for now. This could later be extended of course.

- No connection between the Python code of the plugin and the OGR Python API 
  that is built on top of SWIG. This does not appear to be doable in a 
  reasonable way. Nothing prevents people from using the GDAL/OGR/OSR Python 
  API but the objects exchanged between the OGR core and the Python code will
  not be OGR Python SWIG objects. A 
  typical example is that a plugin will return its CRS as a string (WKT, PROJSON,
  or deprecated PROJ.4 string), but not as a osgeo.osr.SpatialReference object.
  But it is possible to use the osgeo.osr.SpatialReference API to generate this
  WKT string.

- This RFC does not try to cover the management of Python dependencies. It is
  up to the user to do the needed "pip install" or whatever Python package
  management solution it uses.

- The Python "Global Interpreter Lock" is held in the Python drivers, as required
  for safe use of Python. Consequently scaling of such drivers is limited.

- Given the above restrictions, this will remain an "experimental" feature 
  and the GDAL project will not accept such Python drivers to be included in
  the GDAL repository. This is similar to the situation of the QGIS project
  that allows Python plugins outside of the main QGIS repository. If a QGIS plugin
  want to be moved into the main repository, it has to be converted to C++.
  The rationale for this is that the correctness of the Python code can mostly be
  checked at runtime, whereas C++ benefits from static analysis (at compile time,
  and other checkers). In the context of GDAL, this rationale also applies. GDAL
  drivers are also stress-tested by the OSS Fuzz infrastructure, and that requires
  them to be written in C++.

- The interface between the C++ and Python code might break between GDAL feature
  releases. In that case we will increment the expected API version number to
  avoid loading incompatible Python drivers. We will likely not make any effort
  to be able to deal with plugins of incompatible (previous) API version.


SWIG binding changes
--------------------

None

Security implications
---------------------

Similar to the existing native code plugin mechanism of GDAL. If the user
defines the GDAL_PYTHON_DRIVER_PATH environment variable or GDAL_DRIVER_PATH,
annd put .py scripts in them (or in {prefix}/lib/gdalplugins/python as a fallback),
they will be executed.

However, opening a .py file with GDALOpen() or similar mechanisms will not
lead to its execution, so this is safe for normal GDAL usage.

The GDAL_NO_AUTOLOAD compile time #define, already used to disable loading 
of native plugins, is also honoured to disable the loading of Python plugins.

Performance impact
------------------

If no .py script exists in the researched location, the performance impact on
GDALAllRegister() should be within the noise.

Backward compatibility
----------------------

No backward incompatibility. Only functionality addition.

Documentation
-------------

A tutorial will be added to explain how to write such a Python driver:
https://github.com/rouault/gdal/blob/pythondrivers/gdal/doc/source/tutorials/vector_python_driver.rst

Testing
-------

The gdalautotest suite will be extended with the above test Python driver, and
a few error cases:
https://github.com/rouault/gdal/blob/pythondrivers/autotest/ogr/ogr_pythondrivers.py

Previous discussions
--------------------

This topic has been discussed in the past in :

- https://lists.osgeo.org/pipermail/gdal-dev/2017-April/thread.html#46526
- https://lists.osgeo.org/pipermail/gdal-dev/2018-November/thread.html#49294

Implementation
--------------

A candidate implementation is available at in
https://github.com/rouault/gdal/tree/pythondrivers

https://github.com/OSGeo/gdal/compare/master...rouault:pythondrivers

Voting history
--------------

* +1 from EvenR, JukkaR, MateuzL, DanielM
* -0 from SeanG
* +0 from HowardB

Credits
-------

Sponsored by OpenGeoGroep
