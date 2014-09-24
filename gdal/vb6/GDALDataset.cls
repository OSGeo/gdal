'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  VB6 GDALDataset Shadow Class.
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
' Revision 1.3  2005/08/04 20:53:51  fwarmerdam
' convert to DOS text mode
'
' Revision 1.2  2005/04/08 14:36:25  fwarmerdam
' applied owned flag, and auto-destroy
'
' Revision 1.1  2005/03/16 19:45:19  fwarmerdam
' new
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
Attribute VB_Name = "GDALDataset"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False

Option Explicit
Private obj As Long
Private owned As Long
Public XSize As Long
Public YSize As Long
Public BandCount As Long

' ----------------------------------------------------------------------------
Private Sub Class_Initialize()
    obj = 0
    owned = 0
    XSize = 0
    YSize = 0
    BandCount = 0
End Sub

' ----------------------------------------------------------------------------
Private Sub Class_Terminate()
    If obj <> 0 and owned <> 0 Then
        call CloseDS()
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long, owned_in As Long )
    obj = obj_in
    owned = owned_in
    If obj <> 0 Then
        XSize = GDALCore.GDALGetRasterXSize(obj)
        YSize = GDALCore.GDALGetRasterYSize(obj)
        BandCount = GDALCore.GDALGetRasterCount(obj)
    End If
End Sub
' ----------------------------------------------------------------------------
Public Function IsValid() As Boolean

    If obj = 0 Then
        IsValid = False
    Else
        IsValid = True
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetObjPtr() As Long
    GetObjPtr = obj
End Function

' ----------------------------------------------------------------------------
Public Sub CloseDS()
    If obj <> 0 Then
        'Call MsgBox("CloseDS " & GetDescription() )
        Call GDALCore.GDALClose(obj)
        obj = 0
        owned = 0
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub FlushCache()
    If obj <> 0 Then
        Call GDALCore.GDALFlushCache(obj)
    End If
End Sub

' ----------------------------------------------------------------------------
Public Function GetRasterBand(ByVal BandNo As Long) As GDALRasterBand
    Dim c_obj As Long
    Dim band As GDALRasterBand
    
    Set band = Nothing
    If obj <> 0 Then
        c_obj = GDALCore.GDALGetRasterBand(obj, BandNo)
        If c_obj <> 0 Then
            Set band = New GDALRasterBand
            band.CInit (c_obj)
        End If
    End If
    Set GetRasterBand = band
End Function

' ----------------------------------------------------------------------------
Public Function GetProjection() As String
    Dim c_str As Long
    
    If obj <> 0 Then
        c_str = GDALCore.GDALGetProjectionRef(obj)
        GetProjection = GDALCore.CStr2VB(c_str)
    Else
        GetProjection = ""
    End If
End Function
' ----------------------------------------------------------------------------
Public Function SetProjection(WKTProj As String) As String
    Dim err As Long
    
    If obj <> 0 Then
        SetProjection = GDALCore.GDALSetProjection(obj, WKTProj)
    Else
        SetProjection = GDALCore.ObjIsNULLError
    End If
End Function
' ----------------------------------------------------------------------------
Public Function GetGeoTransform(Geotransform() As Double) As Long
    Dim gtsafe() As Double
    ReDim gtsafe(6) As Double
    
    If obj <> 0 Then
        GetGeoTransform = GDALCore.GDALGetGeoTransform(obj, gtsafe(0))
        Geotransform(0) = gtsafe(0)
        Geotransform(1) = gtsafe(1)
        Geotransform(2) = gtsafe(2)
        Geotransform(3) = gtsafe(3)
        Geotransform(4) = gtsafe(4)
        Geotransform(5) = gtsafe(5)
    Else
        GetGeoTransform = GDALCore.ObjIsNULLError
    End If
End Function
' ----------------------------------------------------------------------------
Public Function SetGeoTransform(Geotransform() As Double) As Long
    Dim gtsafe() As Double
    ReDim gtsafe(6) As Double
    
    If obj <> 0 Then
        Dim i As Integer
        For i = 0 To 5
                gtsafe(i) = Geotransform(i)
        Next i
        SetGeoTransform = GDALCore.GDALSetGeoTransform(obj, gtsafe(0))
    Else
        SetGeoTransform = GDALCore.ObjIsNULLError
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
