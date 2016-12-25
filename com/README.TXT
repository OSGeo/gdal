	PROJ.4 COM Interface
	====================

This directory contains a Visual Studio 6 project to build
a COM object with a partial interface to PROJ.4.  

Once built, you should be able to:

  regsvr32 debug\proj4com.dll
  
Then in VB6:

 o Add the type library as a resource and enable it. 

 o Setup code like the following:

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