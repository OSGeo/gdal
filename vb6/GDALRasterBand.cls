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
    Dim CT As New GDALColorTable
    If obj <> 0 Then
        Dim CTHandle As Long

        CTHandle = GDALCore.GDALGetRasterColorTable(obj)
        If CTHandle <> 0 Then
            Call CT.CInit(CTHandle)
        End If
    End If
    Set GetColorTable = CT
End Function

' ----------------------------------------------------------------------------
Public Function SetColorTable(ColorTable As GDALColorTable) As Long
    If obj <> 0 And ColorTable.GetObjPtr() <> 0 Then
        SetColorTable = GDALColor.GDALSetRasterColorTable( _
                obj, ColorTable.GetObjPtr())
    Else
        SetColorTable = GDALCore.ObjIsNULLError
    End If
End Function

