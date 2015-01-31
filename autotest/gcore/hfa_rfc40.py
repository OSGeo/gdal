#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Tests Raster Attribute Table support in the HFA driver and in particular,
#           changes related to RFC40.
# Author:   Sam Gillingham <gillingham.sam@gmail.com>
# 
###############################################################################
# Copyright (c) 2013,  Sam Gillingham <gillingham.sam@gmail.com>
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

import sys
from osgeo import gdal
import numpy

sys.path.append( '../pymod' )

import gdaltest

INT_DATA = numpy.array([197, 83, 46, 29, 1, 78, 23, 90, 12, 45])
DOUBLE_DATA = numpy.array([0.1, 43.2, 78.1, 9.9, 23.0, 0.92, 82.5, 0.0, 1.0, 99.0])
STRING_DATA = numpy.array(["sddf", "wess", "grbgr", "dewd", "ddww", "qwsqw", 
                            "gbfgbf", "wwqw3", "e", ""])
STRING_DATA_INTS = numpy.array(["197", "83", "46", "29", "1", "78", 
                            "23", "90", "12", "45"])
STRING_DATA_DOUBLES = numpy.array(["0.1", "43.2", "78.1", "9.9", "23.0", "0.92", 
                            "82.5", "0.0", "1.0", "99.0"])
LONG_STRING_DATA = numpy.array(["sdfsdfsdfs", "sdweddw", "sdewdweee", "3423dedd", 
                            "jkejjjdjd", "edcdcdcdc", "fcdkmk4m534m", "edwededdd",
                            "dedwedew", "wdedefrfrfrf"])

class HFATestError(Exception):
    pass

def CreateAndWriteRAT(fname):
    """
    Creates file and writes some data
    """
    # create
    driver = gdal.GetDriverByName("HFA")
    ds = driver.Create(fname, 10, 10, 1, gdal.GDT_Byte)

    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])

    # write some image data
    band = ds.GetRasterBand(1)
    data = numpy.zeros((10, 10), numpy.uint8)
    data[5, 5] = 20
    band.WriteArray(data)

    band.SetMetadataItem("LAYER_TYPE", "thematic")

    rat = band.GetDefaultRAT()

    # create some columns
    if rat.CreateColumn("Ints", gdal.GFT_Integer, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("Doubles", gdal.GFT_Real, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("Strings", gdal.GFT_String, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    # for writing as different type
    if rat.CreateColumn("IntAsDouble", gdal.GFT_Integer, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("IntsAsString", gdal.GFT_Integer, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("DoubleAsInt", gdal.GFT_Real, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("DoubleAsString", gdal.GFT_Real, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("StringAsInt", gdal.GFT_String, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")

    if rat.CreateColumn("StringAsDouble", gdal.GFT_String, gdal.GFU_Generic ) != gdal.CE_None:
        raise HFATestError("Create column failed")


    rat.SetRowCount(INT_DATA.size)

    # some basic checks
    if rat.GetRowCount() != INT_DATA.size:
        raise HFATestError('Wrong RowCount')

    if rat.GetColumnCount() != 9:
        raise HFATestError('Wrong Column Count')

    if rat.GetNameOfCol(1) != "Doubles":
        raise HFATestError('Wrong Column Count')

    if rat.GetUsageOfCol(1) != gdal.GFU_Generic:
        raise HFATestError("Wrong column usage")

    if rat.GetTypeOfCol(1) != gdal.GFT_Real:
        raise HFATestError("Wrong column usage")

    if rat.GetColOfUsage(gdal.GFU_Generic) != 0:
        raise HFATestError("Wrong col of usage")

    if not rat.ChangesAreWrittenToFile():
        raise HFATestError("Wrong ChangesAreWrittenToFile")

    # Write data
    if rat.WriteArray(INT_DATA, 0) != gdal.CE_None:
        raise HFATestError("Failed to write int column")

    if rat.WriteArray(DOUBLE_DATA, 1) != gdal.CE_None:
        raise HFATestError("Failed to write double column")

    if rat.WriteArray(STRING_DATA, 2) != gdal.CE_None:
        raise HFATestError("Failed to write string column")

    # different types
    if rat.WriteArray(DOUBLE_DATA, 3) != gdal.CE_None:
        raise HFATestError("Failed to write doubles to int column")

    if rat.WriteArray(STRING_DATA_INTS, 4) != gdal.CE_None:
        raise HFATestError("Failed to write strings to int column")

    if rat.WriteArray(INT_DATA, 5) != gdal.CE_None:
        raise HFATestError("Failed to write ints to doubles column")

    if rat.WriteArray(STRING_DATA_DOUBLES, 6) != gdal.CE_None:
        raise HFATestError("Failed to write strings to doubles column")

    if rat.WriteArray(INT_DATA, 7) != gdal.CE_None:
        raise HFATestError("Failed to write ints to string column")

    if rat.WriteArray(DOUBLE_DATA, 8) != gdal.CE_None:
        raise HFATestError("Failed to write doubles to string column")

    #print('Succeeding writing data')
    #ds.FlushCache()
    ds = None

    return 'success'

def ReadAndCheckValues(fname, numrows):
    ds = gdal.Open(fname)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    if rat.GetRowCount() != numrows:
        raise HFATestError("Wrong number of rows")

    data = rat.ReadAsArray(0, 0, 10)
    if not (data == INT_DATA).all():
        raise HFATestError("Int colunm does not match")

    data = rat.ReadAsArray(1, 0, 10)
    if not (data == DOUBLE_DATA).all():
        raise HFATestError("double colunm does not match")

    data = rat.ReadAsArray(2, 0, 10)
    if not (data == STRING_DATA.astype(numpy.character)).all():
        raise HFATestError("string colunm does not match")

    data = rat.ReadAsArray(3, 0, 10)
    if not (data == DOUBLE_DATA.astype(int)).all():
        raise HFATestError("int as double column does not match")

    data = rat.ReadAsArray(4, 0, 10)
    if not (data == STRING_DATA_INTS.astype(numpy.int)).all():
        raise HFATestError("int as string column does not match")
                
    data = rat.ReadAsArray(5, 0, 10)
    if not (data == INT_DATA).all():
        raise HFATestError("double as int column does not match")

    data = rat.ReadAsArray(6, 0, 10)
    if not (data == STRING_DATA_DOUBLES.astype(numpy.double)).all():
        raise HFATestError("double as string column does not match")
                
    data = rat.ReadAsArray(7, 0, 10)
    if not (data.astype(numpy.int) == INT_DATA).all():
        raise HFATestError("string as int column does not match")

    data = rat.ReadAsArray(8, 0, 10)
    if not (data.astype(numpy.double) == DOUBLE_DATA).all():
        raise HFATestError("string as int column does not match")

    #print('suceeded reading')
    ds = None

    return 'success'

def CheckSetGetValues(fname):
    # check the 'legacy' get and set value calls
    ds = gdal.Open(fname, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    # write data
    nrows = rat.GetRowCount()
    for i in range(nrows):
        # write some data slightly differrent
        rat.SetValueAsInt(i, 0, int(INT_DATA[i] + 1))
        rat.SetValueAsDouble(i, 1, DOUBLE_DATA[i] + 1)
        s = STRING_DATA[i]
        s = s + 'z'
        rat.SetValueAsString(i, 2, s)

    # read data and check
    for i in range(nrows):
        if rat.GetValueAsInt(i, 0) != (INT_DATA[i] + 1):
            raise HFATestError("GetValueAsInt not reading correctly")
        if rat.GetValueAsDouble(i, 1) != (DOUBLE_DATA[i] + 1):
            raise HFATestError("GetValueAsDouble not reading correctly")
        s = STRING_DATA[i]
        s = s + 'z'
        if rat.GetValueAsString(i, 2) != s:
            raise HFATestError("GetValueAsString not reading correctly")

    # no need to check different types as ValuesIO is checked for this above
    # and these calls map to ValuesIO

    # write back old
    for i in range(nrows):
        rat.SetValueAsInt(i, 0, int(INT_DATA[i]))
        rat.SetValueAsDouble(i, 1, DOUBLE_DATA[i])
        rat.SetValueAsString(i, 2, STRING_DATA[i])

    #print("Get/SetValue OK")
    #ds.FlushCache()
    ds = None

    return 'success'

def ExtendAndWrite(fname):
    # write more data to the end of the RAT - will extend it
    ds = gdal.Open(fname, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    noldrows = rat.GetRowCount()

    # extend
    nrows = noldrows + INT_DATA.size
    rat.SetRowCount(nrows)

    # write new data
    if rat.WriteArray(INT_DATA, 0, 10) != gdal.CE_None:
        raise HFATestError("Failed to write int column")
    if rat.WriteArray(DOUBLE_DATA, 1, 10) != gdal.CE_None:
        raise HFATestError("Failed to write double column")
    if rat.WriteArray(STRING_DATA, 2, 10) != gdal.CE_None:
        raise HFATestError("Failed to write string column")

    #print('extend ok')
    #ds.FlushCache()
    ds = None

    return 'success'

def CheckExtension(fname):
    ds = gdal.Open(fname)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    data = rat.ReadAsArray(0, 10, 10)
    if not (data == INT_DATA).all():
        raise HFATestError("Int column does not match")

    data = rat.ReadAsArray(1, 10, 10)
    if not (data == DOUBLE_DATA).all():
        raise HFATestError("Double column does not match")

    data = rat.ReadAsArray(2, 10, 10)
    if not (data == STRING_DATA.astype(numpy.character)).all():
        raise HFATestError("String column does not match")

    #print('extension data ok')
    ds = None

    return 'success'

def WriteLongStrings(fname):
    # this will force the string column to be re-written to accomodate
    # a longer string size
    ds = gdal.Open(fname, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    if rat.WriteArray(LONG_STRING_DATA, 2, 10)!= gdal.CE_None:
        raise HFATestError("Failed to write string column")

    #print("wrote long strings ok")
    #ds.FlushCache()
    ds = None

    return 'success'

def CheckLongStrings(fname):
    ds = gdal.Open(fname)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    data = rat.ReadAsArray(2, 10, 10)
    if not (data == LONG_STRING_DATA.astype(numpy.character)).all():
        raise HFATestError("String column does not match")

    #print("checked long strings ok")
    ds = None

    return 'success'

def SetLinearBinning(fname):
    ds = gdal.Open(fname, gdal.GA_Update)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    if rat.SetLinearBinning(0, 1) != gdal.CE_None:
        raise HFATestError("Error in SetLinearBinning")

    #print("set linear binning ok")
    #ds.FlushCache()
    ds = None

    return 'success'

def CheckLinearBinning(fname):
    ds = gdal.Open(fname)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    (state, min, size) = rat.GetLinearBinning()
    if not state:
        raise HFATestError("GetLinearBinning failed")

    if min != 0 or size != 1:
        raise HFATestError("GetLinearBinning values wrong")

    if rat.GetRowOfValue(3) != 3:
        raise HFATestError("GetRowOfValue value wrong")

    #print('linear binning ok')
    ds = None

    return 'success'

def CheckClone(fname):
    ds = gdal.Open(fname)
    band = ds.GetRasterBand(1)
    rat = band.GetDefaultRAT()

    cloned = rat.Clone()
    
    if cloned.GetValueAsInt(0, 0) != 197:
        raise HFATestError("Cloned info wrong int")
    if cloned.GetValueAsDouble(5, 1) != 0.92:
        raise HFATestError("Cloned info wrong double")
    if cloned.GetValueAsString(1, 2) != "wess":
        raise HFATestError("Cloned info wrong string")

    #print("cloned ok")
    ds = None

    return 'success'

# basic tests
def hfa_rfc40_1():
    return CreateAndWriteRAT("tmp/test.img")

def hfa_rfc40_2():
    return ReadAndCheckValues("tmp/test.img", 10)

# the older interface
def hfa_rfc40_3():
    return CheckSetGetValues("tmp/test.img")

# make sure original data not changed
def hfa_rfc40_4():
    return ReadAndCheckValues("tmp/test.img", 10)

# make it longer - data will be re-written
def hfa_rfc40_5():
    return ExtendAndWrite("tmp/test.img")

# make sure old data not changed
def hfa_rfc40_6():
    return ReadAndCheckValues("tmp/test.img", 20)

# new data at the end ok?
def hfa_rfc40_7():
    return CheckExtension("tmp/test.img")

# write some longer strings - string column will 
# have to be re-written
def hfa_rfc40_8():
    return WriteLongStrings("tmp/test.img")

# make sure old data not changed
def hfa_rfc40_9():
    return ReadAndCheckValues("tmp/test.img", 20)

# check new data ok
def hfa_rfc40_10():
    return CheckLongStrings("tmp/test.img")

# linear binning
def hfa_rfc40_11():
    return SetLinearBinning("tmp/test.img")

# linear binning
def hfa_rfc40_12():
    return CheckLinearBinning("tmp/test.img")

# clone
def hfa_rfc40_13():
    return CheckClone("tmp/test.img")

# serialize not available from Python...

def hfa_rfc40_cleanup():
    gdal.GetDriverByName('HFA').Delete("tmp/test.img")
    return 'success'

gdaltest_list = [
    hfa_rfc40_1,
    hfa_rfc40_2,
    hfa_rfc40_3,
    hfa_rfc40_4,
    hfa_rfc40_5,
    hfa_rfc40_6,
    hfa_rfc40_7,
    hfa_rfc40_8,
    hfa_rfc40_9,
    hfa_rfc40_10,
    hfa_rfc40_11,
    hfa_rfc40_12,
    hfa_rfc40_13,
    hfa_rfc40_cleanup,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'hfa_rfc40' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

