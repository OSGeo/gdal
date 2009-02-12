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
  
  OGRGeometryShadow( OGRwkbGeometryType type ) {
    if (type != wkbUnknown ) {
      return (OGRGeometryShadow*) OGR_G_CreateGeometry( type );
    }
    // throw?
    else return 0;
  }

   retStringAndCPLFree* ExportToWkt()
   {
       char* argout = NULL;
       OGR_G_ExportToWkt(self, &argout);
       return argout;
   }

  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryH pt = OGR_G_CreateGeometry( wkbPoint );
    OGR_G_Centroid( (OGRGeometryH) self, (OGRGeometryH) pt );
    return (OGRGeometryShadow*) pt;
  }
}
