/******************************************************************************
 * $Id$
 *
 * Name:     ogr.i
 * Project:  GDAL Python Interface
 * Purpose:  OGR Core SWIG Interface declarations.
 * Author:   Howard Butler, hobu@iastate.edu
 *

 *
 * $Log$
 * Revision 1.6  2005/02/18 18:28:16  kruland
 * Rename OGRSpatialReferenceH type to SpatialReference to make the ogr and
 * osr modules compatible.
 *
 * Revision 1.5  2005/02/18 18:01:34  hobu
 * Started working on the geometry constructors with a lot
 * of help from Kevin
 *
 * Revision 1.4  2005/02/17 00:01:37  hobu
 * quick start on the datasource/driver stuff
 *
 * Revision 1.3  2005/02/16 21:55:02  hobu
 * started the OGRDriver class stuff.
 * CopyDataSource, CreateDataSource, and
 * DS Open
 *
 * Revision 1.2  2005/02/16 20:02:57  hobu
 * added constants
 *
 * Revision 1.1  2005/02/16 19:47:03  hobu
 * skeleton OGR interface
 *
 *
 *
*/

%module ogr
%import gdal_typemaps.i

%init %{
  if ( OGRGetDriverCount() == 0 ) {
    OGRRegisterAll();
  }
%}

typedef int OGRErr;
  
%pythoncode %{

wkb25Bit = -2147483648 # 0x80000000
wkbUnknown = 0
wkbPoint = 1  
wkbLineString = 2
wkbPolygon = 3
wkbMultiPoint = 4
wkbMultiLineString = 5
wkbMultiPolygon = 6
wkbGeometryCollection = 7
wkbNone = 100
wkbLinearRing = 101
wkbPoint25D =              wkbPoint              + wkb25Bit
wkbLineString25D =         wkbLineString         + wkb25Bit
wkbPolygon25D =            wkbPolygon            + wkb25Bit
wkbMultiPoint25D =         wkbMultiPoint         + wkb25Bit
wkbMultiLineString25D =    wkbMultiLineString    + wkb25Bit
wkbMultiPolygon25D =       wkbMultiPolygon       + wkb25Bit
wkbGeometryCollection25D = wkbGeometryCollection + wkb25Bit

# OGRFieldType

OFTInteger = 0
OFTIntegerList= 1
OFTReal = 2
OFTRealList = 3
OFTString = 4
OFTStringList = 5
OFTWideString = 6
OFTWideStringList = 7
OFTBinary = 8

# OGRJustification

OJUndefined = 0
OJLeft = 1
OJRight = 2

wkbXDR = 0
wkbNDR = 1

###############################################################################
# Constants for testing Capabilities

# Layer
OLCRandomRead          = "RandomRead"
OLCSequentialWrite     = "SequentialWrite"
OLCRandomWrite         = "RandomWrite"
OLCFastSpatialFilter   = "FastSpatialFilter"
OLCFastFeatureCount    = "FastFeatureCount"
OLCFastGetExtent       = "FastGetExtent"
OLCCreateField         = "CreateField"
OLCTransactions        = "Transactions"
OLCDeleteFeature       = "DeleteFeature"
OLCFastSetNextByIndex  = "FastSetNextByIndex"

# DataSource
ODsCCreateLayer        = "CreateLayer"
ODsCDeleteLayer        = "DeleteLayer"

# Driver
ODrCCreateDataSource   = "CreateDataSource"
ODrCDeleteDataSource   = "DeleteDataSource"

%}

%{
#include <iostream>
using namespace std;

#include "ogr_api.h"
#include "ogr_core.h"
#include "cpl_string.h"

typedef void SpatialReference;

%}

/* OGR Driver */

%rename (Driver) OGRSFDriverH;

class OGRSFDriverH {
  OGRSFDriverH();
  ~OGRSFDriverH();
public:
%extend {

%immutable;
  char const *name;
%mutable;


    
%newobject CreateDataSource;
%feature( "kwargs" ) CreateDataSource;
  OGRDataSourceH *CreateDataSource( const char *name, 
                                    char **options = 0 ) {
    OGRDataSourceH *ds = (OGRDataSourceH*) OGR_Dr_CreateDataSource( self, name, options);
    if (ds != NULL) {
      OGR_DS_Dereference(ds);
    }
    return ds;
  }
  
%newobject CopyDataSource;
%feature( "kwargs" ) CopyDataSource;
  OGRDataSourceH *CopyDataSource( OGRDataSourceH* copy_ds, 
                                  const char* name, 
                                  char **options = 0 ) {
    OGRDataSourceH *ds = (OGRDataSourceH*) OGR_Dr_CopyDataSource(self, copy_ds, name, options);
    if (ds != NULL) {
      OGR_DS_Dereference(ds);
    }
      return ds;
  }
  
%newobject Open;
%feature( "kwargs" ) Open;
  OGRDataSourceH *Open( const char* name, 
                        int update=0 ) {
    OGRDataSourceH* ds = (OGRDataSourceH*) OGR_Dr_Open(self, name, update);

      return ds;
  }

  int DeleteDataSource( const char *name ) {
    return OGR_Dr_DeleteDataSource( self, name );
  }
  int TestCapability (const char *cap) {
    return OGR_Dr_TestCapability(self, cap);
  }
  
  const char * GetName() {
    return OGR_Dr_GetName( self );
  }


} /* %extend */
}; /* class OGRSFDriverH */


/* OGR DataSource */

%rename (DataSource) OGRDataSourceH;

class OGRDataSourceH {
  OGRDataSourceH();
  ~OGRDataSourceH();
public:
%extend {

%immutable;
  char const *name;
%mutable;

  void Destroy() {
    OGR_DS_Destroy(self);
  }

  void Release() {
    OGRReleaseDataSource(self);
  }
  
  int Reference() {
    return OGR_DS_Reference(self);
  }
  
  int Dereference() {
    return OGR_DS_Dereference(self);
  }
  
  int GetRefCount() {
    return OGR_DS_GetRefCount(self);
  }
  
  int GetSummaryRefCount() {
    return OGR_DS_GetSummaryRefCount(self);
  }
  
  int GetLayerCount() {
    return OGR_DS_GetLayerCount(self);
  }
  

  const char * GetName() {
    return OGR_DS_GetName(self);
  }
  
  OGRErr DeleteLayer(int index){
    return OGR_DS_DeleteLayer(self, index);
  }
  
%newobject GetLayerByIndex;
%feature( "kwargs" ) GetLayerByIndex;
  OGRLayerH *GetLayerByIndex( int index=0) {
    OGRLayerH* layer = (OGRLayerH*) OGR_DS_GetLayer(self, index);

      return layer;
  }

%newobject GetLayerByName;
  OGRLayerH *GetLayerByName( const char* layer_name) {
    OGRLayerH* layer = (OGRLayerH*) OGR_DS_GetLayerByName(self, layer_name);

      return layer;
  }

%pythoncode {
    def __len__(self):
        """Returns the number of layers on the datasource"""
        return self.GetLayerCount()

    def __getitem__(self, value):
        """Support dictionary, list, and slice -like access to the datasource.
ds[0] would return the first layer on the datasource.
ds['aname'] would return the layer named "aname".
ds[0:4] would return a list of the first four layers."""
        import types
        if isinstance(value, types.SliceType):
            output = []
            for i in xrange(value.start,value.stop,step=value.step):
                try:
                    output.append(self.GetLayer(i))
                except OGRError: #we're done because we're off the end
                    return output
            return output
        if isinstance(value, types.IntType):
            if value > len(self)-1:
                raise IndexError
            return self.GetLayer(value)
        elif isinstance(value,types.StringType):
            return self.GetLayer(value)
        else:
            raise TypeError, 'Input %s is not of String or Int type' % type(value)

    def GetLayer(self,iLayer=0):
        """Return the layer given an index or a name"""
        import types
        if isinstance(iLayer, types.StringType):
            return self.GetLayerByName(iLayer)
        elif isinstance(iLayer, types.IntType):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError, "Input %s is not of String or Int type" % type(iLayer)
}

} /* %extend */


}; /* class OGRDataSourceH */




%{
char const *OGRSFDriverH_name_get( OGRSFDriverH *h ) {
  return OGR_Dr_GetName( h );
}

char const *OGRDataSourceH_name_get( OGRDataSourceH *h ) {
  return OGR_DS_GetName( h );
}

%}

%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (GetOpenDS) OGRGetOpenDS;


%apply (THROW_OGR_ERROR) {OGRErr};
%inline %{
  OGRSFDriverH* GetDriverByName( char const *name ) {
    return (OGRSFDriverH*) OGRGetDriverByName( name );
  }
  
  OGRSFDriverH* GetDriver(int driver_number) {
    return (OGRSFDriverH*) OGRGetDriver(driver_number);
  }
  
  
  
  int OGRGetDriverCount();
  int OGRGetOpenDSCount();

    
  OGRErr OGRSetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER);
  
  
  char* OGRErrors[10] = { "None",
                              "Not enough data",
                              "Unsupported geometry type",
                              "Unsupported operation",
                              "Corrupt data",
                              "General Error",
                              "Unsupported SRS"
                            };
  

%}
%clear (OGRErr);

%newobject GetOpenDS;
%inline %{
  OGRDataSourceH* GetOpenDS(int ds_number) {
    OGRDataSourceH* layer = (OGRDataSourceH*) OGRGetOpenDS(ds_number);
    return layer;
  }
  
%}

%newobject CreateGeometryFromWkb;
%inline %{
  OGRGeometryH CreateGeometryFromWkb( unsigned char * bin_string, 
                                      SpatialReference *reference ) {
    OGRGeometryH geom;
    OGRErr err = OGR_G_CreateFromWkb(bin_string,
                                      reference,
                                      &geom);
    if (err != 0 )
       throw err;
    return geom;
  }
 
%}

%apply (char **ignorechange) { (char **) };
%newobject CreateGeometryFromWkt;
%inline %{
  OGRGeometryH CreateGeometryFromWkt( char **val, 
                                      SpatialReference *reference ) {
    OGRGeometryH geom;
    OGRErr err = OGR_G_CreateFromWkt(val,
                                      reference,
                                      &geom);
    if (err != 0 )
       throw err;
    return geom;
  }
 
%}
%clear (char **);
