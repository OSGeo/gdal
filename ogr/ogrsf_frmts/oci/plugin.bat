REM 
REM This script (with local tweaks) can be used to generate 
REM an OGR OCI Plugin
REM 

call C:\software\vstudio\vc98\bin\vcvars32.bat

set ORACLE_HOME=C:/Software/Oracle/Product/10.1.0/db_1
set GDALBLD=C:/PROGRA~1/FWTools1.0.0a4
set OCI_FILES=oci_utils.cpp ogrocidatasource.cpp ogrocidriver.cpp ogrocilayer.cpp ogrociloaderlayer.cpp ogrociselectlayer.cpp ogrociselectlayer.cpp ogrocisession.cpp ogrocistatement.cpp ogrocistringbuf.cpp ogrocitablelayer.cpp ogrociwritablelayer.cpp
set OCI_LIB=%ORACLE_HOME%\oci\lib\msvc\ociw32.lib %ORACLE_HOME%\oci\lib\msvc\oci.lib

del *.obj

@echo on

cl.exe /c /GX /MD /GR -I. -I%GDALBLD%\include -I%ORACLE_HOME%\oci\include %OCI_FILES%

link.exe /dll *.obj /out:ogr_OCI.dll %GDALBLD%\lib\gdal_i.lib %OCI_LIB%



	
