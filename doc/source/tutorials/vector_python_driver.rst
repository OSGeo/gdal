.. _vector_python_driver_tut:

================================================================================
Vector driver in Python implementation tutorial
================================================================================

.. versionadded:: 3.1

.. highlight:: python

Introduction
------------

Since GDAL 3.1, the capability of writing read-only vector drivers in Python
has been added. It is strongly advised to read the :ref:`vector_driver_tut` first,
which will give the general principles of how a vector driver works.

This capability does not require the use of the GDAL/OGR SWIG Python bindings
(but a vector Python driver may use them.)

Note: per project policies, this is considered as an "experimental" feature
and the GDAL project will not accept such Python drivers to be included in
the GDAL repository. Drivers aiming at inclusion in GDAL master should priorly
be ported to C++. The rationale for this is that:

* the correctness of the Python code can mostly be checked at runtime, whereas
  C++ benefits from static analysis (at compile time, and other checkers).
* Python code is executed under the Python Global Interpreter Lock, which makes
  them not scale.
* Not all builds of GDAL have Python available.


Linking mechanism to a Python interpreter
-----------------------------------------

See :ref:`linking_mechanism_to_python_interpreter`

Driver location
---------------

Driver filenames must start with `gdal_` or `ogr_` and have the `.py` extension.
They will be searched in the following directies:

* the directory pointed by the ``GDAL_PYTHON_DRIVER_PATH`` configuration option
  (there may be several paths separated by `:` on Unix or `;` on Windows)
* if not defined, the directory pointed by the ``GDAL_DRIVER_PATH`` configuration
  option.
* if not defined, in the directory (hardcoded at compilation time on Unix builds)
  where native plugins are located.

GDAL does not try to manage Python dependencies that are imported by the driver
.py script. It is up to the user to make sure its current Python environment
has all required dependencies installed.

Import section
--------------

Drivers must have the following import section to load the base classes.

.. code-block::

    from gdal_python_driver import BaseDriver, BaseDataset, BaseLayer

The ``gdal_python_driver`` module is created dynamically by GDAL and is not
present on the file system.

Metadata section
----------------

In the first 1000 lines of the .py file, a number of required and optional
KEY=VALUE driver directives must be defined. They are parsed by C++ code,
without using the Python interpreter, so it is vital to respect the following
constraints:

* each declaration must be on a single line, and start with ``# gdal: DRIVER_``
  (space character between sharp character and gdal, and between colon character and DRIVER\_)
* the value must be a literal value of type string (except for
  # gdal: DRIVER_SUPPORTED_API_VERSION which can accept an array of integers),
  without expressions, function calls, escape sequences, etc.
* strings may be single or double-quoted

The following directives must be declared:

* ``# gdal: DRIVER_NAME`` = "some_name": the short name of the driver
* ``# gdal: DRIVER_SUPPORTED_API_VERSION`` = [1]: the API version(s) supported by
  the driver. Must include 1, which is the only currently supported version in GDAL 3.1
* ``# gdal: DRIVER_DCAP_VECTOR`` = "YES": declares a vector driver
* ``# gdal: DRIVER_DMD_LONGNAME`` = "a longer description of the driver"

Additional directives:

* ``# gdal: DRIVER_DMD_EXTENSIONS`` = "ext1 ext2": list of extension(s) recognized
  by the driver, without the dot, and separated by space
* ``# gdal: DRIVER_DMD_HELPTOPIC`` = "url_to_hep_page"
* ``# gdal: DRIVER_DMD_OPENOPTIONLIST`` = xml_value where xml_value is an OptionOptionList
  specification, like "<OpenOptionList><Option name='OPT1' type='boolean' description='bla' default='NO'/></OpenOptionList>"**
* and all other metadata items found in gdal.h starting with `GDAL_DMD_` (resp. `GDAL_DCAP`) by
  creating an item name which starts with `# gdal: DRIVER_` and the value of the
  `GDAL_DMD_` (resp. `GDAL_DCAP`) metadata item.
  For example ``#define GDAL_DMD_CONNECTION_PREFIX "DMD_CONNECTION_PREFIX"`` becomes ``# gdal: DRIVER_DMD_CONNECTION_PREFIX``


Example:

.. code-block:: 

    # gdal: DRIVER_NAME = "DUMMY"
    # gdal: DRIVER_SUPPORTED_API_VERSION = [1]
    # gdal: DRIVER_DCAP_VECTOR = "YES"
    # gdal: DRIVER_DMD_LONGNAME = "my super plugin"
    # gdal: DRIVER_DMD_EXTENSIONS = "foo bar"
    # gdal: DRIVER_DMD_HELPTOPIC = "http://example.com/my_help.html"

Driver class
------------

The entry point .py script must contains a single class that inherits from
``gdal_python_driver.BaseDriver``.

That class must define the following methods:

.. py:function:: identify(self, filename, first_bytes, open_flags, open_options={})
    :noindex:

    :param str filename: File name, or more generally, connection string.
    :param binary first_bytes: First bytes of the file (if it is a file).
        At least 1024 (if the file has at least 1024 bytes), or more if a native driver in the driver probe sequence has requested more previously.
    :param int open_flags: Open flags. To be ignored for now.
    :param dict open_options: Open options.
    :return: True if the file is recognized by the driver, False if not, or -1
                if that cannot be known from the first bytes.

.. py:function:: open(self, filename, first_bytes, open_flags, open_options={})
    :noindex:

    :param str filename: File name, or more generally, connection string.
    :param binary first_bytes: First bytes of the file (if it is a file).
        At least 1024 (if the file has at least 1024 bytes), or more if a native driver in the driver probe sequence has requested more previously.
    :param int open_flags: Open flags. To be ignored for now.
    :param dict open_options: Open options.
    :return: an object deriving from gdal_python_driver.BaseDataset or None

Example:

.. code-block::

    # Required: class deriving from BaseDriver
    class Driver(BaseDriver):

        def identify(self, filename, first_bytes, open_flags, open_options={}):
            return filename == 'DUMMY:'

        # Required
        def open(self, filename, first_bytes, open_flags, open_options={}):
            if not self.identify(filename, first_bytes, open_flags):
                return None
            return Dataset(filename)


Dataset class
-------------

The Driver.open() method on success should return an object from a class that
inherits from ``gdal_python_driver.BaseDataset``.

Layers
++++++

The role of this object is to store vector layers. There are two implementation
options. If the number of layers is small or they are fast to construct, then
the ``__init__`` method can defined a ``layers`` attribute that is a sequence of
objects from a class that inherits from ``gdal_python_driver.BaseLayer``.

Example:

.. code-block::

    class Dataset(BaseDataset):

        def __init__(self, filename):
            self.layers = [Layer(filename)]

Otherwise, the following two methods should be defined:

.. py:function:: layer_count(self)
    :noindex:

    :return: the number of layers

.. py:function:: layer(self, idx)
    :noindex:

    :param int idx: Index of the layer to return. Normally between 0 and
                    self.layer_count() - 1, but calling code might pass any
                    value. In case of invalid index, None should be returned.
    :return: an object deriving from gdal_python_driver.BaseLayer or None.
                The C++ code will take care of caching that object, and this method
                will only be called once for a given idx value.

Example:

.. code-block::

    class Dataset(BaseDataset):

        def layer_count(self):
            return 1

        def layer(self, idx):
            return [Layer(self.filename)] if idx = 0 else None

Metadata
++++++++

The dataset may define a ``metadata`` dictionary, in ``__init__`` of
key: value of type string, for the default metadata domain.
Alternatively, the following method may be implemented.

.. py:function:: metadata(self, domain)
    :noindex:

    :param str domain: metadata domain. Empty string for the default one
    :return: None, or a dictionary of key:value pairs of type string;

Other methods
+++++++++++++

The following method may be optionally implemented:

.. py:function:: close(self)
    :noindex:

    Called at the destruction of the C++ peer GDALDataset object. Useful
    to close database connections for example.


Layer class
-----------

The Dataset object will instantiate one or several objects from a class that
inherits from ``gdal_python_driver.BaseLayer``.

Metadata, and other definitions
+++++++++++++++++++++++++++++++

The following attributes are required and must defined at __init__ time:

.. py:attribute:: name
    :noindex:

    Layer name, of type string. If not set, a ``name`` method must
    be defined.

.. py:attribute:: fields

    Sequence of field definitions (may be empty).
    Each field is a dictionary with the following properties:

    .. py:attribute:: name
        :noindex:

        Required

    .. py:attribute:: type
        :noindex:

        A integer value of type ogr.OFT\_ (from the SWIG Python bindings), or
        one of the following string values: ``String``, ``Integer``, ``Integer16``, ``Integer64``,
        ``Boolean``, ``Real``, ``Float``, ``Binary``, ``Date``, ``Time``, ``DateTime``

    If that attribute is not set, a ``fields`` method must be defined and
    return such a sequence.

.. py:attribute:: geometry_fields
    :noindex:

    Sequence of geometry field definitions (may be empty).
    Each field is a dictionary with the following properties:

    .. py:attribute:: name
        :noindex:

        Required. May be empty

    .. py:attribute:: type
        :noindex:

        Required. A integer value of type ogr.wkb\_ (from the SWIG Python bindings), or
        one of the following string values: ``Unknown``, ``Point``, ``LineString``,
        ``Polygon``, ``MultiPoint``, ``MultiLineString``, ``MultiPolygon``,
        ``GeometryCollections`` or all other values returned by :cpp:func:`OGRGeometryTypeToName`

    .. py:attribute:: srs
        :noindex:

        The SRS attached to the geometry field as a string that can be ingested by
        :cpp:func:`OGRSpatialReference::SetFromUserInput`, such as a PROJ string,
        WKT string, or AUTHORITY:CODE.

    If that attribute is not set, a ``geometry_fields`` method must be defined and
    return such a sequence.

The following attributes are optional:

.. py:attribute:: fid_name
    :noindex:

    Feature ID column name, of type string. May be empty string. If not set,
    a ``fid_name`` method may be defined.

.. py:attribute:: metadata
    :noindex:

    A dictionary of key: value strings, corresponding to metadata of the default
    metadata domain. Alternatively, a ``metadata`` method that accepts a domain
    argument may be defined.

.. py:attribute:: iterator_honour_attribute_filter
    :noindex:

    Can be set to True if the feature iterator takes into account the
    ``attribute_filter`` attribute that can be set on the layer.

.. py:attribute:: iterator_honour_spatial_filter
    :noindex:

    Can be set to True if the feature iterator takes into account the
    ``spatial_filter`` attribute that can be set on the layer.

.. py:attribute:: feature_count_honour_attribute_filter
    :noindex:

    Can be set to True if the feature_count method takes into account the
    ``attribute_filter`` attribute that can be set on the layer.

.. py:attribute:: feature_count_honour_spatial_filter
    :noindex:

    Can be set to True if the feature_count method takes into account the
    ``spatial_filter`` attribute that can be set on the layer.

Feature iterator
++++++++++++++++

The Layer class must implement the iterator interface, so typically with
a ``__iter__`` method.

The iterator must return a dictionary with the feature content.

Two keys allowed in the returned dictionary are:

.. py:attribute:: id
    :noindex:

    Strongly recommended. The value must be of type int to be recognized as a FID by GDAL

.. py:attribute:: type
    :noindex:

    Required. The value must be the string "OGRFeature"

.. py:attribute:: fields
    :noindex:

    Required. The value must be a dictionary whose keys are field names, or None

.. py:attribute:: geometry_fields
    :noindex:

    Required. the value must be a dictionary whose keys are geometry field names (possibly
    the empty string for unnamed geometry columns), or None.
    The value of each key must be a geometry encoded as WKT, or None.

.. py:attribute:: style
    :noindex:

    Optional. The value must be a string conforming to the :ref:`ogr_feature_style`.

Filtering
+++++++++

By default, any attribute or spatial filter set by the user of the OGR API will
be evaluated by the generic C++ side of the driver, by iterating over all features of the
layer.

If the ``iterator_honour_attribute_filter`` (resp. ``iterator_honour_spatial_filter``)
attribute of the layer object is set to ``True``, the attribute filter (resp.
spatial filter) must be honoured by the feature iterator method.

The attribute filter is set in the ``attribute_filter`` attribute of the
layer object. It is a string conforming to :ref:`OGR SQL <ogr_sql_dialect>`.
When the attribute filter is changed by the OGR API, the ``attribute_filter_changed``
optional method is called (see below paragraph about optional methods).
An implementation of ``attribute_filter_changed`` may decide to fallback on
evaluation by the generic C++ side of the driver by calling the ``SetAttributeFilter``
method (see below passthrough example)

The geometry filter is set in the ``spatial_filter`` attribute of the
layer object. It is a string encoding as ISO WKT. It is the responsibility of
the user of the OGR API to express it in the CRS of the layer.
When the attribute filter is changed by the OGR API, the ``spatial_filter_changed``
optional method is called (see below paragraph about optional methods).
An implementation of ``spatial_filter_changed`` may decide to fallback on
evaluation by the generic C++ side of the driver by calling the ``SetSpatialFilter``
method (see below passthrough example)

Optional methods
++++++++++++++++

The following methods may be optionally implemented:

.. py:function:: extent(self, force_computation)
    :noindex:

    :return: the list [xmin,ymin,xmax,ymax] with the spatial extent of the layer.

.. py:function:: feature_count(self, force_computation)
    :noindex:

    :return: the number of features of the layer.

    If self.feature_count_honour_attribute_filter or self.feature_count_honour_spatial_filter
    are set to True, the attribute filter and/or spatial filter must be honoured
    by this method.

.. py:function:: feature_by_id(self, fid)
    :noindex:

    :param int fid: feature ID
    :return: a feature object in one of the formats of the ``__next__`` method
             described above, or None if no object matches fid

.. py:function:: attribute_filter_changed(self)
    :noindex:

    This method is called whenever self.attribute_filter has been changed.
    It is the opportunity for the driver to potentially change the value of
    self.iterator_honour_attribute_filter or feature_count_honour_attribute_filter
    attributes.

.. py:function:: spatial_filter_changed(self)
    :noindex:

    This method is called whenever self.spatial_filter has been changed (its value
    is a geometry encoded in WKT)
    It is the opportunity for the driver to potentially change the value of
    self.iterator_honour_spatial_filter or feature_count_honour_spatial_filter
    attributes.

.. py:function:: test_capability(self, cap)
    :noindex:

    :param cap string: potential values are BaseLayer.FastGetExtent,
                       BaseLayer.FastSpatialFilter, BaseLayer.FastFeatureCount,
                       BaseLayer.RandomRead, BaseLayer.StringsAsUTF8 or
                       other strings supported by :cpp:func:`OGRLayer::TestCapability`
    :return: True if the capability is supported, False otherwise.

Full example
------------

The following example is a passthrough driver that forwards the calls to the
SWIG Python GDAL API. It has no practical use, and is just intended to show
case most possible uses of the API. A real-world driver will only use part of the
API demonstrated. For example, the passthrough driver implements attribute and
spatial filters in a completely dummy way, by calling back the C++ part of the
driver. The ``iterator_honour_attribute_filter`` and ``iterator_honour_spatial_filter``
attributes, and the ``attribute_filter_changed`` and ``spatial_filter_changed``
method implementations, could have omitted with the same result.

The connection strings recognized by the drivers are
"PASSHTROUGH:connection_string_supported_by_non_python_drivers". Note that
the prefixing by the driver name is absolutely not a requirement, but something
specific to this particular driver which is a bit artificial (without the prefix,
the connection string would go directly to the native driver). The CityJSON
driver mentioned in the :ref:`Other examples <other_examples>` paragraph does
not need it.

.. code-block::

    #!/usr/bin/env python
    # -*- coding: utf-8 -*-
    # This code is in the public domain, so as to serve as a template for
    # real-world plugins.
    # or, at the choice of the licensee,
    # Copyright 2019 Even Rouault
    # SPDX-License-Identifier: MIT

    # gdal: DRIVER_NAME = "PASSTHROUGH"
    # API version(s) supported. Must include 1 currently
    # gdal: DRIVER_SUPPORTED_API_VERSION = [1]
    # gdal: DRIVER_DCAP_VECTOR = "YES"
    # gdal: DRIVER_DMD_LONGNAME = "Passthrough driver"
    # gdal: DRIVER_DMD_CONNECTION_PREFIX = "PASSTHROUGH:"

    from osgeo import gdal, ogr

    from gdal_python_driver import BaseDriver, BaseDataset, BaseLayer

    class Layer(BaseLayer):

        def __init__(self, gdal_layer):
            self.gdal_layer = gdal_layer
            self.name = gdal_layer.GetName()
            self.fid_name = gdal_layer.GetFIDColumn()
            self.metadata = gdal_layer.GetMetadata_Dict()
            self.iterator_honour_attribute_filter = True
            self.iterator_honour_spatial_filter = True
            self.feature_count_honour_attribute_filter = True
            self.feature_count_honour_spatial_filter = True

        def fields(self):
            res = []
            layer_defn = self.gdal_layer.GetLayerDefn()
            for i in range(layer_defn.GetFieldCount()):
                ogr_field_def = layer_defn.GetFieldDefn(i)
                field_def = {"name": ogr_field_def.GetName(),
                             "type": ogr_field_def.GetType()}
                res.append(field_def)
            return res

        def geometry_fields(self):
            res = []
            layer_defn = self.gdal_layer.GetLayerDefn()
            for i in range(layer_defn.GetGeomFieldCount()):
                ogr_field_def = layer_defn.GetGeomFieldDefn(i)
                field_def = {"name": ogr_field_def.GetName(),
                             "type": ogr_field_def.GetType()}
                srs = ogr_field_def.GetSpatialRef()
                if srs:
                    field_def["srs"] = srs.ExportToWkt()
                res.append(field_def)
            return res

        def test_capability(self, cap):
            if cap in (BaseLayer.FastGetExtent, BaseLayer.StringsAsUTF8,
                    BaseLayer.RandomRead, BaseLayer.FastFeatureCount):
                return self.gdal_layer.TestCapability(cap)
            return False

        def extent(self, force_computation):
            # Impedance mismatch between SWIG GetExtent() and the Python
            # driver API
            minx, maxx, miny, maxy = self.gdal_layer.GetExtent(force_computation)
            return [minx, miny, maxx, maxy]

        def feature_count(self, force_computation):
            # Dummy implementation: we call back the generic C++ implementation
            return self.gdal_layer.GetFeatureCount(True)

        def attribute_filter_changed(self):
            # Dummy implementation: we call back the generic C++ implementation
            if self.attribute_filter:
                self.gdal_layer.SetAttributeFilter(str(self.attribute_filter))
            else:
                self.gdal_layer.SetAttributeFilter(None)

        def spatial_filter_changed(self):
            # Dummy implementation: we call back the generic C++ implementation
            # the 'inf' test is just for a test_ogrsf oddity
            if self.spatial_filter and 'inf' not in self.spatial_filter:
                self.gdal_layer.SetSpatialFilter(
                    ogr.CreateGeometryFromWkt(self.spatial_filter))
            else:
                self.gdal_layer.SetSpatialFilter(None)

        def _translate_feature(self, ogr_f):
            fields = {}
            layer_defn = ogr_f.GetDefnRef()
            for i in range(ogr_f.GetFieldCount()):
                if ogr_f.IsFieldSet(i):
                    fields[layer_defn.GetFieldDefn(i).GetName()] = ogr_f.GetField(i)
            geom_fields = {}
            for i in range(ogr_f.GetGeomFieldCount()):
                g = ogr_f.GetGeomFieldRef(i)
                if g:
                    geom_fields[layer_defn.GetGeomFieldDefn(
                        i).GetName()] = g.ExportToIsoWkt()
            return {'id': ogr_f.GetFID(),
                    'type': 'OGRFeature',
                    'style': ogr_f.GetStyleString(),
                    'fields': fields,
                    'geometry_fields': geom_fields}

        def __iter__(self):
            for f in self.gdal_layer:
                yield self._translate_feature(f)

        def feature_by_id(self, fid):
            ogr_f = self.gdal_layer.GetFeature(fid)
            if not ogr_f:
                return None
            return self._translate_feature(ogr_f)

    class Dataset(BaseDataset):

        def __init__(self, gdal_ds):
            self.gdal_ds = gdal_ds
            self.layers = [Layer(gdal_ds.GetLayer(idx))
                        for idx in range(gdal_ds.GetLayerCount())]
            self.metadata = gdal_ds.GetMetadata_Dict()

        def close(self):
            del self.gdal_ds
            self.gdal_ds = None


    class Driver(BaseDriver):

        def _identify(self, filename):
            prefix = 'PASSTHROUGH:'
            if not filename.startswith(prefix):
                return None
            return gdal.OpenEx(filename[len(prefix):], gdal.OF_VECTOR)

        def identify(self, filename, first_bytes, open_flags, open_options={}):
            return self._identify(filename) is not None

        def open(self, filename, first_bytes, open_flags, open_options={}):
            gdal_ds = self._identify(filename)
            if not gdal_ds:
                return None
            return Dataset(gdal_ds)

.. _other_examples:

Other examples
--------------

Other examples, including a CityJSON driver, may be found at
https://github.com/OSGeo/gdal/tree/master/examples/pydrivers
