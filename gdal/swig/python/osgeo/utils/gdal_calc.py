#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Command line raster calculator with numpy syntax
#  Author:   Chris Yesson, chris.yesson@ioz.ac.uk
#
# ******************************************************************************
#  Copyright (c) 2010, Chris Yesson <chris.yesson@ioz.ac.uk>
#  Copyright (c) 2010-2011, Even Rouault <even dot rouault at spatialys.com>
#  Copyright (c) 2016, Piers Titus van der Torren <pierstitus@gmail.com>
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

################################################################
# Command line raster calculator with numpy syntax. Use any basic arithmetic supported by numpy arrays such as +-*\ along with logical operators such as >.  Note that all files must have the same dimensions, but no projection checking is performed.  Use gdal_calc.py --help for list of options.

# example 1 - add two files together
# gdal_calc.py -A input1.tif -B input2.tif --outfile=result.tif --calc="A+B"

# example 2 - average of two layers
# gdal_calc.py -A input.tif -B input2.tif --outfile=result.tif --calc="(A+B)/2"

# example 3 - set values of zero and below to null
# gdal_calc.py -A input.tif --outfile=result.tif --calc="A*(A>0)" --NoDataValue=0

# example 4 - using logical operator to keep a range of values from input:
# gdal_calc.py -A input.tif --outfile=result.tif --calc="A*logical_and(A>100,A<150)"

# example 5 - work with multiple bands:
# gdal_calc.py -A input.tif --A_band=1 -B input.tif --B_band=2 --outfile=result.tif --calc="(A+B)/2" --calc="A*logical_and(A>100,A<150)"
################################################################

from optparse import OptionParser, OptionConflictError, Values
import os
import os.path
import sys
import shlex
import string
import numbers
from collections import defaultdict
from osgeo.auxiliary.base import is_path_like

import numpy

from osgeo import gdal
from osgeo import gdalnumeric
from osgeo.auxiliary.base import GetOutputDriverFor

# create alphabetic list (lowercase + uppercase) for storing input layers
AlphaList = list(string.ascii_letters)

# set up some default nodatavalues for each datatype
DefaultNDVLookup = {gdal.GDT_Byte: 255, gdal.GDT_UInt16: 65535, gdal.GDT_Int16: -32768,
                    gdal.GDT_UInt32: 4294967293, gdal.GDT_Int32: -2147483647,
                    gdal.GDT_Float32: 3.402823466E+38, gdal.GDT_Float64: 1.7976931348623158E+308}

# tuple of available output datatypes names
GDALDataTypeNames = tuple(gdal.GetDataTypeName(dt) for dt in DefaultNDVLookup.keys())

gdal_dt_to_np_dt = {gdal.GDT_Byte: numpy.uint8,
                    gdal.GDT_UInt16: numpy.uint16,
                    gdal.GDT_Int16: numpy.int16,
                    gdal.GDT_UInt32: numpy.uint32,
                    gdal.GDT_Int32: numpy.int32,
                    gdal.GDT_Float32: numpy.float32,
                    gdal.GDT_Float64: numpy.float64}

EXTENT = int
EXTENT_IGNORE = 0
EXTENT_FAIL = 1
EXTENT_UNION = 2
EXTENT_INTERSECT = 3


def parse_extent(extent):
    if isinstance(extent, str):
        extent = extent.lower()
        if extent == 'ignore':
            return EXTENT_IGNORE
        elif extent == 'fail':
            return EXTENT_FAIL
        elif extent == 'union':
            return EXTENT_UNION
        elif extent == 'intersect':
            return EXTENT_INTERSECT
    elif isinstance(extent, EXTENT):
        return extent
    raise Exception('Error: Unknown extent %s' % extent)


GT_SAME = -1
GT_ALMOST_SAME = -2
GT_COMPATIBLE_DIFF = -3
GT_INCOMPATIBLE_OFFSET = 0
GT_INCOMPATIBLE_PIXEL_SIZE = 1
GT_INCOMPATIBLE_ROTATION = 2
GT_NON_ZERO_ROTATION = 3


def gt_diff(gt0, gt1, diff_support, eps=0.0):
    if gt0 == gt1:
        return GT_SAME
    if isinstance(eps, numbers.Number):
        eps = {GT_INCOMPATIBLE_OFFSET: eps, GT_INCOMPATIBLE_PIXEL_SIZE: eps, GT_INCOMPATIBLE_ROTATION: eps}
    same = {
        GT_INCOMPATIBLE_OFFSET:     eps[GT_INCOMPATIBLE_OFFSET]     >= (abs(gt0[0] - gt1[0]) + abs(gt0[3] - gt1[3])),
        GT_INCOMPATIBLE_PIXEL_SIZE: eps[GT_INCOMPATIBLE_PIXEL_SIZE] >= (abs(gt0[1] - gt1[1]) + abs(gt0[5] - gt1[5])),
        GT_INCOMPATIBLE_ROTATION:   eps[GT_INCOMPATIBLE_ROTATION]   >= (abs(gt0[2] - gt1[2]) + abs(gt0[4] - gt1[4])),
        GT_NON_ZERO_ROTATION:       gt0[2] == gt0[4] == gt1[2] == gt1[4] == 0}
    if same[GT_INCOMPATIBLE_OFFSET] and same[GT_INCOMPATIBLE_PIXEL_SIZE] and same[GT_INCOMPATIBLE_ROTATION]:
        return GT_ALMOST_SAME
    for reason in same.keys():
        if not same[reason] or diff_support[reason]:
            return reason  # incompatible gt, returns the reason
    return GT_COMPATIBLE_DIFF


def doit(opts, args):
    # pylint: disable=unused-argument

    if opts.debug:
        print("gdal_calc.py starting calculation %s" % (opts.calc))

    # set up global namespace for eval with all functions of gdalnumeric
    global_namespace = dict([(key, getattr(gdalnumeric, key))
                             for key in dir(gdalnumeric) if not key.startswith('__')])
    if opts.user_namespace:
        global_namespace.update(opts.user_namespace)

    if not opts.calc:
        raise Exception("No calculation provided.")
    elif not opts.outF and opts.format.upper() != 'MEM':
        raise Exception("No output file provided.")

    if opts.format is None:
        opts.format = GetOutputDriverFor(opts.outF)

    if not hasattr(opts, "color_table"):
        opts.color_table = None
    if not opts.extent:
        opts.extent = EXTENT_IGNORE
    else:
        opts.extent = parse_extent(opts.extent)

    compatible_gt_eps = 0.000001
    gt_diff_support = {
        GT_INCOMPATIBLE_OFFSET: opts.extent != EXTENT_FAIL,
        GT_INCOMPATIBLE_PIXEL_SIZE: False,
        GT_INCOMPATIBLE_ROTATION: False,
        GT_NON_ZERO_ROTATION: False,
    }
    gt_diff_error = {
        GT_INCOMPATIBLE_OFFSET: 'different offset',
        GT_INCOMPATIBLE_PIXEL_SIZE: 'different pixel size',
        GT_INCOMPATIBLE_ROTATION: 'different rotation',
        GT_NON_ZERO_ROTATION: 'non zero rotation',
    }

    ################################################################
    # fetch details of input layers
    ################################################################

    # set up some lists to store data for each band
    myFileNames = []  # input filenames
    myFiles = []  # input DataSets
    myBands = []  # input bands
    myAlphaList = []  # input alpha letter that represents each input file
    myDataType = []  # string representation of the datatype of each input file
    myDataTypeNum = []  # datatype of each input file
    myNDV = []  # nodatavalue for each input file
    DimensionsCheck = None  # dimensions of the output
    Dimensions = []  # Dimensions of input files
    ProjectionCheck = None  # projection of the output
    GeoTransformCheck = None  # GeoTransform of the output
    GeoTransforms = []  # GeoTransform of each input file
    GeoTransformDiffer = False  # True if we have inputs with different GeoTransforms
    myTempFileNames = []  # vrt filename from each input file
    myAlphaFileLists = []  # list of the Alphas which holds a list of inputs

    # loop through input files - checking dimensions
    for alphas, filenames in opts.input_files.items():
        if isinstance(filenames, (list, tuple)):
            # alpha is a list of files
            myAlphaFileLists.append(alphas)
        elif is_path_like(filenames) or isinstance(filenames, gdal.Dataset):
            # alpha is a single filename or a Dataset
            filenames = [filenames]
            alphas = [alphas]
        else:
            # I guess this alphas should be in the global_namespace,
            # It would have been better to pass it as user_namepsace, but I'll accept it anyway
            global_namespace[alphas] = filenames
            continue
        for alpha, filename in zip(alphas*len(filenames), filenames):
            if not alpha.endswith("_band"):
                # check if we have asked for a specific band...
                if "%s_band" % alpha in opts.input_files:
                    myBand = opts.input_files["%s_band" % alpha]
                else:
                    myBand = 1

                myF_is_ds = not is_path_like(filename)
                if myF_is_ds:
                    myFile = filename
                    filename = None
                else:
                    filename = str(filename)
                    myFile = gdal.Open(filename, gdal.GA_ReadOnly)
                if not myFile:
                    raise IOError("No such file or directory: '%s'" % filename)

                myFileNames.append(filename)
                myFiles.append(myFile)
                myBands.append(myBand)
                myAlphaList.append(alpha)
                dt = myFile.GetRasterBand(myBand).DataType
                myDataType.append(gdal.GetDataTypeName(dt))
                myDataTypeNum.append(dt)
                myNDV.append(None if opts.hideNoData else myFile.GetRasterBand(myBand).GetNoDataValue())

                # check that the dimensions of each layer are the same
                myFileDimensions = [myFile.RasterXSize, myFile.RasterYSize]
                if DimensionsCheck:
                    if DimensionsCheck != myFileDimensions:
                        GeoTransformDiffer = True
                        if opts.extent in [EXTENT_IGNORE, EXTENT_FAIL]:
                            raise Exception("Error! Dimensions of file %s (%i, %i) are different from other files (%i, %i).  Cannot proceed" %
                                            (filename, myFileDimensions[0], myFileDimensions[1], DimensionsCheck[0], DimensionsCheck[1]))
                else:
                    DimensionsCheck = myFileDimensions

                # check that the Projection of each layer are the same
                myProjection = myFile.GetProjection()
                if ProjectionCheck:
                    if opts.projectionCheck and ProjectionCheck != myProjection:
                        raise Exception(
                            "Error! Projection of file %s %s are different from other files %s.  Cannot proceed" %
                            (filename, myProjection, ProjectionCheck))
                else:
                    ProjectionCheck = myProjection

                # check that the GeoTransforms of each layer are the same
                myFileGeoTransform = myFile.GetGeoTransform(can_return_null=True)
                if opts.extent == EXTENT_IGNORE:
                    GeoTransformCheck = myFileGeoTransform
                else:
                    Dimensions.append(myFileDimensions)
                    GeoTransforms.append(myFileGeoTransform)
                    if not GeoTransformCheck:
                        GeoTransformCheck = myFileGeoTransform
                    else:
                        my_gt_diff = gt_diff(GeoTransformCheck, myFileGeoTransform, eps=compatible_gt_eps, diff_support=gt_diff_support)
                        if my_gt_diff not in [GT_SAME, GT_ALMOST_SAME]:
                            GeoTransformDiffer = True
                            if my_gt_diff not in [GT_COMPATIBLE_DIFF]:
                                raise Exception(
                                    "Error! GeoTransform of file {} {} is incompatible ({}), first file GeoTransform is {}. Cannot proceed".
                                        format(filename, myFileGeoTransform, gt_diff_error[my_gt_diff], GeoTransformCheck))
                if opts.debug:
                    print("file %s: %s, dimensions: %s, %s, type: %s" % (
                        alpha, filename, DimensionsCheck[0], DimensionsCheck[1], myDataType[-1]))

    # process allBands option
    allBandsIndex = None
    allBandsCount = 1
    if opts.allBands:
        if len(opts.calc) > 1:
            raise Exception("Error! --allBands implies a single --calc")
        try:
            allBandsIndex = myAlphaList.index(opts.allBands)
        except ValueError:
            raise Exception("Error! allBands option was given but Band %s not found.  Cannot proceed" % (opts.allBands))
        allBandsCount = myFiles[allBandsIndex].RasterCount
        if allBandsCount <= 1:
            allBandsIndex = None
    else:
        allBandsCount = len(opts.calc)

    if opts.extent not in [EXTENT_IGNORE, EXTENT_FAIL] and (GeoTransformDiffer or not isinstance(opts.extent, EXTENT)):
        raise Exception('Error! mixing different GeoTransforms/Extents is not supported yet.')

    ################################################################
    # set up output file
    ################################################################

    # open output file exists
    if opts.outF and os.path.isfile(opts.outF) and not opts.overwrite:
        if allBandsIndex is not None:
            raise Exception("Error! allBands option was given but Output file exists, must use --overwrite option!")
        if len(opts.calc) > 1:
            raise Exception("Error! multiple calc options were given but Output file exists, must use --overwrite option!")
        if opts.debug:
            print("Output file %s exists - filling in results into file" % (opts.outF))

        myOut = gdal.Open(opts.outF, gdal.GA_Update)
        if myOut is None:
            error = 'but cannot be opened for update'
        elif [myOut.RasterXSize, myOut.RasterYSize] != DimensionsCheck:
            error = 'but is the wrong size'
        elif ProjectionCheck and ProjectionCheck != myOut.GetProjection():
            error = 'but is the wrong projection'
        elif GeoTransformCheck and GeoTransformCheck != myOut.GetGeoTransform(can_return_null=True):
            error = 'but is the wrong geotransform'
        else:
            error = None
        if error:
            raise Exception("Error! Output exists, %s.  Use the --overwrite option to automatically overwrite the existing file" % error)

        myOutB = myOut.GetRasterBand(1)
        myOutNDV = myOutB.GetNoDataValue()
        myOutType = myOutB.DataType

    else:
        if opts.outF:
            # remove existing file and regenerate
            if os.path.isfile(opts.outF):
                os.remove(opts.outF)
            # create a new file
            if opts.debug:
                print("Generating output file %s" % (opts.outF))
        else:
            opts.outF = ''

        # find data type to use
        if not opts.type:
            # use the largest type of the input files
            myOutType = max(myDataTypeNum)
        else:
            myOutType = opts.type
            if isinstance(myOutType, str):
                myOutType = gdal.GetDataTypeByName(myOutType)

        # create file
        myOutDrv = gdal.GetDriverByName(opts.format)
        myOut = myOutDrv.Create(
            opts.outF, DimensionsCheck[0], DimensionsCheck[1], allBandsCount,
            myOutType, opts.creation_options)

        # set output geo info based on first input layer
        if not GeoTransformCheck:
            GeoTransformCheck = myFiles[0].GetGeoTransform(can_return_null=True)
        if GeoTransformCheck:
            myOut.SetGeoTransform(GeoTransformCheck)

        if not ProjectionCheck:
            ProjectionCheck = myFiles[0].GetProjection()
        if ProjectionCheck:
            myOut.SetProjection(ProjectionCheck)

        if opts.NoDataValue is None:
            myOutNDV = None if opts.hideNoData else DefaultNDVLookup[myOutType]  # use the default noDataValue for this datatype
        elif isinstance(opts.NoDataValue, str) and opts.NoDataValue.lower() == 'none':
            myOutNDV = None  # not to set any noDataValue
        else:
            myOutNDV = opts.NoDataValue  # use the given noDataValue

        for i in range(1, allBandsCount + 1):
            myOutB = myOut.GetRasterBand(i)
            if myOutNDV is not None:
                myOutB.SetNoDataValue(myOutNDV)
            if opts.color_table:
                # set color table and color interpretation
                if is_path_like(opts.color_table):
                    raise Exception('Error! reading a color table from a file is not supported yet')
                myOutB.SetRasterColorTable(opts.color_table)
                myOutB.SetRasterColorInterpretation(gdal.GCI_PaletteIndex)

            myOutB = None  # write to band

    myOutTypeName = gdal.GetDataTypeName(myOutType)
    if opts.debug:
        print("output file: %s, dimensions: %s, %s, type: %s" % (opts.outF, myOut.RasterXSize, myOut.RasterYSize, myOutTypeName))

    ################################################################
    # find block size to chop grids into bite-sized chunks
    ################################################################

    # use the block size of the first layer to read efficiently
    myBlockSize = myFiles[0].GetRasterBand(myBands[0]).GetBlockSize()
    # find total x and y blocks to be read
    nXBlocks = (int)((DimensionsCheck[0] + myBlockSize[0] - 1) / myBlockSize[0])
    nYBlocks = (int)((DimensionsCheck[1] + myBlockSize[1] - 1) / myBlockSize[1])
    myBufSize = myBlockSize[0] * myBlockSize[1]

    if opts.debug:
        print("using blocksize %s x %s" % (myBlockSize[0], myBlockSize[1]))

    # variables for displaying progress
    ProgressCt = -1
    ProgressMk = -1
    ProgressEnd = nXBlocks * nYBlocks * allBandsCount

    ################################################################
    # start looping through each band in allBandsCount
    ################################################################

    for bandNo in range(1, allBandsCount + 1):

        ################################################################
        # start looping through blocks of data
        ################################################################

        # store these numbers in variables that may change later
        nXValid = myBlockSize[0]
        nYValid = myBlockSize[1]

        # loop through X-lines
        for X in range(0, nXBlocks):

            # in case the blocks don't fit perfectly
            # change the block size of the final piece
            if X == nXBlocks - 1:
                nXValid = DimensionsCheck[0] - X * myBlockSize[0]

            # find X offset
            myX = X * myBlockSize[0]

            # reset buffer size for start of Y loop
            nYValid = myBlockSize[1]
            myBufSize = nXValid * nYValid

            # loop through Y lines
            for Y in range(0, nYBlocks):
                ProgressCt += 1
                if 10 * ProgressCt / ProgressEnd % 10 != ProgressMk and not opts.quiet:
                    ProgressMk = 10 * ProgressCt / ProgressEnd % 10
                    from sys import version_info
                    if version_info >= (3, 0, 0):
                        exec('print("%d.." % (10*ProgressMk), end=" ")')
                    else:
                        exec('print 10*ProgressMk, "..",')

                # change the block size of the final piece
                if Y == nYBlocks - 1:
                    nYValid = DimensionsCheck[1] - Y * myBlockSize[1]
                    myBufSize = nXValid * nYValid

                # find Y offset
                myY = Y * myBlockSize[1]

                # create empty buffer to mark where nodata occurs
                myNDVs = None

                # make local namespace for calculation
                local_namespace = {}

                val_lists = defaultdict(list)

                # fetch data for each input layer
                for i, Alpha in enumerate(myAlphaList):

                    # populate lettered arrays with values
                    if allBandsIndex is not None and allBandsIndex == i:
                        myBandNo = bandNo
                    else:
                        myBandNo = myBands[i]
                    myval = gdalnumeric.BandReadAsArray(myFiles[i].GetRasterBand(myBandNo),
                                                        xoff=myX, yoff=myY,
                                                        win_xsize=nXValid, win_ysize=nYValid)
                    if myval is None:
                        raise Exception('Input block reading failed from filename %s' % filename[i])

                    # fill in nodata values
                    if myNDV[i] is not None:
                        # myNDVs is a boolean buffer.
                        # a cell equals to 1 if there is NDV in any of the corresponding cells in input raster bands.
                        if myNDVs is None:
                            # this is the first band that has NDV set. we initializes myNDVs to a zero buffer
                            # as we didn't see any NDV value yet.
                            myNDVs = numpy.zeros(myBufSize)
                            myNDVs.shape = (nYValid, nXValid)
                        myNDVs = 1 * numpy.logical_or(myNDVs == 1, myval == myNDV[i])

                    # add an array of values for this block to the eval namespace
                    if Alpha in myAlphaFileLists:
                        val_lists[Alpha].append(myval)
                    else:
                        local_namespace[Alpha] = myval
                    myval = None

                for lst in myAlphaFileLists:
                    local_namespace[lst] = val_lists[lst]

                # try the calculation on the array blocks
                calc = opts.calc[bandNo-1 if len(opts.calc) > 1 else 0]
                try:
                    myResult = eval(calc, global_namespace, local_namespace)
                except:
                    print("evaluation of calculation %s failed" % (calc))
                    raise

                # Propagate nodata values (set nodata cells to zero
                # then add nodata value to these cells).
                if myNDVs is not None and myOutNDV is not None:
                    myResult = ((1 * (myNDVs == 0)) * myResult) + (myOutNDV * myNDVs)
                elif not isinstance(myResult, numpy.ndarray):
                    myResult = numpy.ones((nYValid, nXValid)) * myResult

                # write data block to the output file
                myOutB = myOut.GetRasterBand(bandNo)
                if gdalnumeric.BandWriteArray(myOutB, myResult, xoff=myX, yoff=myY) != 0:
                    raise Exception('Block writing failed')
                myOutB = None  # write to band

    # remove temp files
    for idx, tempFile in enumerate(myTempFileNames):
        myFiles[idx] = None
        os.remove(tempFile)

    gdal.ErrorReset()
    myOut.FlushCache()
    if gdal.GetLastErrorMsg() != '':
        raise Exception('Dataset writing failed')

    if not opts.quiet:
        print("100 - Done")

    return myOut

################################################################


def Calc(calc, outfile=None, NoDataValue=None, type=None, format=None, creation_options=None, allBands='', overwrite=False,
         hideNoData=False, projectionCheck=False, color_table=None, extent=None, user_namespace=None,
         debug=False, quiet=False, **input_files):

    """ Perform raster calculations with numpy syntax.
    Use any basic arithmetic supported by numpy arrays such as +-* along with logical
    operators such as >. Note that all files must have the same dimensions, but no projection checking is performed.

    Keyword arguments:
        [A-Z]: input files
        [A_band - Z_band]: band to use for respective input file

    Examples:
    add two files together:
        Calc("A+B", A="input1.tif", B="input2.tif", outfile="result.tif")

    average of two layers:
        Calc(calc="(A+B)/2", A="input1.tif", B="input2.tif", outfile="result.tif")

    set values of zero and below to null:
        Calc(calc="A*(A>0)", A="input.tif", A_band=2, outfile="result.tif", NoDataValue=0)

    work with two bands:
        Calc(["(A+B)/2", "A*(A>0)"], A="input.tif", A_band=1, B="input.tif", B_band=2, outfile="result.tif", NoDataValue=0)

    sum all files with hidden noDataValue
        Calc(calc="sum(a,axis=0)", a=['0.tif','1.tif','2.tif'], outfile="sum.tif", hideNoData=True)
    """
    opts = Values()
    opts.input_files = input_files
    # Single calc value compatibility
    # (type is overridden in the parameter list)
    if isinstance(calc, list):
        opts.calc = calc
    else:
        opts.calc = [calc]
    opts.outF = outfile
    opts.NoDataValue = NoDataValue
    opts.type = type
    opts.format = format
    opts.creation_options = [] if creation_options is None else creation_options
    opts.allBands = allBands
    opts.overwrite = overwrite
    opts.hideNoData = hideNoData
    opts.projectionCheck = projectionCheck
    opts.color_table = color_table
    opts.extent = extent
    opts.user_namespace = user_namespace
    opts.debug = debug
    opts.quiet = quiet

    return doit(opts, None)


def store_input_file(option, opt_str, value, parser):
    # pylint: disable=unused-argument
    if not hasattr(parser.values, 'input_files'):
        parser.values.input_files = {}
    key = opt_str.lstrip('-')
    if key in parser.values.input_files:
        value_list = parser.values.input_files[key]
        if not isinstance(value_list, list):
            value_list = [value_list]
        value_list.append(value)
        value = value_list
    parser.values.input_files[key] = value


def add_alpha_args(parser, argv):
    # limit the input file options to the ones in the argument list
    given_args = set([a[1] for a in argv if a[1:2] in AlphaList] + ['A'])
    for myAlpha in given_args:
        try:
            parser.add_option("-%s" % myAlpha, action="callback", callback=store_input_file, type=str, help="input gdal raster file, you can use any letter (A-Z)", metavar='filename')
            parser.add_option("--%s_band" % myAlpha, action="callback", callback=store_input_file, type=int, help="number of raster band for file %s (default 1)" % myAlpha, metavar='n')
        except OptionConflictError:
            pass


def main(argv):
    usage = """usage: %prog --calc=expression --outfile=out_filename [-A filename]
                    [--A_band=n] [-B...-Z filename] [other_options]"""
    parser = OptionParser(usage)

    # define options
    parser.add_option("--calc", dest="calc", action="append", help="calculation in gdalnumeric syntax using +-/* or any numpy array functions (i.e. log10()). May appear multiple times to produce a multi-band file", metavar="expression")
    add_alpha_args(parser, argv)

    parser.add_option("--outfile", dest="outF", help="output file to generate or fill", metavar="filename")
    parser.add_option("--NoDataValue", dest="NoDataValue", type=float, help="output nodata value (default datatype specific value)", metavar="value")
    parser.add_option("--hideNoData", dest="hideNoData", action="store_true", help="ignores the NoDataValues of the input rasters")
    parser.add_option("--type", dest="type", help="output datatype", choices=GDALDataTypeNames, metavar="datatype")
    parser.add_option("--format", dest="format", help="GDAL format for output file", metavar="gdal_format")
    parser.add_option(
        "--creation-option", "--co", dest="creation_options", default=[], action="append",
        help="Passes a creation option to the output format driver. Multiple "
        "options may be listed. See format specific documentation for legal "
        "creation options for each format.", metavar="option")
    parser.add_option("--allBands", dest="allBands", default="", help="process all bands of given raster (a-z, A-Z)", metavar="[a-z, A-Z]")
    parser.add_option("--overwrite", dest="overwrite", action="store_true", help="overwrite output file if it already exists")
    parser.add_option("--debug", dest="debug", action="store_true", help="print debugging information")
    parser.add_option("--quiet", dest="quiet", action="store_true", help="suppress progress messages")
    parser.add_option("--optfile", dest="optfile", metavar="optfile", help="Read the named file and substitute the contents into the command line options list.")

    # parser.add_option("--color_table", dest="color_table", help="color table file name")
    parser.add_option("--extent", dest="extent",
                      choices=['ignore', 'fail'],
                      help="how to treat mixed geotrasnforms")
    parser.add_option("--projectionCheck", dest="projectionCheck", action="store_true",
                      help="check that all rasters share the same projection")

    (opts, args) = parser.parse_args(argv[1:])

    if not hasattr(opts, "input_files"):
        opts.input_files = {}
    if not hasattr(opts, "user_namespace"):
        opts.user_namespace = {}

    if opts.optfile:
        with open(opts.optfile, 'r') as f:
            ofargv = [x for line in f for x in shlex.split(line, comments=True)]
        # Avoid potential recursion.
        parser.remove_option('--optfile')
        add_alpha_args(parser, ofargv)
        ofopts, ofargs = parser.parse_args(ofargv)

        # Let options given directly override the optfile.
        input_files = getattr(ofopts, 'input_files', {})
        input_files.update(opts.input_files)
        user_namespace = getattr(ofopts, 'user_namespace', {})
        user_namespace.update(opts.user_namespace)
        ofopts.__dict__.update({k: v for k, v in vars(opts).items() if v})
        opts = ofopts
        opts.input_files = input_files
        opts.user_namespace = user_namespace
        args = args + ofargs

    if len(argv) == 1:
        parser.print_help()
        return 1
    elif not opts.calc:
        print("No calculation provided. Nothing to do!")
        parser.print_help()
        return 1
    elif not opts.outF:
        print("No output file provided. Cannot proceed.")
        parser.print_help()
        return 1
    else:
        try:
            doit(opts, args)
        except IOError as e:
            print(e)
            return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
