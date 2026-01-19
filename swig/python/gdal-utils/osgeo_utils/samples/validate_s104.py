#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR
# Purpose:  Test compliance of IHO S104 v2.0 dataset
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

# Validates against
# https://registry.iho.int/productspec/view.do?idx=209&product_ID=S-104&statusS=5&domainS=ALL&category=product_ID&searchValue= and
# https://iho.int/uploads/user/pubs/standards/s-100/S-100_5.2.0_Final_Clean.pdf

# "104_DevXXXX" are for traceability with respect to requirements of the spreadsheet:
# https://github.com/iho-ohi/S-100-Validation-Checks/raw/refs/heads/main/Documents/S-158-104/0.2.0/S-158_104_0_2_0_20241209.xlsx
# Note that there are a few checks in that spreadsheet that are specific only of 1.1.0, and not 2.0.0...


import os
import re
import struct
import sys

# Standard Python modules
from collections import namedtuple

# Extension modules
import h5py
import numpy as np

try:
    from osgeo import osr

    osr.UseExceptions()
    gdal_available = True
except ImportError:
    gdal_available = False

ERROR = "Error"
CRITICAL_ERROR = "Critical error"

AttributeDefinition = namedtuple(
    "AttributeDefinition", ["name", "required", "type", "fixed_value"]
)


def _get_int_value_or_none(v):
    try:
        return int(v)
    except ValueError:
        return None


def _get_int_attr_or_none(group, attr_name):
    if attr_name not in group.attrs:
        return None
    return _get_int_value_or_none(group.attrs[attr_name])


def _get_float_value_or_none(v):
    try:
        return float(v)
    except ValueError:
        return None


def _get_float_attr_or_none(group, attr_name):
    if attr_name not in group.attrs:
        return None
    return _get_float_value_or_none(group.attrs[attr_name])


def _cast_to_float32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


class S104ValidationException(Exception):
    pass


class S104Checker:
    def __init__(self, filename, abort_at_first_error=False):
        self.filename = filename
        self.abort_at_first_error = abort_at_first_error
        self.errors = []
        self.warnings = []
        self.checks_done = set([])

    def _log_check(self, name):
        self.checks_done.add(name)

    def _warning(self, msg):
        self.warnings += [msg]

    def _error(self, msg):
        self.errors += [(ERROR, msg)]
        if self.abort_at_first_error:
            raise S104ValidationException(f"{ERROR}: {msg}")

    def _critical_error(self, msg):
        self.errors += [(CRITICAL_ERROR, msg)]
        if self.abort_at_first_error:
            raise S104ValidationException(f"{CRITICAL_ERROR}: {msg}")

    def _is_uint8(self, h5_type):
        return (
            isinstance(h5_type, h5py.h5t.TypeIntegerID)
            and h5_type.get_sign() == h5py.h5t.SGN_NONE
            and h5_type.get_size() == 1
        )

    def _is_uint16(self, h5_type):
        return (
            isinstance(h5_type, h5py.h5t.TypeIntegerID)
            and h5_type.get_sign() == h5py.h5t.SGN_NONE
            and h5_type.get_size() == 2
        )

    def _is_uint32(self, h5_type):
        return (
            isinstance(h5_type, h5py.h5t.TypeIntegerID)
            and h5_type.get_sign() == h5py.h5t.SGN_NONE
            and h5_type.get_size() == 4
        )

    def _is_int16(self, h5_type):
        return (
            isinstance(h5_type, h5py.h5t.TypeIntegerID)
            and h5_type.get_sign() == h5py.h5t.SGN_2
            and h5_type.get_size() == 2
        )

    def _is_int32(self, h5_type):
        return (
            isinstance(h5_type, h5py.h5t.TypeIntegerID)
            and h5_type.get_sign() == h5py.h5t.SGN_2
            and h5_type.get_size() == 4
        )

    def _is_float32(self, h5_type):
        return isinstance(h5_type, h5py.h5t.TypeFloatID) and h5_type.get_size() == 4

    def _is_float64(self, h5_type):
        return isinstance(h5_type, h5py.h5t.TypeFloatID) and h5_type.get_size() == 8

    def _is_string(self, h5_type):
        return isinstance(h5_type, h5py.h5t.TypeStringID)

    def _is_enumeration(self, h5_type):
        return isinstance(h5_type, h5py.h5t.TypeEnumID)

    def _check_attributes(self, ctxt_name, group, attr_list):

        for attr_def in attr_list:
            if attr_def.required and attr_def.name not in group.attrs:
                # 104_Dev1002: check presence of required attributes
                self._error(
                    f"Required {ctxt_name} attribute '{attr_def.name}' is missing"
                )

            elif attr_def.name in group.attrs:
                attr = group.attrs[attr_def.name]
                if isinstance(attr, bytes):
                    attr = attr.decode("utf-8")
                h5_type = group.attrs.get_id(attr_def.name).get_type()

                # 104_Dev1002: check type

                if attr_def.type == "string":
                    if not self._is_string(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string "
                        )

                elif attr_def.type == "time":
                    if not self._is_string(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string"
                        )

                    pattern = re.compile(
                        r"^(?:[01]\d|2[0-3])[0-5]\d[0-5]\d(?:Z|[+-](?:[01]\d|2[0-3])[0-5]\d)$"
                    )
                    if not pattern.match(attr):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a valid time: {attr}"
                        )

                elif attr_def.type == "date":
                    if not isinstance(h5_type, h5py.h5t.TypeStringID):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string"
                        )
                    elif h5_type.get_size() != 8:
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a 8-character string"
                        )

                    pattern = re.compile(
                        r"^(?:[0-9]{4})(?:(?:0[1-9]|1[0-2])(?:0[1-9]|[12][0-9]|3[01]))$"
                    )
                    if not pattern.match(attr):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a valid date: {attr}"
                        )

                elif attr_def.type == "datetime":
                    if not isinstance(h5_type, h5py.h5t.TypeStringID):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string"
                        )
                    pattern = re.compile(
                        r"^(?:"
                        r"([0-9]{4})"  # year
                        r"(?:(?:0[1-9]|1[0-2])"  # month
                        r"(?:0[1-9]|[12][0-9]|3[01]))"  # day
                        r"T"  # literal 'T' separator
                        r"(?:[01]\d|2[0-3])"  # hour
                        r"[0-5]\d"  # minute
                        r"[0-5]\d"  # second
                        r"(?:Z|[+-](?:[01]\d|2[0-3])[0-5]\d)"  # timezone (Z or hhmm)
                        r")$"
                    )
                    if not pattern.match(attr):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a valid datetime: {attr}"
                        )

                elif attr_def.type == "uint8":
                    if not self._is_uint8(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint8"
                        )

                elif attr_def.type == "uint16":
                    if not self._is_uint16(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint16"
                        )

                elif attr_def.type == "uint32":
                    if not self._is_uint32(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint32"
                        )

                elif attr_def.type == "int32":
                    if not self._is_int32(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a int32"
                        )

                elif attr_def.type == "float32":
                    if not self._is_float32(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a float32"
                        )

                elif attr_def.type == "float64":
                    if not self._is_float64(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a float64"
                        )

                elif attr_def.type == "enumeration":
                    if not self._is_enumeration(h5_type):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not an enumeration"
                        )

                else:

                    raise Exception(
                        f"Programming error: unexpected type {attr_def.type}"
                    )

                if attr_def.fixed_value:
                    if attr != attr_def.fixed_value:
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' has value '{attr}', whereas '{attr_def.fixed_value}' is expected"
                        )

        attr_dict = {a.name: a for a in attr_list}
        for attr in group.attrs:
            if attr not in attr_dict:
                self._warning(f"Extra element in {ctxt_name} group: '{attr}'")

    def check(self):

        try:
            f = h5py.File(self.filename, "r")
        except Exception as e:
            self._critical_error(str(e))
            return

        self._log_check("104_Dev9005")
        file_size = os.stat(self.filename).st_size
        if file_size > 10 * 1024 * 1024:
            self._warning(
                f"File size of {self.filename} = {file_size}, which exceeds 10 MB"
            )

        basename = os.path.basename(self.filename)
        if not basename.startswith("104"):
            self._warning("File name should start with '104'")
        if not basename.upper().endswith(".H5"):
            self._warning("File name should end with '.H5'")
        pattern = r"^104[a-zA-Z0-9]{4}[a-zA-Z0-9\-_]{1,54}\.(?:h5|H5)$"
        if not re.match(pattern, basename):
            self._warning(
                f"File name '{basename}' does not match expected pattern '{pattern}'"
            )

        self._log_check("104_Dev1018")
        for key in f.keys():
            if key not in (
                "Group_F",
                "WaterLevel",
            ):
                self._warning(f"Unexpected element {key} in top level group")

        if "Group_F" in f.keys():
            self._validate_group_f(f, f["Group_F"])
        else:
            self._critical_error("No feature information group ('Group_F')")

        # Cf Table 12-1 - General metadata, related to the entire HDF5 file
        topLevelAttributesList = [
            AttributeDefinition(
                name="productSpecification",
                required=True,
                type="string",
                fixed_value="INT.IHO.S-104.2.0",
            ),
            AttributeDefinition(
                name="issueDate", required=True, type="date", fixed_value=None
            ),
            AttributeDefinition(
                name="horizontalCRS", required=True, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="westBoundLongitude",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="eastBoundLongitude",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="southBoundLatitude",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="northBoundLatitude",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="geographicIdentifier",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="nameOfHorizontalCRS",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="typeOfHorizontalCRS",
                required=False,
                type="enumeration",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="horizontalCS", required=False, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="horizontalDatum", required=False, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="nameOfHorizontalDatum",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="primeMeridian", required=False, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="spheroid", required=False, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="projectionMethod", required=False, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="projectionParameter1",
                required=False,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="projectionParameter2",
                required=False,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="projectionParameter3",
                required=False,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="projectionParameter4",
                required=False,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="projectionParameter5",
                required=False,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="falseNorthing", required=False, type="float64", fixed_value=None
            ),
            AttributeDefinition(
                name="falseEasting", required=False, type="float64", fixed_value=None
            ),
            AttributeDefinition(
                name="epoch", required=False, type="string", fixed_value=None
            ),
            AttributeDefinition(
                name="issueTime", required=True, type="time", fixed_value=None
            ),
            AttributeDefinition(
                name="waterLevelTrendThreshold",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="datasetDeliveryInterval",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="trendInterval", required=False, type="uint32", fixed_value=None
            ),
            AttributeDefinition(
                name="verticalDatumEpoch",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalCS", required=True, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="verticalCoordinateBase",
                required=True,
                type="enumeration",
                fixed_value=2,
            ),
            AttributeDefinition(
                name="verticalDatumReference",
                required=True,
                type="enumeration",
                fixed_value=None,
            ),
            # S104_Dev1003
            AttributeDefinition(
                name="verticalDatum", required=True, type="int32", fixed_value=None
            ),
        ]

        self._log_check("104_Dev1002")
        self._log_check("104_Dev1003")
        self._check_attributes("top level", f, topLevelAttributesList)

        self._log_check("104_Dev1004")
        if "epoch" in f.attrs and not f.attrs["epoch"]:
            self._warning("Attribute epoch present but empty or blank.")

        self._log_check("104_Dev1005")
        if "verticalDatum" in f.attrs and not f.attrs["verticalDatum"]:
            self._warning("Attribute verticalDatum present but empty or blank.")

        self._log_check("104_Dev1007")
        self._validate_metadata(f, self.filename)
        self._validate_nameOfHorizontalCRS(f)
        self._validate_typeOfHorizontalCRS(f)
        self._validate_horizontalCS(f)
        self._validate_horizontalDatum(f)
        self._validate_nameOfHorizontalDatum(f)
        self._validate_primeMeridian(f)
        self._validate_spheroid(f)
        self._validate_projectionMethod(f)
        self._validate_projectionParameters(f)
        self._validate_datasetDeliveryInterval(f)
        self._validate_verticalCS(f)
        self._validate_verticalCoordinateBase(f)
        self._validate_verticalDatumReference(f)
        self._validate_verticalDatum("top level", f)
        self._validate_epoch(f)
        self._validate_horizontalCRS(f)
        self._validate_bounds("top level", f)

        if "WaterLevel" in f.keys():
            self._validate_WaterLevel(f)
        else:
            self._critical_error("Missing /WaterLevel group")

        self.checks_done = sorted(self.checks_done)

    def _validate_enumeration(self, group, attr_name, expected_values):
        h5_type = group.attrs.get_id(attr_name).get_type()
        if isinstance(h5_type, h5py.h5t.TypeEnumID):
            if h5_type.get_nmembers() != len(expected_values):
                self._warning(
                    f"Expected {len(expected_values)} members for enumeration {attr_name}"
                )
            else:
                for code in expected_values:
                    try:
                        value = h5_type.enum_nameof(code).decode("utf-8")
                    except Exception:
                        value = None
                        self._warning(
                            f"Enumeration {attr_name}: did not find value for code {code}"
                        )
                    if value:
                        expected = expected_values[code]
                        if value != expected:
                            self._error(
                                f"Enumeration {attr_name}: for code {code}, found value {value}, whereas {expected} was expected"
                            )

    def _validate_metadata(self, f, filename):
        if "metadata" in f.attrs:
            metadata = f.attrs["metadata"]
            if isinstance(metadata, str) and metadata:
                basename = os.path.basename(filename)
                if basename.endswith(".h5") or basename.endswith(".H5"):
                    basename = basename[0:-3]
                if metadata not in (f"MD_{basename}.xml", f"MD_{basename}.XML"):
                    self._critical_error(
                        f"Top level attribute metadata has value '{metadata}', whereas it should be empty, 'MD_{basename}.xml' or 'MD_{basename}.XML'"
                    )

    def _is_horizontalCRS_minus_1(self, f):
        return _get_int_attr_or_none(f, "horizontalCRS") == -1

    def _validate_nameOfHorizontalCRS(self, f):
        if "nameOfHorizontalCRS" in f.attrs:
            nameOfHorizontalCRS = f.attrs["nameOfHorizontalCRS"]
            if isinstance(nameOfHorizontalCRS, str) and not nameOfHorizontalCRS:
                self._warning(
                    "Top level attribute nameOfHorizontalCRS must not be the empty string"
                )
        elif self._is_horizontalCRS_minus_1(f):
            self._warning(
                "Top level attribute nameOfHorizontalCRS is missing, but it is mandatory when horizontalCRS = -1"
            )

    def _validate_typeOfHorizontalCRS(self, f):
        if "typeOfHorizontalCRS" in f.attrs:
            expected_values = {
                1: "geodeticCRS2D",
                2: "projectedCRS",
            }
            self._validate_enumeration(f, "typeOfHorizontalCRS", expected_values)
        elif self._is_horizontalCRS_minus_1(f):
            self._warning(
                "Top level attribute typeOfHorizontalCRS is missing, but it is mandatory when horizontalCRS = -1"
            )

    def _validate_horizontalCS(self, f):
        if "horizontalCS" in f.attrs:
            horizontalCS = _get_int_attr_or_none(f, "horizontalCS")
            typeOfHorizontalCRS = _get_int_attr_or_none(f, "typeOfHorizontalCRS")
            if typeOfHorizontalCRS == 1:  # geodeticCRS2D
                if horizontalCS != 6422:
                    self._warning(
                        "Top level attribute horizontalCS value should be 6422 since typeOfHorizontalCRS=1"
                    )
            elif typeOfHorizontalCRS == 2:  # projectedCRS
                if horizontalCS not in (4400, 4500):
                    self._warning(
                        "Top level attribute horizontalCS value should be 4400 or 4500 since typeOfHorizontalCRS=2"
                    )
        elif self._is_horizontalCRS_minus_1(f):
            self._warning(
                "Top level attribute horizontalCS is missing, but it is mandatory when horizontalCRS = -1"
            )

    @staticmethod
    def _get_proj_db():
        try:
            from osgeo import osr
        except ImportError:
            return None
        for path in osr.GetPROJSearchPaths():
            filename = os.path.join(path, "proj.db")
            if os.path.exists(filename):
                import sqlite3

                return sqlite3.connect(filename)
        return None

    def _validate_horizontalDatum(self, f):
        if "horizontalDatum" in f.attrs:
            horizontalDatum = _get_int_attr_or_none(f, "horizontalDatum")
            if horizontalDatum is not None and horizontalDatum != -1:
                conn = S104Checker._get_proj_db()
                if conn:
                    cursor = conn.cursor()
                    cursor.execute(
                        "SELECT 1 FROM geodetic_datum WHERE auth_name = 'EPSG' and code = ?",
                        (horizontalDatum,),
                    )
                    if not cursor.fetchone():
                        self._warning(
                            f"Top level attribute horizontalDatum = {horizontalDatum} does not match with a known EPSG datum"
                        )

        elif self._is_horizontalCRS_minus_1(f):
            self._warning(
                "Top level attribute horizontalDatum is missing, but it is mandatory when horizontalCRS = -1"
            )

    def _is_horizontalDatum_minus_1(self, f):
        return _get_int_attr_or_none(f, "horizontalDatum") == -1

    def _validate_nameOfHorizontalDatum(self, f):
        if "nameOfHorizontalDatum" in f.attrs:
            nameOfHorizontalDatum = f.attrs["nameOfHorizontalDatum"]
            if isinstance(nameOfHorizontalDatum, str) and not nameOfHorizontalDatum:
                self._warning(
                    "Top level attribute nameOfHorizontalDatum must not be the empty string"
                )
        elif self._is_horizontalDatum_minus_1(f):
            self._warning(
                "Top level attribute nameOfHorizontalDatum is missing, but it is mandatory when horizontalDatum = -1"
            )

    def _validate_primeMeridian(self, f):
        if "primeMeridian" in f.attrs:
            primeMeridian = _get_int_attr_or_none(f, "primeMeridian")
            if primeMeridian is not None:
                conn = S104Checker._get_proj_db()
                if conn:
                    cursor = conn.cursor()
                    cursor.execute(
                        "SELECT 1 FROM prime_meridian WHERE auth_name = 'EPSG' and code = ?",
                        (primeMeridian,),
                    )
                    if not cursor.fetchone():
                        self._warning(
                            f"Top level attribute primeMeridian = {primeMeridian} does not match with a known EPSG prime meridian"
                        )

        elif self._is_horizontalDatum_minus_1(f):
            self._warning(
                "Top level attribute primeMeridian is missing, but it is mandatory when horizontalDatum = -1"
            )

    def _validate_spheroid(self, f):
        if "spheroid" in f.attrs:
            spheroid = _get_int_attr_or_none(f, "spheroid")
            if spheroid is not None:
                conn = S104Checker._get_proj_db()
                if conn:
                    cursor = conn.cursor()
                    cursor.execute(
                        "SELECT 1 FROM ellipsoid WHERE auth_name = 'EPSG' and code = ?",
                        (spheroid,),
                    )
                    if not cursor.fetchone():
                        self._warning(
                            f"Top level attribute spheroid = {spheroid} does not match with a known EPSG spheroid"
                        )

        elif self._is_horizontalDatum_minus_1(f):
            self._warning(
                "Top level attribute spheroid is missing, but it is mandatory when horizontalDatum = -1"
            )

    def _validate_projectionMethod(self, f):
        if "projectionMethod" in f.attrs:
            projectionMethod = _get_int_attr_or_none(f, "projectionMethod")
            if projectionMethod is not None:
                conn = S104Checker._get_proj_db()
                if conn:
                    cursor = conn.cursor()
                    cursor.execute(
                        "SELECT 1 FROM conversion_method WHERE auth_name = 'EPSG' and code = ?",
                        (projectionMethod,),
                    )
                    if not cursor.fetchone():
                        self._warning(
                            f"Top level attribute projectionMethod = {projectionMethod} does not match with a known EPSG projectionMethod"
                        )

        else:
            typeOfHorizontalCRS = _get_int_attr_or_none(f, "typeOfHorizontalCRS")
            if typeOfHorizontalCRS == 2:
                self._warning(
                    "Top level attribute projectionMethod is missing, but it is mandatory when typeOfHorizontalCRS = 2"
                )

    def _validate_projectionParameters(self, f):

        for attr_name in (
            "projectionParameter1",
            "projectionParameter2",
            "projectionParameter3",
            "projectionParameter4",
            "projectionParameter5",
            "falseNorthing",
            "falseEasting",
        ):
            if attr_name in f.attrs and "projectionMethod" not in f.attrs:
                self._warning(
                    f"Top level attribute {attr_name} is present, but it should not be because projectionMethod is not set"
                )

    def _validate_datasetDeliveryInterval(self, f):

        if "datasetDeliveryInterval" in f.attrs:
            datasetDeliveryInterval = f.attrs["datasetDeliveryInterval"]
            if isinstance(datasetDeliveryInterval, str):
                iso8601_duration_regex = re.compile(
                    r"^P"  # starts with 'P'
                    r"(?:(\d+(?:\.\d+)?)Y)?"  # years
                    r"(?:(\d+(?:\.\d+)?)M)?"  # months
                    r"(?:(\d+(?:\.\d+)?)W)?"  # weeks
                    r"(?:(\d+(?:\.\d+)?)D)?"  # days
                    r"(?:T"  # optional time part
                    r"(?:(\d+(?:\.\d+)?)H)?"  # hours
                    r"(?:(\d+(?:\.\d+)?)M)?"  # minutes
                    r"(?:(\d+(?:\.\d+)?)S)?"  # seconds
                    r")?$"
                )
                if not iso8601_duration_regex.match(datasetDeliveryInterval):
                    self._error(
                        "Top level attribute datasetDeliveryInterval is not a valid ISO8601 duration"
                    )

    def _validate_verticalCS(self, f):
        verticalCS = _get_int_attr_or_none(f, "verticalCS")
        if verticalCS is not None and verticalCS not in (6498, 6499):
            self._error("Top level attribute verticalCS must be 6498 or 6499")

    def _validate_verticalCoordinateBase(self, f):
        if "verticalCoordinateBase" in f.attrs:
            expected_values = {
                1: "seaSurface",
                2: "verticalDatum",
                3: "seaBottom",
            }
            self._validate_enumeration(f, "verticalCoordinateBase", expected_values)

    def _validate_verticalDatumReference(self, f):
        if "verticalDatumReference" in f.attrs:
            expected_values = {
                1: "s100VerticalDatum",
                2: "EPSG",
            }
            self._validate_enumeration(f, "verticalDatumReference", expected_values)

    def _validate_verticalDatum(self, ctxt_name, f):
        verticalDatum = _get_int_attr_or_none(f, "verticalDatum")
        if verticalDatum is None:
            return
        verticalDatumReference = _get_int_attr_or_none(f, "verticalDatumReference")
        if verticalDatumReference == 1:
            if not (
                (verticalDatum >= 1 and verticalDatum <= 30)
                or verticalDatum in (44, 46, 47, 48, 49)
            ):
                self._warning(
                    f"{ctxt_name} attribute verticalDatum has value '{verticalDatum}', whereas it should be in [1, 30] range or 44, 46, 47, 48 or 49"
                )
        elif verticalDatumReference == 2:
            conn = S104Checker._get_proj_db()
            if conn:
                cursor = conn.cursor()
                cursor.execute(
                    "SELECT 1 FROM vertical_datum WHERE auth_name = 'EPSG' and code = ?",
                    (verticalDatum,),
                )
                if not cursor.fetchone():
                    self._warning(
                        f"{ctxt_name} attribute verticalDatum = {verticalDatum} does not match with a known EPSG verticalDatum"
                    )

    def _validate_epoch(self, f):
        self._log_check("104_Dev1007")
        epoch = _get_float_attr_or_none(f, "epoch")
        if epoch and not (epoch >= 1980 and epoch <= 2100):
            self._warning(f"Top level attribute epoch has invalid value: {epoch}")

    def _validate_horizontalCRS(self, f):
        self._log_check("104_Dev1009")
        horizontalCRS = _get_int_attr_or_none(f, "horizontalCRS")
        if horizontalCRS is not None and horizontalCRS != -1:
            conn = S104Checker._get_proj_db()
            if conn:
                cursor = conn.cursor()
                cursor.execute(
                    "SELECT 1 FROM crs_view WHERE auth_name = 'EPSG' and code = ? and type in ('geographic 2D', 'projected')",
                    (horizontalCRS,),
                )
                if not cursor.fetchone():
                    self._warning(
                        f"Top level attribute horizontalCRS = {horizontalCRS} does not match with a known EPSG horizontal CRS"
                    )

    def _is_geographic_2D(self, f):
        horizontalCRS = _get_int_attr_or_none(f, "horizontalCRS")
        if horizontalCRS is not None:
            if horizontalCRS == 4326:
                return True
            conn = S104Checker._get_proj_db()
            if conn:
                cursor = conn.cursor()
                cursor.execute(
                    "SELECT 1 FROM geodetic_crs WHERE auth_name = 'EPSG' and code = ? and type = 'geographic 2D'",
                    (horizontalCRS,),
                )
                if cursor.fetchone():
                    return True
        return False

    def _validate_bounds(self, ctxt_name, f):
        west = _get_float_attr_or_none(f, "westBoundLongitude")
        east = _get_float_attr_or_none(f, "eastBoundLongitude")
        north = _get_float_attr_or_none(f, "northBoundLatitude")
        south = _get_float_attr_or_none(f, "southBoundLatitude")
        if (
            west is not None
            and east is not None
            and north is not None
            and south is not None
        ):

            if not (west >= -180 and west <= 180):
                self._warning(
                    f"{ctxt_name}: westBoundLongitude is not in [-180, 180] range"
                )
            if not (east >= -180 and east <= 180):
                self._warning(
                    f"{ctxt_name}: eastBoundLongitude is not in [-180, 180] range"
                )
            if west >= east:
                self._warning(
                    f"{ctxt_name}: westBoundLongitude is greater or equal to eastBoundLongitude"
                )
            if not (north >= -90 and north <= 90):
                self._warning(
                    f"{ctxt_name}: northBoundLatitude is not in [-90, 90] range"
                )
            if not (south >= -90 and south <= 90):
                self._warning(
                    f"{ctxt_name}: southBoundLatitude is not in [-90, 90] range"
                )
            if south >= north:
                self._warning(
                    f"{ctxt_name}: southBoundLatitude is greater or equal to northBoundLatitude"
                )

    def _validate_group_f(self, rootGroup, group_f):

        for key in group_f.keys():
            if key not in (
                "featureCode",
                "WaterLevel",
            ):
                self._warning(f"Unexpected element {key} in Group_F")

        self._log_check("104_Dev1008")
        if "featureCode" in group_f.keys():
            self._validate_group_f_featureCode(
                rootGroup, group_f, group_f["featureCode"]
            )
        else:
            self._critical_error(
                "No featureCode array in feature information group ('/Group_F/featureCode')"
            )

    def _validate_group_f_featureCode(self, rootGroup, group_f, featureCode):

        if not isinstance(featureCode, h5py.Dataset):
            self._critical_error("'/Group_F/featureCode' is not a dataset")
            return

        if len(featureCode.shape) != 1:
            self._critical_error(
                "'/Group_F/featureCode' is not a one-dimensional dataset"
            )
            return

        self._log_check("104_Dev1009")
        values = set([v.decode("utf-8") for v in featureCode[:]])
        if "WaterLevel" not in values:
            self._critical_error("WaterLevel feature missing from featureCode array")

        self._log_check("104_Dev1010")
        for value in values:
            if value not in ("WaterLevel",):
                #
                self._critical_error(
                    f"Group_F feature information must correspond to feature catalog. Did not expect {value}"
                )

            if value not in group_f.keys():
                self._critical_error(
                    f"Feature information dataset for feature type {value} missing"
                )

            if value not in rootGroup.keys():
                self._critical_error(f"No feature instances for feature type {value}")

        if "WaterLevel" in group_f.keys():
            self._validate_group_f_WaterLevel(group_f)

    def _validate_group_f_WaterLevel(self, group_f):
        self._log_check("104_Dev1012")

        WaterLevel = group_f["WaterLevel"]
        if not isinstance(WaterLevel, h5py.Dataset):
            self._critical_error("'/Group_F/WaterLevel' is not a dataset")
        elif len(WaterLevel.shape) != 1:
            self._critical_error(
                "'/Group_F/WaterLevel' is not a one-dimensional dataset"
            )
        elif WaterLevel.dtype != [
            ("code", "O"),
            ("name", "O"),
            ("uom.name", "O"),
            ("fillValue", "O"),
            ("datatype", "O"),
            ("lower", "O"),
            ("upper", "O"),
            ("closure", "O"),
        ]:
            self._critical_error("'/Group_F/WaterLevel' has not expected data type")
        else:
            self._log_check("104_Dev1013")

            if WaterLevel.shape not in ((2,), (3,)):
                self._critical_error("'/Group_F/WaterLevel' is not of shape 2 or 3")

            type = WaterLevel.id.get_type()
            assert isinstance(type, h5py.h5t.TypeCompoundID)
            for member_idx in range(type.get_nmembers()):
                subtype = type.get_member_type(member_idx)
                if not isinstance(subtype, h5py.h5t.TypeStringID):
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/WaterLevel is not a string"
                    )
                    return
                if not subtype.is_variable_str():
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/WaterLevel is not a variable length string"
                    )

            values = WaterLevel[:]
            expected_values = [
                (0, 0, "waterLevelHeight"),
                (0, 1, "Water Level Height"),
                (0, 2, "metre"),
                (0, 3, "-9999.00"),
                (0, 4, "H5T_FLOAT"),
                (0, 5, "-99.99"),
                (0, 6, "99.99"),
                (0, 7, "closedInterval"),
                (1, 0, "waterLevelTrend"),
                (1, 1, "Water Level Trend"),
                (1, 2, ""),
                (1, 3, "0"),
                (1, 4, "H5T_ENUM"),
                (1, 5, ""),
                (1, 6, ""),
                (1, 7, ""),
                (2, 0, "uncertainty"),
                (2, 1, "Uncertainty"),
                (2, 2, "metre"),
                (2, 3, "-1.00"),
                (2, 4, "H5T_FLOAT"),
                (2, 5, "0.00"),
                (2, 6, "99.99"),
                (2, 7, "closedInterval"),
            ]

            for row, col, expected_value in expected_values:
                if row < WaterLevel.shape[0]:
                    value = values[row][col].decode("utf-8")
                    if value != expected_value:
                        self._critical_error(
                            f"/Group_F/WaterLevel: row {row}, {col}, got value '{value}', whereas '{expected_value}' is expected"
                        )

    def _validate_WaterLevel(self, f):
        WaterLevel = f["WaterLevel"]
        if not isinstance(WaterLevel, h5py.Group):
            self._critical_error("/WaterLevel is not a group")
            return

        # Cf Table 12-2 - Feature Type metadata, pertaining to the WaterLevel feature type

        self._log_check("104_Dev2002")  # for dimension
        attr_list = [
            AttributeDefinition(
                name="dataCodingFormat",
                required=True,
                type="enumeration",
                fixed_value=2,
            ),
            AttributeDefinition(
                name="dimension",
                required=True,
                type="uint8",
                fixed_value=2,
            ),
            AttributeDefinition(
                name="commonPointRule",
                required=True,
                type="enumeration",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="horizontalPositionUncertainty",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalUncertainty",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="timeUncertainty",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="numInstances",
                required=True,
                type="uint32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="methodWaterLevelProduct",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="minDatasetHeight",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="maxDatasetHeight",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="sequencingRule.type",
                required=True,
                type="enumeration",
                fixed_value=1,
            ),
            AttributeDefinition(
                name="sequencingRule.scanDirection",
                required=True,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="interpolationType",
                required=True,
                type="enumeration",
                fixed_value=1,
            ),
            AttributeDefinition(
                name="dataOffsetCode",
                required=False,
                type="enumeration",
                fixed_value=5,
            ),
        ]

        self._log_check("104_Dev2001")
        self._log_check("104_Dev2008")
        self._log_check("104_Dev2009")
        self._log_check("104_Dev2018")
        self._log_check("104_Dev2019")
        self._check_attributes("WaterLevel group", WaterLevel, attr_list)

        self._log_check("104_Dev2003")
        if "commonPointRule" in WaterLevel.attrs:
            expected_values = {
                1: "average",
                2: "low",
                3: "high",
                4: "all",
            }
            self._validate_enumeration(WaterLevel, "commonPointRule", expected_values)

            self._log_check("104_Dev2004")
            commonPointRule = _get_int_attr_or_none(WaterLevel, "commonPointRule")
            if commonPointRule != 4:
                self._warning(
                    '/WaterLevel["commonPointRule"] attribute value is not the recommended value 4 (all)'
                )

        if "dataCodingFormat" in WaterLevel.attrs:
            expected_values = {
                1: "Fixed Stations",
                2: "Regular Grid",
                3: "Ungeorectified Grid",
                4: "Moving Platform",
                5: "Irregular Grid",
                6: "Variable cell size",
                7: "TIN",
                8: "Fixed Stations (Stationwise)",
                9: "Feature oriented Regular Grid",
            }
            self._validate_enumeration(WaterLevel, "dataCodingFormat", expected_values)

        self._log_check("104_Dev2005")
        horizontalPositionUncertainty = _get_float_attr_or_none(
            WaterLevel, "horizontalPositionUncertainty"
        )
        if horizontalPositionUncertainty and not (
            horizontalPositionUncertainty == -1.0 or horizontalPositionUncertainty >= 0
        ):
            self._warning(
                '/WaterLevel["horizontalPositionUncertainty"] attribute value must be -1 or positive'
            )

        verticalUncertainty = _get_float_attr_or_none(WaterLevel, "verticalUncertainty")
        if verticalUncertainty and not (
            verticalUncertainty == -1.0 or verticalUncertainty >= 0
        ):
            self._warning(
                '/WaterLevel["verticalUncertainty"] attribute value must be -1 or positive'
            )

        self._log_check("104_Dev2006")
        timeUncertainty = _get_float_attr_or_none(WaterLevel, "timeUncertainty")
        if timeUncertainty and not (timeUncertainty == -1.0 or timeUncertainty >= 0):
            self._warning(
                '/WaterLevel["timeUncertainty"] attribute value must be -1 or positive'
            )

        self._log_check("104_Dev2007")
        numInstances = _get_int_attr_or_none(WaterLevel, "numInstances")
        if numInstances is not None:
            if numInstances <= 0:
                self._critical_error(
                    '/WaterLevel["numInstances"] attribute value must be >= 1'
                )
                numInstances = None

        scanDirection_values = None
        if "sequencingRule.scanDirection" in WaterLevel.attrs:
            scanDirection = WaterLevel.attrs["sequencingRule.scanDirection"]
            if isinstance(scanDirection, str):
                # strip leading space. IMHO there should not be any, but
                # the examples in the specification sometimes show one...
                scanDirection_values = [x.lstrip() for x in scanDirection.split(",")]

                self._log_check("104_Dev2016")
                if len(scanDirection_values) != 2:
                    self._warning(
                        '/WaterLevel["sequencingRule.scanDirection"] attribute should have 2 values'
                    )
                elif "axisNames" in WaterLevel.keys():

                    scanDirection_values_without_orientation = []
                    for v in scanDirection_values:
                        if v.startswith("-"):
                            scanDirection_values_without_orientation.append(v[1:])
                        else:
                            scanDirection_values_without_orientation.append(v)
                    scanDirection_values_without_orientation = set(
                        scanDirection_values_without_orientation
                    )

                    self._log_check("104_Dev2017")
                    axisNames = WaterLevel["axisNames"]
                    if (
                        isinstance(axisNames, h5py.Dataset)
                        and axisNames.shape == (2,)
                        and isinstance(axisNames.id.get_type(), h5py.h5t.TypeStringID)
                    ):
                        axisNames_values = set(
                            [v.decode("utf-8") for v in axisNames[:]]
                        )
                        if scanDirection_values_without_orientation != axisNames_values:
                            self._warning(
                                f"Sequencing rule scanDirection contents ({scanDirection_values_without_orientation}) does not match axis names ({axisNames_values}"
                            )

        self._validate_axisNames(f, WaterLevel)

        subgroups = set(
            [name for name, item in WaterLevel.items() if isinstance(item, h5py.Group)]
        )

        minDatasetHeight = _get_float_attr_or_none(WaterLevel, "minDatasetHeight")
        if (
            minDatasetHeight is not None
            and minDatasetHeight != -9999.0
            and minDatasetHeight < -99.99
        ):
            self._warning(
                f"{WaterLevel.name}: minDatasetHeight={minDatasetHeight} should be in [-99.99, 99.99] range"
            )

        maxDatasetHeight = _get_float_attr_or_none(WaterLevel, "maxDatasetHeight")
        if maxDatasetHeight is not None and maxDatasetHeight > 99.99:
            self._warning(
                f"{WaterLevel.name}: maxDatasetHeight={maxDatasetHeight} should be in [-99.99, 99.99] range"
            )

        if (
            minDatasetHeight is not None
            and maxDatasetHeight is not None
            and minDatasetHeight != -9999.0
            and maxDatasetHeight != -9999.0
            and minDatasetHeight > maxDatasetHeight
        ):
            self._warning(
                f"Group_001: minDatasetHeight={minDatasetHeight} > maxDatasetHeight={maxDatasetHeight}"
            )

        self._log_check("104_Dev2013")
        if len(subgroups) == 0:
            self._critical_error("/WaterLevel has no groups")
        else:
            for i in range(1, len(subgroups) + 1):
                expected_name = "WaterLevel.%02d" % i
                if expected_name not in subgroups:
                    self._critical_error(
                        "/WaterLevel/{expected_name} group does not exist"
                    )

            for name in subgroups:
                if not name.startswith("WaterLevel."):
                    self._warning("/WaterLevel/{expected_name} is an unexpected group")

        self._log_check("104_Dev2014")
        if numInstances and len(subgroups) != numInstances:
            self._critical_error(
                f"/WaterLevel has {len(subgroups)} groups whereas numInstances={numInstances}"
            )

        self._log_check("104_Dev2015")
        self._validate_sequencingRuleType(WaterLevel)

        # Attributes and groups already checked above
        self._log_check("104_Dev2021")
        for name, item in WaterLevel.items():
            if isinstance(item, h5py.Dataset) and name != "axisNames":
                self._warning(f"/WaterLevel has unexpected dataset {name}")

            if isinstance(item, h5py.Group) and name.startswith("WaterLevel."):
                self._validate_WaterLevel_instance(f, WaterLevel, item)

    def _validate_sequencingRuleType(self, f):
        if "sequencingRule.type" in f.attrs:
            expected_values = {
                1: "linear",
                2: "boustrophedonic",
                3: "CantorDiagonal",
                4: "spiral",
                5: "Morton",
                6: "Hilbert",
            }
            self._validate_enumeration(f, "sequencingRule.type", expected_values)

    def _validate_WaterLevel_instance(self, f, WaterLevel, instance):

        # Cf Table 12-3 - Feature Instance metadata, pertaining to the feature instance
        attr_list = [
            AttributeDefinition(
                name="westBoundLongitude",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="eastBoundLongitude",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="southBoundLatitude",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="northBoundLatitude",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="numberOfTimes",
                required=True,
                type="uint32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="timeRecordInterval",
                required=False,
                type="uint16",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="dateTimeOfFirstRecord",
                required=True,
                type="datetime",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="dateTimeOfLastRecord",
                required=True,
                type="datetime",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="numGRP",
                required=True,
                type="uint32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="dataDynamicity",
                required=True,
                type="enumeration",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalDatumEpoch",
                required=False,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="gridOriginLongitude",
                required=True,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="gridOriginLatitude",
                required=True,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="gridSpacingLongitudinal",
                required=True,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="gridSpacingLatitudinal",
                required=True,
                type="float64",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="numPointsLongitudinal",
                required=True,
                type="uint32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="numPointsLatitudinal",
                required=True,
                type="uint32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="startSequence",
                required=True,
                type="string",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalDatumReference",
                required=False,
                type="enumeration",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalDatum",
                required=False,
                type="int32",
                fixed_value=None,
            ),
        ]

        self._log_check("104_Dev3001")
        self._log_check("104_Dev3005")
        self._log_check("104_Dev3019")
        self._log_check("104_Dev3020")
        self._check_attributes(
            f"WaterLevel feature instance group {instance.name}",
            instance,
            attr_list,
        )

        self._log_check("104_Dev3021")
        countGroups = 0
        for name, item in instance.items():
            if isinstance(item, h5py.Dataset) and name not in (
                "uncertainty",
                "domainExtent.polygon",
            ):
                self._warning(
                    f"WaterLevel feature instance group {instance.name} has unexpected dataset '{name}'"
                )

            elif isinstance(item, h5py.Group):
                if name.startswith("Group_"):
                    countGroups += 1
                else:
                    self._warning(
                        f"WaterLevel feature instance group {instance.name} has unexpected group '{name}'"
                    )

        if (
            "dateTimeOfFirstRecord" in instance.attrs
            and "dateTimeOfLastRecord" in instance.attrs
        ):
            dateTimeOfFirstRecord = instance.attrs["dateTimeOfFirstRecord"]
            dateTimeOfLastRecord = instance.attrs["dateTimeOfLastRecord"]
            if isinstance(dateTimeOfLastRecord, str) and isinstance(
                dateTimeOfLastRecord, str
            ):
                self._log_check("104_Dev3006")
                if dateTimeOfLastRecord < dateTimeOfFirstRecord:
                    self._error(
                        f"WaterLevel feature instance group {instance.name}: dateTimeOfLastRecord < dateTimeOfFirstRecord"
                    )
            else:
                dateTimeOfFirstRecord = None
                dateTimeOfLastRecord = None
        else:
            dateTimeOfFirstRecord = None
            dateTimeOfLastRecord = None

        numGRP = _get_int_attr_or_none(instance, "numGRP")
        if numGRP is not None:
            self._log_check("104_Dev3007")
            if numGRP <= 0:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: numGRP is <= 0"
                )
            self._log_check("104_Dev3023")
            if numGRP != countGroups:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: Count of values groups does not match attribute numGRP in instance group"
                )

        numberOfTimes = _get_int_attr_or_none(instance, "numberOfTimes")
        if numberOfTimes is not None:
            self._log_check("104_Dev3003")
            if numberOfTimes <= 0:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: numberOfTimes is <= 0"
                )
            if numGRP is not None and numberOfTimes != numGRP:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: numberOfTimes is different from numGRP"
                )

        timeRecordInterval = _get_int_attr_or_none(instance, "timeRecordInterval")
        if timeRecordInterval is not None:
            self._log_check("104_Dev3004")
            if timeRecordInterval <= 0:
                self._critical_error(
                    f"WaterLevel feature instance group {instance.name}: timeRecordInterval is <= 0"
                )
            elif (
                dateTimeOfFirstRecord
                and dateTimeOfLastRecord
                and len(dateTimeOfFirstRecord) == len("YYYYMMDDTHHMMSSZ")
                and len(dateTimeOfLastRecord) == len("YYYYMMDDTHHMMSSZ")
                and numberOfTimes
            ):
                from datetime import datetime, timezone

                start = (
                    datetime.strptime(dateTimeOfFirstRecord, "%Y%m%dT%H%M%SZ")
                    .replace(tzinfo=timezone.utc)
                    .timestamp()
                )
                end = (
                    datetime.strptime(dateTimeOfLastRecord, "%Y%m%dT%H%M%SZ")
                    .replace(tzinfo=timezone.utc)
                    .timestamp()
                )
                computedNumberOfTimes = 1 + (end - start) / timeRecordInterval
                if computedNumberOfTimes != numberOfTimes:
                    self._warning(
                        f"WaterLevel feature instance group {instance.name}: given dateTimeOfFirstRecord, dateTimeOfFirstRecord and timeRecordInterval, the number of groups should be {computedNumberOfTimes} whereas it is {numberOfTimes}"
                    )

        present = []
        missing = []
        for name in (
            "westBoundLongitude",
            "eastBoundLongitude",
            "northBoundLatitude",
            "southBoundLatitude",
        ):
            if name in instance.attrs:
                present.append(name)
            else:
                missing.append(name)

        if present and missing:
            self._critical_error(
                f"WaterLevel feature instance group {instance.name}: attributes {present} are present, but {missing} are missing"
            )

        westBoundLongitude = _get_float_attr_or_none(instance, "westBoundLongitude")
        eastBoundLongitude = _get_float_attr_or_none(instance, "eastBoundLongitude")
        northBoundLatitude = _get_float_attr_or_none(instance, "northBoundLatitude")
        southBoundLatitude = _get_float_attr_or_none(instance, "southBoundLatitude")

        top_westBoundLongitude = _get_float_attr_or_none(f, "westBoundLongitude")
        top_eastBoundLongitude = _get_float_attr_or_none(f, "eastBoundLongitude")
        top_northBoundLatitude = _get_float_attr_or_none(f, "northBoundLatitude")
        top_southBoundLatitude = _get_float_attr_or_none(f, "southBoundLatitude")

        if (
            westBoundLongitude is not None
            and eastBoundLongitude is not None
            and northBoundLatitude is not None
            and southBoundLatitude is not None
        ):

            horizontalCRS = _get_int_attr_or_none(f, "horizontalCRS")
            if horizontalCRS and horizontalCRS > 0:
                if self._is_geographic_2D(f):
                    self._validate_bounds(
                        f"WaterLevel feature instance group {instance.name}",
                        instance,
                    )

                    if (
                        top_westBoundLongitude is not None
                        and top_eastBoundLongitude is not None
                        and top_northBoundLatitude is not None
                        and top_southBoundLatitude is not None
                    ):
                        if westBoundLongitude < top_westBoundLongitude:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: westBoundLongitude={westBoundLongitude} < top_westBoundLongitude={top_westBoundLongitude}"
                            )
                        if southBoundLatitude < top_southBoundLatitude:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: southBoundLatitude={southBoundLatitude} < top_southBoundLatitude={top_southBoundLatitude}"
                            )
                        if eastBoundLongitude > top_eastBoundLongitude:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: eastBoundLongitude={eastBoundLongitude} > top_eastBoundLongitude={top_eastBoundLongitude}"
                            )
                        if northBoundLatitude > top_northBoundLatitude:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: northBoundLatitude={northBoundLatitude} > top_northBoundLatitude={top_northBoundLatitude}"
                            )

                else:
                    if (
                        abs(westBoundLongitude) <= 180
                        and abs(eastBoundLongitude) <= 180
                        and abs(northBoundLatitude) <= 90
                        and abs(southBoundLatitude) <= 90
                    ):
                        self._error(
                            f"WaterLevel feature instance group {instance.name}: westBoundLongitude, eastBoundLongitude, northBoundLatitude, southBoundLatitude are longitudes/latitudes whereas they should be projected coordinates, given the horizontalCRS is projected"
                        )

                    if gdal_available:
                        horizontalCRS_srs = osr.SpatialReference()
                        horizontalCRS_srs.SetAxisMappingStrategy(
                            osr.OAMS_TRADITIONAL_GIS_ORDER
                        )
                        horizontalCRS_srs.ImportFromEPSG(int(horizontalCRS))

                        longlat_srs = osr.SpatialReference()
                        longlat_srs.SetAxisMappingStrategy(
                            osr.OAMS_TRADITIONAL_GIS_ORDER
                        )
                        longlat_srs.ImportFromEPSG(4326)
                        ct = osr.CoordinateTransformation(
                            horizontalCRS_srs, longlat_srs
                        )
                        westLon, southLat, eastLon, northLat = ct.TransformBounds(
                            westBoundLongitude,
                            southBoundLatitude,
                            eastBoundLongitude,
                            northBoundLatitude,
                            21,
                        )

                        self._log_check("104_Dev3004")
                        crs_area_of_use = horizontalCRS_srs.GetAreaOfUse()
                        # Add a substantial epsilon as going a bit outside of the CRS area of use is usually fine
                        epsilon = 1
                        if westLon + epsilon < crs_area_of_use.west_lon_degree:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: westLon={westLon} < crs_area_of_use.west_lon_degree={crs_area_of_use.west_lon_degree}"
                            )
                        if southLat + epsilon < crs_area_of_use.south_lat_degree:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: southLat={southLat} < crs_area_of_use.south_lat_degree={crs_area_of_use.south_lat_degree}"
                            )
                        if eastLon - epsilon > crs_area_of_use.east_lon_degree:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: eastLon={eastLon} > crs_area_of_use.east_lon_degree={crs_area_of_use.east_lon_degree}"
                            )
                        if northLat - epsilon > crs_area_of_use.north_lat_degree:
                            self._error(
                                f"WaterLevel feature instance group {instance.name}: northLat={northLat} > crs_area_of_use.north_lat_degree={crs_area_of_use.north_lat_degree}"
                            )

                        if (
                            top_westBoundLongitude is not None
                            and top_eastBoundLongitude is not None
                            and top_northBoundLatitude is not None
                            and top_southBoundLatitude is not None
                        ):
                            # Add an epsilon to take into account potential different ways of doing bounding box reprojection
                            epsilon = 0.01
                            if westLon + epsilon < top_westBoundLongitude:
                                self._error(
                                    f"WaterLevel feature instance group {instance.name}: westBoundLongitude={westLon} ({westBoundLongitude}) < top_westBoundLongitude={top_westBoundLongitude}"
                                )
                            if southLat + epsilon < top_southBoundLatitude:
                                self._error(
                                    f"WaterLevel feature instance group {instance.name}: southBoundLatitude={southLat} ({southBoundLatitude}) < top_southBoundLatitude={top_southBoundLatitude}"
                                )
                            if eastLon - epsilon > top_eastBoundLongitude:
                                self._error(
                                    f"WaterLevel feature instance group {instance.name}: eastBoundLongitude={eastLon} ({eastBoundLongitude}) > top_eastBoundLongitude={top_eastBoundLongitude}"
                                )
                            if northLat - epsilon > top_northBoundLatitude:
                                self._error(
                                    f"WaterLevel feature instance group {instance.name}: northBoundLatitude={northLat} ({northBoundLatitude}) > top_northBoundLatitude={top_northBoundLatitude}"
                                )

                    else:
                        self._warning(
                            "Test checking consistency of bounds in WaterLevel feature instance group compared to top level attributes skipped due to GDAL not available"
                        )

            if eastBoundLongitude <= westBoundLongitude:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: eastBoundLongitude <= westBoundLongitude"
                )
            if northBoundLatitude <= southBoundLatitude:
                self._error(
                    f"WaterLevel feature instance group {instance.name}: northBoundLatitude <= southBoundLatitude"
                )

        if "domainExtent.polygon" in instance.keys() and present:
            self._error(
                f"BathymetryCoverage feature instance group {instance.name}: both dataset 'domainExtent.polygon' and westBoundLongitude, eastBoundLongitude, northBoundLatitude, southBoundLatitude attributes are present"
            )

        gridOriginLongitude = _get_float_attr_or_none(instance, "gridOriginLongitude")
        gridOriginLatitude = _get_float_attr_or_none(instance, "gridOriginLatitude")
        if gridOriginLongitude is not None and gridOriginLatitude is not None:

            if (
                westBoundLongitude is not None
                and eastBoundLongitude is not None
                and northBoundLatitude is not None
                and southBoundLatitude is not None
            ):
                self._log_check("104_Dev3009")

                # gridOriginLongitude is encoded as a float64, whereas westBoundLongitude on a float32
                # hence add some tolerance so comparison is fair
                if (
                    gridOriginLongitude + 1e-6 * abs(gridOriginLongitude)
                    < westBoundLongitude
                ):
                    self._error(
                        f"WaterLevel feature instance group {instance.name}: gridOriginLongitude={gridOriginLongitude} < westBoundLongitude={westBoundLongitude}"
                    )
                if (
                    gridOriginLongitude - 1e-6 * abs(gridOriginLongitude)
                    > eastBoundLongitude
                ):
                    self._error(
                        f"WaterLevel feature instance group {instance.name}: gridOriginLongitude={gridOriginLongitude} > eastBoundLongitude={eastBoundLongitude}"
                    )
                if (
                    gridOriginLatitude + 1e-6 * abs(gridOriginLatitude)
                    < southBoundLatitude
                ):
                    self._error(
                        f"WaterLevel feature instance group {instance.name}: gridOriginLatitude={gridOriginLatitude} < southBoundLatitude={southBoundLatitude}"
                    )
                if (
                    gridOriginLatitude - 1e-6 * abs(gridOriginLatitude)
                    > northBoundLatitude
                ):
                    self._error(
                        f"WaterLevel feature instance group {instance.name}: gridOriginLatitude={gridOriginLatitude} > northBoundLatitude={northBoundLatitude}"
                    )

                if gdal_available and horizontalCRS > 0:
                    horizontalCRS_srs = osr.SpatialReference()
                    horizontalCRS_srs.SetAxisMappingStrategy(
                        osr.OAMS_TRADITIONAL_GIS_ORDER
                    )
                    horizontalCRS_srs.ImportFromEPSG(horizontalCRS)

                    longlat_srs = osr.SpatialReference()
                    longlat_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
                    longlat_srs.ImportFromEPSG(4326)
                    ct = osr.CoordinateTransformation(horizontalCRS_srs, longlat_srs)
                    origin_long, origin_lat, _ = ct.TransformPoint(
                        gridOriginLongitude, gridOriginLatitude, 0
                    )

                    crs_area_of_use = horizontalCRS_srs.GetAreaOfUse()
                    # Add a substantial epsilon as going a bit outside of the CRS area of use is usually fine
                    epsilon = 1
                    if origin_long + epsilon < crs_area_of_use.west_lon_degree:
                        self._error(
                            f"WaterLevel feature instance group {instance.name}: origin_long={origin_long} < crs_area_of_use.west_lon_degree={crs_area_of_use.west_lon_degree}"
                        )
                    if origin_lat + epsilon < crs_area_of_use.south_lat_degree:
                        self._error(
                            f"WaterLevel feature instance group {instance.name}: origin_lat={origin_lat} < crs_area_of_use.south_lat_degree={crs_area_of_use.south_lat_degree}"
                        )
                    if origin_long - epsilon > crs_area_of_use.east_lon_degree:
                        self._error(
                            f"WaterLevel feature instance group {instance.name}: origin_long={origin_long} > crs_area_of_use.east_lon_degree={crs_area_of_use.east_lon_degree}"
                        )
                    if origin_lat - epsilon > crs_area_of_use.north_lat_degree:
                        self._error(
                            f"WaterLevel feature instance group {instance.name}: origin_lat={origin_lat} > crs_area_of_use.north_lat_degree={crs_area_of_use.north_lat_degree}"
                        )

        self._log_check("104_Dev3010")
        gridSpacingLongitudinal = _get_float_attr_or_none(
            instance, "gridSpacingLongitudinal"
        )
        if gridSpacingLongitudinal is not None and gridSpacingLongitudinal <= 0:
            self._critical_error(
                f"WaterLevel feature instance group {instance.name}: Grid spacing attribute in instance group has value out of range: gridSpacingLongitudinal <= 0"
            )

        self._log_check("104_Dev3010")
        gridSpacingLatitudinal = _get_float_attr_or_none(
            instance, "gridSpacingLatitudinal"
        )
        if gridSpacingLatitudinal is not None and gridSpacingLatitudinal <= 0:
            self._critical_error(
                f"WaterLevel feature instance group {instance.name}: Grid spacing attribute in instance group has value out of range: gridSpacingLatitudinal <= 0"
            )

        self._log_check("104_Dev3011")
        if (
            gridSpacingLongitudinal is not None
            and eastBoundLongitude is not None
            and westBoundLongitude is not None
            and gridSpacingLongitudinal * (1 - 1e-2)
            > 0.5 * (eastBoundLongitude - westBoundLongitude)
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Value of gridSpacingLongitudinal or gridSpacingLatitudinal in instance group too high: gridSpacingLongitudinal={gridSpacingLongitudinal} > 0.5 * (eastBoundLongitude - westBoundLongitude)={0.5 * (eastBoundLongitude - westBoundLongitude)}"
            )

        self._log_check("104_Dev3011")
        if (
            gridSpacingLatitudinal is not None
            and southBoundLatitude is not None
            and northBoundLatitude is not None
            and gridSpacingLatitudinal * (1 - 1e-2)
            > 0.5 * (northBoundLatitude - southBoundLatitude)
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Value of gridSpacingLongitudinal or gridSpacingLatitudinal in instance group too high: gridSpacingLatitudinal={gridSpacingLatitudinal} > 0.5 * (northBoundLatitude - southBoundLatitude)={0.5 * (northBoundLatitude - southBoundLatitude)}"
            )

        self._log_check("104_Dev3012")
        numPointsLongitudinal = _get_int_attr_or_none(instance, "numPointsLongitudinal")
        if numPointsLongitudinal < 1:
            self._critical_error(
                f"WaterLevel feature instance group {instance.name}: Grid must be at least 1X1: numPointsLongitudinal < 1"
            )

        self._log_check("104_Dev3012")
        numPointsLatitudinal = _get_int_attr_or_none(instance, "numPointsLatitudinal")
        if numPointsLatitudinal < 1:
            self._critical_error(
                f"WaterLevel feature instance group {instance.name}: Grid must be at least 1X1: numPointsLatitudinal < 1"
            )

        self._log_check("104_Dev3013")
        if (
            gridSpacingLongitudinal is not None
            and eastBoundLongitude is not None
            and westBoundLongitude is not None
            and numPointsLongitudinal is not None
            and numPointsLongitudinal > 1
            and abs(
                gridSpacingLongitudinal
                - (eastBoundLongitude - westBoundLongitude)
                / (numPointsLongitudinal - 1)
            )
            > 1e-2 * gridSpacingLongitudinal
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Grid dimensions are incompatible with instance bounding box: gridSpacingLongitudinal={gridSpacingLongitudinal} != (eastBoundLongitude - westBoundLongitude) / (numPointsLongitudinal - 1)={(eastBoundLongitude - westBoundLongitude) / (numPointsLongitudinal - 1)}"
            )

        self._log_check("104_Dev3009")
        if (
            gridSpacingLatitudinal is not None
            and southBoundLatitude is not None
            and northBoundLatitude is not None
            and numPointsLatitudinal is not None
            and numPointsLatitudinal > 1
            and (
                gridSpacingLatitudinal
                - (northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)
            )
            > 1e-2 * gridSpacingLatitudinal
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Grid dimensions are incompatible with instance bounding box: gridSpacingLatitudinal={gridSpacingLatitudinal} != (northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)={(northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)}"
            )

        self._log_check("104_Dev3014")
        # gridOriginLongitude is encoded as a float64, whereas westBoundLongitude on a float32
        # hence add some tolerance so comparison is fair
        if (
            westBoundLongitude is not None
            and gridOriginLongitude is not None
            and abs(westBoundLongitude - gridOriginLongitude)
            > 1e-6 * abs(westBoundLongitude)
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Grid origin does not coincide with instance bounding box; westBoundLongitude={westBoundLongitude} !=  gridOriginLongitude={_cast_to_float32(gridOriginLongitude)}"
            )

        self._log_check("104_Dev3014")
        if (
            southBoundLatitude is not None
            and gridOriginLatitude is not None
            and abs(southBoundLatitude - gridOriginLatitude)
            > 1e-6 * abs(southBoundLatitude)
        ):
            self._warning(
                f"WaterLevel feature instance group {instance.name}: Grid origin does not coincide with instance bounding box: southBoundLatitude={southBoundLatitude} !=  gridOriginLatitude={_cast_to_float32(gridOriginLatitude)}"
            )

        self._log_check("104_Dev3015")
        if "startSequence" in instance.attrs:
            startSequence = instance.attrs["startSequence"]
            if isinstance(startSequence, str):
                startSequence = startSequence.split(",")
                if (
                    len(startSequence) != 2
                    or _get_int_value_or_none(startSequence[0]) is None
                    or _get_int_value_or_none(startSequence[1]) is None
                ):
                    self._warning(
                        f"WaterLevel feature instance group {instance.name}: invalid content for startSequence in instance"
                    )
                else:
                    self._log_check("104_Dev3016")
                    if startSequence != ["0", "0"]:
                        # other tests are probably not compatible of a non (0,0) startSequence
                        self._warning(
                            f"WaterLevel feature instance group {instance.name}: Values in startSequence in instance group are incompatible with the scan direction in sequencingRule"
                        )

        self._log_check("104_Dev3022")
        for idx_grp in range(1, numGRP + 1):
            grp_name = "Group_%03d" % idx_grp
            if grp_name not in instance.keys() or not isinstance(
                instance[grp_name], h5py.Group
            ):
                self._critical_error(
                    f"WaterLevel feature instance group {instance.name}: no {grp_name} subgroup"
                )
            else:
                self._validate_Group_XXX(
                    f,
                    instance[grp_name],
                    numPointsLongitudinal,
                    numPointsLatitudinal,
                    dateTimeOfFirstRecord,
                    dateTimeOfLastRecord,
                )

        if "uncertainty" in instance.keys() and isinstance(
            instance["uncertainty"], h5py.Dataset
        ):
            uncertainty = instance["uncertainty"]
            if uncertainty.shape != (1,):
                self._critical_error(
                    f"{instance.name}/uncertainty' is not a one-dimensional dataset of shape 1"
                )
            elif uncertainty.dtype not in (
                [
                    ("name", "O"),
                    ("value", "d"),
                ],
                [
                    ("name", "O"),
                    ("value", "f"),
                ],
            ):
                self._critical_error(
                    f"{instance.name}/uncertainty' has not expected data type"
                )

        self._validate_verticalDatum(instance.name, instance)
        verticalDatum = _get_int_attr_or_none(instance, "verticalDatum")
        topVerticalDatum = _get_int_attr_or_none(f, "verticalDatum")
        if verticalDatum is not None and topVerticalDatum is not None:
            if verticalDatum == topVerticalDatum:
                self._error(
                    f"WaterLevel feature instance group {instance.name} has same value for 'verticalDatum' attribute as top level attribute"
                )

    def _validate_Group_XXX(
        self,
        f,
        Group_XXX,
        numPointsLongitudinal,
        numPointsLatitudinal,
        dateTimeOfFirstRecord,
        dateTimeOfLastRecord,
    ):

        # Cf Table 12-4 - Values Group attributes
        attr_list = [
            AttributeDefinition(
                name="timePoint",
                required=True,
                type="datetime",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="waterLevelTrendThreshold",
                required=False,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="timeRecordInterval",
                required=False,
                type="uint32",
                fixed_value=None,
            ),
        ]

        self._log_check("104_Dev5001")
        self._check_attributes(
            "Group_XXX",
            Group_XXX,
            attr_list,
        )

        if (
            "timePoint" in Group_XXX.attrs
            and dateTimeOfFirstRecord
            and dateTimeOfLastRecord
        ):
            timePoint = Group_XXX.attrs["timePoint"]
            if isinstance(timePoint, str):
                self._log_check("104_Dev5002")
                if not (
                    timePoint >= dateTimeOfFirstRecord
                    and timePoint <= dateTimeOfLastRecord
                ):
                    self._warning(
                        f"{Group_XXX.name}: timePoint value not in [dateTimeOfFirstRecord, dateTimeOfLastRecord] range"
                    )

        self._log_check("104_Dev5003")
        if "values" not in Group_XXX.keys() or not isinstance(
            Group_XXX["values"], h5py.Dataset
        ):
            self._critical_error(f"{Group_XXX.name}/values dataset missing")
        else:
            self._validate_values(
                f,
                Group_XXX["values"],
                numPointsLongitudinal,
                numPointsLatitudinal,
            )

    def _validate_values(
        self,
        f,
        values,
        numPointsLongitudinal,
        numPointsLatitudinal,
    ):

        self._log_check("104_Dev5005")
        if len(values.shape) != 2:
            self._critical_error(f"{values.name} dataset is not 2-dimensional")
            return

        self._log_check("104_Dev5006")
        if (
            numPointsLatitudinal
            and numPointsLongitudinal
            and values.shape != (numPointsLatitudinal, numPointsLongitudinal)
        ):
            self._critical_error(
                f"{values.name} dataset shape is {values.shape} instead of {(numPointsLatitudinal, numPointsLongitudinal)}"
            )
            return

        self._log_check("104_Dev5011")
        values_type = values.id.get_type()
        if not isinstance(values_type, h5py.h5t.TypeCompoundID):
            self._critical_error(f"{values.name} type is not compound")
            return

        self._log_check("104_Dev5012")
        Group_F_WaterLevel = None
        if "Group_F" in f:
            Group_F = f["Group_F"]
            if isinstance(Group_F, h5py.Group) and "WaterLevel" in Group_F:
                Group_F_WaterLevel = Group_F["WaterLevel"]
                if (
                    isinstance(Group_F_WaterLevel, h5py.Dataset)
                    and len(Group_F_WaterLevel.shape) == 1
                ):
                    num_components = None
                    if num_components and values_type.get_nmembers() != num_components:
                        self._critical_error(
                            f"{values.name} type has {values_type.get_nmembers()} members whereas {num_components} are expected from /Group_F/WaterLevel"
                        )
                        return
                else:
                    Group_F_WaterLevel = None

        # Check consistency between "values" and "/Group_F/WaterLevel"
        found_waterLevelHeight = False
        found_waterLevelTrend = False
        found_uncertainty = False
        for member_idx in range(values_type.get_nmembers()):
            subtype = values_type.get_member_type(member_idx)
            component_name = values_type.get_member_name(member_idx)
            if Group_F_WaterLevel:
                expected = Group_F_WaterLevel[member_idx][0]
                if component_name != expected:
                    self._critical_error(
                        f"{values.name} member {member_idx} name = {component_name} is not Group_F_WaterLevel[{member_idx}]['name']] = {expected}"
                    )
            assert isinstance(component_name, bytes)
            if component_name == b"waterLevelHeight":
                found_waterLevelHeight = True
                if not self._is_float32(subtype):
                    self._critical_error(
                        f"{values.name} member {component_name} is not a float32"
                    )
            elif component_name == b"waterLevelTrend":
                found_waterLevelTrend = True
                if not self._is_enumeration(subtype):
                    self._critical_error(
                        f"{values.name} member {component_name} is not an enumeration"
                    )
            elif component_name == b"uncertainty":
                found_uncertainty = True
                if not self._is_float32(subtype):
                    self._critical_error(
                        f"{values.name} member {component_name} is not a float32"
                    )
        minDatasetHeight = _get_float_attr_or_none(f["WaterLevel"], "minDatasetHeight")
        maxDatasetHeight = _get_float_attr_or_none(f["WaterLevel"], "maxDatasetHeight")
        if found_waterLevelHeight and minDatasetHeight and maxDatasetHeight:
            if minDatasetHeight > maxDatasetHeight:
                self._error("minDatasetHeight > maxDatasetHeight")
            else:
                self._log_check("104_Dev5013")
                masked_height = np.ma.masked_equal(values[:]["waterLevelHeight"], -9999)
                actualMinHeight = masked_height.min()
                if actualMinHeight < minDatasetHeight:
                    self._error(
                        f"{values.name} : minimum waterLevelHeight is {actualMinHeight}, whereas minDatasetHeight attribute = {minDatasetHeight}"
                    )

                actualMaxHeight = masked_height.max()
                if actualMaxHeight > maxDatasetHeight:
                    self._error(
                        f"{values.name} : maximum waterLevelHeight is {actualMaxHeight}, whereas maxDatasetHeight attribute = {maxDatasetHeight}"
                    )

        if found_waterLevelTrend:
            masked_trend = np.ma.masked_equal(values[:]["waterLevelTrend"], 0)
            actualMinTrend = masked_trend.min()
            if actualMinTrend < 1:
                self._error(
                    f"{values.name} : minimum waterLevelTrend is {actualMinTrend}, whereas it should be >= 1"
                )
            actualMaxTrend = masked_trend.max()
            if actualMaxTrend > 3:
                self._error(
                    f"{values.name} : maximum waterLevelTrend is {actualMaxTrend}, whereas it should be < 3"
                )

        if found_uncertainty:
            masked_uncertainty = np.ma.masked_equal(values[:]["uncertainty"], -1.0)
            actualMinUncertainty = masked_uncertainty.min()
            if actualMinUncertainty < 0:
                self._error(
                    f"{values.name} : minimum uncertainty is {actualMinUncertainty}, whereas it should be >= 0"
                )

    def _validate_axisNames(self, f, group):

        groupName = group.name

        self._log_check("104_Dev2012")
        if "axisNames" not in group.keys():
            self._error(f"{groupName}/axisNames dataset does not exist")
        elif not isinstance(group["axisNames"], h5py.Dataset):
            self._error(f"{groupName}/axisNames is not a dataset")
        else:
            axisNames = group["axisNames"]
            if axisNames.shape != (2,):
                self._error(
                    f"{groupName}/axisNames dataset is not a one-dimensional array of length 2"
                )
            else:
                type = axisNames.id.get_type()
                if not isinstance(type, h5py.h5t.TypeStringID):
                    self._error(f"{groupName}/axisNames type is not a string")
                else:
                    values = [v.decode("utf-8") for v in axisNames[:]]
                    if values not in (
                        ["Easting", "Northing"],
                        ["Latitude", "Longitude"],
                    ):
                        self._error(
                            f'{groupName}/axisNames must conform to CRS. Expected ["Easting", "Northing"] or ["Latitude", "Longitude"]. Got {values}'
                        )
                    elif "horizontalCRS" in f.attrs:
                        horizontalCRS = f.attrs["horizontalCRS"]
                        if isinstance(horizontalCRS, int):
                            if self._is_geographic_2D(f):
                                if values != ["Latitude", "Longitude"]:
                                    self._error(
                                        f'{groupName}/axisNames must conform to CRS. Expected ["Latitude", "Longitude"]'
                                    )
                            else:
                                if values != ["Easting", "Northing"]:
                                    self._error(
                                        f'{groupName}/axisNames must conform to CRS. Expected ["Easting", "Northing"]'
                                    )


# Public function
def check(
    filename,
    abort_at_first_error=False,
):
    """Check specified filename and return a tuple (errors, warnings, checks_done)"""
    checker = S104Checker(
        filename,
        abort_at_first_error=abort_at_first_error,
    )
    checker.check()
    return checker.errors, checker.warnings, checker.checks_done


def usage():
    print("Usage: validate_s104.py [-q] <filename>")
    print("")
    print("Validates a S104 files against the Edition 2.0 specification.")
    print("")
    print("-q: quiet mode. Only exit code indicates success (0) or error (1)")


def main(argv=sys.argv):
    filename = None
    quiet = False

    for arg in argv[1:]:
        if arg == "-q":
            quiet = True
        elif arg == "-h":
            usage()
            return 0
        elif arg[0] == "-":
            print(f"Invalid option: {arg}\n")
            return 2
        else:
            filename = arg

    if filename is None:
        print("Filename missing\n")
        return 2

    errors, warnings, checks_done = check(
        filename,
        abort_at_first_error=False,
    )

    if not quiet:
        print(f"Checks done: {checks_done}")

        if warnings:
            print("")
            print("Warnings:")
            for msg in warnings:
                print(f"Warning: {msg}")

        if errors:
            print("")
            print("Errors:")
            for criticity, msg in errors:
                print(f"{criticity}: {msg}")
            print("")
            print("Errors found: validation failed!")
        else:
            print("")
            print("No errors found: validation succeeded.")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
