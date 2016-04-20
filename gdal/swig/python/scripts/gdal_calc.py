#!/usr/bin/env python
# -*- coding: utf-8 -*-
#******************************************************************************
#
#  Project:  GDAL
#  Purpose:  Command line raster calculator with numpy syntax
#  Author:   Chris Yesson, chris.yesson@ioz.ac.uk
#
#******************************************************************************
#  Copyright (c) 2010, Chris Yesson <chris.yesson@ioz.ac.uk>
#  Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
#******************************************************************************

################################################################
# Command line raster calculator with numpy syntax. Use any basic arithmetic supported by numpy arrays such as +-*\ along with logical operators such as >.  Note that all files must have the same dimensions, but no projection checking is performed.  Use gdal_calc.py --help for list of options.

# example 1 - add two files together
# gdal_calc.py -A input1.tif -B input2.tif --outfile=result.tif --calc="A+B"

# example 2 - average of two layers
# gdal_calc.py -A input.tif -B input2.tif --outfile=result.tif --calc="(A+B)/2"

# example 3 - set values of zero and below to null
# gdal_calc.py -A input.tif --outfile=result.tif --calc="A*(A>0)" --NoDataValue=0
################################################################

from optparse import OptionParser
import os
import sys

import numpy

from osgeo import gdal
from osgeo import gdalnumeric


# create alphabetic list for storing input layers
AlphaList=["A","B","C","D","E","F","G","H","I","J","K","L","M",
           "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"]

# set up some default nodatavalues for each datatype
DefaultNDVLookup={'Byte':255, 'UInt16':65535, 'Int16':-32767, 'UInt32':4294967293, 'Int32':-2147483647, 'Float32':1.175494351E-38, 'Float64':1.7976931348623158E+308}

#3.402823466E+38

################################################################
def doit(opts, args):

    if opts.debug:
        print("gdal_calc.py starting calculation %s" %(opts.calc))

    # set up global namespace for eval with all functions of gdalnumeric
    global_namespace = dict([(key, getattr(gdalnumeric, key))
        for key in dir(gdalnumeric) if not key.startswith('__')])

    ################################################################
    # fetch details of input layers
    ################################################################

    # set up some lists to store data for each band
    myFiles=[]
    myBands=[]
    myAlphaList=[]
    myDataType=[]
    myDataTypeNum=[]
    myNDV=[]
    DimensionsCheck=None

    # loop through input files - checking dimensions
    for i,myI in enumerate(AlphaList[0:len(sys.argv)-1]):
        myF = eval("opts.%s" %(myI))
        myBand = eval("opts.%s_band" %(myI))
        if myF:
            myFiles.append(gdal.Open(myF, gdal.GA_ReadOnly))
            # check if we have asked for a specific band...
            if myBand:
                myBands.append(myBand)
            else:
                myBands.append(1)
            myAlphaList.append(myI)
            myDataType.append(gdal.GetDataTypeName(myFiles[i].GetRasterBand(myBands[i]).DataType))
            myDataTypeNum.append(myFiles[i].GetRasterBand(myBands[i]).DataType)
            myNDV.append(myFiles[i].GetRasterBand(myBands[i]).GetNoDataValue())
            # check that the dimensions of each layer are the same
            if DimensionsCheck:
                if DimensionsCheck!=[myFiles[i].RasterXSize, myFiles[i].RasterYSize]:
                    print("Error! Dimensions of file %s (%i, %i) are different from other files (%i, %i).  Cannot proceed" % \
                            (myF,myFiles[i].RasterXSize, myFiles[i].RasterYSize,DimensionsCheck[0],DimensionsCheck[1]))
                    return
            else:
                DimensionsCheck=[myFiles[i].RasterXSize, myFiles[i].RasterYSize]

            if opts.debug:
                print("file %s: %s, dimensions: %s, %s, type: %s" %(myI,myF,DimensionsCheck[0],DimensionsCheck[1],myDataType[i]))

    # process allBands option
    allBandsIndex=None
    allBandsCount=1
    if opts.allBands:
        try:
            allBandsIndex=myAlphaList.index(opts.allBands)
        except ValueError:
            print("Error! allBands option was given but Band %s not found.  Cannot proceed" % (opts.allBands))
            return
        allBandsCount=myFiles[allBandsIndex].RasterCount
        if allBandsCount <= 1:
            allBandsIndex=None

    ################################################################
    # set up output file
    ################################################################

    # open output file exists
    if os.path.isfile(opts.outF) and not opts.overwrite:
        if allBandsIndex is not None:
            print("Error! allBands option was given but Output file exists, must use --overwrite option!")
            return
        if opts.debug:
            print("Output file %s exists - filling in results into file" %(opts.outF))
        myOut=gdal.Open(opts.outF, gdal.GA_Update)
        if [myOut.RasterXSize,myOut.RasterYSize] != DimensionsCheck:
            print("Error! Output exists, but is the wrong size.  Use the --overwrite option to automatically overwrite the existing file")
            return
        myOutB=myOut.GetRasterBand(1)
        myOutNDV=myOutB.GetNoDataValue()
        myOutType=gdal.GetDataTypeName(myOutB.DataType)

    else:
        # remove existing file and regenerate
        if os.path.isfile(opts.outF):
            os.remove(opts.outF)
        # create a new file
        if opts.debug:
            print("Generating output file %s" %(opts.outF))

        # find data type to use
        if not opts.type:
            # use the largest type of the input files
            myOutType=gdal.GetDataTypeName(max(myDataTypeNum))
        else:
            myOutType=opts.type

        # create file
        myOutDrv = gdal.GetDriverByName(opts.format)
        myOut = myOutDrv.Create(
            opts.outF, DimensionsCheck[0], DimensionsCheck[1], allBandsCount,
            gdal.GetDataTypeByName(myOutType), opts.creation_options)

        # set output geo info based on first input layer
        myOut.SetGeoTransform(myFiles[0].GetGeoTransform())
        myOut.SetProjection(myFiles[0].GetProjection())

        if opts.NoDataValue!=None:
            myOutNDV=opts.NoDataValue
        else:
            myOutNDV=DefaultNDVLookup[myOutType]

        for i in range(1,allBandsCount+1):
            myOutB=myOut.GetRasterBand(i)
            myOutB.SetNoDataValue(myOutNDV)
            # write to band
            myOutB=None

    if opts.debug:
        print("output file: %s, dimensions: %s, %s, type: %s" %(opts.outF,myOut.RasterXSize,myOut.RasterYSize,myOutType))

    ################################################################
    # find block size to chop grids into bite-sized chunks
    ################################################################

    # use the block size of the first layer to read efficiently
    myBlockSize=myFiles[0].GetRasterBand(myBands[0]).GetBlockSize();
    # store these numbers in variables that may change later
    nXValid = myBlockSize[0]
    nYValid = myBlockSize[1]
    # find total x and y blocks to be read
    nXBlocks = (int)((DimensionsCheck[0] + myBlockSize[0] - 1) / myBlockSize[0]);
    nYBlocks = (int)((DimensionsCheck[1] + myBlockSize[1] - 1) / myBlockSize[1]);
    myBufSize = myBlockSize[0]*myBlockSize[1]

    if opts.debug:
        print("using blocksize %s x %s" %(myBlockSize[0], myBlockSize[1]))

    # variables for displaying progress
    ProgressCt=-1
    ProgressMk=-1
    ProgressEnd=nXBlocks*nYBlocks*allBandsCount

    ################################################################
    # start looping through each band in allBandsCount
    ################################################################

    for bandNo in range(1,allBandsCount+1):

        ################################################################
        # start looping through blocks of data
        ################################################################

        # loop through X-lines
        for X in range(0,nXBlocks):

            # in the rare (impossible?) case that the blocks don't fit perfectly
            # change the block size of the final piece
            if X==nXBlocks-1:
                nXValid = DimensionsCheck[0] - X * myBlockSize[0]
                myBufSize = nXValid*nYValid

            # find X offset
            myX=X*myBlockSize[0]

            # reset buffer size for start of Y loop
            nYValid = myBlockSize[1]
            myBufSize = nXValid*nYValid

            # loop through Y lines
            for Y in range(0,nYBlocks):
                ProgressCt+=1
                if 10*ProgressCt/ProgressEnd%10!=ProgressMk:
                    ProgressMk=10*ProgressCt/ProgressEnd%10
                    from sys import version_info
                    if version_info >= (3,0,0):
                        exec('print("%d.." % (10*ProgressMk), end=" ")')
                    else:
                        exec('print 10*ProgressMk, "..",')

                # change the block size of the final piece
                if Y==nYBlocks-1:
                    nYValid = DimensionsCheck[1] - Y * myBlockSize[1]
                    myBufSize = nXValid*nYValid

                # find Y offset
                myY=Y*myBlockSize[1]

                # create empty buffer to mark where nodata occurs
                myNDVs=numpy.zeros(myBufSize)
                myNDVs.shape=(nYValid,nXValid)

                # modules available to calculation
                local_namespace = {}

                # fetch data for each input layer
                for i,Alpha in enumerate(myAlphaList):

                    # populate lettered arrays with values
                    if allBandsIndex is not None and allBandsIndex==i:
                        myBandNo=bandNo
                    else:
                        myBandNo=myBands[i]
                    myval=gdalnumeric.BandReadAsArray(myFiles[i].GetRasterBand(myBandNo),
                                          xoff=myX, yoff=myY,
                                          win_xsize=nXValid, win_ysize=nYValid)

                    # fill in nodata values
                    myNDVs=1*numpy.logical_or(myNDVs==1, myval==myNDV[i])

                    # add an array of values for this block to the eval namespace
                    local_namespace[Alpha] = myval
                    myval=None


                # try the calculation on the array blocks
                try:
                    myResult = eval(opts.calc, global_namespace, local_namespace)
                except:
                    print("evaluation of calculation %s failed" %(opts.calc))
                    raise

                # Propagate nodata values (set nodata cells to zero
                # then add nodata value to these cells).
                myResult = ((1*(myNDVs==0))*myResult) + (myOutNDV*myNDVs)

                # write data block to the output file
                myOutB=myOut.GetRasterBand(bandNo)
                gdalnumeric.BandWriteArray(myOutB, myResult, xoff=myX, yoff=myY)

    print("100 - Done")
    #print("Finished - Results written to %s" %opts.outF)

    return

################################################################
def main():
    usage = "usage: %prog [-A <filename>] [--A_band] [-B...-Z filename] [other_options]"
    parser = OptionParser(usage)

    # define options
    parser.add_option("--calc", dest="calc", help="calculation in gdalnumeric syntax using +-/* or any numpy array functions (i.e. logical_and())")
    # hack to limit the number of input file options close to required number
    for myAlpha in AlphaList[0:len(sys.argv)-1]:
        eval('parser.add_option("-%s", dest="%s", help="input gdal raster file, note you can use any letter A-Z")' %(myAlpha, myAlpha))
        eval('parser.add_option("--%s_band", dest="%s_band", default=1, type=int, help="number of raster band for file %s (default 1)")' %(myAlpha, myAlpha, myAlpha))

    parser.add_option("--outfile", dest="outF", default='gdal_calc.tif', help="output file to generate or fill")
    parser.add_option("--NoDataValue", dest="NoDataValue", type=float, help="set output nodata value (Defaults to datatype specific value)")
    parser.add_option("--type", dest="type", help="output datatype, must be one of %s" % list(DefaultNDVLookup.keys()))
    parser.add_option("--format", dest="format", default="GTiff", help="GDAL format for output file (default 'GTiff')")
    parser.add_option(
        "--creation-option", "--co", dest="creation_options", default=[], action="append",
        help="Passes a creation option to the output format driver. Multiple "
        "options may be listed. See format specific documentation for legal "
        "creation options for each format.")
    parser.add_option("--allBands", dest="allBands", default="", help="process all bands of given raster (A-Z)")
    parser.add_option("--overwrite", dest="overwrite", action="store_true", help="overwrite output file if it already exists")
    parser.add_option("--debug", dest="debug", action="store_true", help="print debugging information")

    (opts, args) = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help()
    elif not opts.calc:
        print("No calculation provided.  Nothing to do!")
        parser.print_help()
    else:
        doit(opts, args)

if __name__ == "__main__":
    main()
