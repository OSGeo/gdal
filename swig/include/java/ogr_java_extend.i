/******************************************************************************
 * $Id: ogr_java_extend.i 10639 2007-01-17 20:57:32Z tamas $
 *
 * Name:     ogr_java_extend.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Java specific OGR extensions 
 * Author:   Andrea Aime (andrea.aime@gmail.com)
 *
 */
 
/******************************************************************************
 * OGR WKB import and  export. Both extensions make sure byte[] is used in    *
 * java instead of char[]                                                     *
 ******************************************************************************/
 

%extend OGRGeometryShadow 
{
  
  %feature("kwargs") OGRGeometryShadow;
  OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int nLen= 0, unsigned char *pBuf = 0, char *gml = 0 ) {
    if (type != wkbUnknown ) {
      return (OGRGeometryShadow*) OGR_G_CreateGeometry( type );
    }
    else if ( wkt != 0 ) {
      return CreateGeometryFromWkt( &wkt );
    }
    else if ( nLen != 0 ) {
      return CreateGeometryFromWkb( nLen, pBuf );
    }
    else if ( gml != 0 ) {
      return CreateGeometryFromGML( gml );
    }
    // throw?
    else return 0;
  }


  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb(int nLen, unsigned char *pBuf, OGRwkbByteOrder byte_order) {
    return OGR_G_ExportToWkb(self, byte_order, pBuf);
  }

  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = new_OGRGeometryShadow( wkbPoint );
    OGR_G_Centroid( self, pt );
    return pt;
  }
 
}






%newobject sum;
%feature("kwargs") sum;
%inline {
int sum(int a, int b) {
    return a + b;
}  
}



/******************************************************************************
 * $Id: ogr_java_extend.i 10639 2007-01-17 20:57:32Z tamas $
 *
 * Name:     ogr_java_extend.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Java specific OGR extensions 
 * Author:   Andrea Aime (andrea.aime@gmail.com)
 *
 */
 
/******************************************************************************
 * OGR WKB import and  export. Both extensions make sure byte[] is used in    *
 * java instead of char[]                                                     *
 ******************************************************************************/
 

%extend OGRGeometryShadow 
{
  
  %feature("kwargs") OGRGeometryShadow;
  OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int nLen= 0, unsigned char *pBuf = 0, char *gml = 0 ) {
    if (type != wkbUnknown ) {
      return (OGRGeometryShadow*) OGR_G_CreateGeometry( type );
    }
    else if ( wkt != 0 ) {
      return CreateGeometryFromWkt( &wkt );
    }
    else if ( nLen != 0 ) {
      return CreateGeometryFromWkb( nLen, pBuf );
    }
    else if ( gml != 0 ) {
      return CreateGeometryFromGML( gml );
    }
    // throw?
    else return 0;
  }


  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb(int nLen, unsigned char *pBuf, OGRwkbByteOrder byte_order) {
    return OGR_G_ExportToWkb(self, byte_order, pBuf);
  }

  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = new_OGRGeometryShadow( wkbPoint );
    OGR_G_Centroid( self, pt );
    return pt;
  }
 
}