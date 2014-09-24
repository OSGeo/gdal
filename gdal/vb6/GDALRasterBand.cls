'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  VB6 GDALRasterBand Shadow Class.
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
' Revision 1.5  2005/08/04 20:53:51  fwarmerdam
' convert to DOS text mode
'
' Revision 1.4  2005/04/08 14:36:25  fwarmerdam
' applied owned flag, and auto-destroy
'
' Revision 1.3  2005/04/06 22:30:39  fwarmerdam
' fixed some color table stuff, added headers
'
'

VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "GDALRasterBand"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False

' ----------------------------------------------------------------------------
Option Explicit

Private obj As Long
Public XSize As Long
Public YSize As Long
Public BlockXSize As Long
Public BlockYSize As Long
Public DataType As Long

' ----------------------------------------------------------------------------
Private Sub Class_Initialize()
    obj = 0
    XSize = 0
    YSize = 0
    BlockXSize = 0
    BlockYSize = 0
    DataType = 0
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long)
    obj = obj_in
    XSize = GDALCore.GDALGetRasterBandXSize(obj)
    YSize = GDALCore.GDALGetRasterBandYSize(obj)
    Call GDALCore.GDALGetBlockSize(obj, BlockXSize, BlockYSize)
    DataType = GDALCore.GDALGetRasterDataType(obj)
End Sub

' ----------------------------------------------------------------------------
Public Function GetMinimum()
    Dim Success As Long
    If obj <> 0 Then
        GetMinimum = GDALCore.GDALGetRasterMinimum(obj, Success)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetMaximum()
    Dim Success As Long
    If obj <> 0 Then
        GetMaximum = GDALCore.GDALGetRasterMaximum(obj, Success)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetOffset() As Double
    Dim Success As Long
    If obj <> 0 Then
        GetOffset = GDALCore.GDALGetRasterOffset(obj, Success)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetOffset(ByVal Offset As Double) As Long
    If obj <> 0 Then
        SetOffset = GDALCore.GDALSetRasterOffset(obj, Offset)
    Else
        SetOffset = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetScale() As Double
    Dim Success As Long
    If obj <> 0 Then
        GetScale = GDALCore.GDALGetRasterScale(obj, Success)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetScale(ByVal NewScale As Double) As Long
    If obj <> 0 Then
        SetScale = GDALCore.GDALSetRasterScale(obj, NewScale)
    Else
        SetScale = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetNoDataValue(ByRef Success As Long)
    If obj <> 0 Then
        GetNoDataValue = GDALCore.GDALGetRasterNoDataValue(obj, Success)
    Else
        Success = 0
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetNoDataValue(ByVal NoDataValue As Double)
    If obj <> 0 Then
        SetNoDataValue = GDALCore.GDALSetRasterNoDataValue(obj, NoDataValue)
    Else
        SetNoDataValue = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function RasterIO _
    (ByVal RWFlag As Long, _
     ByVal XOff As Long, ByVal YOff As Long, _
     ByVal XSize As Long, ByVal YSize As Long, _
     DataArray As Variant) As Long

    If obj <> 0 Then
        Dim BufXSize, BufYSize, RawPtr, DataType As Long
        RawPtr = GDALCore.SafeArrayToPtr(DataArray, DataType, _
                                          BufXSize, BufYSize)

        If RawPtr <> 0 Then
            RasterIO = GDALCore.GDALRasterIO( _
                obj, RWFlag, XOff, YOff, XSize, YSize, _
                RawPtr, BufXSize, BufYSize, DataType, 0, 0)
        Else
            RasterIO = 3 ' CE_Failure
        End If
    Else
        RasterIO = GDALCore.ObjIsNULLError
    End If

End Function

' ----------------------------------------------------------------------------
Public Function GetMetadata(Domain As String) As Variant
    If obj <> 0 Then
        GetMetadata = GDALCore.GetMetadata(obj, Domain)
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetMetadata(MetaData As Variant, Domain As String) As Variant
    If obj <> 0 Then
        SetMetadata = GDALCore.SetMetadata(obj, MetaData, Domain)
    Else
        SetMetadata = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetMetadataItem(Name As String, Domain As String) As String
    If obj <> 0 Then
        GetMetadataItem = GDALCore.CStr2VB(GDALCore.GDALGetMetadataItem(obj, Name, Domain))
    Else
        GetMetadataItem = vbNullString
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetMetadataItem(Name As String, _
                                Value As String, Domain As String) As Long
    If obj <> 0 Then
        SetMetadataItem = GDALCore.GDALSetMetadataItem(obj, Name, Value, Domain)
    Else
        SetMetadataItem = GDALCore.ObjIsNULLError
    End If
End Function
' ----------------------------------------------------------------------------
Public Function GetDescription() As String
    If obj <> 0 Then
        GetDescription = GDALCore.CStr2VB(GDALCore.GDALGetDescription(obj))
    End If
End Function
' ----------------------------------------------------------------------------
Public Function GetColorTable() As GDALColorTable
    Dim ct As New GDALColorTable
    If obj <> 0 Then
        Dim CTHandle As Long

        CTHandle = GDALCore.GDALGetRasterColorTable(obj)
        If CTHandle <> 0 Then
            Call ct.CInit(CTHandle,0)
        End If
    End If
    Set GetColorTable = ct
End Function

' ----------------------------------------------------------------------------
Public Function SetColorTable(ColorTable As GDALColorTable) As Long
    If obj <> 0 And ColorTable.GetObjPtr() <> 0 Then
        SetColorTable = GDALCore.GDALSetRasterColorTable( _
                obj, ColorTable.GetObjPtr())
    Else
        SetColorTable = GDALCore.ObjIsNULLError
    End If
End Function

