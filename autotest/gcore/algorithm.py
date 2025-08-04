#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  Test gdalalgorithm.h functionality
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import pytest

from osgeo import gdal


def test_algorithm(tmp_path):
    reg = gdal.GetGlobalAlgorithmRegistry()

    names = reg.GetAlgNames()
    assert isinstance(names, list)
    assert "raster" in names

    alg = reg.InstantiateAlg("non_existing")
    assert alg is None

    with pytest.raises(Exception, match="NULL"):
        reg.InstantiateAlg(None)

    alg = reg.InstantiateAlg("raster")
    assert alg

    assert alg.GetName() == "raster"
    assert alg.GetDescription() == "Raster commands."
    assert alg.GetLongDescription() == ""
    assert alg.GetHelpFullURL() == "https://gdal.org/programs/gdal_raster.html"

    assert alg.HasSubAlgorithms()

    with pytest.raises(Exception, match="NULL"):
        alg.InstantiateSubAlgorithm(None)
    assert alg.InstantiateSubAlgorithm("non_existing") is None

    subalgs = alg.GetSubAlgorithmNames()
    assert isinstance(subalgs, list)
    assert "info" in subalgs

    convert = alg.InstantiateSubAlgorithm("convert")
    assert convert

    assert json.loads(convert.GetUsageAsJSON())["name"] == "convert"

    outfilename = str(tmp_path / "out.tif")
    assert convert.ParseCommandLineArguments(["data/byte.tif", outfilename])

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        last_pct[0] = pct
        return True

    assert convert.Run(my_progress)
    assert last_pct[0] == 1.0

    assert convert.Finalize()

    with gdal.Open(outfilename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    gdal.Unlink(outfilename)

    last_pct = [0]

    convert = alg.InstantiateSubAlgorithm("convert")
    assert convert.ParseRunAndFinalize(["data/byte.tif", outfilename], my_progress)
    assert last_pct[0] == 1.0

    assert convert.Finalize()

    with gdal.Open(outfilename) as ds:
        assert ds.GetRasterBand(1).Checksum() == 4672

    argNames = convert.GetArgNames()
    assert isinstance(argNames, list)
    assert "input" in argNames

    with pytest.raises(Exception):
        convert.GetArg(None)

    assert convert.GetArg("non_existing") is None

    arg = convert.GetArg("append")
    assert arg.GetName() == "append"
    assert arg.GetType() == gdal.GAAT_BOOLEAN
    assert arg.GetShortName() == ""
    assert arg.GetDescription() == "Append as a subdataset to existing output"
    assert arg.GetAliases() is None
    assert arg.GetMetaVar() == ""
    assert arg.GetCategory() == "Base"
    assert not arg.IsPositional()
    assert not arg.IsRequired()
    assert arg.GetMinCount() == 0
    assert arg.GetMaxCount() == 1
    assert arg.GetPackedValuesAllowed()
    assert arg.GetRepeatedArgAllowed()
    assert arg.GetChoices() is None
    assert not arg.IsExplicitlySet()
    assert arg.HasDefaultValue()
    assert arg.GetDefaultAsBoolean() is False
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsInteger()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsDouble()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsString()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsIntegerList()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsDoubleList()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetDefaultAsStringList()
    assert not arg.IsHiddenForCLI()
    assert not arg.IsOnlyForCLI()
    assert arg.IsInput()
    assert not arg.IsOutput()
    assert arg.GetMutualExclusionGroup() == "overwrite-append"
    assert arg.GetAsBoolean() is False
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsInteger()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsDouble()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsString()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsDatasetValue()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsIntegerList()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsDoubleList()
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsStringList()
    assert arg.Get() is False
    assert (
        arg.GetDatasetType()
        == gdal.OF_RASTER | gdal.OF_VECTOR | gdal.OF_MULTIDIM_RASTER
    )
    assert arg.GetDatasetInputFlags() == gdal.GADV_NAME | gdal.GADV_OBJECT
    assert arg.GetDatasetOutputFlags() == gdal.GADV_OBJECT

    arg = convert.GetArg("input")
    with pytest.raises(Exception, match="must only be called on arguments of type"):
        arg.GetAsBoolean()


###############################################################################
# Test running an algorithm without using the ParseCommandLineArguments()
# interface, but directly setting argument values.


def test_algorithm_dataset_value(tmp_path):

    reg = gdal.GetGlobalAlgorithmRegistry()
    raster = reg.InstantiateAlg("raster")
    convert = raster.InstantiateSubAlgorithm("convert")

    input_arg = convert.GetArg("input")
    input_arg_value = input_arg.Get()
    input_arg_value.SetName("data/byte.tif")
    assert input_arg_value.GetName() == "data/byte.tif"
    assert input_arg_value.GetDataset() is None

    output_arg = convert.GetArg("output")
    outfilename = str(tmp_path / "out.tif")
    output_arg_value = output_arg.Get()
    output_arg_value.SetName(outfilename)

    assert convert.Run()

    in_ds = input_arg_value.GetDataset()
    assert in_ds is not None

    out_ds = output_arg_value.GetDataset()
    assert out_ds is not None
    assert out_ds.GetRasterBand(1).Checksum() == 4672

    output_arg_value.SetDataset(None)
    with pytest.raises(
        Exception,
        match="Dataset object 'output' is created by algorithm and cannot be set as an input",
    ):
        output_arg.SetDataset(None)


###############################################################################
# Test Set() on an argument of type Bool


def test_algorithm_arg_set_bool():
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["info"]

    alg["no-mask"] = True
    assert alg["no-mask"] == True

    alg["no-mask"] = 1
    assert alg["no-mask"] == True

    alg["no-mask"] = 0
    assert alg["no-mask"] == False

    alg["no-mask"] = "True"
    assert alg["no-mask"] == True


###############################################################################
# Test Set() on an argument of type Int


def test_algorithm_arg_set_int():
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["info"]

    alg["subdataset"] = 1
    assert alg["subdataset"] == 1

    alg["subdataset"] = 1.0
    assert alg["subdataset"] == 1

    alg["subdataset"] = "1"
    assert alg["subdataset"] == 1

    alg["subdataset"] = [1]
    assert alg["subdataset"] == 1

    alg["subdataset"] = [1.0]
    assert alg["subdataset"] == 1

    alg["subdataset"] = ["1"]
    assert alg["subdataset"] == 1

    with pytest.raises(TypeError):
        alg["subdataset"] = None

    with pytest.raises(TypeError):
        alg["subdataset"] = 1.5

    with pytest.raises(TypeError):
        alg["subdataset"] = []

    with pytest.raises(TypeError):
        alg["subdataset"] = [None]

    with pytest.raises(TypeError):
        alg["subdataset"] = [1.5]

    with pytest.raises(
        RuntimeError, match="Only one value supported for an argument of type Integer"
    ):
        alg["subdataset"] = [1, 2]


###############################################################################
# Test Set() on an argument of type Real


def test_algorithm_arg_set_real():
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["vector"]["geom"]["simplify"]

    alg["tolerance"] = 1
    assert alg["tolerance"] == 1

    alg["tolerance"] = 1.5
    assert alg["tolerance"] == 1.5

    alg["tolerance"] = "1.5"
    assert alg["tolerance"] == 1.5

    alg["tolerance"] = [1]
    assert alg["tolerance"] == 1

    alg["tolerance"] = [1.5]
    assert alg["tolerance"] == 1.5

    alg["tolerance"] = ["1.5"]
    assert alg["tolerance"] == 1.5

    with pytest.raises(TypeError):
        alg["tolerance"] = None

    with pytest.raises(TypeError):
        alg["tolerance"] = []

    with pytest.raises(TypeError):
        alg["tolerance"] = [None]

    with pytest.raises(
        RuntimeError, match="Only one value supported for an argument of type Real"
    ):
        alg["tolerance"] = [1, 2]


###############################################################################
# Test Set() on an argument of type String


def test_algorithm_arg_set_string(tmp_path):
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["info"]

    alg["mdd"] = "foo"
    assert alg["mdd"] == "foo"

    alg["mdd"] = 5
    assert alg["mdd"] == "5"

    alg["mdd"] = 5.5
    assert alg["mdd"] == "5.5"

    alg["mdd"] = tmp_path
    assert alg["mdd"] == str(tmp_path)

    alg["mdd"] = ["foo"]
    assert alg["mdd"] == "foo"

    alg["mdd"] = [5]
    assert alg["mdd"] == "5"

    alg["mdd"] = [5.5]
    assert alg["mdd"] == "5.5"

    alg["mdd"] = [tmp_path]
    assert alg["mdd"] == str(tmp_path)

    with pytest.raises(TypeError):
        alg["mdd"] = None

    with pytest.raises(TypeError):
        alg["mdd"] = []

    with pytest.raises(TypeError):
        alg["mdd"] = [None]

    with pytest.raises(
        RuntimeError, match="Only one value supported for an argument of type String"
    ):
        alg["mdd"] = ["foo", "bar"]


###############################################################################
# Test Set() on an argument of type IntegerList


def test_algorithm_arg_set_int_list():
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["overview"]["add"]

    alg["levels"] = 2
    assert alg["levels"] == [2]

    alg["levels"] = "2"
    assert alg["levels"] == [2]

    alg["levels"] = 2.0
    assert alg["levels"] == [2]

    alg["levels"] = [2, 4]
    assert alg["levels"] == [2, 4]

    alg["levels"] = ["2", "4"]
    assert alg["levels"] == [2, 4]

    alg["levels"] = [2.0, 4.0]
    assert alg["levels"] == [2, 4]

    with pytest.raises(TypeError):
        alg["levels"] = None

    with pytest.raises(TypeError):
        alg["levels"] = [None]

    with pytest.raises(TypeError):
        alg["levels"] = 2.5


###############################################################################
# Test Set() on an argument of type DoubleList


def test_algorithm_arg_set_double_list():
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["footprint"]

    alg["src-nodata"] = 2
    assert alg["src-nodata"] == [2]

    alg["src-nodata"] = "2"
    assert alg["src-nodata"] == [2]

    alg["src-nodata"] = 2.5
    assert alg["src-nodata"] == [2.5]

    alg["src-nodata"] = [2, 4]
    assert alg["src-nodata"] == [2, 4]

    alg["src-nodata"] = ["2", "4"]
    assert alg["src-nodata"] == [2, 4]

    alg["src-nodata"] = [2.5, 4.5]
    assert alg["src-nodata"] == [2.5, 4.5]

    with pytest.raises(TypeError):
        alg["src-nodata"] = None

    with pytest.raises(TypeError):
        alg["src-nodata"] = [None]


###############################################################################
# Test Set() on an argument of type Dataset


def test_algorithm_arg_set_dataset(tmp_path):
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["convert"]

    alg["input"] = tmp_path
    alg["input"] = "foo"
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["input"] = [tmp_path]
    alg["input"] = ["foo"]
    alg["input"] = [gdal.GetDriverByName("MEM").Create("", 1, 1)]

    with pytest.raises(TypeError):
        alg["input"] = None
    with pytest.raises(TypeError):
        alg["input"] = []
    with pytest.raises(TypeError):
        alg["input"] = [None]
    with pytest.raises(
        RuntimeError, match="Only one value supported for an argument of type Dataset"
    ):
        alg["input"] = ["foo", "bar"]


###############################################################################
# Test Set() on an argument of type DatasetList


def test_algorithm_arg_set_dataset_list(tmp_path):
    reg = gdal.GetGlobalAlgorithmRegistry()
    alg = reg["raster"]["mosaic"]

    alg["input"] = tmp_path
    alg["input"] = "foo"
    alg["input"] = gdal.GetDriverByName("MEM").Create("", 1, 1)
    alg["input"] = [tmp_path]
    alg["input"] = ["foo"]
    alg["input"] = [gdal.GetDriverByName("MEM").Create("", 1, 1)]
    alg["input"] = []
    alg["input"] = ["foo", "bar"]

    with pytest.raises(TypeError):
        alg["input"] = None
    with pytest.raises(TypeError):
        alg["input"] = [None]
