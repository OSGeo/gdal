VERSION 1.0 CLASS
BEGIN
  MultiUse = -1  'True
  Persistable = 0  'NotPersistable
  DataBindingBehavior = 0  'vbNone
  DataSourceBehavior  = 0  'vbNone
  MTSTransactionMode  = 0  'NotAnMTSObject
END
Attribute VB_Name = "OGRCoordinateTransformation"
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
    obj = 0
    owned = 0
End Sub

' ----------------------------------------------------------------------------
Private Sub Class_Terminate()
    If obj <> 0 And owned <> 0 Then
        Call Destroy
    End If
End Sub

' ----------------------------------------------------------------------------
Public Sub CInit(obj_in As Long, owned_in As Long)
    If obj <> 0 Then
        Call GDALCore.OCTDestroyCoordinateTransformation(obj)
    End If
    obj = obj_in
    owned = owned_in
End Sub

' ----------------------------------------------------------------------------
Public Sub Destroy()
    If obj <> 0 Then
        Call GDALCore.OCTDestroyCoordinateTransformation(obj)
'        Call MsgBox("Destroying OGRCoordinateTransformation")
        obj = 0
        owned = 0
    End If
End Sub

' ----------------------------------------------------------------------------
Public Function IsValid() As Boolean
    If obj <> 0 Then
        IsValid = True
    Else
        IsValid = False
    End If
End Function

' ----------------------------------------------------------------------------
' Eventually we might add a multi-point version but this is simple
' to do for now, and quite useful.
Public Function TransformOne(ByRef X As Double, ByRef Y As Double, _
                              ByRef Z As Double) As Boolean
    If obj <> 0 Then
        Dim Success As Long
        Dim RetValue As Long

        RetValue = GDALCore.OCTTransformEx(obj, 1, X, Y, Z, Success)
        If Success = 0 Or RetValue = 0 Then
            TransformOne = False
        Else
            TransformOne = True
        End If
    Else
        TransformOne = False
    End If
End Function
