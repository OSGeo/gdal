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

%pragma(java) modulecode=%{

    /* Uninstanciable class */
    private ogr()
    {
    }

    public static String[] GeneralCmdLineProcessor(String[] args, int nOptions)
    {
        java.util.Vector vArgs = new java.util.Vector();
        int i;
        for(i=0;i<args.length;i++)
            vArgs.addElement(args[i]);

        vArgs = GeneralCmdLineProcessor(vArgs, nOptions);
        java.util.Enumeration eArgs = vArgs.elements();
        args = new String[vArgs.size()];
        i = 0;
        while(eArgs.hasMoreElements())
        {
            String arg = (String)eArgs.nextElement();
            args[i++] = arg;
        }

        return args;
    }

    public static String[] GeneralCmdLineProcessor(String[] args)
    {
        return GeneralCmdLineProcessor(args, 0);
    }

    public static DataSource Open(String filename, boolean update)
    {
        return Open(filename, (update)?1:0);
    }
%}

%extend OGRGeometryShadow 
{
  OGRGeometryShadow( OGRwkbGeometryType type, char *wkt, int nLen, unsigned char *pBuf, const char *gml ) {
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
