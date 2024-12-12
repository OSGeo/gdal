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
