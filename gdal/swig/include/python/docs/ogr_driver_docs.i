%extend OGRDriverShadow {
// File: ogrsfdriver_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrsfdriver.cpp
14432 2008-05-10 18:47:46Z warmerdam $\") ";

%feature("docstring")  CreateDataSource "OGRDataSourceH
OGR_Dr_CreateDataSource(OGRSFDriverH hDriver, const char *pszName,
char **papszOptions)

This function attempts to create a new data source based on the passed
driver. The papszOptions argument can be used to control driver
specific creation options. These options are normally documented in
the format specific documentation.

This function is the same as the C++ method
OGRSFDriver::CreateDataSource().

Parameters:
-----------

hDriver:  handle to the driver on which data source creation is based.

pszName:  the name for the new data source.

papszOptions:  a StringList of name=value options. Options are driver
specific, and driver information can be found at the following
url:http://www.gdal.org/ogr/ogr_formats.html

NULL is returned on failure, or a new OGRDataSource handle on success.
";

%feature("docstring")  DeleteDataSource "OGRErr
OGR_Dr_DeleteDataSource(OGRSFDriverH hDriver, const char
*pszDataSource) ";

%feature("docstring")  GetName "const char*
OGR_Dr_GetName(OGRSFDriverH hDriver)

Fetch name of driver (file format). This name should be relatively
short (10-40 characters), and should reflect the underlying file
format. For instance \"ESRI Shapefile\".

This function is the same as the C++ method OGRSFDriver::GetName().

Parameters:
-----------

hDriver:  handle to the the driver to get the name from.

driver name. This is an internal string and should not be modified or
freed. ";

%feature("docstring")  Open "OGRDataSourceH OGR_Dr_Open(OGRSFDriverH
hDriver, const char *pszName, int bUpdate)

Attempt to open file with this driver.

This function is the same as the C++ method OGRSFDriver::Open().

Parameters:
-----------

hDriver:  handle to the driver that is used to open file.

pszName:  the name of the file, or data source to try and open.

bUpdate:  TRUE if update access is required, otherwise FALSE (the
default).

NULL on error or if the pass name is not supported by this driver,
otherwise an handle to an OGRDataSource. This OGRDataSource should be
closed by deleting the object when it is no longer needed. ";

%feature("docstring")  TestCapability "int
OGR_Dr_TestCapability(OGRSFDriverH hDriver, const char *pszCap)

Test if capability is available.

One of the following data source capability names can be passed into
this function, and a TRUE or FALSE value will be returned indicating
whether or not the capability is available for this object.

ODrCCreateDataSource: True if this driver can support creating data
sources.

ODrCDeleteDataSource: True if this driver supports deleting data
sources.

The #define macro forms of the capability names should be used in
preference to the strings themselves to avoid mispelling.

This function is the same as the C++ method
OGRSFDriver::TestCapability().

Parameters:
-----------

hDriver:  handle to the driver to test the capability against.

pszCap:  the capability to test.

TRUE if capability available otherwise FALSE. ";

%feature("docstring")  CopyDataSource "OGRDataSourceH
OGR_Dr_CopyDataSource(OGRSFDriverH hDriver, OGRDataSourceH hSrcDS,
const char *pszNewName, char **papszOptions) ";

}