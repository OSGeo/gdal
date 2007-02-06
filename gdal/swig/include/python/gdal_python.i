/*
 * $Id$
 *
 * python specific code for gdal bindings.
 */

/*
 * $Log$
 * Revision 1.5  2006/12/02 05:16:12  hobu
 * Dataset.WriteRaster Dataset.ReadRaster
 *
 * Revision 1.4  2005/09/30 20:19:19  kruland
 * Moved cpl_exceptions.i code into gdal_python so the other languages can
 * have a common mechanism.
 *
 * Revision 1.3  2005/09/13 03:05:05  kruland
 * Exception generation code was moved to the cpl_exceptions.i file.
 *
 * Revision 1.2  2005/09/02 21:42:42  kruland
 * The compactdefaultargs feature should be turned on for all bindings not just
 * python.
 *
 * Revision 1.1  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 */

%feature("autodoc");

%init %{
  /* gdal_python.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%pythoncode %{
  from gdalconst import *
  import gdalconst

  import sys
  byteorders = {"little": "<",
                "big": ">"}
  array_modes = { gdalconst.GDT_Int16:    ("%si2" % byteorders[sys.byteorder]),
                  gdalconst.GDT_UInt16:   ("%su2" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Int32:    ("%si4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_UInt32:   ("%su4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Float32:  ("%sf4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Float64:  ("%sf8" % byteorders[sys.byteorder]),
                  gdalconst.GDT_CFloat32: ("%sf4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_CFloat64: ("%sf8" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Byte:     ("%st8" % byteorders[sys.byteorder]),
  }
%}

/*
 * This was the cpl_exceptions.i code.  But since python is the only one
 * different (should support old method as well as new one)
 * it was moved into this file.
 */
%{
int bUseExceptions=0;

void VeryQuiteErrorHandler(CPLErr eclass, int code, const char *msg ) {
  /* If the error class is CE_Fatal, we want to have a message issued
     because the CPL support code does an abort() before any exception
     can be generated */
  if (eclass == CE_Fatal ) {
    CPLDefaultErrorHandler(eclass, code, msg );
  }
}
%}

%inline %{
void UseExceptions() {
  bUseExceptions = 1;
  CPLSetErrorHandler( (CPLErrorHandler) VeryQuiteErrorHandler );
}

void DontUseExceptions() {
  bUseExceptions = 0;
  CPLSetErrorHandler( CPLDefaultErrorHandler );
}
%}

%include exception.i

%exception {
    CPLErrorReset();
    $action
    if ( bUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
}


%extend GDAL_GCP {
%pythoncode {
  def __str__(self):
    str = '%s (%.2fP,%.2fL) -> (%.7fE,%.7fN,%.2f) %s '\
          % (self.Id, self.GCPPixel, self.GCPLine,
             self.GCPX, self.GCPY, self.GCPZ, self.Info )
    return str
    def serialize(self,with_Z=0):
        base = [CXT_Element,'GCP']
        base.append([CXT_Attribute,'Id',[CXT_Text,self.Id]])
        pixval = '%0.15E' % self.GCPPixel       
        lineval = '%0.15E' % self.GCPLine
        xval = '%0.15E' % self.GCPX
        yval = '%0.15E' % self.GCPY
        zval = '%0.15E' % self.GCPZ
        base.append([CXT_Attribute,'Pixel',[CXT_Text,pixval]])
        base.append([CXT_Attribute,'Line',[CXT_Text,lineval]])
        base.append([CXT_Attribute,'X',[CXT_Text,xval]])
        base.append([CXT_Attribute,'Y',[CXT_Text,yval]])
        if with_Z:
            base.append([CXT_Attribute,'Z',[CXT_Text,zval]])        
        return base
} /* pythoncode */
}

%extend GDALRasterBandShadow {
%pythoncode {
  def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_obj=None):
      import gdalnumeric

      return gdalnumeric.BandReadAsArray( self, xoff, yoff,
                                          win_xsize, win_ysize,
                                          buf_xsize, buf_ysize, buf_obj )
    
  def WriteArray(self, array, xoff=0, yoff=0):
      import gdalnumeric

      return gdalnumeric.BandWriteArray( self, array, xoff, yoff )

  def __get_array_interface__(self):
      shape = [1, self.XSize, self.YSize]
      
}
}

%extend GDALDatasetShadow {
%pythoncode {
    def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_obj=None):
        import gdalnumeric
        return gdalnumeric.DatasetReadAsArray( self, xoff, yoff, xsize, ysize )
    def WriteRaster(self, xoff, yoff, xsize, ysize,
                    buf_string,
                    buf_xsize = None, buf_ysize = None, buf_type = None,
                    band_list = None ):

        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;
        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if buf_type is None:
            buf_type = self._band[band_list[0]-1].DataType;

        if len(buf_string) < buf_xsize * buf_ysize * len(band_list) \
           * (_gdal.GetDataTypeSize(buf_type) / 8):
            raise ValueError, "raster buffer too small in WriteRaster"
        else:    
            return _gdal.Dataset_WriteRaster(self,
                 xoff, yoff, xsize, ysize,
                buf_string, buf_xsize, buf_ysize, buf_type, band_list )

    def ReadRaster(self, xoff, yoff, xsize, ysize,
                   buf_xsize = None, buf_ysize = None, buf_type = None,
                   band_list = None ):

        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;

        if buf_type is None:
            buf_type = self.GetRasterBand(1).DataType;
        return _gdal.Dataset_ReadRaster(self, xoff, yoff, xsize, ysize,
                                           buf_xsize, buf_ysize, buf_type,
                                           band_list)
}
}

%extend GDALMajorObjectShadow {
%pythoncode {
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
}
}

%import typemaps_python.i
