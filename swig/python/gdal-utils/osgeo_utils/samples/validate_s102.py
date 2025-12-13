#!/usr/bin/env python3
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR
# Purpose:  Test compliance of IHO S102 v3.0 dataset
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

# Validates against
# https://iho-ohi.github.io/S-102-Product-Specification/documents/3.0.0/document.html and
# https://iho.int/uploads/user/pubs/standards/s-100/S-100_5.2.0_Final_Clean.pdf

# "102_DevXXXX" are for traceability with respect to requirements of the spreadsheet:
# https://raw.githubusercontent.com/iho-ohi/S-100-Validation-Checks/refs/heads/main/Documents/S-158-102/0.2.0/S-158_102_0_2_0_20241118.xlsx
# Note that there are a few checks in that spreadsheet that are specific only of 2.3.0, and not 3.0.0...

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


class S102ValidationException(Exception):
    pass


class S102Checker:
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
            raise S102ValidationException(f"{ERROR}: {msg}")

    def _critical_error(self, msg):
        self.errors += [(CRITICAL_ERROR, msg)]
        if self.abort_at_first_error:
            raise S102ValidationException(f"{CRITICAL_ERROR}: {msg}")

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
                # 102_Dev1002: check presence of required attributes
                self._critical_error(
                    f"Required {ctxt_name} attribute '{attr_def.name}' is missing"
                )

            elif attr_def.name in group.attrs:
                attr = group.attrs[attr_def.name]
                if isinstance(attr, bytes):
                    attr = attr.decode("utf-8")
                h5_type = group.attrs.get_id(attr_def.name).get_type()

                # 102_Dev1004: check type

                if attr_def.type == "string":
                    if not self._is_string(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string "
                        )

                elif attr_def.type == "time":
                    if not self._is_string(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string"
                        )

                    # 102_Dev1005: validate date or time
                    self._log_check("102_Dev1005")
                    pattern = re.compile(
                        r"^(?:[01]\d|2[0-3])[0-5]\d[0-5]\d(?:Z|[+-](?:[01]\d|2[0-3])[0-5]\d)$"
                    )
                    if not pattern.match(attr):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a valid time: {attr}"
                        )

                elif attr_def.type == "date":
                    if not isinstance(h5_type, h5py.h5t.TypeStringID):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a string"
                        )
                    elif h5_type.get_size() != 8:
                        self._warning(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a 8-character string"
                        )

                    # 102_Dev1005: validate date or time
                    self._log_check("102_Dev1005")
                    pattern = re.compile(
                        r"^(?:[0-9]{4})(?:(?:0[1-9]|1[0-2])(?:0[1-9]|[12][0-9]|3[01]))$"
                    )
                    if not pattern.match(attr):
                        self._error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a valid date: {attr}"
                        )

                elif attr_def.type == "uint8":
                    if not self._is_uint8(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint8"
                        )

                elif attr_def.type == "uint16":
                    if not self._is_uint16(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint16"
                        )

                elif attr_def.type == "uint32":
                    if not self._is_uint32(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a uint32"
                        )

                elif attr_def.type == "int32":
                    if not self._is_int32(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a int32"
                        )

                elif attr_def.type == "float32":
                    if not self._is_float32(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a float32"
                        )

                elif attr_def.type == "float64":
                    if not self._is_float64(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not a float64"
                        )

                elif attr_def.type == "enumeration":
                    if not self._is_enumeration(h5_type):
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' is not an enumeration"
                        )

                else:

                    raise Exception(
                        f"Programming error: unexpected type {attr_def.type}"
                    )

                if attr_def.fixed_value:
                    self._log_check("102_Dev1006")
                    if attr != attr_def.fixed_value:
                        self._critical_error(
                            f"{ctxt_name} attribute '{attr_def.name}' has value '{attr}', whereas '{attr_def.fixed_value}' is expected"
                        )

        self._log_check("102_Dev1028")
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

        self._log_check("102_Dev9005")
        file_size = os.stat(self.filename).st_size
        if file_size > 10 * 1024 * 1024:
            self._warning(
                f"File size of {self.filename} = {file_size}, which exceeds 10 MB"
            )

        basename = os.path.basename(self.filename)
        if not basename.startswith("102"):
            self._warning("File name should start with '102'")
        if not basename.upper().endswith(".H5"):
            self._warning("File name should end with '.H5'")
        pattern = r"^102[a-zA-Z0-9]{4}[a-zA-Z0-9_]{1,12}\.(?:h5|H5)$"
        if not re.match(pattern, basename):
            self._warning(
                f"File name '{basename}' does not match expected pattern '{pattern}'"
            )

        self._log_check("102_Dev1028")
        for key in f.keys():
            if key not in (
                "Group_F",
                "BathymetryCoverage",
                "QualityOfBathymetryCoverage",
            ):
                self._warning(f"Unexpected element {key} in top level group")

        self._log_check("102_Dev1001")
        if "Group_F" in f.keys():
            self._validate_group_f(f, f["Group_F"])
        else:
            self._critical_error("No feature information group ('Group_F')")

        # Cf Table 10-2 - Root group attributes
        topLevelAttributesList = [
            AttributeDefinition(
                name="productSpecification",
                required=True,
                type="string",
                fixed_value="INT.IHO.S-102.3.0.0",
            ),
            AttributeDefinition(
                name="issueTime", required=False, type="time", fixed_value=None
            ),
            AttributeDefinition(
                name="issueDate", required=True, type="date", fixed_value=None
            ),
            AttributeDefinition(
                name="horizontalCRS", required=True, type="int32", fixed_value=None
            ),
            AttributeDefinition(
                name="epoch", required=False, type="string", fixed_value=None
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
                name="metadata", required=False, type="string", fixed_value=None
            ),
            # 102_Dev1020
            AttributeDefinition(
                name="verticalCS", required=True, type="int32", fixed_value=6498
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
                fixed_value=1,
            ),
            AttributeDefinition(
                name="verticalDatum", required=True, type="uint16", fixed_value=None
            ),
        ]

        self._log_check("102_Dev1002")
        self._log_check("102_Dev1003")
        self._log_check("102_Dev1004")
        self._check_attributes("top level", f, topLevelAttributesList)
        if _get_int_attr_or_none(f, "verticalCS"):
            self._log_check("102_Dev1020")

        self._validate_verticalCoordinateBase(f)
        self._validate_verticalDatumReference(f)
        self._validate_verticalDatum("top level", f)
        self._validate_epoch(f)
        self._validate_metadata(f, self.filename)
        self._validate_horizontalCRS(f)
        self._validate_bounds("top level", f)

        if "BathymetryCoverage" in f.keys():
            self._validate_BathymetryCoverage(f)
        else:
            self._log_check("102_Dev1026")
            self._critical_error("Missing /BathymetryCoverage group")

        if "QualityOfBathymetryCoverage" in f.keys():
            self._validate_QualityOfBathymetryCoverage(f)

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
        if verticalDatum is not None and not (
            (verticalDatum >= 1 and verticalDatum <= 30) or verticalDatum == 44
        ):
            # 102_Dev1006
            self._critical_error(
                f"{ctxt_name} attribute verticalDatum has value '{verticalDatum}', whereas it should be in [1, 30] range or 44"
            )

    def _validate_epoch(self, f):
        self._log_check("102_Dev1007")
        epoch = _get_float_attr_or_none(f, "epoch")
        if epoch and not (epoch >= 1980 and epoch <= 2100):
            self._warning(f"Top level attribute epoch has invalid value: {epoch}")

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

    def _validate_horizontalCRS(self, f):
        self._log_check("102_Dev1009")
        horizontalCRS = _get_int_attr_or_none(f, "horizontalCRS")
        if horizontalCRS and not (
            horizontalCRS in (4326, 5041, 5042)
            or (horizontalCRS >= 32601 and horizontalCRS <= 32660)
            or (horizontalCRS >= 32701 and horizontalCRS <= 32760)
        ):
            self._critical_error(
                f"Top level attribute 'horizontalCRS'={horizontalCRS} must be 4326, 5041, 5042 or in [32601,32660] or [32701,32760] ranges"
            )

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
                "BathymetryCoverage",
                "QualityOfBathymetryCoverage",
            ):
                self._warning(f"Unexpected element {key} in Group_F")

        self._log_check("102_Dev1021")
        if "featureCode" in group_f.keys():
            self._validate_group_f_featureCode(
                rootGroup, group_f, group_f["featureCode"]
            )
        else:
            self._critical_error(
                "No featureCode array in feature information group ('/Group_F/featureCode')"
            )

    def _validate_group_f_featureCode(self, rootGroup, group_f, featureCode):

        self._log_check("102_Dev1021")
        if not isinstance(featureCode, h5py.Dataset):
            self._critical_error("'/Group_F/featureCode' is not a dataset")
            return

        if len(featureCode.shape) != 1:
            self._critical_error(
                "'/Group_F/featureCode' is not a one-dimensional dataset"
            )
            return

        self._log_check("102_Dev1022")
        values = set([v.decode("utf-8") for v in featureCode[:]])
        if "BathymetryCoverage" not in values:
            self._critical_error(
                "Bathymetry data feature missing from featureCode array"
            )

        self._log_check("102_Dev1023")
        if (
            "QualityOfBathymetryCoverage" not in values
            or "QualityOfBathymetryCoverage" not in rootGroup
        ):
            self._warning("Quality feature not used")

        self._log_check("102_Dev1024")
        for value in values:
            if value not in ("BathymetryCoverage", "QualityOfBathymetryCoverage"):
                #
                self._critical_error(
                    f"Group_F feature information must correspond to feature catalog. Did not expect {value}"
                )

            self._log_check("102_Dev1025")
            if value not in group_f.keys():
                self._critical_error(
                    f"Feature information dataset for feature type {value} missing"
                )

            self._log_check("102_Dev1026")
            if value not in rootGroup.keys():
                self._critical_error(f"No feature instances for feature type {value}")

        if "BathymetryCoverage" in group_f.keys():
            self._validate_group_f_BathymetryCoverage(group_f)

        if "QualityOfBathymetryCoverage" in group_f.keys():
            self._validate_group_f_QualityOfBathymetryCoverage(group_f)

    def _validate_group_f_BathymetryCoverage(self, group_f):
        self._log_check("102_Dev1027")

        BathymetryCoverage = group_f["BathymetryCoverage"]
        if not isinstance(BathymetryCoverage, h5py.Dataset):
            self._critical_error("'/Group_F/BathymetryCoverage' is not a dataset")
        elif BathymetryCoverage.shape not in ((1,), (2,)):
            self._critical_error(
                "'/Group_F/BathymetryCoverage' is not a one-dimensional dataset of shape 1 or 2"
            )
        elif BathymetryCoverage.dtype != [
            ("code", "O"),
            ("name", "O"),
            ("uom.name", "O"),
            ("fillValue", "O"),
            ("datatype", "O"),
            ("lower", "O"),
            ("upper", "O"),
            ("closure", "O"),
        ]:
            self._critical_error(
                "'/Group_F/BathymetryCoverage' has not expected data type"
            )
        else:
            type = BathymetryCoverage.id.get_type()
            assert isinstance(type, h5py.h5t.TypeCompoundID)
            for member_idx in range(type.get_nmembers()):
                subtype = type.get_member_type(member_idx)
                if not isinstance(subtype, h5py.h5t.TypeStringID):
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/BathymetryCoverage is not a string"
                    )
                    return
                if not subtype.is_variable_str():
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/BathymetryCoverage is not a variable length string"
                    )

            values = BathymetryCoverage[:]
            expected_values = [
                (0, 0, "depth"),
                (0, 1, "depth"),
                (0, 2, "metres"),
                (0, 3, "1000000"),
                (0, 4, "H5T_FLOAT"),
                (0, 5, "-14"),
                (0, 6, "11050"),
                (0, 7, "closedInterval"),
                (1, 0, "uncertainty"),
                (1, 1, "uncertainty"),
                (1, 2, "metres"),
                (1, 3, "1000000"),
                (1, 4, "H5T_FLOAT"),
                (1, 5, "0"),
                (1, 6, ""),
                (1, 7, "geSemiInterval"),
            ]

            for row, col, expected_value in expected_values:
                if row < BathymetryCoverage.shape[0]:
                    value = values[row][col].decode("utf-8")
                    if value != expected_value:
                        self._critical_error(
                            f"/Group_F/BathymetryCoverage: row {row}, {col}, got value '{value}', whereas '{expected_value}' is expected"
                        )

    def _validate_group_f_QualityOfBathymetryCoverage(self, group_f):
        self._log_check("102_Dev1027")

        QualityOfBathymetryCoverage = group_f["QualityOfBathymetryCoverage"]
        if not isinstance(QualityOfBathymetryCoverage, h5py.Dataset):
            self._critical_error(
                "'/Group_F/QualityOfBathymetryCoverage' is not a dataset"
            )
        elif QualityOfBathymetryCoverage.shape != (1,):
            self._critical_error(
                "'/Group_F/QualityOfBathymetryCoverage' is not a one-dimensional dataset of shape 1"
            )
        elif QualityOfBathymetryCoverage.dtype != [
            ("code", "O"),
            ("name", "O"),
            ("uom.name", "O"),
            ("fillValue", "O"),
            ("datatype", "O"),
            ("lower", "O"),
            ("upper", "O"),
            ("closure", "O"),
        ]:
            self._critical_error(
                "'/Group_F/QualityOfBathymetryCoverage' has not expected data type"
            )
        else:
            type = QualityOfBathymetryCoverage.id.get_type()
            assert isinstance(type, h5py.h5t.TypeCompoundID)
            for member_idx in range(type.get_nmembers()):
                subtype = type.get_member_type(member_idx)
                if not isinstance(subtype, h5py.h5t.TypeStringID):
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/QualityOfBathymetryCoverage is not a string"
                    )
                    return
                if not subtype.is_variable_str():
                    self._critical_error(
                        f"Member of index {member_idx} in /Group_F/QualityOfBathymetryCoverage is not a variable length string"
                    )

            values = QualityOfBathymetryCoverage[:]
            expected_values = [
                (0, 0, "iD"),
                (0, 1, "ID"),
                (0, 2, ""),
                (0, 3, "0"),
                (0, 4, "H5T_INTEGER"),
                (0, 5, "1"),
                (0, 6, ""),
                (0, 7, "geSemiInterval"),
            ]

            for row, col, expected_value in expected_values:
                value = values[row][col].decode("utf-8")
                if value != expected_value:
                    self._critical_error(
                        f"/Group_F/QualityOfBathymetryCoverage: row {row}, {col}, got value '{value}', whereas '{expected_value}' is expected"
                    )

    def _validate_BathymetryCoverage(self, f):
        BathymetryCoverage = f["BathymetryCoverage"]
        if not isinstance(BathymetryCoverage, h5py.Group):
            self._critical_error("/BathymetryCoverage is not a group")
            return

        # Cf Table 10-4 - Attributes of BathymetryCoverage feature container group
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
                fixed_value=2,
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
                name="numInstances",
                required=True,
                type="uint8",
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
                required=True,
                type="enumeration",
                fixed_value=5,
            ),
        ]

        self._log_check("102_Dev2001")
        self._check_attributes(
            "BathymetryCoverage group", BathymetryCoverage, attr_list
        )

        numInstances = _get_int_attr_or_none(BathymetryCoverage, "numInstances")
        if numInstances is not None:
            if numInstances <= 0:
                self._critical_error(
                    '/BathymetryCoverage["numInstances"] attribute value must be >= 1'
                )
                numInstances = None

        if "commonPointRule" in BathymetryCoverage.attrs:
            expected_values = {
                1: "average",
                2: "low",
                3: "high",
                4: "all",
            }
            self._validate_enumeration(
                BathymetryCoverage, "commonPointRule", expected_values
            )

        if "dataCodingFormat" in BathymetryCoverage.attrs:
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
            self._validate_enumeration(
                BathymetryCoverage, "dataCodingFormat", expected_values
            )

        horizontalPositionUncertainty = _get_float_attr_or_none(
            BathymetryCoverage, "horizontalPositionUncertainty"
        )
        if horizontalPositionUncertainty and not (
            horizontalPositionUncertainty == -1.0 or horizontalPositionUncertainty >= 0
        ):
            self._warning(
                '/BathymetryCoverage["horizontalPositionUncertainty"] attribute value must be -1 or positive'
            )

        verticalUncertainty = _get_float_attr_or_none(
            BathymetryCoverage, "verticalUncertainty"
        )
        if verticalUncertainty and not (
            verticalUncertainty == -1.0 or verticalUncertainty >= 0
        ):
            self._warning(
                '/BathymetryCoverage["verticalUncertainty"] attribute value must be -1 or positive'
            )

        scanDirection_values = None
        if "sequencingRule.scanDirection" in BathymetryCoverage.attrs:
            scanDirection = BathymetryCoverage.attrs["sequencingRule.scanDirection"]
            if isinstance(scanDirection, str):
                # strip leading space. IMHO there should not be any, but
                # the examples in the specification sometimes show one...
                scanDirection_values = [x.lstrip() for x in scanDirection.split(",")]

                self._log_check("102_Dev2011")
                if len(scanDirection_values) != 2:
                    self._warning(
                        '/BathymetryCoverage["sequencingRule.scanDirection"] attribute should have 2 values'
                    )
                elif "axisNames" in BathymetryCoverage.keys():

                    scanDirection_values_without_orientation = []
                    for v in scanDirection_values:
                        if v.startswith("-"):
                            scanDirection_values_without_orientation.append(v[1:])
                        else:
                            scanDirection_values_without_orientation.append(v)
                    scanDirection_values_without_orientation = set(
                        scanDirection_values_without_orientation
                    )

                    axisNames = BathymetryCoverage["axisNames"]
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

        # Check that QualityOfBathymetryCoverage has (almost) the same attributes as BathymetryCoverage
        if "QualityOfBathymetryCoverage" in f.keys():
            QualityOfBathymetryCoverage = f["QualityOfBathymetryCoverage"]
            if not isinstance(QualityOfBathymetryCoverage, h5py.Group):
                self._critical_error("/QualityOfBathymetryCoverage is not a group")
            else:
                attr_list[0] = AttributeDefinition(
                    name="dataCodingFormat",
                    required=True,
                    type="enumeration",
                    fixed_value=9,
                )
                self._log_check("102_Dev2002")
                self._check_attributes(
                    "QualityOfBathymetryCoverage group",
                    QualityOfBathymetryCoverage,
                    attr_list,
                )

        self._validate_axisNames(f, BathymetryCoverage)

        subgroups = set(
            [
                name
                for name, item in BathymetryCoverage.items()
                if isinstance(item, h5py.Group)
            ]
        )

        self._log_check("102_Dev2007")
        if len(subgroups) == 0:
            self._critical_error("/BathymetryCoverage has no groups")
        else:
            for i in range(1, len(subgroups) + 1):
                expected_name = "BathymetryCoverage.%02d" % i
                if expected_name not in subgroups:
                    self._critical_error(
                        "/BathymetryCoverage/{expected_name} group does not exist"
                    )

            for name in subgroups:
                if not name.startswith("BathymetryCoverage."):
                    self._warning(
                        "/BathymetryCoverage/{expected_name} is an unexpected group"
                    )

        self._log_check("102_Dev2008")
        if numInstances and len(subgroups) != numInstances:
            self._critical_error(
                f"/BathymetryCoverage has {len(subgroups)} groups whereas numInstances={numInstances}"
            )

        # Attributes and groups already checked above
        self._log_check("102_Dev2012")
        for name, item in BathymetryCoverage.items():
            if isinstance(item, h5py.Dataset) and name != "axisNames":
                self._warning(f"/BathymetryCoverage has unexpected dataset {name}")

            if isinstance(item, h5py.Group) and name.startswith("BathymetryCoverage."):
                self._validate_BathymetryCoverage_instance(f, BathymetryCoverage, item)

    def _validate_BathymetryCoverage_instance(self, f, BathymetryCoverage, instance):

        # Cf Table 10-6 - Attributes of BathymetryCoverage feature instance group
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
                name="numGRP",
                required=True,
                type="uint8",
                fixed_value=1,
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
                name="verticalDatum",
                required=False,
                type="uint16",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="verticalDatumReference",
                required=False,
                type="uint8",
                fixed_value=1,
            ),
        ]

        self._log_check("102_Dev3001")
        self._check_attributes(
            f"BathymetryCoverage feature instance group {instance.name}",
            instance,
            attr_list,
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
                f"BathymetryCoverage feature instance group {instance.name}: attributes {present} are present, but {missing} are missing"
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
            if horizontalCRS:
                if horizontalCRS == 4326:
                    # 102_Dev3002
                    self._validate_bounds(
                        f"BathymetryCoverage feature instance group {instance.name}",
                        instance,
                    )

                    if (
                        top_westBoundLongitude is not None
                        and top_eastBoundLongitude is not None
                        and top_northBoundLatitude is not None
                        and top_southBoundLatitude is not None
                    ):
                        self._log_check("102_Dev3004")
                        if westBoundLongitude < top_westBoundLongitude:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: westBoundLongitude={westBoundLongitude} < top_westBoundLongitude={top_westBoundLongitude}"
                            )
                        if southBoundLatitude < top_southBoundLatitude:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: southBoundLatitude={southBoundLatitude} < top_southBoundLatitude={top_southBoundLatitude}"
                            )
                        if eastBoundLongitude > top_eastBoundLongitude:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: eastBoundLongitude={eastBoundLongitude} > top_eastBoundLongitude={top_eastBoundLongitude}"
                            )
                        if northBoundLatitude > top_northBoundLatitude:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: northBoundLatitude={northBoundLatitude} > top_northBoundLatitude={top_northBoundLatitude}"
                            )

                else:
                    if (
                        abs(westBoundLongitude) <= 180
                        and abs(eastBoundLongitude) <= 180
                        and abs(northBoundLatitude) <= 90
                        and abs(southBoundLatitude) <= 90
                    ):
                        self._error(
                            f"BathymetryCoverage feature instance group {instance.name}: westBoundLongitude, eastBoundLongitude, northBoundLatitude, southBoundLatitude are longitudes/latitudes whereas they should be projected coordinates, given the horizontalCRS is projected"
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

                        self._log_check("102_Dev3004")
                        crs_area_of_use = horizontalCRS_srs.GetAreaOfUse()
                        # Add a substantial epsilon as going a bit outside of the CRS area of use is usually fine
                        epsilon = 1
                        if westLon + epsilon < crs_area_of_use.west_lon_degree:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: westLon={westLon} < crs_area_of_use.west_lon_degree={crs_area_of_use.west_lon_degree}"
                            )
                        if southLat + epsilon < crs_area_of_use.south_lat_degree:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: southLat={southLat} < crs_area_of_use.south_lat_degree={crs_area_of_use.south_lat_degree}"
                            )
                        if eastLon - epsilon > crs_area_of_use.east_lon_degree:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: eastLon={eastLon} > crs_area_of_use.east_lon_degree={crs_area_of_use.east_lon_degree}"
                            )
                        if northLat - epsilon > crs_area_of_use.north_lat_degree:
                            self._error(
                                f"BathymetryCoverage feature instance group {instance.name}: northLat={northLat} > crs_area_of_use.north_lat_degree={crs_area_of_use.north_lat_degree}"
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
                                    f"BathymetryCoverage feature instance group {instance.name}: westBoundLongitude={westLon} ({westBoundLongitude}) < top_westBoundLongitude={top_westBoundLongitude}"
                                )
                            if southLat + epsilon < top_southBoundLatitude:
                                self._error(
                                    f"BathymetryCoverage feature instance group {instance.name}: southBoundLatitude={southLat} ({southBoundLatitude}) < top_southBoundLatitude={top_southBoundLatitude}"
                                )
                            if eastLon - epsilon > top_eastBoundLongitude:
                                self._error(
                                    f"BathymetryCoverage feature instance group {instance.name}: eastBoundLongitude={eastLon} ({eastBoundLongitude}) > top_eastBoundLongitude={top_eastBoundLongitude}"
                                )
                            if northLat - epsilon > top_northBoundLatitude:
                                self._error(
                                    f"BathymetryCoverage feature instance group {instance.name}: northBoundLatitude={northLat} ({northBoundLatitude}) > top_northBoundLatitude={top_northBoundLatitude}"
                                )

                    else:
                        self._warning(
                            "Test checking consistency of bounds in BathymetryCoverage feature instance group compared to top level attributes skipped due to GDAL not available"
                        )

            self._log_check("102_Dev3003")
            if eastBoundLongitude <= westBoundLongitude:
                self._error(
                    f"BathymetryCoverage feature instance group {instance.name}: eastBoundLongitude <= westBoundLongitude"
                )
            if northBoundLatitude <= southBoundLatitude:
                self._error(
                    f"BathymetryCoverage feature instance group {instance.name}: northBoundLatitude <= southBoundLatitude"
                )

        if len(present) == 0 and "domainExtent.polygon" not in instance.keys():
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: dataset 'domainExtent.polygon' missing"
            )
        elif "domainExtent.polygon" in instance.keys() and present:
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
                self._log_check("102_Dev3005")

                # gridOriginLongitude is encoded as a float64, whereas westBoundLongitude on a float32
                # hence add some tolerance so comparison is fair
                if (
                    gridOriginLongitude + 1e-6 * abs(gridOriginLongitude)
                    < westBoundLongitude
                ):
                    self._error(
                        f"BathymetryCoverage feature instance group {instance.name}: gridOriginLongitude={gridOriginLongitude} < westBoundLongitude={westBoundLongitude}"
                    )
                if (
                    gridOriginLongitude - 1e-6 * abs(gridOriginLongitude)
                    > eastBoundLongitude
                ):
                    self._error(
                        f"BathymetryCoverage feature instance group {instance.name}: gridOriginLongitude={gridOriginLongitude} > eastBoundLongitude={eastBoundLongitude}"
                    )
                if (
                    gridOriginLatitude + 1e-6 * abs(gridOriginLatitude)
                    < southBoundLatitude
                ):
                    self._error(
                        f"BathymetryCoverage feature instance group {instance.name}: gridOriginLatitude={gridOriginLatitude} < southBoundLatitude={southBoundLatitude}"
                    )
                if (
                    gridOriginLatitude - 1e-6 * abs(gridOriginLatitude)
                    > northBoundLatitude
                ):
                    self._error(
                        f"BathymetryCoverage feature instance group {instance.name}: gridOriginLatitude={gridOriginLatitude} > northBoundLatitude={northBoundLatitude}"
                    )

                if gdal_available and horizontalCRS:
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
                            f"BathymetryCoverage feature instance group {instance.name}: origin_long={origin_long} < crs_area_of_use.west_lon_degree={crs_area_of_use.west_lon_degree}"
                        )
                    if origin_lat + epsilon < crs_area_of_use.south_lat_degree:
                        self._error(
                            f"BathymetryCoverage feature instance group {instance.name}: origin_lat={origin_lat} < crs_area_of_use.south_lat_degree={crs_area_of_use.south_lat_degree}"
                        )
                    if origin_long - epsilon > crs_area_of_use.east_lon_degree:
                        self._error(
                            f"BathymetryCoverage feature instance group {instance.name}: origin_long={origin_long} > crs_area_of_use.east_lon_degree={crs_area_of_use.east_lon_degree}"
                        )
                    if origin_lat - epsilon > crs_area_of_use.north_lat_degree:
                        self._error(
                            f"BathymetryCoverage feature instance group {instance.name}: origin_lat={origin_lat} > crs_area_of_use.north_lat_degree={crs_area_of_use.north_lat_degree}"
                        )

        self._log_check("102_Dev3006")
        gridSpacingLongitudinal = _get_float_attr_or_none(
            instance, "gridSpacingLongitudinal"
        )
        if gridSpacingLongitudinal is not None and gridSpacingLongitudinal <= 0:
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: Grid spacing attribute in instance group has value out of range: gridSpacingLongitudinal <= 0"
            )

        self._log_check("102_Dev3006")
        gridSpacingLatitudinal = _get_float_attr_or_none(
            instance, "gridSpacingLatitudinal"
        )
        if gridSpacingLatitudinal is not None and gridSpacingLatitudinal <= 0:
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: Grid spacing attribute in instance group has value out of range: gridSpacingLatitudinal <= 0"
            )

        self._log_check("102_Dev3007")
        if (
            gridSpacingLongitudinal is not None
            and eastBoundLongitude is not None
            and westBoundLongitude is not None
            and gridSpacingLongitudinal > (eastBoundLongitude - westBoundLongitude)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Value of gridSpacingLongitudinal or gridSpacingLatitudinal in instance group too high: gridSpacingLongitudinal > (eastBoundLongitude - westBoundLongitude)"
            )

        self._log_check("102_Dev3007")
        if (
            gridSpacingLatitudinal is not None
            and southBoundLatitude is not None
            and northBoundLatitude is not None
            and gridSpacingLatitudinal > (northBoundLatitude - southBoundLatitude)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Value of gridSpacingLongitudinal or gridSpacingLatitudinal in instance group too high: gridSpacingLatitudinal > (northBoundLatitude - southBoundLatitude)"
            )

        self._log_check("102_Dev3010")
        numPointsLongitudinal = _get_int_attr_or_none(instance, "numPointsLongitudinal")
        if numPointsLongitudinal < 1:
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: Grid must be at least 1X1: numPointsLongitudinal < 1"
            )

        self._log_check("102_Dev3010")
        numPointsLatitudinal = _get_int_attr_or_none(instance, "numPointsLatitudinal")
        if numPointsLatitudinal < 1:
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: Grid must be at least 1X1: numPointsLatitudinal < 1"
            )

        self._log_check("102_Dev3009")
        if (
            gridSpacingLongitudinal is not None
            and eastBoundLongitude is not None
            and westBoundLongitude is not None
            and numPointsLongitudinal is not None
            and numPointsLongitudinal > 1
            and gridSpacingLongitudinal * (1 - 1e-6)
            > (eastBoundLongitude - westBoundLongitude) / (numPointsLongitudinal - 1)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Grid dimensions are incompatible with instance bounding box: gridSpacingLongitudinal={gridSpacingLongitudinal} > (eastBoundLongitude - westBoundLongitude) / (numPointsLongitudinal -  1)={(eastBoundLongitude - westBoundLongitude) / (numPointsLongitudinal - 1)}"
            )

        self._log_check("102_Dev3009")
        if (
            gridSpacingLatitudinal is not None
            and southBoundLatitude is not None
            and northBoundLatitude is not None
            and numPointsLatitudinal is not None
            and numPointsLatitudinal > 1
            and gridSpacingLatitudinal * (1 - 1e-6)
            > (northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Grid dimensions are incompatible with instance bounding box: gridSpacingLatitudinal={gridSpacingLatitudinal} > (northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)={(northBoundLatitude - southBoundLatitude) / (numPointsLatitudinal - 1)}"
            )

        self._log_check("102_Dev3012")
        # gridOriginLongitude is encoded as a float64, whereas westBoundLongitude on a float32
        # hence add some tolerance so comparison is fair
        if (
            westBoundLongitude is not None
            and gridOriginLongitude is not None
            and abs(westBoundLongitude - gridOriginLongitude)
            > 1e-6 * abs(westBoundLongitude)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Grid origin does not coincide with instance bounding box; westBoundLongitude={westBoundLongitude} !=  gridOriginLongitude={_cast_to_float32(gridOriginLongitude)}"
            )

        self._log_check("102_Dev3012")
        if (
            southBoundLatitude is not None
            and gridOriginLatitude is not None
            and abs(southBoundLatitude - gridOriginLatitude)
            > 1e-6 * abs(southBoundLatitude)
        ):
            self._warning(
                f"BathymetryCoverage feature instance group {instance.name}: Grid origin does not coincide with instance bounding box: southBoundLatitude={southBoundLatitude} !=  gridOriginLatitude={_cast_to_float32(gridOriginLatitude)}"
            )

        self._log_check("102_Dev3013")
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
                        f"BathymetryCoverage feature instance group {instance.name}: invalid content for startSequence in instance"
                    )
                else:
                    self._log_check("102_Dev3014")
                    if startSequence != ["0", "0"]:
                        # other tests are probably not compatible of a non (0,0) startSequence
                        self._warning(
                            f"BathymetryCoverage feature instance group {instance.name}: Values in startSequence in instance group are incompatible with the scan direction in sequencingRule"
                        )

        self._log_check("102_Dev3015")
        # Attributes already checked above
        countGroups = 0
        for name, item in instance.items():
            if isinstance(item, h5py.Dataset) and name != "domainExtent.polygon":
                # 102_Dev2012
                self._warning(
                    f"BathymetryCoverage feature instance group {instance.name} has unexpected dataset '{name}'"
                )

            elif isinstance(item, h5py.Group):
                countGroups += 1
                if name != "Group_001":
                    self._warning(
                        f"BathymetryCoverage feature instance group {instance.name} has unexpected group '{name}'"
                    )

        self._log_check("102_Dev3016")
        numGRP = _get_int_attr_or_none(instance, "numGRP")
        if numGRP is not None:
            if numGRP != countGroups:
                self._critical_error(
                    f"BathymetryCoverage feature instance group {instance.name}: Count of values groups does not match attribute numGRP in instance group"
                )

        self._validate_verticalDatum(instance.name, instance)
        verticalDatum = _get_int_attr_or_none(instance, "verticalDatum")
        topVerticalDatum = _get_int_attr_or_none(f, "verticalDatum")
        if verticalDatum is not None and topVerticalDatum is not None:
            if verticalDatum == topVerticalDatum:
                self._error(
                    f"BathymetryCoverage feature instance group {instance.name} has same value for 'verticalDatum' attribute as top level attribute"
                )

        # Check that QualityOfBathymetryCoverage.QualityOfBathymetryCoverage.01
        # has same attributes as BathymetryCoverage.BathymetryCoverage.01
        self._log_check("102_Dev3017")
        if "QualityOfBathymetryCoverage" in f.keys():
            QualityOfBathymetryCoverage = f["QualityOfBathymetryCoverage"]
            if isinstance(QualityOfBathymetryCoverage, h5py.Group):
                if (
                    "QualityOfBathymetryCoverage.01"
                    in QualityOfBathymetryCoverage.keys()
                ):
                    QualityOfBathymetryCoverage01 = QualityOfBathymetryCoverage[
                        "QualityOfBathymetryCoverage.01"
                    ]
                    if isinstance(QualityOfBathymetryCoverage01, h5py.Group):
                        set1 = set([name for name in instance.attrs.keys()])
                        set2 = set(
                            [
                                name
                                for name in QualityOfBathymetryCoverage01.attrs.keys()
                            ]
                        )
                        if set1 != set2:
                            self._error(
                                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01 has not same set of attributes ({set1}) as /BathymetryCoverage/BathymetryCoverage.01 ({set2})"
                            )

                        for name in set1:
                            attr1 = instance.attrs[name]
                            if name in set2:
                                attr2 = QualityOfBathymetryCoverage01.attrs[name]
                                if attr1 != attr2:
                                    self._error(
                                        f'/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01["{name}"] = {attr1} has not same same value as /BathymetryCoverage/BathymetryCoverage.01["{name}"] = {attr2}'
                                    )

        if "Group_001" not in instance.keys() or not isinstance(
            instance["Group_001"], h5py.Group
        ):
            self._critical_error(
                f"BathymetryCoverage feature instance group {instance.name}: no Group_001 subgroup"
            )
        else:
            self._validate_Group_001(
                f, instance["Group_001"], numPointsLongitudinal, numPointsLatitudinal
            )

    def _validate_Group_001(
        self, f, Group_001, numPointsLongitudinal, numPointsLatitudinal
    ):

        # Cf Table 10-7 - Attributes of values group
        attr_list = [
            AttributeDefinition(
                name="minimumDepth",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="maximumDepth",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="minimumUncertainty",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="maximumUncertainty",
                required=True,
                type="float32",
                fixed_value=None,
            ),
            AttributeDefinition(
                name="timePoint",
                required=True,
                type="string",
                fixed_value="00010101T000000Z",
            ),
        ]

        self._log_check("102_Dev5001")
        self._check_attributes(
            "Group_001",
            Group_001,
            attr_list,
        )

        self._log_check("102_Dev5002")
        minimumDepth = _get_float_attr_or_none(Group_001, "minimumDepth")
        if minimumDepth is not None and not (
            minimumDepth >= -14 and minimumDepth <= 11050
        ):
            self._warning(
                f"Group_001: minimumDepth={minimumDepth} should be in [-14, 11050] range"
            )

        maximumDepth = _get_float_attr_or_none(Group_001, "maximumDepth")
        if maximumDepth is not None and not (
            maximumDepth >= -14 and maximumDepth <= 11050
        ):
            self._warning(
                f"Group_001: maximumDepth={maximumDepth} should be in [-14, 11050] range"
            )

        if (
            minimumDepth is not None
            and maximumDepth is not None
            and minimumDepth > maximumDepth
        ):
            self._warning(
                f"Group_001: minimumDepth={minimumDepth} > maximumDepth={maximumDepth}"
            )

        minimumUncertainty = _get_float_attr_or_none(Group_001, "minimumUncertainty")
        if minimumUncertainty is not None and not (
            minimumUncertainty >= 0 or minimumUncertainty == 1000000
        ):
            self._warning(
                f"Group_001: minimumUncertainty={minimumUncertainty} should be in [0, inf) range or equal to 1000000"
            )

        maximumUncertainty = _get_float_attr_or_none(Group_001, "maximumUncertainty")
        if maximumUncertainty is not None and not (
            maximumUncertainty >= 0 or maximumUncertainty == 1000000
        ):
            self._warning(
                f"Group_001: maximumUncertainty={maximumUncertainty} should be in [0, inf) range or equal to 1000000"
            )

        if (
            minimumUncertainty is not None
            and maximumUncertainty is not None
            and minimumUncertainty != 1000000
            and maximumUncertainty != 1000000
            and minimumUncertainty > maximumUncertainty
        ):
            self._warning(
                f"Group_001: minimumUncertainty={minimumUncertainty} > maximumUncertainty={maximumUncertainty}"
            )

        self._log_check("102_Dev5003")
        if "values" not in Group_001.keys() or not isinstance(
            Group_001["values"], h5py.Dataset
        ):
            self._critical_error(
                "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values dataset missing"
            )
        else:
            self._validate_values(
                f,
                Group_001["values"],
                numPointsLongitudinal,
                numPointsLatitudinal,
                minimumDepth,
                maximumDepth,
                minimumUncertainty,
                maximumUncertainty,
            )

    def _validate_values(
        self,
        f,
        values,
        numPointsLongitudinal,
        numPointsLatitudinal,
        minimumDepth,
        maximumDepth,
        minimumUncertainty,
        maximumUncertainty,
    ):

        self._log_check("102_Dev5004")
        if len(values.shape) != 2:
            self._critical_error(
                "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values dataset is not 2-dimensional"
            )
            return

        if (
            numPointsLatitudinal
            and numPointsLongitudinal
            and values.shape != (numPointsLatitudinal, numPointsLongitudinal)
        ):
            self._critical_error(
                f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values dataset shape is {values.shape} instead of {(numPointsLatitudinal, numPointsLongitudinal)}"
            )
            return

        self._log_check("102_Dev5005")
        values_type = values.id.get_type()
        if not isinstance(values_type, h5py.h5t.TypeCompoundID):
            self._critical_error(
                "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values type is not compound"
            )
            return

        Group_F_BathymetryCoverage = None
        if "Group_F" in f:
            Group_F = f["Group_F"]
            if isinstance(Group_F, h5py.Group) and "BathymetryCoverage" in Group_F:
                Group_F_BathymetryCoverage = Group_F["BathymetryCoverage"]
                if (
                    isinstance(Group_F_BathymetryCoverage, h5py.Dataset)
                    and len(Group_F_BathymetryCoverage.shape) == 1
                ):
                    num_components = Group_F_BathymetryCoverage.shape[0]
                    if values_type.get_nmembers() != num_components:
                        self._critical_error(
                            f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values type has {values_type.get_nmembers()} members whereas {num_components} are expected from /Group_F/BathymetryCoverage"
                        )
                        return
                else:
                    Group_F_BathymetryCoverage = None

        # Check consistency between "values" and "/Group_F/BathymetryCoverage"
        found_depth = False
        found_uncertainty = False
        for member_idx in range(values_type.get_nmembers()):
            subtype = values_type.get_member_type(member_idx)
            component_name = values_type.get_member_name(member_idx)
            if Group_F_BathymetryCoverage:
                expected = Group_F_BathymetryCoverage[member_idx][0]
                if component_name != expected:
                    self._critical_error(
                        f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values member {member_idx} name = {component_name} is not Group_F_BathymetryCoverage[{member_idx}]['name']] = {expected}"
                    )
            if not self._is_float32(subtype):
                self._critical_error(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values member {component_name} is not a float32"
                )

            if component_name == b"depth":
                found_depth = True
            elif component_name == b"uncertainty":
                found_uncertainty = True

        self._log_check("102_Dev5006")
        if found_depth:
            masked_depth = np.ma.masked_equal(values[:]["depth"], 1000000)

            actualMinDepth = masked_depth.min()
            if minimumDepth and actualMinDepth < minimumDepth:
                self._critical_error(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: minimum depth is {actualMinDepth}, whereas minimumDepth attribute = {minimumDepth}"
                )

            actualMaxDepth = masked_depth.max()
            if maximumDepth and actualMaxDepth > maximumDepth:
                self._critical_error(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: minimum depth is {actualMaxDepth}, whereas maximumDepth attribute = {maximumDepth}"
                )

            self._log_check("102_Dev5009")
            # check if the precision of any depth or uncertainty value exceeds 0.01 meters
            depth_100 = values[:]["depth"] * 100
            depth_100_round = np.round(depth_100)
            max_prec_cm = np.max(np.abs(depth_100 - depth_100_round))
            if max_prec_cm > 0.001:  # tolerate some epsilon
                self._warning(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: maximum precision of depth is {max_prec_cm} cm, whereas it should not be better than centimetric"
                )

        if found_uncertainty:
            masked_uncertainty = np.ma.masked_equal(values[:]["uncertainty"], 1000000)

            actualMinUncertainty = masked_uncertainty.min()
            if minimumUncertainty and actualMinUncertainty < minimumUncertainty:
                self._critical_error(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: minimum uncertainty is {actualMinUncertainty}, whereas minimumUncertainty attribute = {minimumUncertainty}"
                )

            actualMaxUncertainty = masked_uncertainty.max()
            if maximumUncertainty and actualMaxUncertainty > maximumUncertainty:
                self._critical_error(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: minimum uncertainty is {actualMaxUncertainty}, whereas maximumUncertainty attribute = {maximumUncertainty}"
                )

            self._log_check("102_Dev5009")
            # check if the precision of any depth or uncertainty value exceeds 0.01 meters
            depth_100 = values[:]["uncertainty"] * 100
            depth_100_round = np.round(depth_100)
            max_prec_cm = np.max(np.abs(depth_100 - depth_100_round))
            if max_prec_cm > 0.001:  # tolerate some epsilon
                self._warning(
                    f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values: maximum precision of uncertainty is {max_prec_cm} cm, whereas it should not be better than centimetric"
                )

    def _validate_QualityOfBathymetryCoverage(self, f):

        QualityOfBathymetryCoverage = f["QualityOfBathymetryCoverage"]
        if not isinstance(QualityOfBathymetryCoverage, h5py.Group):
            self._critical_error("/QualityOfBathymetryCoverage is not a group")
            return

        self._validate_axisNames(f, QualityOfBathymetryCoverage)

        self._validate_featureAttributeTable(QualityOfBathymetryCoverage)

        subgroups = set(
            [
                name
                for name, item in QualityOfBathymetryCoverage.items()
                if isinstance(item, h5py.Group)
            ]
        )
        self._log_check("102_Dev2009")
        if len(subgroups) == 0:
            self._critical_error("/QualityOfBathymetryCoverage has no groups")
        else:
            self._log_check("102_Dev2010")
            for i in range(1, len(subgroups) + 1):
                expected_name = "QualityOfBathymetryCoverage.%02d" % i
                if expected_name not in subgroups:
                    self._critical_error(
                        "/QualityOfBathymetryCoverage/{expected_name} group does not exist"
                    )

            for name in subgroups:
                if not name.startswith("QualityOfBathymetryCoverage."):
                    self._warning(
                        "/QualityOfBathymetryCoverage/{expected_name} is an unexpected group"
                    )

        if "numInstances" in QualityOfBathymetryCoverage.attrs:
            numInstances = QualityOfBathymetryCoverage.attrs["numInstances"]
            if not isinstance(numInstances, int):
                numInstances = None
        else:
            numInstances = None

        self._log_check("102_Dev2010")
        if numInstances and len(subgroups) != numInstances:
            self._warning(
                "/QualityOfBathymetryCoverage has {len(subgroups)} groups whereas numInstances={numInstances}"
            )

        # Attributes and groups already checked above
        self._log_check("102_Dev2012")
        for name, item in QualityOfBathymetryCoverage.items():
            if isinstance(item, h5py.Dataset) and name not in (
                "axisNames",
                "featureAttributeTable",
            ):
                self._warning(
                    f"/QualityOfBathymetryCoverage has unexpected dataset {name}"
                )

        if "QualityOfBathymetryCoverage.01" in subgroups and isinstance(
            QualityOfBathymetryCoverage["QualityOfBathymetryCoverage.01"], h5py.Group
        ):
            QualityOfBathymetryCoverage_01 = QualityOfBathymetryCoverage[
                "QualityOfBathymetryCoverage.01"
            ]
            self._validate_QualityOfBathymetryCoverage_01(
                QualityOfBathymetryCoverage, QualityOfBathymetryCoverage_01
            )

    def _validate_QualityOfBathymetryCoverage_01(
        self, QualityOfBathymetryCoverage, QualityOfBathymetryCoverage_01
    ):
        self._log_check("102_Dev5010")
        subgroups = set(
            [
                name
                for name, item in QualityOfBathymetryCoverage_01.items()
                if isinstance(item, h5py.Group)
            ]
        )
        if subgroups != set(["Group_001"]):
            self._warning(
                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01 has unexpected group list: {subgroups}"
            )

        datasets = set(
            [
                name
                for name, item in QualityOfBathymetryCoverage_01.items()
                if isinstance(item, h5py.Dataset)
            ]
        )
        if datasets:
            self._warning(
                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01 has unexpected dataset list: {datasets}"
            )

        if "Group_001" in subgroups and isinstance(
            QualityOfBathymetryCoverage_01["Group_001"], h5py.Group
        ):

            numPointsLongitudinal = _get_int_attr_or_none(
                QualityOfBathymetryCoverage_01, "numPointsLongitudinal"
            )
            numPointsLatitudinal = _get_int_attr_or_none(
                QualityOfBathymetryCoverage_01, "numPointsLatitudinal"
            )

            Group_001 = QualityOfBathymetryCoverage_01["Group_001"]
            self._validate_QualityOfBathymetryCoverage_01_Group_001(
                QualityOfBathymetryCoverage,
                Group_001,
                numPointsLongitudinal,
                numPointsLatitudinal,
            )

    def _validate_QualityOfBathymetryCoverage_01_Group_001(
        self,
        QualityOfBathymetryCoverage,
        Group_001,
        numPointsLongitudinal,
        numPointsLatitudinal,
    ):
        if "values" in Group_001 and isinstance(Group_001["values"], h5py.Dataset):
            values = Group_001["values"]
            self._validate_QualityOfBathymetryCoverage_01_Group_001_values(
                QualityOfBathymetryCoverage,
                values,
                numPointsLongitudinal,
                numPointsLatitudinal,
            )
        else:
            self._critical_error(
                "Missing /QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01/Group_001/values dataset"
            )

        self._log_check("102_Dev5010")
        subgroups = set(
            [name for name, item in Group_001.items() if isinstance(item, h5py.Group)]
        )
        if subgroups:
            self._warning(
                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01/Group_001 has unexpected group list: {subgroups}"
            )

        datasets = set(
            [name for name, item in Group_001.items() if isinstance(item, h5py.Dataset)]
        )
        if datasets != set(["values"]):
            self._warning(
                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01/Group_001 has unexpected dataset list: {datasets}"
            )

    def _validate_QualityOfBathymetryCoverage_01_Group_001_values(
        self,
        QualityOfBathymetryCoverage,
        values,
        numPointsLongitudinal,
        numPointsLatitudinal,
    ):

        self._log_check("102_Dev5007")
        if len(values.shape) != 2:
            self._critical_error(
                "/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01/Group_001/values dataset is not 2-dimensional"
            )
            return

        if (
            numPointsLatitudinal
            and numPointsLongitudinal
            and values.shape != (numPointsLatitudinal, numPointsLongitudinal)
        ):
            self._critical_error(
                f"/QualityOfBathymetryCoverage/QualityOfBathymetryCoverage.01/Group_001/values dataset shape is {values.shape} instead of {(numPointsLatitudinal, numPointsLongitudinal)}"
            )
            return

        self._log_check("102_Dev5007")
        values_type = values.id.get_type()
        if not self._is_uint32(values_type):
            self._critical_error(
                "/BathymetryCoverage/BathymetryCoverage.01/Group_001/values type is not uint32"
            )
            if (
                isinstance(values_type, h5py.h5t.TypeCompoundID)
                and values_type.get_nmembers() == 1
                and self._is_uint32(values_type.get_member_type(0))
            ):
                # Tolerance for dataset 102DE00CA22_UNC_MD.H5 to proceed to further checks
                values = values[:][values_type.get_member_name(0).decode("utf-8")]
            else:
                return

        self._log_check("102_Dev5008")
        if "featureAttributeTable" in QualityOfBathymetryCoverage and isinstance(
            QualityOfBathymetryCoverage["featureAttributeTable"], h5py.Dataset
        ):
            fat = QualityOfBathymetryCoverage["featureAttributeTable"]
            fat_type = fat.id.get_type()
            if len(fat.shape) == 1 and isinstance(fat_type, h5py.h5t.TypeCompoundID):
                try:
                    idx = fat_type.get_member_index(b"id")
                except Exception:
                    idx = -1
                if idx >= 0:
                    set_values = set(np.unique(values))
                    set_fat_values = set(np.unique(fat[:]["id"]))
                    for v in set_values:
                        if v != 0 and v not in set_fat_values:
                            self._error(
                                f"/BathymetryCoverage/BathymetryCoverage.01/Group_001/values contain value {v}, which is not a valid 'id' of the featureAttributeTable"
                            )

    def _validate_featureAttributeTable(self, QualityOfBathymetryCoverage):
        self._log_check("102_Dev2005")
        if "featureAttributeTable" not in QualityOfBathymetryCoverage:
            self._error(
                "/QualityOfBathymetryCoverage/featureAttributeTable dataset does not exist"
            )
        elif not isinstance(
            QualityOfBathymetryCoverage["featureAttributeTable"],
            h5py.Dataset,
        ):
            self._error(
                "/QualityOfBathymetryCoverage/featureAttributeTable is not a dataset"
            )
        else:
            self._log_check("102_Dev2006")
            featureAttributeTable = QualityOfBathymetryCoverage["featureAttributeTable"]

            # Cf Table 10-8 - Elements of featureAttributeTable compound datatype
            if len(featureAttributeTable.shape) != 1:
                self._error(
                    "/QualityOfBathymetryCoverage/featureAttributeTable is not a one-dimensional dataset"
                )
                return

            type = featureAttributeTable.id.get_type()
            if not isinstance(type, h5py.h5t.TypeCompoundID):
                self._error(
                    "/QualityOfBathymetryCoverage/featureAttributeTable type is not compound"
                )
                return

            try:
                idx = type.get_member_index(b"id")
            except Exception:
                idx = -1
            if idx < 0:
                self._error(
                    "/QualityOfBathymetryCoverage/featureAttributeTable compound type does not contain an 'id' member"
                )
                return
            h5_type = type.get_member_type(idx)
            if not (
                isinstance(h5_type, h5py.h5t.TypeIntegerID)
                and h5_type.get_sign() == h5py.h5t.SGN_NONE
                and h5_type.get_size() == 4
            ):
                self._error(
                    "/QualityOfBathymetryCoverage/featureAttributeTable['id'] type is not uint32"
                )
                return

            MemberDefinition = namedtuple(
                "MemberDefinition", ["name", "type", "allowed_values"]
            )

            allowed_members = [
                MemberDefinition("dataAssessment", "uint8", (1, 2, 3)),
                MemberDefinition(
                    "featuresDetected.leastDepthOfDetectedFeaturesMeasured",
                    "uint8",
                    (0, 1),
                ),
                MemberDefinition(
                    "featuresDetected.significantFeaturesDetected", "uint8", (0, 1)
                ),
                MemberDefinition(
                    "featuresDetected.sizeOfFeaturesDetected", "float32", None
                ),
                MemberDefinition("featureSizeVar", "float32", None),
                MemberDefinition("fullSeafloorCoverageAchieved", "uint8", (0, 1)),
                MemberDefinition("bathyCoverage", "uint8", (0, 1)),
                MemberDefinition(
                    "zoneOfConfidence.horizontalPositionUncertainty.uncertaintyFixed",
                    "float32",
                    None,
                ),
                MemberDefinition(
                    "zoneOfConfidence.horizontalPositionUncertainty.uncertaintyVariableFactor",
                    "float32",
                    None,
                ),
                MemberDefinition("surveyDateRange.dateStart", "date", None),
                MemberDefinition("surveyDateRange.dateEnd", "date", None),
                MemberDefinition("sourceSurveyID", "string", None),
                MemberDefinition("surveyAuthority", "string", None),
                MemberDefinition(
                    "typeOfBathymetricEstimationUncertainty", "enumeration", None
                ),
            ]

            allowed_members_dict = {t.name: t for t in allowed_members}

            for idx in range(type.get_nmembers()):
                name = type.get_member_name(idx).decode("utf-8")
                if name == "id":
                    continue
                if name not in allowed_members_dict:
                    self._error(
                        f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not an allowed member"
                    )
                    continue
                h5_type = type.get_member_type(idx)
                expected_type = allowed_members_dict[name].type
                if expected_type == "uint8":
                    if not self._is_uint8(h5_type):
                        self._error(
                            f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not of type uint8, but {h5_type}"
                        )
                elif expected_type == "float32":
                    if not self._is_float32(h5_type):
                        self._error(
                            f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not of type float32, but {h5_type}"
                        )
                elif expected_type == "date":
                    if not self._is_string(h5_type):
                        self._error(
                            f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not of type date, but {h5_type}"
                        )
                elif expected_type == "string":
                    if not self._is_string(h5_type):
                        self._error(
                            f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not of type string, but {h5_type}"
                        )
                elif expected_type == "enumeration":
                    if not self._is_enumeration(h5_type):
                        self._error(
                            f"/QualityOfBathymetryCoverage/featureAttributeTable['{name}'] is not of type enumeration, but {h5_type}"
                        )
                else:
                    raise Exception(
                        f"Programming error: unexpected type {expected_type}"
                    )

    def _validate_axisNames(self, f, group):

        groupName = group.name

        self._log_check("102_Dev2003")
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
                    self._log_check("102_Dev2004")
                    values = [v.decode("utf-8") for v in axisNames[:]]
                    if values not in (
                        ["Easting", "Northing"],
                        ["Latitude", "Longitude"],
                    ):
                        self._error(
                            f'{groupName}/axisNames must conform to CRS. Expected ["Easting", "Northing"] or ["Latitude", "Longitude"]'
                        )
                    elif "horizontalCRS" in f.attrs:
                        horizontalCRS = f.attrs["horizontalCRS"]
                        if isinstance(horizontalCRS, int):
                            if horizontalCRS == 4326:
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
    checker = S102Checker(
        filename,
        abort_at_first_error=abort_at_first_error,
    )
    checker.check()
    return checker.errors, checker.warnings, checker.checks_done


def usage():
    print("Usage: validate_s102.py [-q] <filename>")
    print("")
    print("Validates a S102 files against the Edition 3.0.0 specification.")
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
