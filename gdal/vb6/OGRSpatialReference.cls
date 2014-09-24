'*****************************************************************************


' $Id$


'


' Project:  GDAL VB6 Bindings


' Purpose:  VB6 OGRSpatialReference Shadow Class.


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
' Revision 1.4  2006/03/22 04:38:09  fwarmerdam
' Added copyright header
'
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
Attribute VB_Name = "OGRSpatialReference"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False

' ----------------------------------------------------------------------------
Option Explicit

Private obj As Long
Private owned As Long

' ----------------------------------------------------------------------------
Private Sub Class_Initialize()
    obj = GDALCore.OSRNewSpatialReference("")
    owned = 1
End Sub

' ----------------------------------------------------------------------------
Private Sub Class_Terminate()
    If obj <> 0 And owned <> 0 Then
        Call Destroy
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long, owned_in As Long)
    If obj <> 0 And owned <> 0 Then
        Call Destroy
        Call GDALCore.OSRDestroySpatialReference(obj)
    End If
    obj = obj_in
    owned = owned_in
End Sub

' ----------------------------------------------------------------------------
Public Sub Destroy()
    If obj <> 0 Then
        'Call MsgBox("Destroy OGRSpatialReference: " & ExportToWkt())
        Call GDALCore.OSRDestroySpatialReference(obj)
        obj = 0
        owned = 0
    End If
End Sub

' ----------------------------------------------------------------------------
Public Function GetObjPtr() As Long
    GetObjPtr = obj
End Function

' ----------------------------------------------------------------------------
Public Function SetFromUserInput(InputText As String) As Long
    If obj <> 0 Then
        SetFromUserInput = GDALCore.OSRSetFromUserInput(obj, InputText)
    Else
        SetFromUserInput = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function Clone() As OGRSpatialReference
    Dim srs As New OGRSpatialReference
    If obj <> 0 Then
        Dim obj_new As Long
  
        obj_new = GDALCore.OSRClone(obj)
        Call srs.CInit(obj_new, 1)
    End If
    Set Clone = srs
End Function

' ----------------------------------------------------------------------------
Public Function CloneGeogCS() As OGRSpatialReference
    Dim srs As New OGRSpatialReference
    If obj <> 0 Then
        Dim obj_new As Long
  
        obj_new = GDALCore.OSRCloneGeogCS(obj)
        Call srs.CInit(obj_new, 1)
    End If
    Set CloneGeogCS = srs
End Function

' ----------------------------------------------------------------------------
Public Function ImportFromEPSG(EPSGCode As Long) As Long
    If obj <> 0 Then
        ImportFromEPSG = GDALCore.OSRImportFromEPSG(obj, EPSGCode)
    Else
        ImportFromEPSG = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function ExportToWkt() As String
    If obj <> 0 Then
        Dim wktptr As Long
        Dim err As Long
        
        err = GDALCore.OSRExportToWkt(obj, wktptr)
        ExportToWkt = GDALCore.CStr2VB(wktptr)
        ' We really should free wktptr here.
    Else
        ExportToWkt = ""
    End If
End Function

' ----------------------------------------------------------------------------
Public Function ExportToPrettyWkt(bSimplify As Long) As String
    If obj <> 0 Then
        Dim wktptr As Long
        Dim err As Long
        
        err = GDALCore.OSRExportToPrettyWkt(obj, wktptr, bSimplify)
        ExportToPrettyWkt = GDALCore.CStr2VB(wktptr)
        ' We really should free wktptr here.
    Else
        ExportToPrettyWkt = ""
    End If
End Function

' ----------------------------------------------------------------------------
Public Function SetAttrValue(NodePath As String, NodeValue As String) As Long
    If obj <> 0 Then
        SetAttrValue = GDALCore.OSRSetAttrValue(obj, NodePath, NodeValue)
    Else
        SetAttrValue = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetAttrValue(NodePath As String, iChild As Long) As String
    If obj <> 0 Then
        Dim c_str As Long
        c_str = GDALCore.OSRGetAttrValue(obj, NodePath, iChild)
        If c_str = 0 Then
            GetAttrValue = ""
        Else
            GetAttrValue = GDALCore.CStr2VB(c_str)
        End If
    Else
        GetAttrValue = ""
    End If
End Function

