'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  GDAL VB6 internal support functions.  Items in this module should
'           only be used by the VB6 shadow classes and GDAL.bas.  Application
'           level code should not need access to this module.
' Author:   Frank Warmerdam, warmerdam@pobox.com
'
'*****************************************************************************
' Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
'
' Permission is hereby granted, free of charge, to any person obtaining a
' copy of this software and associated documentation files (the "Software"),
' to deal in the Software without restriction, including without limitation
' the rights to use, copy, modify, merge, publish, distribute, sublicense,
' and/or sell copies of the Software, and to permit persons to whom the
' Software is furnished to do so, subject to the following conditions:
'
' The above copyright notice and this permission notice shall be included
' in all copies or substantial portions of the Software.
'
' THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
' OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
' FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
' THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
' LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
' FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
' DEALINGS IN THE SOFTWARE.
'*****************************************************************************
'
' $Log$
' Revision 1.1  2005/03/16 19:45:19  fwarmerdam
' new
'
'

Attribute VB_Name = "GDALCore"

' ****************************************************************************
'               Declarations for C API functions.
' ****************************************************************************

Public Const ObjIsNULLError = 1001
 
' ----------------------------------------------------------------------------
'       Misc
' ----------------------------------------------------------------------------

Public Declare Function GDALGetDataTypeName _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDataTypeName@4" _
    (ByVal DataType As Long) As Long

Public Declare Function GDALGetDataTypeSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDataTypeSize@4" _
    (ByVal DataType As Long) As Long

Public Declare Function CPLGetLastErrorMsg _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_CPLGetLastErrorMsg@0" _
    () As Long
    
Public Declare Sub CSLDestroy _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_CSLDestroy@4" _
    (ByVal CSLList As Long)
    
' ----------------------------------------------------------------------------
'       GDALMajorObject
' ----------------------------------------------------------------------------
Public Declare Function GDALGetMetadataItem _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetMetadataItem@12" _
    (ByVal Handle As Long, ByVal Name As String, _
     ByVal Domain As String) As Long

Public Declare Function GDALGetMetadata _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetMetadata@8" _
    (ByVal Handle As Long, ByVal Domain As String) As Long

Public Declare Function GDALSetMetadataItem _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetMetadataItem@16" _
    (ByVal Handle As Long, ByVal Name As String, _
     ByVal Value As String, ByVal Domain As String) As Long

Public Declare Function GDALSetMetadata _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetMetadata@12" _
    (ByVal Handle As Long, ByVal MetaData As Long, _
     ByVal Domain As String) As Long

Public Declare Function GDALGetDescription _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDescription@4" _
    (ByVal Handle As Long) As Long

Public Declare Sub GDALSetDescription _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetDescription@8" _
    (ByVal Handle As Long, ByVal Description As String)

' ----------------------------------------------------------------------------
'       GDAL Dataset
' ----------------------------------------------------------------------------
Public Declare Function GDALOpen _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALOpen@8" _
    (ByVal Filename As String, ByVal Access As Long) As Long
    
Public Declare Function GDALOpenShared _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALOpenShared@8" _
    (ByVal Filename As String, ByVal Access As Long) As Long
    
Public Declare Sub GDALClose _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALClose@4" _
    (ByVal Handle As Long)

Public Declare Function GDALGetRasterXSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterXSize@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALGetRasterYSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterYSize@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALGetRasterCount _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterCount@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALGetRasterBand _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterBand@8" _
    (ByVal Handle As Long, ByVal BandNo As Long) As Long

Public Declare Function GDALGetProjectionRef _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetProjectionRef@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALSetProjection _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetProjection@8" _
    (ByVal Handle As Long, ByVal WKTProj As String) As Long

Public Declare Function GDALGetGeoTransform _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetGeoTransform@8" _
    (ByVal Handle As Long, ByRef Geotransform As Double) As Long

Public Declare Function GDALSetGeoTransform _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetGeoTransform@8" _
    (ByVal Handle As Long, ByRef Geotransform As Double) As Long

Public Declare Function GDALReferenceDataset _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALReferenceDataset@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALDereferenceDataset _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALDereferenceDataset@4" _
    (ByVal Handle As Long) As Long

Public Declare Sub GDALFlushCache _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALFlushCache@4" _
    (ByVal Handle As Long)

' ----------------------------------------------------------------------------
'       GDALRasterBand
' ----------------------------------------------------------------------------
Public Declare Function GDALGetRasterDataType _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterDataType@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALGetRasterBandXSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterBandXSize@4" _
    (ByVal Handle As Long) As Long

Public Declare Function GDALGetRasterBandYSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterBandYSize@4" _
    (ByVal Handle As Long) As Long

Public Declare Sub GDALGetBlockSize _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetBlockSize@12" _
    (ByVal Handle As Long, ByRef XSize As Long, ByRef YSize As Long)

Public Declare Function GDALGetRasterNoDataValue _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterNoDataValue@8" _
    (ByVal Handle As Long, ByRef bSuccess As Long) As Double
    
Public Declare Function GDALSetRasterNoDataValue _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetRasterNoDataValue@12" _
    (ByVal Handle As Long, ByVal NoDataValue As Double) As Long

Public Declare Function GDALGetRasterMinimum _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterMinimum@8" _
    (ByVal Handle As Long, ByRef bSuccess As Long) As Double

Public Declare Function GDALGetRasterMaximum _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterMaximum@8" _
    (ByVal Handle As Long, ByRef bSuccess As Long) As Double

Public Declare Function GDALGetRasterOffset _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterOffset@8" _
    (ByVal Handle As Long, ByRef bSuccess As Long) As Double

Public Declare Function GDALSetRasterOffset _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetRasterOffset@12" _
    (ByVal Handle As Long, ByVal Offset As Double) As Long

Public Declare Function GDALGetRasterScale _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetRasterScale@8" _
    (ByVal Handle As Long, ByRef bSuccess As Long) As Double

Public Declare Function GDALSetRasterScale _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALSetRasterScale@12" _
    (ByVal Handle As Long, ByVal NewScale As Double) As Long

Public Declare Function GDALRasterIO _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALRasterIO@48" _
    (ByVal Handle As Long, ByVal RWFlag As Long, _
     ByVal XOff As Long, ByVal YOff As Long, _
     ByVal XSize As Long, ByVal YSize As Long, _
     ByVal pData As Long, ByVal BufXSize As Long, ByVal BufYSize As Long, _
     ByVal DataType As Long, _
     ByVal PixelSpace As Long, ByVal LineSpace As Long) As Long

' ----------------------------------------------------------------------------
'       GDALDriver
' ----------------------------------------------------------------------------
Public Declare Sub GDALAllRegister _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALAllRegister@0" ()

Public Declare Function GDALGetDriverByName _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDriverByName@4" _
    (ByVal Filename As String) As Long

Public Declare Function GDALGetDriverCount _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDriverCount@0" () As Long

Public Declare Function GDALGetDriver _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALGetDriver@4" _
    (ByVal DriverIndex As Long) As Long

Public Declare Sub GDALDestroyDriverManager _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALDestroyDriverManager@0" ()

Public Declare Function GDALCreate _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALCreate@28" _
    (ByVal Handle As Long, ByVal Filename As String, _
     ByVal XSize As Long, ByVal YSize As Long, ByVal BandCount As Long, _
     ByVal DataType As Long, ByVal Options As Long) As Long

Public Declare Function GDALCreateCopy _
    Lib "D:\warmerda\fao\gdal\gdal12vb.dll" _
    Alias "_GDALCreateCopy@28" _
    (ByVal Handle As Long, ByVal Filename As String, _
     ByVal SrcDS As Long, ByVal ApproxOK As Long, ByVal Options As Long, _
     ByVal ProgressFunc As Long, ByVal ProgressArg As Long) As Long

' ----------------------------------------------------------------------------
' Special VB6 Support functions
' ----------------------------------------------------------------------------
Public Declare Function CStringToVB6 _
    Lib "D:\local\bin\gdalvb.dll" _
    Alias "_vbCStringToVB6@8" _
    (result As Variant, ByVal cString As Long) As Long
    
Public Declare Function VariantToCSL _
    Lib "D:\local\bin\gdalvb.dll" _
    Alias "_vbVariantToCSL@4" _
    (InList As Variant) As Long
    
Public Declare Sub CSLToVariant _
    Lib "D:\local\bin\gdalvb.dll" _
    Alias "_vbCSLToVariant@8" _
    (ByVal InList As Long, OutList As Variant)
    
Public Declare Function SafeArrayToPtr _
    Lib "D:\local\bin\gdalvb.dll" _
    Alias "_vbSafeArrayToPtr@16" _
    (InArray As Variant, ByRef DataType As Long, _
     ByRef XSize As Long, ByRef YSize As Long) As Long
    
' ****************************************************************************
'       VB Wrapper functions.
' ****************************************************************************

' ----------------------------------------------------------------------------
Public Function CStr2VB(c_str As Long)
    Dim msg As Variant
    Dim n As Long
    n = CStringToVB6(msg, c_str)
    CStr2VB = msg
End Function

' ----------------------------------------------------------------------------
Public Function GetMetadata(MajorObject As Long, Domain As String)
    Dim CSLMetadata As Long
    Dim ResultMD As Variant
   
    CSLMetadata = GDALGetMetadata(MajorObject, Domain)
    Call CSLToVariant(CSLMetadata, ResultMD)
    GetMetadata = ResultMD
End Function

Public Function SetMetadata(Object As Long, MetaData As Variant, _
                             Domain As String)
    Dim CSLMetadata As Long

    CSLMetadata = VariantToCSL(MetaData)
    SetMetadata = GDALSetMetadata(Object, CSLMetadata, Domain)
    Call CSLDestroy(CSLMetadata)
End Function

