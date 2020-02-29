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
# gdal: DRIVER_NAME = "PASSTHROUGH"
# API version(s) supported. Must include 1 currently
# gdal: DRIVER_SUPPORTED_API_VERSION = [1]
# gdal: DRIVER_DCAP_VECTOR = "YES"
# gdal: DRIVER_DMD_LONGNAME = "Passthrough driver"
# gdal: DRIVER_DMD_CONNECTION_PREFIX = "PASSTHROUGH:"

from osgeo import gdal, ogr

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
        RandomRead = 'RandomRead'
        FastSpatialFilter = 'FastSpatialFilter'
        FastFeatureCount = 'FastFeatureCount'
        FastGetExtent = 'FastGetExtent'
        StringsAsUTF8 = 'StringsAsUTF8'
        pass


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
        return self.gdal_layer.GetFeatureCount(True)

    def attribute_filter_changed(self):
        if self.attribute_filter:
            self.gdal_layer.SetAttributeFilter(str(self.attribute_filter))
        else:
            self.gdal_layer.SetAttributeFilter(None)

    def spatial_filter_changed(self):
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
                fields[layer_defn.GetFieldDefn(
                    i).GetName()] = ogr_f.GetField(i)
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

    # Required
    def identify(self, filename, first_bytes, open_flags, open_options={}):
        return self._identify(filename) is not None

    # Required
    def open(self, filename, first_bytes, open_flags, open_options={}):
        gdal_ds = self._identify(filename)
        if not gdal_ds:
            return None
        return Dataset(gdal_ds)


# Test as standalone
if __name__ == '__main__':
    import sys
    drv = Driver()
    assert drv.identify(sys.argv[1], None, 0)
    ds = drv.open(sys.argv[1], None, 0)
    for l in ds.layers:
        l.geometry_fields()
        l.fields()
        l.test_capability(BaseLayer.FastGetExtent)
        l.extent(True)
        l.feature_count(True)
        for f in l:
            print(f)
