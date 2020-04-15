#!/usr/bin/env python
###############################################################################
#
# Purpose:  CityJSON OGR driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even dot rouault at spatialys.com>
# SPDX-License-Identifier: MIT
###############################################################################

# Metadata parsed by GDAL C++ code at driver pre-loading, starting with '# gdal: '
# gdal: DRIVER_NAME = "CityJSON"
# gdal: DRIVER_SUPPORTED_API_VERSION = [1]
# gdal: DRIVER_DCAP_VECTOR = "YES"
# gdal: DRIVER_DCAP_VIRTUALIO = "YES"
# gdal: DRIVER_DMD_LONGNAME = "CityJSON"
# gdal: DRIVER_DMD_EXTENSIONS = "json"

import json
import os
from gdal_python_driver import BaseDriver, BaseDataset, BaseLayer

class Layer(BaseLayer):

    def __init__(self, filename, content):
        self.content = content
        self.name = os.path.splitext(os.path.basename(filename))[0]
        self.fields = [
            {'name': 'id'},
            {'name': 'type'},
        ]

        self.count = 0
        set_attrs = set()
        for key_value in content['CityObjects'].items():
            value = key_value[1]
            if 'attributes' in value:
                for kv in value['attributes'].items():
                    if not kv[0] in set_attrs:
                        set_attrs.add(kv[0])
                        v = kv[1]
                        t = 'String'
                        if isinstance(v, int):
                            t = 'Integer'
                        elif isinstance(v, float):
                            t = 'Real'
                        elif 'Date' in kv[0]:
                            t = 'Date'
                        self.fields.append({'name': kv[0], 'type': t})
            for geom in value['geometry']:
                if geom['type'] in ('Solid', 'MultiSurface'):
                    self.count += 1

        md = content['metadata']
        srs = md['referenceSystem'] if 'referenceSystem' in md else None
        if not srs and 'crs' in md:
            if isinstance(md['crs'], dict) and "epsg" in md['crs']:
                srs = 'EPSG:' + str(md['crs']['epsg'])
        self.geometry_fields = [
            {'type': 'MultiPolygonZ',
             'srs': srs}
        ]
        if 'transform' in content:
            self.translate = content['transform']['translate']
            self.scale = content['transform']['scale']
        else:
            self.translate = [0, 0, 0]
            self.scale = [1, 1, 1]
        self.vertices = content['vertices']

    def extent(self, force):
        md = self.content['metadata']
        if 'geographicalExtent' in md:
            bbox = md['geographicalExtent']
        else:
            bbox = md['bbox']
        return [bbox[0], bbox[1], bbox[3], bbox[4]]

    def feature_count(self, force):
        return self.count

    def __iter__(self):
        fid = 1
        vertices = self.vertices
        scale = self.scale
        translate = self.translate
        for key_value in self.content['CityObjects'].items():
            value = key_value[1]
            for geom in value['geometry']:
                geom_in = None
                boundaries = geom['boundaries']
                if geom['type'] == 'Solid':
                    assert len(boundaries) == 1, (len(boundaries), key_value)
                    geom_in = boundaries[0]
                elif geom['type'] == 'MultiSurface':
                    geom_in = boundaries
                if geom_in:
                    out_mpoly = []
                    for poly in geom_in:
                        out_poly = []
                        for ring in poly:
                            out_ring = []
                            for vertex_idx in ring:
                                v = vertices[vertex_idx]
                                c = [v*s+t
                                     for v, s, t in zip(v, scale, translate)]
                                out_ring.append(c)
                            out_ring.append(out_ring[0])
                            out_poly.append(out_ring)
                        out_mpoly.append(out_poly)
                    properties = {'id': key_value[0],
                                  'type': value['type']}
                    if 'attributes' in value:
                        properties.update(value['attributes'])

                    wkt = 'MULTIPOLYGON Z('
                    first_poly = True
                    for poly in out_mpoly:
                        if not first_poly:
                            wkt += ','
                        else:
                            first_poly = False
                        wkt += '('
                        first_ring = True
                        for ring in poly:
                            if not first_ring:
                                wkt += ','
                            else:
                                first_ring = False
                            wkt += '('
                            first_vertex = True
                            for v in ring:
                                if not first_vertex:
                                    wkt += ','
                                else:
                                    first_vertex = False
                                wkt += '%.18g %.18g %.18g' % (v[0], v[1], v[2])
                            wkt += ')'
                        wkt += ')'
                    wkt += ')'

                    f = {'type': 'OGRFeature',
                         'id': fid,
                         'fields': properties,
                         'geometry_fields': {'': wkt}}
                    yield f
                    fid += 1


class Dataset(BaseDataset):

    def __init__(self, filename, content):
        self.layers = [Layer(filename, content)]


class Driver(BaseDriver):

    def identify(self, filename, first_bytes, open_flags, open_options={}):
        return b'"type":"CityJSON"' in first_bytes or \
               b'"type": "CityJSON"' in first_bytes

    def open(self, filename, first_bytes, open_flags, open_options={}):
        if not self.identify(filename, first_bytes, open_flags):
            return None
        if filename.startswith('/vsi'):
            from osgeo import gdal
            f = gdal.VSIFOpenL(filename, 'rb')
            if not f:
                return None
            gdal.VSIFSeekL(f, 0, 2)
            length = gdal.VSIFTellL(f)
            gdal.VSIFSeekL(f, 0, 0)
            try:
                content = json.loads(gdal.VSIFReadL(1, length, f))
            finally:
                gdal.VSIFCloseL(f)
        else:
            with open(filename, 'rt') as f:
                content = json.loads(f.read())
        return Dataset(filename, content)
