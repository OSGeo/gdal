'*****************************************************************************
' $Id$
'
' Project:  GDAL VB6 Bindings
' Purpose:  test form
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
' Revision 1.6  2006/03/22 04:40:57  fwarmerdam
' added copyright header
'
'

VERSION 5.00
Object = "{F9043C88-F6F2-101A-A3C9-08002B2F49FB}#1.2#0"; "COMDLG32.OCX"
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "MSCOMCTL.OCX"
Begin VB.Form frmMain 
   Caption         =   "GDALVB6Test"
   ClientHeight    =   5970
   ClientLeft      =   165
   ClientTop       =   855
   ClientWidth     =   8565
   LinkTopic       =   "Form1"
   ScaleHeight     =   5970
   ScaleWidth      =   8565
   StartUpPosition =   3  'Windows Default
   Begin MSComDlg.CommonDialog FileDlg 
      Left            =   7800
      Top             =   120
      _ExtentX        =   847
      _ExtentY        =   847
      _Version        =   393216
      FileName        =   "dolphins_i.png"
   End
   Begin MSComctlLib.StatusBar sbStatusBar 
      Align           =   2  'Align Bottom
      Height          =   270
      Left            =   0
      TabIndex        =   0
      Top             =   5700
      Width           =   8565
      _ExtentX        =   15108
      _ExtentY        =   476
      _Version        =   393216
      BeginProperty Panels {8E3867A5-8586-11D1-B16A-00C0F0283628} 
         NumPanels       =   3
         BeginProperty Panel1 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            AutoSize        =   1
            Object.Width           =   9446
            Text            =   "Status"
            TextSave        =   "Status"
         EndProperty
         BeginProperty Panel2 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            Style           =   6
            AutoSize        =   2
            TextSave        =   "4/11/2005"
         EndProperty
         BeginProperty Panel3 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            Style           =   5
            AutoSize        =   2
            TextSave        =   "3:59 PM"
         EndProperty
      EndProperty
   End
   Begin VB.Menu mnuFile 
      Caption         =   "&File"
      Begin VB.Menu mnuOpen 
         Caption         =   "&Open"
      End
      Begin VB.Menu mnuFileExit 
         Caption         =   "E&xit"
      End
   End
   Begin VB.Menu mnuTools 
      Caption         =   "&Tools"
      Begin VB.Menu mnuToolsTest1 
         Caption         =   "Read Test"
      End
      Begin VB.Menu mnuCSInfo 
         Caption         =   "Coordinate System Info"
      End
      Begin VB.Menu mnuListDrivers 
         Caption         =   "List Drivers"
      End
      Begin VB.Menu mnuCCTest 
         Caption         =   "CreateCopy Test"
      End
      Begin VB.Menu mnuCreate 
         Caption         =   "Create Test"
      End
   End
End
Attribute VB_Name = "frmMain"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

Private Sub Form_Load()
    Me.Left = GetSetting(App.Title, "Settings", "MainLeft", 1000)
    Me.Top = GetSetting(App.Title, "Settings", "MainTop", 1000)
    Me.Width = GetSetting(App.Title, "Settings", "MainWidth", 6500)
    Me.Height = GetSetting(App.Title, "Settings", "MainHeight", 6500)

    Call GDAL.AllRegister
    Call GDAL.SetConfigOption("GDAL_DATA", "D:\warmerda\fao\bld\data")
End Sub


Private Sub Form_Unload(Cancel As Integer)
    Dim i As Integer
    
    'close all sub forms
    For i = Forms.Count - 1 To 1 Step -1
        Unload Forms(i)
    Next
    If Me.WindowState <> vbMinimized Then
        SaveSetting App.Title, "Settings", "MainLeft", Me.Left
        SaveSetting App.Title, "Settings", "MainTop", Me.Top
        SaveSetting App.Title, "Settings", "MainWidth", Me.Width
        SaveSetting App.Title, "Settings", "MainHeight", Me.Height
    End If
End Sub


Private Sub mnuCCTest_Click()
    Dim SrcFilename As String, DstFilename As String
    Dim Drv As GDALDriver
    Dim SrcDS As GDALDataset, DstDS As GDALDataset
        
    SrcFilename = FileDlg.Filename
    
    DstFilename = "out.tif"
    
    Call GDALCore.GDALAllRegister
    Set SrcDS = GDAL.OpenDS(SrcFilename, GDAL.GA_ReadOnly)
    
    Set Drv = GDAL.GetDriverByName("GTiff")
 
    Set DstDS = Drv.CreateCopy(DstFilename, SrcDS, True, Nothing)
    If DstDS.IsValid() Then
        Print "CreateCopy Succeeded, output is " & DstFilename
    Else
        Print "Create Copy Failed: " & GDAL.GetLastErrorMsg()
    End If
    Call DstDS.CloseDS
    Call SrcDS.CloseDS
    
End Sub


Private Sub mnuCreate_Click()
    Dim SrcFilename As String, DstFilename As String
    Dim Drv As GDALDriver
    Dim SrcDS As GDALDataset, DstDS As GDALDataset
    Dim err As Long
        
    SrcFilename = FileDlg.Filename
    DstFilename = "out_create.tif"
    
    Call GDALCore.GDALAllRegister
    Set SrcDS = GDAL.OpenDS(SrcFilename, GDAL.GA_ReadOnly)
    If Not SrcDS.IsValid() Then
        Print GDAL.GetLastErrorMsg()
        Exit Sub
    End If
    
    Set Drv = GDAL.GetDriverByName("GTiff")
    Set DstDS = Drv.Create(DstFilename, SrcDS.XSize, SrcDS.YSize, _
                           SrcDS.BandCount, GDAL.GDT_Byte, Nothing)
    If DstDS.IsValid() Then
        Print "Create Succeeded, file is " & DstFilename
    Else
        Print "Create Failed: " & GDAL.GetLastErrorMsg()
        Exit Sub
    End If
    
    ' Copy geotransform
    Dim gt(6) As Double
    
    err = SrcDS.GetGeoTransform(gt)
    If err = 0 Then
        Call DstDS.SetGeoTransform(gt)
    End If
    
    ' Copy projection
    Call DstDS.SetProjection(SrcDS.GetProjection())
    
    ' Copy metadata.
    Call DstDS.SetMetadata(SrcDS.GetMetadata(""), "")
    
    ' Copy band info
    Dim Scanline() As Double
    ReDim Scanline(SrcDS.XSize) As Double
   
    For iBand = 1 To SrcDS.BandCount
        Dim SrcBand As GDALRasterBand, DstBand As GDALRasterBand
        
        Set SrcBand = SrcDS.GetRasterBand(iBand)
        Set DstBand = DstDS.GetRasterBand(iBand)
        
        Call DstBand.SetMetadata(SrcBand.GetMetadata(""), "")
        Call DstBand.SetOffset(SrcBand.GetOffset())
        Call DstBand.SetScale(SrcBand.GetScale())
        
        Dim NoDataValue As Double, Success As Long
        
        NoDataValue = SrcBand.GetNoDataValue(Success)
        If Success <> 0 Then
            Call DstBand.SetNoDataValue(NoDataValue)
        End If
                
        ' Copy Paletted if one present.
        Dim ct As GDALColorTable
        Set ct = SrcBand.GetColorTable()
        If ct.IsValid() Then
            ' We manually copy the color table.  This isn't really
            ' necessary, but gives us a chance to try out all color
            ' table methods.
            Dim ct2 As GDALColorTable
            Dim iColor As Integer
            Dim Tuple(4) As Integer

            Set ct2 = GDAL.CreateColorTable(GDAL.GPI_RGB)
            For iColor = 0 To ct.EntryCount
                Call ct.GetColorEntryAsRGB(iColor, Tuple)
                Call ct2.SetColorEntry(iColor, Tuple)
            Next iColor
            err = DstBand.SetColorTable(ct2)
        End If
        
        ' Copy band raster data.
        For iLine = 0 To SrcDS.YSize - 1
            Call SrcBand.RasterIO(GDAL.GF_Read, 0, iLine, SrcDS.XSize, 1, Scanline)
            Call DstBand.RasterIO(GDAL.GF_Write, 0, iLine, SrcDS.XSize, 1, Scanline)
        Next iLine
    Next iBand
    
    Call DstDS.CloseDS
    Call SrcDS.CloseDS
    
    Print "Copy seems to have completed."
End Sub


Private Sub mnuCSInfo_Click()
    Dim ds As GDALDataset
    
    Call GDALCore.GDALAllRegister
    Set ds = GDAL.OpenDS(FileDlg.Filename, GDAL.GA_ReadOnly)
    
    If ds.IsValid() Then
        Dim Geotransform(6) As Double
        Dim err As Long
        Dim srs As New OGRSpatialReference
        Dim latlong_srs As OGRSpatialReference
        Dim ct As New OGRCoordinateTransformation
        Dim WKT As String
        
        Call ds.GetGeoTransform(Geotransform)
        
        Print "Size: " & ds.XSize & "P x " & ds.YSize & "L"
        
        ' report projection in pretty format.
        WKT = ds.GetProjection()
        If Len(WKT) > 0 Then
            Print "Projection: "
            Call srs.SetFromUserInput(WKT)
            Print srs.ExportToPrettyWkt(0)

            If srs.GetAttrValue("PROJECTION", 0) <> "" Then
                Set latlong_srs = srs.CloneGeogCS()
                Set ct = GDAL.CreateCoordinateTransformation(srs, latlong_srs)
            End If
        End If
        
        Print "Origin: " & Geotransform(0) & "," & Geotransform(3)
        Print "Pixel Size: " & Geotransform(1) & "x" & (-1 * Geotransform(5))
        
        Call ReportCorner("Top Left      ", 0, 0, _
                          Geotransform, ct)
        Call ReportCorner("Top Right     ", ds.XSize, 0, _
                          Geotransform, ct)
        Call ReportCorner("Bottom Left   ", 0, ds.YSize, _
                          Geotransform, ct)
        Call ReportCorner("Bottom Right  ", ds.XSize, ds.YSize, _
                          Geotransform, ct)
        Call ReportCorner("Center        ", ds.XSize / 2#, ds.YSize / 2#, _
                          Geotransform, ct)
    Else
        Print GDAL.GetLastErrorMsg()
    End If
End Sub

Private Sub ReportCorner(CornerName As String, pixel As Double, line As Double, _
                         gt() As Double, ct As OGRCoordinateTransformation)
                             
    Dim geox As Double, geoy As Double
    Dim longitude As Double, latitude As Double, Z As Double
    Dim latlong_valid As Boolean
    
    geox = gt(0) + pixel * gt(1) + line * gt(2)
    geoy = gt(3) + pixel * gt(4) + line * gt(5)

    latlong_valid = False
    
    If ct.IsValid() Then
        Z = 0
        longitude = geox
        latitude = geoy
        latlong_valid = ct.TransformOne(longitude, latitude, Z)
    End If
                         
    If latlong_valid Then
        Print CornerName & geox & "," & geoy & "    " & longitude & "," & latitude
    Else
        Print CornerName & geox & "," & geoy
    End If
End Sub

                          

Private Sub mnuListDrivers_Click()
    Dim Drv As GDALDriver
    
    Print "GDAL_DATA = " & GDAL.GetConfigOption("GDAL_DATA", "<not set>")
    
    If GDAL.GetDriverCount() < 1 Then
        Call GDAL.AllRegister
    End If
    drvCount = GDAL.GetDriverCount
    Print "Count = " & drvCount
    For drvIndex = 0 To drvCount - 1
        Set Drv = GDAL.GetDriver(drvIndex)
        If Drv.GetMetadataItem(GDAL.DCAP_CREATE, "") = "YES" _
            Or Drv.GetMetadataItem(GDAL.DCAP_CREATECOPY, "") = "YES" Then
            xMsg = " (Read/Write)"
        Else
            xMsg = " (ReadOnly)"
        End If
              
        Print Drv.GetShortName() & ": " & Drv.GetMetadataItem(GDAL.DMD_LONGNAME, "") & xMsg
    Next drvIndex
End Sub

Private Sub mnuOpen_Click()
    Call FileDlg.ShowOpen
    Print "Filename " & FileDlg.Filename & " selected."
End Sub

Private Sub mnuToolsTest1_Click()
    Dim ds As GDALDataset
    Dim Filename As String
    
    Filename = FileDlg.Filename
    
    Call GDALCore.GDALAllRegister
    Set ds = GDAL.OpenDS(Filename, GDAL.GA_ReadOnly)
    
    If ds.IsValid() Then
        Dim Geotransform(6) As Double
        Dim MD As Variant
        Dim i, err As Integer
        
        Print "Open succeeded"
        Print "Size: " & ds.XSize & "x" & ds.YSize & "x" & ds.BandCount
    
        MD = ds.GetMetadata(vbNullString)
        If (UBound(MD) > 0) Then
            Print "Metadata:"
            For i = 1 To UBound(MD)
                Print "  " & MD(i)
            Next i
        End If
        
        For i = 1 To ds.BandCount
            Dim band As GDALRasterBand
            
            Set band = ds.GetRasterBand(i)
            Print "Band " & i & " BlockSize: " & band.BlockXSize & "x" & band.BlockYSize
            Print "     DataType=" & GDAL.GetDataTypeName(band.DataType) _
                & " Offset=" & band.GetOffset() & " Scale=" & band.GetScale() _
                & " Min=" & band.GetMinimum() & " Max=" & band.GetMaximum()
                
            Dim RawData() As Double
            ReDim RawData(ds.XSize) As Double
            err = band.RasterIO(GDAL.GF_Read, 0, 0, ds.XSize, 1, RawData)
            Print "    Data: " & RawData(1) & " " & RawData(10)
            
            Dim ct As GDALColorTable
            Set ct = band.GetColorTable()
            If ct.IsValid() Then
                Dim CEntry(4) As Integer
                Print "    Has Color Table, " & ct.EntryCount & " entries"
                For iColor = 0 To ct.EntryCount - 1
                    Call ct.GetColorEntryAsRGB(iColor, CEntry)
                    Print "      " & iColor & ": " & CEntry(0) & "," & CEntry(1) & "," & CEntry(2) & "," & CEntry(3)
                Next iColor
            End If
        Next i
        
        Call ds.CloseDS
    Else
        Print GDAL.GetLastErrorMsg()
    End If
End Sub

Private Sub mnuFileExit_Click()
    'unload the form
    Unload Me

End Sub

