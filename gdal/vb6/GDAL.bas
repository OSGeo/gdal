Attribute VB_Name = "GDAL"
'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  Main GDAL Public Module - public non-class GDAL declarations.
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
' Revision 1.6  2005/08/04 20:53:51  fwarmerdam
' convert to DOS text mode
'
' Revision 1.5  2005/04/11 19:58:47  fwarmerdam
' added CPLSet/GetConfigOption
'
' Revision 1.4  2005/04/08 14:36:25  fwarmerdam
' applied owned flag, and auto-destroy
'
' Revision 1.3  2005/04/06 22:29:50  fwarmerdam
' added CreateCoordinateTransformation() function
'
' Revision 1.2  2005/03/16 23:34:07  fwarmerdam
' fixed up open to always return a GDALDataset
'
' Revision 1.1  2005/03/16 19:40:49  fwarmerdam
' new
'
'


' GDALAccess (for open)
Public Const GA_ReadOnly As Long = 0
Public Const GA_Update As Long = 1

' GDALDataTypes
Public Const GDT_Unknown As Long = 0
Public Const GDT_Byte As Long = 1
Public Const GDT_UInt16 As Long = 2
Public Const GDT_Int16 As Long = 3
Public Const GDT_UInt32 As Long = 4
Public Const GDT_Int32 As Long = 5
Public Const GDT_Float32 As Long = 6
Public Const GDT_Float64 As Long = 7
Public Const GDT_CInt16 As Long = 8
Public Const GDT_CInt32 As Long = 9
Public Const GDT_CFloat32 As Long = 10
Public Const GDT_CFloat64 As Long = 11
Public Const GDT_TypeCount As Long = 12

' read/write flags for RasterIO
Public Const GF_Read As Long = 0
Public Const GF_Write As Long = 1

' Palette Interpretation
Public Const GPI_Gray As Long = 0
Public Const GPI_RGB As Long = 1
Public Const GPI_CMYK As Long = 2
Public Const GPI_HLS As Long = 3

' Driver metadata items.
Public Const DMD_SHORTNAME As String = "DMD_SHORTNAME"
Public Const DMD_LONGNAME As String = "DMD_LONGNAME"
Public Const DMD_HELPTOPIC As String = "DMD_HELPTOPIC"
Public Const DMD_MIMETYPE As String = "DMD_MIMETYPE"
Public Const DMD_EXTENSION As String = "DMD_EXTENSION"
Public Const DMD_CREATIONOPTIONLIST As String = "DMD_CREATIONOPTIONLIST"
Public Const DMD_CREATIONDATATYPES As String = "DMD_CREATIONDATATYPES"

Public Const DCAP_CREATE As String = "DCAP_CREATE"
Public Const DCAP_CREATECOPY As String = "DCAP_CREATECOPY"

' ----------------------------------------------------------------------------
Public Function GetLastErrorMsg() As String
    GetLastErrorMsg = CStr2VB(GDALCore.CPLGetLastErrorMsg())
End Function

' ----------------------------------------------------------------------------
Public Function GetConfigOption(Key As String, Default As String)
    GetConfigOption = CStr2VB(GDALCore.CPLGetConfigOption(Key, Default))
End Function

' ----------------------------------------------------------------------------
Public Sub SetConfigOption(Key As String, Value As String)
    Call GDALCore.CPLSetConfigOption(Key, Value)
End Sub

' ----------------------------------------------------------------------------
Public Sub AllRegister()
    Call GDALCore.GDALAllRegister
End Sub
' ----------------------------------------------------------------------------
Public Function GetDriverByName(DriverName As String) As GDALDriver
    Dim drv_c As Long
    drv_c = GDALCore.GDALGetDriverByName(DriverName)
    If drv_c <> 0 Then
        Set GetDriverByName = New GDALDriver
        GetDriverByName.CInit (drv_c)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetDriver(ByVal DriverIndex As Long) As GDALDriver
    Dim drv_c As Long
    drv_c = GDALCore.GDALGetDriver(DriverIndex)
    If drv_c <> 0 Then
        Set GetDriver = New GDALDriver
        GetDriver.CInit (drv_c)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetDriverCount() As Long
    GetDriverCount = GDALCore.GDALGetDriverCount()
End Function

' ----------------------------------------------------------------------------
Public Function GetDataTypeName(ByVal DataType As Long) As String
    GetDataTypeName = GDALCore.CStr2VB(GDALCore.GDALGetDataTypeName(DataType))
End Function

' ----------------------------------------------------------------------------
Public Function GetDataTypeSize(ByVal DataType As Long) As Long
    GetDataTypeSize = GDALCore.GDALGetDataTypeSize(DataType)
End Function

' ----------------------------------------------------------------------------
Public Function OpenDS(Filename As String, ByVal Access As Long) As GDALDataset
    Set OpenDS = New GDALDataset
    Call OpenDS.CInit(GDALCore.GDALOpen(Filename, Access), 1)
End Function

' ----------------------------------------------------------------------------
Public Function OpenSharedDS(Filename As String, ByVal Access As Long) As GDALDataset
    Set OpenSharedDS = New GDALDataset
    Call OpenSharedDS.CInit(GDALCore.GDALOpenShared(Filename, Access), 1)
End Function

' ----------------------------------------------------------------------------
Public Function CreateColorTable(ByVal PaletteInterp As Long) _
                As GDALColorTable
    Dim obj As Long
    obj = GDALCore.GDALCreateColorTable(PaletteInterp)
    If obj <> 0 Then
        Set CreateColorTable = New GDALColorTable
        Call CreateColorTable.CInit(obj, 1)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function CreateCoordinateTransformation( _
        SrcSRS As OGRSpatialReference, TrgSRS As OGRSpatialReference) _
        As OGRCoordinateTransformation
        
    Dim obj As Long
    
    Set ct = New OGRCoordinateTransformation

    obj = GDALCore.OCTNewCoordinateTransformation(SrcSRS.GetObjPtr(), _
                                                  TrgSRS.GetObjPtr())
    If obj <> 0 Then
        Call ct.CInit(obj, 1)
    End If
    Set CreateCoordinateTransformation = ct
End Function

