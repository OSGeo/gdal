'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  VB6 GDALDriver Shadow Class.
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
Attribute VB_Name = "GDALDriver"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit
Private obj As Long

' ----------------------------------------------------------------------------
Private Sub Class_Initialize()
    obj = 0
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long)
    obj = obj_in
End Sub

' ----------------------------------------------------------------------------
Public Function Create(ByVal Filename As String, _
     ByVal XSize As Long, ByVal YSize As Long, ByVal BandCount As Long, _
     ByVal DataType As Long, Options As Variant) As GDALDataset

    If obj <> 0 Then
        Dim CSLOptions As Long, ds_obj As Long

        CSLOptions = GDALCore.VariantToCSL(Options)
        ds_obj = GDALCore.GDALCreate(obj, Filename, XSize, YSize, BandCount, _
                                     DataType, CSLOptions)
        Call GDALCore.CSLDestroy(CSLOptions)

        If ds_obj <> 0 Then
            Dim ds As New GDALDataset
            Call ds.CInit(ds_obj,1)
            Set Create = ds
        End If
    End If
End Function

' ----------------------------------------------------------------------------
Public Function CreateCopy(Filename As String, SrcDS As GDALDataset, _
     ApproxOK As Boolean, Options As Variant) As GDALDataset

    If obj <> 0 And SrcDS.GetObjPtr() Then
        Dim CSLOptions As Long, ds_obj As Long, ApproxOK_Int As Long

        If ApproxOK Then
            ApproxOK_Int = 1
        Else
            ApproxOK_Int = 0
        End If
  
        CSLOptions = GDALCore.VariantToCSL(Options)
        ds_obj = GDALCore.GDALCreateCopy(obj, Filename, SrcDS.GetObjPtr(), _
                                         ApproxOK_Int, CSLOptions, 0, 0)
        Call GDALCore.CSLDestroy(CSLOptions)

        If ds_obj <> 0 Then
            Dim ds As New GDALDataset
            Call ds.CInit(ds_obj,1)
            Set CreateCopy = ds
        End If
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
        GetMetadataItem = GDALCore.CStr2VB(GDALCore.GDALGetMetadataItem(obj, _
					   Name, Domain))
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
Public Function GetShortName() As String
    If obj <> 0 Then
        GetShortName = GDALCore.CStr2VB(GDALCore.GDALGetDescription(obj))
    End If
End Function

