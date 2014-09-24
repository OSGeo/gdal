'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  VB6 GDALColorTable Shadow Class.
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
' Revision 1.1  2005/03/16 23:34:27  fwarmerdam
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
Attribute VB_Name = "GDALColorTable"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False


Option Explicit
Private obj As Long
Private owned As Long
Public EntryCount As Long
Public PaletteInterpretation As Long

' ----------------------------------------------------------------------------
Private Sub Class_Initialize()
    obj = 0
    owned = 0
    EntryCount = 0
    PaletteInterpretation = 0
End Sub

' ----------------------------------------------------------------------------
Private Sub Class_Terminate()
    If obj <> 0 and owned <> 0 Then
        call Destroy()
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long, owned_in As Long )
    if obj <> 0 and owned <> 0 Then
	Call Destroy()
    End If
    obj = obj_in
    owned = owned_in
    If obj <> 0 Then
        EntryCount = GDALCore.GDALGetColorEntryCount(obj)
        PaletteInterpretation = GDALCore.GDALGetPaletteInterpretation(obj)
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub Destroy()
    If obj <> 0 Then
	'Call MsgBox( "Destroying ColorTable" )
	Call GDALCore.GDALDestroyColorTable( obj )
	obj = 0
	owned = 0
    End If 
End Sub

' ----------------------------------------------------------------------------
Public Function GetObjPtr() As Long
    GetObjPtr = obj
End Function

' ----------------------------------------------------------------------------
Public Function IsValid() As Boolean
    If obj = 0 Then
        IsValid = False
    Else
        IsValid = True
    End If
End Function

' ----------------------------------------------------------------------------
Public Function Clone()
    Dim other_c_obj As Long
    Dim other_ct As GDALColorTable

    If obj <> 0 Then
        other_c_obj = GDALCore.GDALCloneColorTable(obj)
        Set other_ct = New GDALColorTable
        Call other_ct.CInit(other_c_obj,1)
        Set Clone = other_ct
    End If
End Function
' ----------------------------------------------------------------------------
Public Sub SetColorEntry(ByVal ColorIndex As Long, _
                               ColorEntry() As Integer)

    If obj <> 0 Then
        Dim SafeColorEntry(4) As Integer
        SafeColorEntry(0) = ColorEntry(0)
        SafeColorEntry(1) = ColorEntry(1)
        SafeColorEntry(2) = ColorEntry(2)
        SafeColorEntry(3) = ColorEntry(3)

        Call GDALCore.GDALSetColorEntry(obj, ColorIndex, SafeColorEntry(0))
    End If
End Sub

' ----------------------------------------------------------------------------
Public Function GetColorEntry(ByVal ColorIndex As Long, ColorEntry() As Integer) As Long

    If obj <> 0 Then
        Dim SafeColorEntry(4) As Integer
        GetColorEntry = GDALCore.GDALGetColorEntry(obj, ColorIndex, _
                                                    SafeColorEntry(0))
        ColorEntry(0) = SafeColorEntry(0)
        ColorEntry(1) = SafeColorEntry(1)
        ColorEntry(2) = SafeColorEntry(2)
        ColorEntry(3) = SafeColorEntry(3)
    Else
        GetColorEntry = GDALCore.ObjIsNULLError
    End If
End Function

' ----------------------------------------------------------------------------
Public Function GetColorEntryAsRGB(ByVal ColorIndex As Long, _
                                    ColorEntry() As Integer) As Long

    If obj <> 0 Then
        Dim SafeColorEntry(4) As Integer

        GetColorEntryAsRGB = GDALCore.GDALGetColorEntryAsRGB( _
                obj, ColorIndex, SafeColorEntry(0))
        ColorEntry(0) = SafeColorEntry(0)
        ColorEntry(1) = SafeColorEntry(1)
        ColorEntry(2) = SafeColorEntry(2)
        ColorEntry(3) = SafeColorEntry(3)
    Else
        GetColorEntryAsRGB = GDALCore.ObjIsNULLError
    End If
End Function
