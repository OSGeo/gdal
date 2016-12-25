VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "Form1"
   ClientHeight    =   3090
   ClientLeft      =   60
   ClientTop       =   450
   ClientWidth     =   4680
   LinkTopic       =   "Form1"
   ScaleHeight     =   3090
   ScaleWidth      =   4680
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command1 
      Caption         =   "Command1"
      Height          =   615
      Left            =   1680
      TabIndex        =   0
      Top             =   2160
      Width           =   975
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Command1_Click()
    Dim pUTM As PROJ4Lib.IProjDef
    Dim pLL As PROJ4Lib.IProjDef
    
    Set pUTM = New PROJ4Lib.ProjDef
    Set pLL = New PROJ4Lib.ProjDef

    pUTM.Initialize ("+proj=utm +zone=11 +datum=WGS84")
    pLL.Initialize ("+proj=latlong +datum=WGS84")
    
    Dim X As Double, Y As Double, Z As Double
    
    X = 25000
    Y = 3000000
    Z = 0
    
    MsgBox X & " " & Y
    
    If pLL.TransformPoint3D(pUTM, X, Y, Z) = 0 Then
        MsgBox "TransformPoint3D " & pLL.GetLastError()
    End If
    
    MsgBox X & " " & Y
       
End Sub
