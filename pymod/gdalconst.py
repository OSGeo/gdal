#******************************************************************************
#  $Id$
# 
#  Name:     gdalconst.py
#  Project:  GDAL Python Interface
#  Purpose:  GDAL Constants Definitions
#  Author:   Frank Warmerdam, warmerda@home.com
# 
#******************************************************************************
#  Copyright (c) 2000, Frank Warmerdam
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
# 
# $Log$
# Revision 1.5  2002/11/16 20:39:34  warmerda
# added CXT_Literal
#
# Revision 1.4  2002/06/27 15:41:49  warmerda
# added minixml read/write stuff
#
# Revision 1.3  2000/07/19 13:58:13  warmerda
# fixed complex type codes
#
# Revision 1.2  2000/06/05 17:24:06  warmerda
# added real complex support
#
# Revision 1.1  2000/03/08 21:05:42  warmerda
# New
#
 

# GDALDataType
GDT_Unknown = 0
GDT_Byte = 1
GDT_UInt16 = 2
GDT_Int16 = 3
GDT_UInt32 = 4
GDT_Int32 = 5
GDT_Float32 = 6
GDT_Float64 = 7
GDT_CInt16 = 8
GDT_CInt32 = 9
GDT_CFloat32 = 10
GDT_CFloat64 = 11

# GDALAccess
GA_ReadOnly = 0
GA_Update = 1

# GDALRWFlag
GF_Read = 0
GF_Write = 1

# GDALColorInterp
GCI_Undefined=0
GCI_GrayIndex=1
GCI_PaletteIndex=2
GCI_RedBand=3
GCI_GreenBand=4
GCI_BlueBand=5
GCI_AlphaBand=6
GCI_HueBand=7
GCI_SaturationBand=8
GCI_LightnessBand=9
GCI_CyanBand=10
GCI_MagentaBand=11
GCI_YellowBand=12
GCI_BlackBand=13

# GDALPaletteInterp
GPI_Gray=0
GPI_RGB=1
GPI_CMYK=2
GPI_HLS=3

CXT_Element=0
CXT_Text=1
CXT_Attribute=2
CXT_Comment=3
CXT_Literal=4


