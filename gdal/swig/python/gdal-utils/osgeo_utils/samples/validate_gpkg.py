#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR
# Purpose:  Test compliance of GeoPackage database w.r.t GeoPackage spec
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import datetime
import os
import sqlite3
import struct
import sys

# GDAL may be used for checks on tile content for the tiled gridded extension.
# If not available, those tests will be skipped
try:
    from osgeo import gdal
    from osgeo import ogr
    has_gdal = True
except ImportError:
    has_gdal = False


def _esc_literal(literal):
    return literal.replace("'", "''")


def _esc_id(identifier):
    return '"' + identifier.replace('"', "\"\"") + '"'


def _is_valid_data_type(typ):
    return typ in ('BOOLEAN', 'TINYINT', 'SMALLINT', 'MEDIUMINT',
                   'INT', 'INTEGER', 'FLOAT', 'DOUBLE', 'REAL',
                   'TEXT', 'BLOB', 'DATE', 'DATETIME') or \
        typ.startswith('TEXT(') or typ.startswith('BLOB(')


class GPKGCheckException(Exception):
    pass


class GPKGChecker(object):

    EXT_GEOM_TYPES = ('CIRCULARSTRING', 'COMPOUNDCURVE', 'CURVEPOLYGON',
                      'MULTICURVE', 'MULTISURFACE', 'CURVE', 'SURFACE')

    def __init__(self, filename, abort_at_first_error=True, verbose=False):
        self.filename = filename
        self.extended_pragma_info = False
        self.abort_at_first_error = abort_at_first_error
        self.verbose = verbose
        self.errors = []

    def _log(self, msg):
        if self.verbose:
            print(msg)

    def _assert(self, cond, req, msg):
        # self._log('Verified requirement %s' % req)
        if not cond:
            self.errors += [(req, msg)]
            if self.abort_at_first_error:
                if req:
                    raise GPKGCheckException('Req %s: %s' % (str(req), msg))
                else:
                    raise GPKGCheckException(msg)
        return cond

    def _check_structure(self, columns, expected_columns, req, table_name):
        self._assert(len(columns) == len(expected_columns), req,
                     'Table %s has %d columns, whereas %d are expected' %
                     (table_name, len(columns), len(expected_columns)))
        for (_, expected_name, expected_type, expected_notnull,
             expected_default, expected_pk) in expected_columns:
            found = False
            for (_, name, typ, notnull, default, pk) in columns:
                if name != expected_name:
                    continue

                if 'INTEGER' in expected_type and expected_pk:
                    expected_notnull = 1
                if typ == 'INTEGER' and pk:
                    notnull = 1
                if not self.extended_pragma_info and expected_pk > 1:
                    expected_pk = 1

                self._assert(typ in expected_type, req,
                             'Wrong type for %s of %s. Expected %s, got %s' %
                             (name, table_name, str(expected_type), typ))
                self._assert(notnull == expected_notnull, req,
                             ('Wrong notnull for %s of %s. ' +
                              'Expected %s, got %s') %
                             (name, table_name, expected_notnull, notnull))
                self._assert(default == expected_default, req,
                             ('Wrong default for %s of %s. ' +
                              'Expected %s, got %s') %
                             (name, table_name, expected_default, default))
                self._assert(pk == expected_pk, req,
                             'Wrong pk for %s of %s. Expected %s, got %s' %
                             (name, table_name, expected_pk, pk))
                found = True
                break

            self._assert(found, req, 'Column %s of %s not found!' %
                         (expected_name, table_name))

    def _check_gpkg_spatial_ref_sys(self, c):

        self._log('Checking gpkg_spatial_ref_sys')

        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_spatial_ref_sys'")
        if not self._assert(c.fetchone() is not None, 10,
                            "gpkg_spatial_ref_sys table missing"):
            return

        c.execute("PRAGMA table_info(gpkg_spatial_ref_sys)")
        columns = c.fetchall()
        has_definition_12_063 = False
        for (_, name, _, _, _, _) in columns:
            if name == 'definition_12_063':
                has_definition_12_063 = True

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        row = None
        if c.fetchone() is not None:
            c.execute("SELECT scope FROM gpkg_extensions WHERE "
                      "extension_name = 'gpkg_crs_wkt'")
            row = c.fetchone()
        if row:
            scope, = row
            self._assert(scope == 'read-write', 145,
                         'scope of gpkg_crs_wkt extension should be read-write')
            self._assert(
                has_definition_12_063, 145,
                "gpkg_spatial_ref_sys should have a definition_12_063 column, "
                "as gpkg_crs_wkt extension is declared")
        else:
            self._assert(
                not has_definition_12_063, 145,
                "gpkg_extensions should declare gpkg_crs_wkt extension "
                "as gpkg_spatial_ref_sys has a definition_12_063 column")

        if has_definition_12_063:
            expected_columns = [
                (0, 'srs_name', 'TEXT', 1, None, 0),
                (1, 'srs_id', 'INTEGER', 1, None, 1),
                (2, 'organization', 'TEXT', 1, None, 0),
                (3, 'organization_coordsys_id', 'INTEGER', 1, None, 0),
                (4, 'definition', 'TEXT', 1, None, 0),
                (5, 'description', 'TEXT', 0, None, 0),
                (6, 'definition_12_063', 'TEXT', 1, None, 0)
            ]
        else:
            expected_columns = [
                (0, 'srs_name', 'TEXT', 1, None, 0),
                (1, 'srs_id', 'INTEGER', 1, None, 1),
                (2, 'organization', 'TEXT', 1, None, 0),
                (3, 'organization_coordsys_id', 'INTEGER', 1, None, 0),
                (4, 'definition', 'TEXT', 1, None, 0),
                (5, 'description', 'TEXT', 0, None, 0)
            ]
        self._check_structure(columns, expected_columns, 10,
                              'gpkg_spatial_ref_sys')

        if has_definition_12_063:
            c.execute("SELECT srs_id, organization, organization_coordsys_id, "
                      "definition, definition_12_063 "
                      "FROM gpkg_spatial_ref_sys "
                      "WHERE srs_id IN (-1, 0, 4326) ORDER BY srs_id")
        else:
            c.execute("SELECT srs_id, organization, organization_coordsys_id, "
                      "definition FROM gpkg_spatial_ref_sys "
                      "WHERE srs_id IN (-1, 0, 4326) ORDER BY srs_id")
        ret = c.fetchall()
        self._assert(len(ret) == 3, 11,
                     'There should be at least 3 records in '
                     'gpkg_spatial_ref_sys')
        if len(ret) != 3:
            return
        self._assert(ret[0][1] == 'NONE', 11,
                     'wrong value for organization for srs_id = -1: %s' %
                     ret[0][1])
        self._assert(ret[0][2] == -1, 11,
                     'wrong value for organization_coordsys_id for '
                     'srs_id = -1: %s' % ret[0][2])
        self._assert(ret[0][3] == 'undefined', 11,
                     'wrong value for definition for srs_id = -1: %s' %
                     ret[0][3])
        if has_definition_12_063:
            self._assert(ret[0][4] == 'undefined', 116,
                         'wrong value for definition_12_063 for ' +
                         'srs_id = -1: %s' % ret[0][4])

        self._assert(ret[1][1] == 'NONE', 11,
                     'wrong value for organization for srs_id = 0: %s' %
                     ret[1][1])
        self._assert(ret[1][2] == 0, 11,
                     'wrong value for organization_coordsys_id for '
                     'srs_id = 0: %s' % ret[1][2])
        self._assert(ret[1][3] == 'undefined', 11,
                     'wrong value for definition for srs_id = 0: %s' %
                     ret[1][3])
        if has_definition_12_063:
            self._assert(ret[1][4] == 'undefined', 116,
                         'wrong value for definition_12_063 for ' +
                         'srs_id = 0: %s' % ret[1][4])

        self._assert(ret[2][1].lower() == 'epsg', 11,
                     'wrong value for organization for srs_id = 4326: %s' %
                     ret[2][1])
        self._assert(ret[2][2] == 4326, 11,
                     'wrong value for organization_coordsys_id for '
                     'srs_id = 4326: %s' % ret[2][2])
        self._assert(ret[2][3] != 'undefined', 11,
                     'wrong value for definition for srs_id = 4326: %s' %
                     ret[2][3])
        if has_definition_12_063:
            self._assert(ret[2][4] != 'undefined', 116,
                         'wrong value for definition_12_063 for ' +
                         'srs_id = 4326: %s' % ret[2][4])

        if has_definition_12_063:
            c.execute("SELECT srs_id FROM gpkg_spatial_ref_sys "
                      "WHERE srs_id NOT IN (0, -1) AND "
                      "definition = 'undefined' AND "
                      "definition_12_063 = 'undefined'")
            rows = c.fetchall()
            for (srs_id, ) in rows:
                self._assert(False, 117,
                             'srs_id = %d has both definition and ' % srs_id +
                             'definition_12_063 undefined')

    def _check_gpkg_contents(self, c):

        self._log('Checking gpkg_contents')

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_contents'")
        self._assert(c.fetchone() is not None, 13,
                     "gpkg_contents table missing")

        c.execute("PRAGMA table_info(gpkg_contents)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 1, None, 1),
            (1, 'data_type', 'TEXT', 1, None, 0),
            (2, 'identifier', 'TEXT', 0, None, 0),
            (3, 'description', 'TEXT', 0, "''", 0),
            (4, 'last_change', 'DATETIME', 1,
             "strftime('%Y-%m-%dT%H:%M:%fZ','now')", 0),
            (5, 'min_x', 'DOUBLE', 0, None, 0),
            (6, 'min_y', 'DOUBLE', 0, None, 0),
            (7, 'max_x', 'DOUBLE', 0, None, 0),
            (8, 'max_y', 'DOUBLE', 0, None, 0),
            (9, 'srs_id', 'INTEGER', 0, None, 0)
        ]
        self._check_structure(columns, expected_columns, 13, 'gpkg_contents')

        c.execute("SELECT 1 FROM gpkg_contents "
                  "WHERE data_type IN ('features', 'tiles')")
        self._assert(c.fetchone() is not None, 17,
                     'gpkg_contents should at least have one table with '
                     'data_type = features and/or tiles')

        c.execute("SELECT table_name, data_type FROM gpkg_contents "
                  "WHERE data_type NOT IN "
                  "('features', 'tiles', 'attributes', '2d-gridded-coverage')")
        ret = c.fetchall()
        self._assert(len(ret) == 0, 17,
                     'Unexpected data types in gpkg_contents: %s' % str(ret))

        c.execute('SELECT table_name, last_change, srs_id FROM gpkg_contents')
        rows = c.fetchall()
        for (table_name, last_change, srs_id) in rows:
            c.execute("SELECT 1 FROM sqlite_master WHERE "
                      "lower(name) = lower(?) AND type IN ('table', 'view')", (table_name,))
            self._assert(c.fetchone() is not None, 14,
                         ('table_name=%s in gpkg_contents is not a ' +
                          'table or view') % table_name)

            try:
                datetime.datetime.strptime(
                    last_change, '%Y-%m-%dT%H:%M:%S.%fZ')
            except ValueError:
                self._assert(False, 15,
                             ('last_change = %s for table_name = %s ' +
                              'is invalid datetime') %
                             (last_change, table_name))

            if srs_id is not None:
                c.execute('SELECT 1 FROM gpkg_spatial_ref_sys '
                          'WHERE srs_id = ?', (srs_id, ))
                self._assert(c.fetchone() is not None, 14,
                             ("table_name=%s has srs_id=%d in gpkg_contents " +
                              "which isn't found in gpkg_spatial_ref_sys") %
                             (table_name, srs_id))

    def _check_vector_user_table(self, c, table_name):

        self._log('Checking vector user table ' + table_name)

        c.execute("SELECT column_name, z, m, geometry_type_name, srs_id "
                  "FROM gpkg_geometry_columns WHERE table_name = ?",
                  (table_name,))
        rows_gpkg_geometry_columns = c.fetchall()
        self._assert(len(rows_gpkg_geometry_columns) == 1, 22,
                     ('table_name = %s is not registered in ' +
                      'gpkg_geometry_columns') % table_name)
        geom_column_name = rows_gpkg_geometry_columns[0][0]
        z = rows_gpkg_geometry_columns[0][1]
        m = rows_gpkg_geometry_columns[0][2]
        geometry_type_name = rows_gpkg_geometry_columns[0][3]
        srs_id = rows_gpkg_geometry_columns[0][4]

        c.execute('PRAGMA table_info(%s)' % _esc_id(table_name))
        base_geom_types = ('GEOMETRY', 'POINT', 'LINESTRING', 'POLYGON',
                           'MULTIPOINT', 'MULTILINESTRING', 'MULTIPOLYGON',
                           'GEOMETRYCOLLECTION')
        cols = c.fetchall()
        found_geom = False
        count_pkid = 0
        for (_, name, typ, notnull, default, pk) in cols:
            if name.lower() == geom_column_name.lower():
                found_geom = True
                self._assert(
                    typ in base_geom_types or
                    typ in GPKGChecker.EXT_GEOM_TYPES,
                    25, ('invalid type (%s) for geometry ' +
                         'column of table %s') % (typ, table_name))
                self._assert(typ == geometry_type_name, 31,
                             ('table %s has geometry column of type %s in ' +
                              'SQL and %s in geometry_type_name of ' +
                              'gpkg_geometry_columns') %
                             (table_name, typ, geometry_type_name))

            elif pk == 1:
                count_pkid += 1
                self._assert(typ == 'INTEGER', 29,
                             ('table %s has a PRIMARY KEY of type %s ' +
                              'instead of INTEGER') % (table_name, typ))

            else:
                self._assert(_is_valid_data_type(typ), 5,
                             ('table %s has column %s of unexpected type %s'
                              % (table_name, name, typ)))
        self._assert(found_geom, 24,
                     'table %s has no %s column' %
                     (table_name, geom_column_name))

        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "type = 'table' AND name = ?", (table_name,))
        if c.fetchone():
            self._assert(count_pkid == 1, 29,
                         'table %s has no INTEGER PRIMARY KEY' % table_name)
        else:
            self._assert(len(cols) > 0 and cols[0][2] == 'INTEGER',
                         150, 'view %s has no INTEGER first column' % table_name)

            c.execute("SELECT COUNT(*) - COUNT(DISTINCT %s) FROM %s" %
                      (_esc_id(cols[0][1]), _esc_id(table_name)))
            self._assert(c.fetchone()[0] == 0, 150,
                         'First column of view %s should contain '
                         'unique values' % table_name)

        self._assert(z in (0, 1, 2), 27, ("z value of %s is %d. " +
                                          "Expected 0, 1 or 2") % (table_name, z))

        self._assert(m in (0, 1, 2), 27, ("m value of %s is %d. " +
                                          "Expected 0, 1 or 2") % (table_name, m))

        if geometry_type_name in GPKGChecker.EXT_GEOM_TYPES:
            c.execute("SELECT 1 FROM gpkg_extensions WHERE "
                      "extension_name = 'gpkg_geom_%s' AND "
                      "table_name = ? AND column_name = ? AND "
                      "scope = 'read-write'" % geometry_type_name,
                      (table_name, geom_column_name))
            self._assert(c.fetchone() is not None, 68,
                         "gpkg_geom_%s extension should be declared for "
                         "table %s" % (geometry_type_name, table_name))

        wkb_geometries = base_geom_types + GPKGChecker.EXT_GEOM_TYPES
        c.execute("SELECT %s FROM %s " %
                  (_esc_id(geom_column_name), _esc_id(table_name)))
        found_geom_types = set()
        for (blob,) in c.fetchall():
            if blob is None:
                continue

            self._assert(len(blob) >= 8, 19, 'Invalid geometry')
            max_size_needed = min(len(blob), 8 + 4 * 2 * 8 + 5)
            blob_ar = struct.unpack('B' * max_size_needed,
                                    blob[0:max_size_needed])
            self._assert(blob_ar[0] == ord('G'), 19, 'Invalid geometry')
            self._assert(blob_ar[1] == ord('P'), 19, 'Invalid geometry')
            self._assert(blob_ar[2] == 0, 19, 'Invalid geometry')
            flags = blob_ar[3]
            empty_flag = ((flags >> 3) & 1) == 1
            big_endian = (flags & 1) == 0
            env_ind = (flags >> 1) & 7
            self._assert(((flags >> 5) & 1) == 0, 19,
                         'Invalid geometry: ExtendedGeoPackageBinary not '
                         'allowed')
            self._assert(env_ind <= 4, 19,
                         'Invalid geometry: invalid envelope indicator code')
            endian_prefix = '>' if big_endian else '<'
            geom_srs_id = struct.unpack((endian_prefix + 'I') * 1, blob[4:8])[0]
            self._assert(srs_id == geom_srs_id, 33,
                         ('table %s has geometries with SRID %d, ' +
                          'whereas only %d is expected') %
                         (table_name, geom_srs_id, srs_id))

            self._assert(not (empty_flag and env_ind != 0), 152,
                         "Invalid empty geometry")

            if env_ind == 0:
                coord_dim = 0
            elif env_ind == 1:
                coord_dim = 2
            elif env_ind == 2 or env_ind == 3:
                coord_dim = 3
            else:
                coord_dim = 4

            # if env_ind == 2 or env_ind == 4:
            #    self._assert(z > 0, 19,
            #        'z found in geometry, but not in gpkg_geometry_columns')
            # if env_ind == 3 or env_ind == 4:
            #    self._assert(m > 0, 19,
            #        'm found in geometry, but not in gpkg_geometry_columns')

            header_len = 8 + coord_dim * 2 * 8
            self._assert(len(blob) >= header_len, 19, 'Invalid geometry')
            wkb_endianness = blob_ar[header_len]
            wkb_big_endian = (wkb_endianness == 0)
            wkb_endian_prefix = '>' if wkb_big_endian else '<'
            wkb_geom_type = struct.unpack(
                (wkb_endian_prefix + 'I') * 1, blob[header_len + 1:header_len + 5])[0]
            self._assert(wkb_geom_type >= 0 and
                         (wkb_geom_type % 1000) < len(wkb_geometries),
                         19, 'Invalid WKB geometry type')

            wkb_dim = int(wkb_geom_type / 1000)
            if z == 1:
                self._assert(wkb_dim == 1 or wkb_dim == 3, 19,
                             'geometry without Z found')
            if m == 1:
                self._assert(wkb_dim == 2 or wkb_dim == 3, 19,
                             'geometry without M found')
            if wkb_dim == 1 or wkb_dim == 3:  # Z or ZM
                self._assert(z > 0, 19,
                             'z found in geometry, but not in '
                             'gpkg_geometry_columns')
            if wkb_dim == 2 or wkb_dim == 3:  # M or ZM
                self._assert(m > 0, 19,
                             'm found in geometry, but not in '
                             'gpkg_geometry_columns')

            found_geom_types.add(wkb_geometries[wkb_geom_type % 1000])

            if has_gdal:
                geom = ogr.CreateGeometryFromWkb(blob[header_len:])
                self._assert(geom is not None, 19, 'Invalid geometry')

                self._assert((geom.IsEmpty() and empty_flag) or
                             (not geom.IsEmpty() and not empty_flag), 152,
                             'Inconsistent empty_flag vs geometry content')

        if geometry_type_name in ('POINT', 'LINESTRING', 'POLYGON',
                                  'MULTIPOINT', 'MULTILINESTRING',
                                  'MULTIPOLYGON'):
            self._assert(not found_geom_types or
                         found_geom_types == set([geometry_type_name]), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))
        elif geometry_type_name == 'GEOMETRYCOLLECTION':
            self._assert(not found_geom_types or
                         not found_geom_types.difference(
                             set(['GEOMETRYCOLLECTION', 'MULTIPOINT',
                                  'MULTILINESTRING', 'MULTIPOLYGON',
                                  'MULTICURVE', 'MULTISURFACE'])), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))
        elif geometry_type_name in ('CURVEPOLYGON', 'SURFACE'):
            self._assert(not found_geom_types or
                         not found_geom_types.difference(
                             set(['POLYGON', 'CURVEPOLYGON'])), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))
        elif geometry_type_name == 'MULTICURVE':
            self._assert(not found_geom_types or
                         not found_geom_types.difference(
                             set(['MULTILINESTRING', 'MULTICURVE'])), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))
        elif geometry_type_name == 'MULTISURFACE':
            self._assert(not found_geom_types or
                         not found_geom_types.difference(
                             set(['MULTIPOLYGON', 'MULTISURFACE'])), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))
        elif geometry_type_name == 'CURVE':
            self._assert(not found_geom_types or
                         not found_geom_types.difference(
                             set(['LINESTRING', 'CIRCULARSTRING',
                                  'COMPOUNDCURVE'])), 32,
                         'in table %s, found geometry types %s' %
                         (table_name, str(found_geom_types)))

        for geom_type in found_geom_types:
            if geom_type in GPKGChecker.EXT_GEOM_TYPES:
                c.execute("SELECT 1 FROM gpkg_extensions WHERE "
                          "extension_name = 'gpkg_geom_%s' AND "
                          "table_name = ? AND column_name = ? AND "
                          "scope = 'read-write'" % geom_type,
                          (table_name, geom_column_name))
                self._assert(c.fetchone() is not None, 68,
                             "gpkg_geom_%s extension should be declared for "
                             "table %s" % (geom_type, table_name))

        rtree_name = 'rtree_%s_%s' % (table_name, geom_column_name)
        c.execute("SELECT 1 FROM sqlite_master WHERE name = ?", (rtree_name,))
        has_rtree = c.fetchone() is not None
        if has_rtree:
            c.execute("SELECT 1 FROM gpkg_extensions WHERE "
                      "extension_name = 'gpkg_rtree_index' AND "
                      "table_name=? AND column_name=? AND "
                      "scope='write-only'",
                      (table_name, geom_column_name))
            self._assert(c.fetchone() is not None, 78,
                         ("Table %s has a RTree, but not declared in " +
                          "gpkg_extensions") % table_name)

            c.execute('PRAGMA table_info(%s)' % _esc_id(rtree_name))
            columns = c.fetchall()
            expected_columns = [
                (0, 'id', ['', 'INT'], 0, None, 0),
                (1, 'minx', ['', 'NUM', 'REAL'], 0, None, 0),
                (2, 'maxx', ['', 'NUM', 'REAL'], 0, None, 0),
                (3, 'miny', ['', 'NUM', 'REAL'], 0, None, 0),
                (4, 'maxy', ['', 'NUM', 'REAL'], 0, None, 0)
            ]
            self._check_structure(columns, expected_columns, 77, rtree_name)

            c.execute("SELECT 1 FROM sqlite_master WHERE type = 'trigger' " +
                      "AND name = '%s_insert'" % _esc_literal(rtree_name))
            self._assert(c.fetchone() is not None, 75,
                         "%s_insert trigger missing" % rtree_name)

            for i in range(4):
                c.execute("SELECT 1 FROM sqlite_master WHERE " +
                          "type = 'trigger' " +
                          "AND name = '%s_update%d'" %
                          (_esc_literal(rtree_name), i + 1))
                self._assert(c.fetchone() is not None, 75,
                             "%s_update%d trigger missing" % (rtree_name, i + 1))

            c.execute("SELECT 1 FROM sqlite_master WHERE type = 'trigger' " +
                      "AND name = '%s_delete'" % _esc_literal(rtree_name))
            self._assert(c.fetchone() is not None, 75,
                         "%s_delete trigger missing" % rtree_name)

    def _check_features(self, c):

        self._log('Checking features')

        c.execute("SELECT 1 FROM gpkg_contents WHERE data_type = 'features'")
        if c.fetchone() is None:
            self._log('... No features table')
            return

        self._log('Checking gpkg_geometry_columns')
        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_geometry_columns'")
        self._assert(c.fetchone() is not None, 21,
                     "gpkg_geometry_columns table missing")

        c.execute("PRAGMA table_info(gpkg_geometry_columns)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 1, None, 1),
            (1, 'column_name', 'TEXT', 1, None, 2),
            (2, 'geometry_type_name', 'TEXT', 1, None, 0),
            (3, 'srs_id', 'INTEGER', 1, None, 0),
            (4, 'z', 'TINYINT', 1, None, 0),
            (5, 'm', 'TINYINT', 1, None, 0)
        ]
        self._check_structure(columns, expected_columns, 21,
                              'gpkg_geometry_columns')

        c.execute("SELECT table_name FROM gpkg_contents WHERE "
                  "data_type = 'features'")
        rows = c.fetchall()
        for (table_name,) in rows:
            self._check_vector_user_table(c, table_name)

        c.execute("SELECT table_name, srs_id FROM gpkg_geometry_columns")
        rows = c.fetchall()
        for (table_name, srs_id) in rows:
            c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ? " +
                      "AND data_type='features'", (table_name,))
            ret = c.fetchall()
            self._assert(len(ret) == 1, 23,
                         ('table_name = %s is registered in ' +
                          'gpkg_geometry_columns, but not in gpkg_contents') %
                         table_name)

            c.execute('SELECT 1 FROM gpkg_spatial_ref_sys WHERE ' +
                      'srs_id = ?', (srs_id, ))
            self._assert(c.fetchone() is not None, 14,
                         ("table_name=%s has srs_id=%d in " +
                          "gpkg_geometry_columns which isn't found in " +
                          "gpkg_spatial_ref_sys") % (table_name, srs_id))

        c.execute("SELECT a.table_name, a.srs_id, b.srs_id FROM " +
                  "gpkg_geometry_columns a, gpkg_contents b " +
                  "WHERE a.table_name = b.table_name AND a.srs_id != b.srs_id")
        rows = c.fetchall()
        for (table_name, a_srs_id, b_srs_id) in rows:
            self._assert(False, 146,
                         "Table %s is declared with srs_id %d in "
                         "gpkg_geometry_columns and %d in gpkg_contents" %
                         (table_name, a_srs_id, b_srs_id))

    def _check_attribute_user_table(self, c, table_name):
        self._log('Checking attributes table ' + table_name)

        c.execute('PRAGMA table_info(%s)' % _esc_id(table_name))
        cols = c.fetchall()
        count_pkid = 0
        for (_, name, typ, _, _, pk) in cols:
            if pk == 1:
                count_pkid += 1
                self._assert(typ == 'INTEGER', 119,
                             ('table %s has a PRIMARY KEY of type %s ' +
                              'instead of INTEGER') % (table_name, typ))

            else:
                self._assert(_is_valid_data_type(typ), 5,
                             'table %s has column %s of unexpected type %s'
                             % (table_name, name, typ))

        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "type = 'table' AND name = ?", (table_name,))
        if c.fetchone():
            self._assert(count_pkid == 1, 119,
                         'table %s has no INTEGER PRIMARY KEY' % table_name)
        else:
            self._assert(len(cols) > 0 and cols[0][2] == 'INTEGER',
                         151, 'view %s has no INTEGER first column' % table_name)

            c.execute("SELECT COUNT(*) - COUNT(DISTINCT %s) FROM %s" %
                      (_esc_id(cols[0][1]), _esc_id(table_name)))
            self._assert(c.fetchone()[0] == 0, 151,
                         'First column of view %s should contain '
                         'unique values' % table_name)

    def _check_attributes(self, c):

        self._log('Checking attributes')
        c.execute("SELECT table_name FROM gpkg_contents WHERE "
                  "data_type = 'attributes'")
        rows = c.fetchall()
        if not rows:
            self._log('... No attributes table')
        for (table_name,) in rows:
            self._check_attribute_user_table(c, table_name)

    def _check_tile_user_table(self, c, table_name, data_type):

        self._log('Checking tile pyramid user table ' + table_name)

        c.execute("PRAGMA table_info(%s)" % _esc_id(table_name))
        columns = c.fetchall()
        expected_columns = [
            (0, 'id', 'INTEGER', 0, None, 1),
            (1, 'zoom_level', 'INTEGER', 1, None, 0),
            (2, 'tile_column', 'INTEGER', 1, None, 0),
            (3, 'tile_row', 'INTEGER', 1, None, 0),
            (4, 'tile_data', 'BLOB', 1, None, 0)
        ]

        self._check_structure(columns, expected_columns, 54,
                              'gpkg_tile_matrix_set')

        c.execute("SELECT DISTINCT zoom_level FROM %s" % _esc_id(table_name))
        rows = c.fetchall()
        for (zoom_level, ) in rows:
            c.execute("SELECT 1 FROM gpkg_tile_matrix WHERE table_name = ? "
                      "AND zoom_level = ?", (table_name, zoom_level))
            self._assert(c.fetchone() is not None, 44,
                         ("Table %s has data for zoom_level = %d, but no " +
                          "corresponding row in gpkg_tile_matrix") %
                         (table_name, zoom_level))

        zoom_other_levels = False
        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        if c.fetchone() is not None:
            c.execute("SELECT column_name FROM gpkg_extensions WHERE "
                      "table_name = ? "
                      "AND extension_name = 'gpkg_zoom_other'", (table_name,))
            row = c.fetchone()
            if row is not None:
                (column_name, ) = row
                self._assert(column_name == 'tile_data', 88,
                             'Wrong column_name in gpkg_extensions for '
                             'gpkg_zoom_other')
                zoom_other_levels = True

        c.execute("SELECT zoom_level, pixel_x_size, pixel_y_size "
                  "FROM gpkg_tile_matrix "
                  "WHERE table_name = ? ORDER BY zoom_level", (table_name,))
        rows = c.fetchall()
        prev_zoom_level = None
        prev_pixel_x_size = None
        prev_pixel_y_size = None
        for (zoom_level, pixel_x_size, pixel_y_size) in rows:
            if prev_pixel_x_size is not None:
                self._assert(
                    pixel_x_size < prev_pixel_x_size and
                    pixel_y_size < prev_pixel_y_size,
                    53,
                    ('For table %s, pixel size are not consistent ' +
                     'with zoom_level') % table_name)
            if prev_zoom_level is not None and \
               zoom_level == prev_zoom_level + 1 and not zoom_other_levels:
                self._assert(
                    abs((pixel_x_size - prev_pixel_x_size / 2) /
                        prev_pixel_x_size) < 1e-5, 35,
                    "Expected pixel_x_size=%f for zoom_level=%d. Got %f" %
                    (prev_pixel_x_size / 2, zoom_level, pixel_x_size))
                self._assert(
                    abs((pixel_y_size - prev_pixel_y_size / 2) /
                        prev_pixel_y_size) < 1e-5, 35,
                    "Expected pixel_y_size=%f for zoom_level=%d. Got %f" %
                    (prev_pixel_y_size / 2, zoom_level, pixel_y_size))

            prev_pixel_x_size = pixel_x_size
            prev_pixel_y_size = pixel_y_size
            prev_zoom_level = zoom_level

        c.execute("SELECT max_x - min_x, "
                  "       MIN(matrix_width * tile_width * pixel_x_size), "
                  "       MAX(matrix_width * tile_width * pixel_x_size), "
                  "       max_y - min_y, "
                  "       MIN(matrix_height * tile_height * pixel_y_size), "
                  "       MAX(matrix_height * tile_height * pixel_y_size) "
                  "FROM gpkg_tile_matrix tm JOIN gpkg_tile_matrix_set tms "
                  "ON tm.table_name = tms.table_name WHERE tm.table_name = ?",
                  (table_name,))
        rows = c.fetchall()
        if rows:
            (dx, min_dx, max_dx, dy, min_dy, max_dy) = rows[0]
            self._assert(abs((min_dx - dx) / dx) < 1e-3 and
                         abs((max_dx - dx) / dx) < 1e-3 and
                         abs((min_dy - dy) / dy) < 1e-3 and
                         abs((max_dy - dy) / dy) < 1e-3, 45,
                         ("Inconsistent values in gpkg_tile_matrix and " +
                          "gpkg_tile_matrix_set for table %s") % table_name)

        c.execute("SELECT DISTINCT zoom_level FROM %s" % _esc_id(table_name))
        rows = c.fetchall()
        for (zoom_level,) in rows:
            c.execute(("SELECT MIN(tile_column), MAX(tile_column), " +
                       "MIN(tile_row), MAX(tile_row) FROM %s " +
                       "WHERE zoom_level = %d") %
                      (_esc_id(table_name), zoom_level))
            min_col, max_col, min_row, max_row = c.fetchone()

            c.execute("SELECT matrix_width, matrix_height FROM "
                      "gpkg_tile_matrix "
                      "WHERE table_name = ? AND zoom_level = ?",
                      (table_name, zoom_level))
            rows2 = c.fetchall()
            if not rows2:
                self._assert(False, 55,
                             "Invalid zoom_level in %s" % table_name)
            else:
                matrix_width, matrix_height = rows2[0]
                self._assert(min_col >= 0 and min_col < matrix_width, 56,
                             "Invalid tile_col in %s" % table_name)
                self._assert(min_row >= 0 and min_row < matrix_height, 57,
                             "Invalid tile_row in %s" % table_name)

        c.execute("SELECT tile_data FROM %s" % _esc_id(table_name))
        found_webp = False
        for (blob,) in c.fetchall():
            self._assert(blob is not None and len(blob) >= 12, 19,
                         'Invalid blob')
            max_size_needed = 12
            blob_ar = struct.unpack('B' * max_size_needed,
                                    blob[0:max_size_needed])
            is_jpeg = blob_ar[0:3] == (0xff, 0xd8, 0xff)
            is_png = blob_ar[0:4] == (0x89, 0x50, 0x4E, 0x47)
            is_webp = blob_ar[0:4] == (ord('R'), ord('I'),
                                       ord('F'), ord('F')) and \
                blob_ar[8:12] == (ord('W'), ord('E'), ord('B'), ord('P'))
            is_tiff = blob_ar[0:4] == (0x49, 0x49, 0x2A, 0x00) or \
                blob_ar[0:4] == (0x4D, 0x4D, 0x00, 0x2A)
            self._assert(is_jpeg or is_png or is_webp or is_tiff, 36,
                         'Unrecognized image mime type')
            if data_type == 'tiles':
                self._assert(is_jpeg or is_png or is_webp, 36,
                             'Unrecognized image mime type')
            elif data_type == '2d-gridded-coverage':
                self._assert(is_png or is_tiff, 36,
                             'Unrecognized image mime type')

            if is_webp:
                found_webp = True

        if found_webp:
            c.execute("SELECT 1 FROM gpkg_extensions WHERE "
                      "table_name = ? AND column_name = 'tile_data' AND "
                      "extension_name = 'gpkg_webp' AND "
                      "scope = 'read-write'", (table_name, ))
            self._assert(c.fetchone() is not None, 91,
                         ("Table %s has webp content, but not registered "
                          "in gpkg_extensions" % table_name))

    def _check_tiles(self, c):

        self._log('Checking tiles')

        c.execute("SELECT 1 FROM gpkg_contents WHERE data_type IN "
                  "('tiles', '2d-gridded-coverage')")
        if c.fetchone() is None:
            self._log('... No tiles table')
            return

        self._log('Checking gpkg_tile_matrix_set ')
        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_tile_matrix_set'")
        self._assert(c.fetchone() is not None, 38,
                     "gpkg_tile_matrix_set table missing")

        c.execute("PRAGMA table_info(gpkg_tile_matrix_set)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 1, None, 1),
            (1, 'srs_id', 'INTEGER', 1, None, 0),
            (2, 'min_x', 'DOUBLE', 1, None, 0),
            (3, 'min_y', 'DOUBLE', 1, None, 0),
            (4, 'max_x', 'DOUBLE', 1, None, 0),
            (5, 'max_y', 'DOUBLE', 1, None, 0)]
        self._check_structure(columns, expected_columns, 38,
                              'gpkg_tile_matrix_set')

        c.execute("SELECT table_name, srs_id FROM gpkg_tile_matrix_set")
        rows = c.fetchall()
        for (table_name, srs_id) in rows:
            c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ? " +
                      "AND data_type IN ('tiles', '2d-gridded-coverage')",
                      (table_name,))
            ret = c.fetchall()
            self._assert(len(ret) == 1, 39,
                         ('table_name = %s is registered in ' +
                          'gpkg_tile_matrix_set, but not in gpkg_contents') %
                         table_name)

            c.execute('SELECT 1 FROM gpkg_spatial_ref_sys WHERE srs_id = ?',
                      (srs_id, ))
            self._assert(c.fetchone() is not None, 41,
                         ("table_name=%s has srs_id=%d in " +
                          "gpkg_tile_matrix_set which isn't found in " +
                          "gpkg_spatial_ref_sys") % (table_name, srs_id))

        self._log('Checking gpkg_tile_matrix')
        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_tile_matrix'")
        self._assert(c.fetchone() is not None, 42,
                     "gpkg_tile_matrix table missing")

        c.execute("PRAGMA table_info(gpkg_tile_matrix)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 1, None, 1),
            (1, 'zoom_level', 'INTEGER', 1, None, 2),
            (2, 'matrix_width', 'INTEGER', 1, None, 0),
            (3, 'matrix_height', 'INTEGER', 1, None, 0),
            (4, 'tile_width', 'INTEGER', 1, None, 0),
            (5, 'tile_height', 'INTEGER', 1, None, 0),
            (6, 'pixel_x_size', 'DOUBLE', 1, None, 0),
            (7, 'pixel_y_size', 'DOUBLE', 1, None, 0)
        ]
        self._check_structure(columns, expected_columns, 42,
                              'gpkg_tile_matrix')

        c.execute("SELECT table_name, zoom_level, matrix_width, "
                  "matrix_height, tile_width, tile_height, pixel_x_size, "
                  "pixel_y_size FROM gpkg_tile_matrix")
        rows = c.fetchall()
        for (table_name, zoom_level, matrix_width, matrix_height, tile_width,
             tile_height, pixel_x_size, pixel_y_size) in rows:
            c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ? "
                      "AND data_type IN ('tiles', '2d-gridded-coverage')",
                      (table_name,))
            ret = c.fetchall()
            self._assert(len(ret) == 1, 43,
                         ('table_name = %s is registered in ' +
                          'gpkg_tile_matrix, but not in gpkg_contents') %
                         table_name)
            self._assert(zoom_level >= 0, 46,
                         "Invalid zoom_level = %d for table %s" %
                         (zoom_level, table_name))
            self._assert(matrix_width > 0, 47,
                         "Invalid matrix_width = %d for table %s" %
                         (matrix_width, table_name))
            self._assert(matrix_height > 0, 48,
                         "Invalid matrix_height = %d for table %s" %
                         (matrix_height, table_name))
            self._assert(tile_width > 0, 49,
                         "Invalid tile_width = %d for table %s" %
                         (tile_width, table_name))
            self._assert(tile_height > 0, 50,
                         "Invalid tile_height = %d for table %s" %
                         (tile_height, table_name))
            self._assert(pixel_x_size > 0, 51,
                         "Invalid pixel_x_size = %f for table %s" %
                         (pixel_x_size, table_name))
            self._assert(pixel_y_size > 0, 52,
                         "Invalid pixel_y_size = %f for table %s" %
                         (pixel_y_size, table_name))

        c.execute("SELECT table_name, data_type FROM gpkg_contents WHERE "
                  "data_type IN ('tiles', '2d-gridded-coverage')")
        rows = c.fetchall()
        for (table_name, data_type) in rows:
            self._check_tile_user_table(c, table_name, data_type)

    def _check_tiled_gridded_coverage_data(self, c):

        self._log('Checking tiled gridded elevation data')

        c.execute("SELECT table_name FROM gpkg_contents WHERE "
                  "data_type = '2d-gridded-coverage'")
        tables = c.fetchall()
        if not tables:
            self._log('... No tiled gridded coverage table')
            return
        tables = [tables[i][0] for i in range(len(tables))]

        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_2d_gridded_coverage_ancillary'")
        self._assert(c.fetchone() is not None, 'gpkg_2d_gridded_coverage#1',
                     'gpkg_2d_gridded_coverage_ancillary table is missing')

        c.execute("PRAGMA table_info(gpkg_2d_gridded_coverage_ancillary)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'id', 'INTEGER', 1, None, 1),
            (1, 'tile_matrix_set_name', 'TEXT', 1, None, 0),
            (2, 'datatype', 'TEXT', 1, "'integer'", 0),
            (3, 'scale', 'REAL', 1, '1.0', 0),
            (4, 'offset', 'REAL', 1, '0.0', 0),
            (5, 'precision', 'REAL', 0, '1.0', 0),
            (6, 'data_null', 'REAL', 0, None, 0),
            (7, 'grid_cell_encoding', 'TEXT', 0, "'grid-value-is-center'", 0),
            (8, 'uom', 'TEXT', 0, None, 0),
            (9, 'field_name', 'TEXT', 0, "'Height'", 0),
            (10, 'quantity_definition', 'TEXT', 0, "'Height'", 0)
        ]
        self._check_structure(columns, expected_columns, 'gpkg_2d_gridded_coverage#1',
                              'gpkg_2d_gridded_coverage_ancillary')

        c.execute("SELECT 1 FROM sqlite_master WHERE "
                  "name = 'gpkg_2d_gridded_tile_ancillary'")
        self._assert(c.fetchone() is not None, 'gpkg_2d_gridded_coverage#2',
                     'gpkg_2d_gridded_tile_ancillary table is missing')

        c.execute("PRAGMA table_info(gpkg_2d_gridded_tile_ancillary)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'id', 'INTEGER', 0, None, 1),
            (1, 'tpudt_name', 'TEXT', 1, None, 0),
            (2, 'tpudt_id', 'INTEGER', 1, None, 0),
            (3, 'scale', 'REAL', 1, '1.0', 0),
            (4, 'offset', 'REAL', 1, '0.0', 0),
            (5, 'min', 'REAL', 0, 'NULL', 0),
            (6, 'max', 'REAL', 0, 'NULL', 0),
            (7, 'mean', 'REAL', 0, 'NULL', 0),
            (8, 'std_dev', 'REAL', 0, 'NULL', 0)
        ]

        self._check_structure(columns, expected_columns, 'gpkg_2d_gridded_coverage#2',
                              'gpkg_2d_gridded_tile_ancillary')

        c.execute("SELECT srs_id, organization, organization_coordsys_id, "
                  "definition FROM gpkg_spatial_ref_sys "
                  "WHERE srs_id = 4979")
        ret = c.fetchall()
        self._assert(len(ret) == 1, 'gpkg_2d_gridded_coverage#3',
                     "gpkg_spatial_ref_sys shall have a row for srs_id=4979")
        self._assert(ret[0][1].lower() == 'epsg', 'gpkg_2d_gridded_coverage#3',
                     'wrong value for organization for srs_id = 4979: %s' %
                     ret[0][1])
        self._assert(ret[0][2] == 4979, 'gpkg_2d_gridded_coverage#3',
                     ('wrong value for organization_coordsys_id for ' +
                      'srs_id = 4979: %s') % ret[0][2])

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        self._assert(c.fetchone() is not None, 'gpkg_2d_gridded_coverage#6',
                     'gpkg_extensions does not exist')
        c.execute("SELECT table_name, column_name, definition, scope FROM "
                  "gpkg_extensions WHERE "
                  "extension_name = 'gpkg_2d_gridded_coverage'")
        rows = c.fetchall()
        self._assert(len(rows) == 2 + len(tables), 'gpkg_2d_gridded_coverage#6',
                     "Wrong number of entries in gpkg_extensions with "
                     "2d_gridded_coverage extension name")
        found_gpkg_2d_gridded_coverage_ancillary = False
        found_gpkg_2d_gridded_tile_ancillary = False
        expected_def = \
            'http://docs.opengeospatial.org/is/17-066r1/17-066r1.html'
        for (table_name, column_name, definition, scope) in rows:
            if table_name == 'gpkg_2d_gridded_coverage_ancillary':
                found_gpkg_2d_gridded_coverage_ancillary = True
                self._assert(column_name is None, 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for "
                             "gpkg_2d_gridded_coverage_ancillary "
                             "in gpkg_extensions")
                self._assert(definition == expected_def, 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry (definition) for "
                             "gpkg_2d_gridded_coverage_ancillary "
                             "in gpkg_extensions")
                self._assert(scope == 'read-write', 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for "
                             "gpkg_2d_gridded_coverage_ancillary "
                             "in gpkg_extensions")
            elif table_name == 'gpkg_2d_gridded_tile_ancillary':
                found_gpkg_2d_gridded_tile_ancillary = True
                self._assert(column_name is None, 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for "
                             "gpkg_2d_gridded_tile_ancillary "
                             "in gpkg_extensions")
                self._assert(definition == expected_def, 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry (definition) for "
                             "gpkg_2d_gridded_tile_ancillary "
                             "in gpkg_extensions")
                self._assert(scope == 'read-write', 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for "
                             "gpkg_2d_gridded_tile_ancillary "
                             "in gpkg_extensions")
            else:
                self._assert(table_name in tables, 'gpkg_2d_gridded_coverage#6',
                             "Unexpected table_name registered for " +
                             "2d_gridded_coverage: %s" % table_name)
                self._assert(column_name == 'tile_data', 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for %s " % table_name +
                             "in gpkg_extensions")
                self._assert(definition == expected_def, 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry (definition) for %s " % table_name +
                             "in gpkg_extensions")
                self._assert(scope == 'read-write', 'gpkg_2d_gridded_coverage#6',
                             "Wrong entry for %s " % table_name +
                             "in gpkg_extensions")

        self._assert(found_gpkg_2d_gridded_coverage_ancillary, 'gpkg_2d_gridded_coverage#6',
                     "gpkg_2d_gridded_coverage_ancillary not registered "
                     "for 2d_gridded_coverage")
        self._assert(found_gpkg_2d_gridded_tile_ancillary, 'gpkg_2d_gridded_coverage#6',
                     "gpkg_2d_gridded_tile_ancillary not registered "
                     "for 2d_gridded_coverage")

        c.execute("SELECT tile_matrix_set_name, datatype FROM "
                  "gpkg_2d_gridded_coverage_ancillary")
        rows = c.fetchall()
        self._assert(len(rows) == len(tables), 'gpkg_2d_gridded_coverage#7',
                     "Wrong number of entries in "
                     "gpkg_2d_gridded_coverage_ancillary")
        for (tile_matrix_set_name, datatype) in rows:
            self._assert(tile_matrix_set_name in tables, 'gpkg_2d_gridded_coverage#7',
                         "Table %s has a row in " % tile_matrix_set_name +
                         "gpkg_2d_gridded_coverage_ancillary, but not in "
                         "gpkg_contents")
            c.execute('SELECT 1 FROM gpkg_tile_matrix_set WHERE '
                      'table_name = ?', (tile_matrix_set_name,))
            self._assert(c.fetchone() is not None, 'gpkg_2d_gridded_coverage#8',
                         'missing entry in gpkg_tile_matrix_set ' +
                         'for %s' % tile_matrix_set_name)
            self._assert(datatype in ('integer', 'float'), 'gpkg_2d_gridded_coverage#9',
                         'Unexpected datatype = %s' % datatype)

        for table in tables:
            c.execute("SELECT COUNT(*) FROM %s" % _esc_id(table))
            count_tpudt = c.fetchone()
            c.execute("SELECT COUNT(*) FROM gpkg_2d_gridded_tile_ancillary "
                      "WHERE tpudt_name = ?", (table, ))
            count_tile_ancillary = c.fetchone()
            self._assert(count_tpudt == count_tile_ancillary, 'gpkg_2d_gridded_coverage#10',
                         ("Inconsistent number of rows in " +
                          "gpkg_2d_gridded_tile_ancillary for %s") % table)

        c.execute("SELECT DISTINCT tpudt_name FROM "
                  "gpkg_2d_gridded_tile_ancillary")
        rows = c.fetchall()
        for (tpudt_name, ) in rows:
            self._assert(tpudt_name in tables, 'gpkg_2d_gridded_coverage#11',
                         "tpudt_name = %s is invalid" % tpudt_name)

        c.execute("SELECT tile_matrix_set_name FROM "
                  "gpkg_2d_gridded_coverage_ancillary WHERE "
                  "datatype = 'float'")
        rows = c.fetchall()
        for (tile_matrix_set_name, ) in rows:
            c.execute("SELECT 1 FROM gpkg_2d_gridded_tile_ancillary WHERE "
                      "tpudt_name = ? AND "
                      "NOT (offset == 0.0 AND scale == 1.0)",
                      (tile_matrix_set_name,))
            self._assert(len(c.fetchall()) == 0, 'gpkg_2d_gridded_coverage#9',
                         "Wrong scale and offset values " +
                         "for %s " % tile_matrix_set_name +
                         "in gpkg_2d_gridded_coverage_ancillary")

        for table in tables:
            c.execute("SELECT 1 FROM gpkg_2d_gridded_tile_ancillary WHERE " +
                      "tpudt_name = ? AND tpudt_id NOT IN (SELECT id FROM " +
                      "%s)" % table, (table,))
            self._assert(len(c.fetchall()) == 0, 'gpkg_2d_gridded_coverage#12',
                         "tpudt_id in gpkg_2d_gridded_coverage_ancillary " +
                         "not referencing an id from %s" % table)

        c.execute("SELECT tile_matrix_set_name, datatype FROM "
                  "gpkg_2d_gridded_coverage_ancillary")
        rows = c.fetchall()
        warn_gdal_not_available = False
        for (table_name, datatype) in rows:
            c.execute("SELECT id, tile_data FROM %s" % _esc_id(table_name))
            for (ident, blob) in c.fetchall():
                self._assert(blob is not None and len(blob) >= 12, 19,
                             'Invalid blob')
                max_size_needed = 12
                blob_ar = struct.unpack('B' * max_size_needed,
                                        blob[0:max_size_needed])
                is_png = blob_ar[0:4] == (0x89, 0x50, 0x4E, 0x47)
                is_tiff = blob_ar[0:4] == (0x49, 0x49, 0x2A, 0x00) or \
                    blob_ar[0:4] == (0x4D, 0x4D, 0x00, 0x2A)
                if datatype == 'integer':
                    self._assert(is_png, 'gpkg_2d_gridded_coverage#13',
                                 'Tile for %s should be PNG' % table_name)
                    if has_gdal:
                        tmp_file = '/vsimem/temp_validate_gpkg.tif'
                        gdal.FileFromMemBuffer(tmp_file, bytes(blob))
                        ds = gdal.Open(tmp_file)
                        try:
                            self._assert(ds is not None, 'gpkg_2d_gridded_coverage#13',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            self._assert(ds.RasterCount == 1, 'gpkg_2d_gridded_coverage#13',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            self._assert(ds.GetRasterBand(1).DataType ==
                                         gdal.GDT_UInt16, 'gpkg_2d_gridded_coverage#13',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                        finally:
                            gdal.Unlink(tmp_file)
                    else:
                        if not warn_gdal_not_available:
                            warn_gdal_not_available = True
                            self._log('GDAL not available. Req gpkg_2d_gridded_coverage#13 not tested')

                elif datatype == 'float':
                    self._assert(is_tiff, 'gpkg_2d_gridded_coverage#14',
                                 'Tile for %s should be TIFF' % table_name)
                    if has_gdal:
                        tmp_file = '/vsimem/temp_validate_gpkg.tif'
                        gdal.FileFromMemBuffer(tmp_file, bytes(blob))
                        ds = gdal.Open(tmp_file)
                        try:
                            self._assert(ds is not None, 'gpkg_2d_gridded_coverage#15',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            self._assert(ds.RasterCount == 1, 'gpkg_2d_gridded_coverage#16',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            self._assert(ds.GetRasterBand(1).DataType ==
                                         gdal.GDT_Float32, 'gpkg_2d_gridded_coverage#17',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            compression = ds.GetMetadataItem('COMPRESSION',
                                                             'IMAGE_STRUCTURE')
                            self._assert(compression is None or
                                         compression == 'LZW', 'gpkg_2d_gridded_coverage#18',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            ovr_count = ds.GetRasterBand(1).GetOverviewCount()
                            self._assert(not ds.GetSubDatasets() and
                                         ovr_count == 0, 'gpkg_2d_gridded_coverage#19',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                            (blockxsize, _) = \
                                ds.GetRasterBand(1).GetBlockSize()
                            self._assert(blockxsize == ds.RasterXSize, 'gpkg_2d_gridded_coverage#20',
                                         'Invalid tile %d in %s' %
                                         (ident, table_name))
                        finally:
                            gdal.Unlink(tmp_file)
                    else:
                        if not warn_gdal_not_available:
                            warn_gdal_not_available = True
                            self._log('GDAL not available. '
                                      'Req gpkg_2d_gridded_coverage#15 to gpkg_2d_gridded_coverage#19 not tested')

    def _check_gpkg_extensions(self, c):

        self._log('Checking gpkg_extensions')
        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        if c.fetchone() is None:
            self._log('... No extensions')
            return

        c.execute("PRAGMA table_info(gpkg_extensions)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 0, None, 0),
            (1, 'column_name', 'TEXT', 0, None, 0),
            (2, 'extension_name', 'TEXT', 1, None, 0),
            (3, 'definition', 'TEXT', 1, None, 0),
            (4, 'scope', 'TEXT', 1, None, 0)]
        self._check_structure(columns, expected_columns, 58,
                              'gpkg_extensions')

        c.execute("SELECT table_name, column_name FROM gpkg_extensions WHERE "
                  "table_name IS NOT NULL")
        rows = c.fetchall()
        for (table_name, column_name) in rows:

            # Doesn't work for gpkg_2d_gridded_coverage_ancillary
            # c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ?", \
            #          (table_name,) )
            # ret = c.fetchall()
            # self._assert(len(ret) == 1, \
            #    60, ('table_name = %s is registered in ' +\
            #    'gpkg_extensions, but not in gpkg_contents') % table_name)

            if column_name is not None:
                try:
                    c.execute('SELECT %s FROM %s' %
                              (_esc_id(column_name), _esc_id(table_name)))
                    c.fetchone()
                except:
                    self._assert(False, 61,
                                 ("Column %s of table %s mentioned in " +
                                  "gpkg_extensions doesn't exist") %
                                 (column_name, table_name))

        c.execute("SELECT extension_name FROM gpkg_extensions")
        rows = c.fetchall()
        KNOWN_EXTENSIONS = ['gpkg_rtree_index',
                            'gpkg_zoom_other',
                            'gpkg_webp',
                            'gpkg_metadata',
                            'gpkg_schema',
                            'gpkg_crs_wkt',
                            'gpkg_elevation_tiles',  # deprecated one
                            'gpkg_2d_gridded_coverage'
                            ]
        for geom_name in GPKGChecker.EXT_GEOM_TYPES:
            KNOWN_EXTENSIONS += ['gpkg_geom_' + geom_name]

        for (extension_name,) in rows:

            if extension_name.startswith('gpkg_'):
                self._assert(extension_name in KNOWN_EXTENSIONS,
                             62,
                             "extension_name %s not valid" % extension_name)
            else:
                self._assert('_' in extension_name,
                             62,
                             "extension_name %s not valid" % extension_name)
                author = extension_name[0:extension_name.find('_')]
                ext_name = extension_name[extension_name.find('_') + 1:]
                for x in author:
                    self._assert((x >= 'a' and x <= 'z') or
                                 (x >= 'A' and x <= 'Z') or
                                 (x >= '0' and x <= '9'),
                                 62,
                                 "extension_name %s not valid" %
                                 extension_name)
                for x in ext_name:
                    self._assert((x >= 'a' and x <= 'z') or
                                 (x >= 'A' and x <= 'Z') or
                                 (x >= '0' and x <= '9') or x == '_',
                                 62,
                                 "extension_name %s not valid" %
                                 extension_name)

        # c.execute("SELECT extension_name, definition FROM gpkg_extensions "
        #           "WHERE definition NOT LIKE 'Annex %' AND "
        #           "definition NOT LIKE 'http%' AND "
        #           "definition NOT LIKE 'mailto:%' AND "
        #           "definition NOT LIKE 'Extension Title%' ")
        # rows = c.fetchall()
        # for (extension_name, definition) in rows:
        #     self._assert(False, 63,
        #                  "extension_name %s has invalid definition %s" %
        #                  (extension_name, definition))

        c.execute("SELECT extension_name, scope FROM gpkg_extensions "
                  "WHERE scope NOT IN ('read-write', 'write-only')")
        rows = c.fetchall()
        for (extension_name, scope) in rows:
            self._assert(False, 64,
                         "extension_name %s has invalid scope %s" %
                         (extension_name, scope))

        c.execute("SELECT table_name, scope FROM gpkg_extensions "
                  "WHERE extension_name = 'gpkg_rtree_index' ")
        rows = c.fetchall()
        for (table_name, scope) in rows:
            c.execute("SELECT 1 FROM gpkg_contents WHERE lower(table_name) = lower(?) "
                      "AND data_type = 'features'", (table_name,))
            self._assert(c.fetchone() is not None, 75,
                         ('gpkg_extensions declares gpkg_rtree_index for %s,' +
                          ' but this is not a features table') % table_name)

            self._assert(scope == 'write-only', 75,
                         'Invalid scope %s for gpkg_rtree_index' % scope)

    def _check_metadata(self, c):

        self._log('Checking gpkg_metadata')

        must_have_gpkg_metadata = False
        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        if c.fetchone() is not None:
            c.execute("SELECT scope FROM gpkg_extensions WHERE "
                      "extension_name = 'gpkg_metadata'")
            row = c.fetchone()
            if row is not None:
                must_have_gpkg_metadata = True
                (scope, ) = row
                self._assert(scope == 'read-write', 140,
                             "Wrong scope for gpkg_metadata in "
                             "gpkg_extensions")

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_metadata'")
        if c.fetchone() is None:
            if must_have_gpkg_metadata:
                self._assert(False, 140, "gpkg_metadata table missing")
            else:
                self._log('... No metadata')
            return

        c.execute("PRAGMA table_info(gpkg_metadata)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'id', 'INTEGER', 1, None, 1),
            (1, 'md_scope', 'TEXT', 1, "'dataset'", 0),
            (2, 'md_standard_uri', 'TEXT', 1, None, 0),
            (3, 'mime_type', 'TEXT', 1, "'text/xml'", 0),
            (4, 'metadata', 'TEXT', 1, "''", 0)
        ]
        self._check_structure(columns, expected_columns, 93,
                              'gpkg_metadata')

        c.execute("SELECT 1 FROM sqlite_master "
                  "WHERE name = 'gpkg_metadata_reference'")
        self._assert(c.fetchone() is not None, 95,
                     "gpkg_metadata_reference is missing")

        c.execute("PRAGMA table_info(gpkg_metadata_reference)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'reference_scope', 'TEXT', 1, None, 0),
            (1, 'table_name', 'TEXT', 0, None, 0),
            (2, 'column_name', 'TEXT', 0, None, 0),
            (3, 'row_id_value', 'INTEGER', 0, None, 0),
            (4, 'timestamp', 'DATETIME', 1,
             "strftime('%Y-%m-%dT%H:%M:%fZ','now')", 0),
            (5, 'md_file_id', 'INTEGER', 1, None, 0),
            (6, 'md_parent_id', 'INTEGER', 0, None, 0)
        ]
        self._check_structure(columns, expected_columns, 95,
                              'gpkg_metadata_reference')

        c.execute("SELECT DISTINCT md_scope FROM gpkg_metadata WHERE "
                  "md_scope NOT IN ('undefined', 'fieldSession', "
                  "'collectionSession', 'series', 'dataset', 'featureType', "
                  "'feature', 'attributeType', 'attribute', 'tile', "
                  "'model', 'catalog', 'schema', 'taxonomy', 'software', "
                  "'service', 'collectionHardware', 'nonGeographicDataset', "
                  "'dimensionGroup')")
        rows = c.fetchall()
        for (md_scope, ) in rows:
            self._assert(False, 94, 'Invalid md_scope %s found' % md_scope)

        c.execute("SELECT DISTINCT reference_scope FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope NOT IN ('geopackage', 'table', "
                  "'column', 'row', 'row/col')")
        rows = c.fetchall()
        for (md_scope, ) in rows:
            self._assert(False, 96,
                         'Invalid reference_scope %s found' % md_scope)

        c.execute("SELECT table_name FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope = 'geopackage' AND table_name is NOT NULL")
        rows = c.fetchall()
        for (table_name, ) in rows:
            self._assert(False, 97,
                         "row in gpkg_metadata_reference with table_name " +
                         "not null (%s)" % table_name +
                         "but reference_scope = geopackage")

        c.execute("SELECT table_name FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope != 'geopackage'")
        rows = c.fetchall()
        for (table_name, ) in rows:
            self._assert(table_name is not None, 97,
                         "row in gpkg_metadata_reference with null table_name")
            c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ?",
                      (table_name,))
            self._assert(c.fetchone() is not None, 97,
                         "row in gpkg_metadata_reference with table_name " +
                         "not null (%s) with no reference in " % table_name +
                         "gpkg_contents but reference_scope != geopackage")

        c.execute("SELECT table_name FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope IN ('geopackage', 'table', 'row') "
                  "AND column_name is NOT NULL")
        rows = c.fetchall()
        for (table_name, ) in rows:
            self._assert(False, 98,
                         "row in gpkg_metadata_reference with column_name " +
                         "not null (table=%s)" % table_name +
                         "but reference_scope = geopackage, table or row")

        c.execute("SELECT table_name, column_name FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope NOT IN ('geopackage', 'table', 'row')")
        rows = c.fetchall()
        for (table_name, column_name) in rows:
            self._assert(column_name is not None, 98,
                         "row in gpkg_metadata_reference with null "
                         "column_name")
            try:
                c.execute("SELECT %s FROM %s" %
                          (_esc_id(column_name), _esc_id(table_name)))
            except:
                self._assert(False, 98,
                             "column %s of %s does not exist" %
                             (column_name, table_name))

        c.execute("SELECT table_name FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope IN ('geopackage', 'table', 'column') "
                  "AND row_id_value is NOT NULL")
        rows = c.fetchall()
        for (table_name, ) in rows:
            self._assert(False, 99,
                         "row in gpkg_metadata_reference with row_id_value " +
                         "not null (table=%s)" % table_name +
                         "but reference_scope = geopackage, table or column")

        c.execute("SELECT table_name, row_id_value FROM "
                  "gpkg_metadata_reference WHERE "
                  "reference_scope NOT IN ('geopackage', 'table', 'column')")
        rows = c.fetchall()
        for (table_name, row_id_value) in rows:
            self._assert(row_id_value is not None, 99,
                         "row in gpkg_metadata_reference with null "
                         "row_id_value")
            c.execute("SELECT 1 FROM %s WHERE ROWID = ?" %
                      _esc_id(column_name), (row_id_value, ))
            self._assert(c.fetchone() is not None, 99,
                         "row %s of %s does not exist" %
                         (str(row_id_value), table_name))

        c.execute("SELECT timestamp FROM gpkg_metadata_reference")
        rows = c.fetchall()
        for (timestamp, ) in rows:
            try:
                datetime.datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S.%fZ')
            except ValueError:
                self._assert(False, 100,
                             ('timestamp = %s in gpkg_metadata_reference' +
                              'is invalid datetime') % (timestamp))

        c.execute("SELECT md_file_id FROM gpkg_metadata_reference")
        rows = c.fetchall()
        for (md_file_id, ) in rows:
            c.execute("SELECT 1 FROM gpkg_metadata WHERE id = ?",
                      (md_file_id,))
            self._assert(c.fetchone() is not None, 101,
                         "md_file_id = %s " % str(md_file_id) +
                         "does not have a row in gpkg_metadata")

        c.execute("SELECT md_parent_id FROM gpkg_metadata_reference "
                  "WHERE md_parent_id IS NOT NULL")
        rows = c.fetchall()
        for (md_parent_id, ) in rows:
            c.execute("SELECT 1 FROM gpkg_metadata WHERE id = ?",
                      (md_parent_id,))
            self._assert(c.fetchone() is not None, 102,
                         "md_parent_id = %s " % str(md_parent_id) +
                         "does not have a row in gpkg_metadata")

        c.execute("SELECT md_file_id FROM "
                  "gpkg_metadata_reference WHERE md_parent_id IS NOT NULL "
                  "AND md_file_id = md_parent_id")
        rows = c.fetchall()
        for (md_file_id, ) in rows:
            self._assert(False, 102,
                         "Row with md_file_id = md_parent_id = %s " %
                         str(md_file_id))

    def _check_schema(self, c):

        self._log('Checking gpkg_schema')

        must_have_gpkg_schema = False
        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_extensions'")
        if c.fetchone() is not None:
            c.execute("SELECT scope FROM gpkg_extensions WHERE "
                      "extension_name = 'gpkg_schema'")
            row = c.fetchone()
            if row is not None:
                must_have_gpkg_schema = True
                (scope, ) = row
                self._assert(scope == 'read-write', 141,
                             "Wrong scope for gpkg_schema in "
                             "gpkg_extensions")

                self._assert(c.fetchone() is not None, 141,
                             "There should be exactly 2 rows with " +
                             "extension_name = " +
                             "'gpkg_schema' in gpkg_extensions")
                self._assert(c.fetchone() is None, 141,
                             "There should be exactly 2 rows with " +
                             "extension_name = " +
                             "'gpkg_schema' in gpkg_extensions")

                c.execute("SELECT 1 FROM gpkg_extensions WHERE "
                          "extension_name = 'gpkg_schema' AND "
                          "column_name IS NOT NULL")
                row = c.fetchone()
                if row is not None:
                    self._assert(False, 141,
                                 "gpkg_extensions contains row(s) with " +
                                 "gpkg_schema and a not-NULL column_name")

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_data_columns'")
        if c.fetchone() is None:
            if must_have_gpkg_schema:
                self._assert(False, 141, "gpkg_data_columns table missing.")
            else:
                self._log('... No schema')
            return

        c.execute("PRAGMA table_info(gpkg_data_columns)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'table_name', 'TEXT', 1, None, 1),
            (1, 'column_name', 'TEXT', 1, None, 2),
            (2, 'name', 'TEXT', 0, None, 0),
            (3, 'title', 'TEXT', 0, None, 0),
            (4, 'description', 'TEXT', 0, None, 0),
            (5, 'mime_type', 'TEXT', 0, None, 0),
            (6, 'constraint_name', 'TEXT', 0, None, 0)
        ]
        self._check_structure(columns, expected_columns, 103,
                              'gpkg_data_columns')

        c.execute("SELECT table_name, column_name FROM gpkg_data_columns")
        rows = c.fetchall()
        for (table_name, column_name) in rows:
            c.execute("SELECT 1 FROM gpkg_contents WHERE table_name = ?",
                      (table_name,))
            if c.fetchone() is None:
                c.execute("SELECT 1 FROM gpkg_extensions WHERE table_name = ?",
                          (table_name,))
                self._assert(c.fetchone(), 104,
                             ("table_name = %s " % table_name +
                              "in gpkg_data_columns refer to non-existing " +
                              "table/view in gpkg_contents or gpkg_extensions"))

            try:
                c.execute("SELECT %s FROM %s" % (_esc_id(column_name),
                                                 _esc_id(table_name)))
            except sqlite3.OperationalError:
                self._assert(False, 105,
                             ("table_name = %s, " % table_name +
                              "column_name = %s " % column_name +
                              "in gpkg_data_columns refer to non-existing " +
                              "column"))

        c.execute("SELECT 1 FROM sqlite_master WHERE name = 'gpkg_data_column_constraints'")
        if c.fetchone() is None:
            self._assert(False, 141, "gpkg_data_column_constraints table missing.")

        c.execute("PRAGMA table_info(gpkg_data_column_constraints)")
        columns = c.fetchall()
        expected_columns = [
            (0, 'constraint_name', 'TEXT', 1, None, 0),
            (1, 'constraint_type', 'TEXT', 1, None, 0),
            (2, 'value', 'TEXT', 0, None, 0),
            (3, 'min', 'NUMERIC', 0, None, 0),
            (4, 'min_is_inclusive', 'BOOLEAN', 0, None, 0),
            (5, 'max', 'NUMERIC', 0, None, 0),
            (6, 'max_is_inclusive', 'BOOLEAN', 0, None, 0),
            (7, 'description', 'TEXT', 0, None, 0),
        ]
        self._check_structure(columns, expected_columns, 107,
                              'gpkg_data_column_constraints')

        c.execute("SELECT DISTINCT constraint_type FROM " +
                  "gpkg_data_column_constraints WHERE constraint_type " +
                  "NOT IN ('range', 'enum', 'glob')")
        if c.fetchone() is not None:
            self._assert(False, 108,
                         "gpkg_data_column_constraints.constraint_type " +
                         "contains value other than range, enum, glob")

        c.execute("SELECT 1 FROM (SELECT COUNT(constraint_name) AS c FROM " +
                  "gpkg_data_column_constraints WHERE constraint_type " +
                  "IN ('range', 'glob') GROUP BY constraint_name) u " +
                  "WHERE u.c != 1")
        if c.fetchone() is not None:
            self._assert(False, 109,
                         "gpkg_data_column_constraints contains non unique " +
                         "constraint_name for constraints of type range/glob")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND value IS NOT NULL")
        if c.fetchone() is not None:
            self._assert(False, 110,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose 'value' column is not null")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND min IS NULL")
        if c.fetchone() is not None:
            self._assert(False, 111,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose min value is NULL")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND max IS NULL")
        if c.fetchone() is not None:
            self._assert(False, 111,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose max value is NULL")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND min >= max")
        if c.fetchone() is not None:
            self._assert(False, 111,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose min value is not less than max")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND min_is_inclusive NOT IN (0,1)")
        if c.fetchone() is not None:
            self._assert(False, 112,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose min_is_inclusive value is " +
                         "not 0 or 1")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'range' AND max_is_inclusive NOT IN (0,1)")
        if c.fetchone() is not None:
            self._assert(False, 112,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type range whose max_is_inclusive value is " +
                         "not 0 or 1")

        for col_name in ('min', 'min_is_inclusive', 'max', 'max_is_inclusive'):
            c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                      "constraint_type IN ('enum', 'glob') AND " +
                      col_name + " IS NOT NULL")
            if c.fetchone() is not None:
                self._assert(False, 113,
                             "gpkg_data_column_constraints contains constraint " +
                             "of type enum or glob whose " + col_name +
                             " column is not NULL")

        c.execute("SELECT 1 FROM gpkg_data_column_constraints WHERE " +
                  "constraint_type = 'enum' AND value IS NULL")
        if c.fetchone() is not None:
            self._assert(False, 114,
                         "gpkg_data_column_constraints contains constraint " +
                         "of type enum whose value column is NULL")

    def check(self):
        self._assert(os.path.exists(self.filename), None,
                     "%s does not exist" % self.filename)

        self._assert(self.filename.lower().endswith('.gpkg'), 3,
                     "filename extension isn't .gpkg'")

        with open(self.filename, 'rb') as f:
            f.seek(68, 0)
            application_id = struct.unpack('B' * 4, f.read(4))
            gp10 = struct.unpack('B' * 4, 'GP10'.encode('ASCII'))
            gp11 = struct.unpack('B' * 4, 'GP11'.encode('ASCII'))
            gpkg = struct.unpack('B' * 4, 'GPKG'.encode('ASCII'))
            self._assert(application_id in (gp10, gp11, gpkg), 2,
                         ("Wrong application_id: %s. " +
                          "Expected one of GP10, GP11, GPKG") %
                         str(application_id))

            if application_id == gpkg:
                f.seek(60, 0)
                user_version = f.read(4)
                expected_version = 10200
                user_version = struct.unpack('>I', user_version)[0]
                self._assert(user_version >= expected_version, 2,
                             'Wrong user_version: %d. Expected >= %d' %
                             (user_version, expected_version))

        conn = sqlite3.connect(':memory:')
        c = conn.cursor()
        c.execute('CREATE TABLE foo(one TEXT, two TEXT, '
                  'CONSTRAINT pk PRIMARY KEY (one, two))')
        c.execute('PRAGMA table_info(foo)')
        rows = c.fetchall()
        if rows[1][5] == 2:
            self.extended_pragma_info = True
        c.close()
        conn.close()

        conn = sqlite3.connect(self.filename)
        c = conn.cursor()
        try:
            try:
                c.execute('SELECT 1 FROM sqlite_master')
                c.fetchone()
            except:
                self._assert(False, 1, 'not a sqlite3 database')

            c.execute('PRAGMA foreign_key_check')
            ret = c.fetchall()
            self._assert(len(ret) == 0, 7,
                         'foreign_key_check failed: %s' % str(ret))

            c.execute('PRAGMA integrity_check')
            self._assert(c.fetchone()[0] == 'ok', 6, 'integrity_check failed')

            self._check_gpkg_spatial_ref_sys(c)

            self._check_gpkg_contents(c)

            self._check_features(c)

            self._check_tiles(c)

            self._check_attributes(c)

            self._check_tiled_gridded_coverage_data(c)

            self._check_gpkg_extensions(c)

            self._check_metadata(c)

            self._check_schema(c)

        finally:
            c.close()
            conn.close()


def check(filename, abort_at_first_error=True, verbose=False):

    checker = GPKGChecker(filename,
                          abort_at_first_error=abort_at_first_error,
                          verbose=verbose)
    checker.check()
    return checker.errors


def Usage():
    print('validate_gpkg.py [[-v]|[-q]] [-k] my.gpkg')
    print('')
    print('-q: quiet mode')
    print('-k: (try to) keep going when error is encountered')
    return 1


def main(argv):
    filename = None
    verbose = False
    abort_at_first_error = True
    if len(argv) == 1:
        return Usage()
    for arg in argv[1:]:
        if arg == '-k':
            abort_at_first_error = False
        elif arg == '-q':
            verbose = False
        elif arg == '-v':
            verbose = True
        elif arg[0] == '-':
            return Usage()
        else:
            filename = arg
    if filename is None:
        return Usage()
    ret = check(filename, abort_at_first_error=abort_at_first_error,
                verbose=verbose)
    if not abort_at_first_error:
        if not ret:
            return 0
        else:
            for (req, msg) in ret:
                if req:
                    print('Req %d: %s' % (req, msg))
                else:
                    print(msg)
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
