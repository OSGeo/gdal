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

try:
    # The gdal_python_driver module is defined by the GDAL library at runtime
    from gdal_python_driver import BaseDriver, BaseDataset, BaseLayer
except ImportError:
    # To be able to run in standalone mode
    class BaseDriver(object):
        pass

    class BaseDataset(object):
        pass

    class BaseLayer(object):
        pass


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
    def close(self):
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
