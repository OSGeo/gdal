/*
 * $Id$
 *
 * python specific code for gdal bindings.
 */


%feature("autodoc");

%init %{
  /* gdal_python.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%pythoncode %{

  
  have_warned = 0
  def deprecation_warn( module ):
    global have_warned

    if have_warned == 1:
        return

    have_warned = 1

    from warnings import warn
    warn('%s.py was placed in a namespace, it is now available as osgeo.%s' % (module,module),
         DeprecationWarning)
         
         
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

  def RGBFile2PCTFile( src_filename, dst_filename ):
    src_ds = Open(src_filename)
    if src_ds is None or src_ds == 'NULL':
        return 1

    ct = ColorTable()
    err = ComputeMedianCutPCT( src_ds.GetRasterBand(1),
                               src_ds.GetRasterBand(2),
                               src_ds.GetRasterBand(3),
                               256, ct )
    if err != 0:
        return err

    gtiff_driver = GetDriverByName('GTiff')
    if gtiff_driver is None:
        return 1

    dst_ds = gtiff_driver.Create( dst_filename,
                                  src_ds.RasterXSize, src_ds.RasterYSize )
    dst_ds.GetRasterBand(1).SetRasterColorTable( ct )

    err = DitherRGB2PCT( src_ds.GetRasterBand(1),
                         src_ds.GetRasterBand(2),
                         src_ds.GetRasterBand(3),
                         dst_ds.GetRasterBand(1),
                         ct )
    dst_ds = None
    src_ds = None

    return 0
%}



%include "python_exceptions.i"
%include "python_strings.i"

%import typemaps_python.i

/* -------------------------------------------------------------------- */
/*      VSIFReadL()                                                     */
/* -------------------------------------------------------------------- */

%rename (VSIFReadL) wrapper_VSIFReadL;

%apply ( void **outPythonObject ) { (void **buf ) };
%inline %{
int wrapper_VSIFReadL( void **buf, int nMembSize, int nMembCount, VSILFILE *fp)
{
    GIntBig buf_size = nMembSize * nMembCount;

    if (buf_size == 0)
    {
        *buf = NULL;
        return 0;
    }
#if PY_VERSION_HEX >= 0x03000000 
    *buf = (void *)PyBytes_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return 0;
    }
    PyObject* o = (PyObject*) *buf;
    char *data = PyBytes_AsString(o); 
    GIntBig nRet = (GIntBig)VSIFReadL( data, nMembSize, nMembCount, fp );
    if (nRet * nMembSize < buf_size)
    {
        _PyBytes_Resize(&o, nRet * nMembSize);
        *buf = o;
    }
    return nRet;
#else 
    *buf = (void *)PyString_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return 0;
    }
    PyObject* o = (PyObject*) *buf;
    char *data = PyString_AsString(o); 
    GIntBig nRet = (GIntBig)VSIFReadL( data, nMembSize, nMembCount, fp );
    if (nRet * nMembSize < buf_size)
    {
        _PyString_Resize(&o, nRet * nMembSize);
        *buf = o;
    }
    return nRet;
#endif
}
%}
%clear (void **buf );

/* -------------------------------------------------------------------- */
/*      GDAL_GCP                                                        */
/* -------------------------------------------------------------------- */

%extend GDAL_GCP {
%pythoncode %{
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
%} /* pythoncode */
}

%extend GDALRasterBandShadow {
%apply ( void **outPythonObject ) { (void **buf ) };
%apply ( int *optional_int ) {(int*)};
%apply ( GIntBig *optional_GIntBig ) {(GIntBig*)};
%feature( "kwargs" ) ReadRaster1;
  CPLErr ReadRaster1( double xoff, double yoff, double xsize, double ysize,
                     void **buf,
                     int *buf_xsize = 0,
                     int *buf_ysize = 0,
                     int *buf_type = 0,
                     GIntBig *buf_pixel_space = 0,
                     GIntBig *buf_line_space = 0,
                     GDALRIOResampleAlg resample_alg = GRIORA_NearestNeighbour,
                     GDALProgressFunc callback = NULL,
                     void* callback_data=NULL) {
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;

    GIntBig buf_size = ComputeBandRasterIOSize( nxsize, nysize, GDALGetDataTypeSize( ntype ) / 8,
                                            pixel_space, line_space, FALSE ); 
    if (buf_size == 0)
    {
        *buf = NULL;
        return CE_Failure;
    }
%#if PY_VERSION_HEX >= 0x03000000 
    *buf = (void *)PyBytes_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyBytes_AsString( (PyObject *)*buf ); 
%#else 
    *buf = (void *)PyString_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyString_AsString( (PyObject *)*buf ); 
%#endif

    /* Should we clear the buffer in case there are hole in it ? */
    if( line_space != 0 && pixel_space != 0 && line_space > pixel_space * nxsize )
    {
        memset(data, 0, buf_size);
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = resample_alg;
    sExtraArg.pfnProgress = callback;
    sExtraArg.pProgressData = callback_data;
    int nXOff = (int)(xoff + 0.5);
    int nYOff = (int)(yoff + 0.5);
    int nXSize = (int)(xsize + 0.5);
    int nYSize = (int)(ysize + 0.5);
    if( fabs(xoff-nXOff) > 1e-8 || fabs(yoff-nYOff) > 1e-8 ||
        fabs(xsize-nXSize) > 1e-8 || fabs(ysize-nYSize) > 1e-8 )
    {
        sExtraArg.bFloatingPointWindowValidity = TRUE;
        sExtraArg.dfXOff = xoff;
        sExtraArg.dfYOff = yoff;
        sExtraArg.dfXSize = xsize;
        sExtraArg.dfYSize = ysize;
    }

    CPLErr eErr = GDALRasterIOEx( self, GF_Read, nXOff, nYOff, nXSize, nYSize, 
                         (void *) data, nxsize, nysize, ntype, 
                         pixel_space, line_space, &sExtraArg ); 
    if (eErr == CE_Failure)
    {
        Py_DECREF((PyObject*)*buf);
        *buf = NULL;
    }
    return eErr;
  }
%clear (void **buf );
%clear (int*);
%clear (GIntBig*);

%apply ( void **outPythonObject ) { (void **buf ) };
%feature( "kwargs" ) ReadBlock;
  CPLErr ReadBlock( int xoff, int yoff, void **buf) {

    int nBlockXSize, nBlockYSize;
    GDALGetBlockSize(self, &nBlockXSize, &nBlockYSize);
    int nDataTypeSize = (GDALGetDataTypeSize(GDALGetRasterDataType(self)) / 8);
    GIntBig buf_size = (GIntBig)nBlockXSize * nBlockYSize * nDataTypeSize;

%#if PY_VERSION_HEX >= 0x03000000 
    *buf = (void *)PyBytes_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyBytes_AsString( (PyObject *)*buf ); 
%#else 
    *buf = (void *)PyString_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyString_AsString( (PyObject *)*buf ); 
%#endif
    CPLErr eErr = GDALReadBlock( self, xoff, yoff, (void *) data); 
    if (eErr == CE_Failure)
    {
        Py_DECREF((PyObject*)*buf);
        *buf = NULL;
    }
    return eErr;
  }
%clear (void **buf );


%pythoncode %{

  def ReadRaster(self, xoff = 0, yoff = 0, xsize = None, ysize = None,
                   buf_xsize = None, buf_ysize = None, buf_type = None,
                   buf_pixel_space = None, buf_line_space = None,
                   resample_alg = GRIORA_NearestNeighbour,
                   callback = None,
                   callback_data = None):

      if xsize is None:
          xsize = self.XSize
      if ysize is None:
          ysize = self.YSize

      return _gdal.Band_ReadRaster1(self, xoff, yoff, xsize, ysize,
                                    buf_xsize, buf_ysize, buf_type,
                                    buf_pixel_space, buf_line_space,
                                    resample_alg, callback, callback_data)

  def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_type=None, buf_obj=None,
                  resample_alg = GRIORA_NearestNeighbour,
                  callback = None,
                  callback_data = None):
      """ Reading a chunk of a GDAL band into a numpy array. The optional (buf_xsize,buf_ysize,buf_type)
      parameters should generally not be specified if buf_obj is specified. The array is returned"""

      import gdalnumeric

      return gdalnumeric.BandReadAsArray( self, xoff, yoff,
                                          win_xsize, win_ysize,
                                          buf_xsize, buf_ysize, buf_type, buf_obj,
                                          resample_alg = resample_alg,
                                          callback = callback,
                                          callback_data = callback_data)
    
  def WriteArray(self, array, xoff=0, yoff=0,
                 resample_alg = GRIORA_NearestNeighbour,
                 callback = None,
                 callback_data = None):
      import gdalnumeric

      return gdalnumeric.BandWriteArray( self, array, xoff, yoff,
                                         resample_alg = resample_alg,
                                         callback = callback,
                                         callback_data = callback_data )

  def GetVirtualMemArray(self, eAccess = gdalconst.GF_Read, xoff=0, yoff=0,
                         xsize=None, ysize=None, bufxsize=None, bufysize=None,
                         datatype = None,
                         cache_size = 10 * 1024 * 1024, page_size_hint = 0,
                         options = None):
        """Return a NumPy array for the band, seen as a virtual memory mapping.
           An element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        import gdalnumeric
        if xsize is None:
            xsize = self.XSize
        if ysize is None:
            ysize = self.YSize
        if bufxsize is None:
            bufxsize = self.XSize
        if bufysize is None:
            bufysize = self.YSize
        if datatype is None:
            datatype = self.DataType
        if options is None:
            virtualmem = self.GetVirtualMem(eAccess,xoff,yoff,xsize,ysize,bufxsize,bufysize,datatype,cache_size,page_size_hint)
        else:
            virtualmem = self.GetVirtualMem(eAccess,xoff,yoff,xsize,ysize,bufxsize,bufysize,datatype,cache_size,page_size_hint,options)
        return gdalnumeric.VirtualMemGetArray( virtualmem )

  def GetVirtualMemAutoArray(self, eAccess = gdalconst.GF_Read, options = None):
        """Return a NumPy array for the band, seen as a virtual memory mapping.
           An element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        import gdalnumeric
        if options is None:
            virtualmem = self.GetVirtualMemAuto(eAccess)
        else:
            virtualmem = self.GetVirtualMemAuto(eAccess,options)
        return gdalnumeric.VirtualMemGetArray( virtualmem )

  def GetTiledVirtualMemArray(self, eAccess = gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, tilexsize=256, tileysize=256,
                           datatype = None,
                           cache_size = 10 * 1024 * 1024, options = None):
        """Return a NumPy array for the band, seen as a virtual memory mapping with
           a tile organization.
           An element is accessed with array[tiley][tilex][y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        import gdalnumeric
        if xsize is None:
            xsize = self.XSize
        if ysize is None:
            ysize = self.YSize
        if datatype is None:
            datatype = self.DataType
        if options is None:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,cache_size)
        else:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,cache_size,options)
        return gdalnumeric.VirtualMemGetArray( virtualmem )

  def __get_array_interface__(self):
      shape = [1, self.XSize, self.YSize]
      
%}
}

%extend GDALDatasetShadow {
%feature("kwargs") ReadRaster1;
%apply (int *optional_int) { (GDALDataType *buf_type) };
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply ( void **outPythonObject ) { (void **buf ) };
%apply ( int *optional_int ) {(int*)};
%apply ( GIntBig *optional_GIntBig ) {(GIntBig*)};
CPLErr ReadRaster1(  int xoff, int yoff, int xsize, int ysize,
                    void **buf,
                    int *buf_xsize = 0, int *buf_ysize = 0,
                    GDALDataType *buf_type = 0,
                    int band_list = 0, int *pband_list = 0,
                    GIntBig* buf_pixel_space = 0, GIntBig* buf_line_space = 0, GIntBig* buf_band_space = 0,
                    GDALRIOResampleAlg resample_alg = GRIORA_NearestNeighbour,
                    GDALProgressFunc callback = NULL,
                    void* callback_data=NULL )
{
    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype;
    if ( buf_type != 0 ) {
      ntype = (GDALDataType) *buf_type;
    } else {
      int lastband = GDALGetRasterCount( self ) - 1;
      if (lastband < 0)
      {
          *buf = NULL;
          return CE_Failure;
      }
      ntype = GDALGetRasterDataType( GDALGetRasterBand( self, lastband ) );
    }

    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;
    GIntBig band_space = (buf_band_space == 0) ? 0 : *buf_band_space;

    int ntypesize = GDALGetDataTypeSize( ntype ) / 8;
    GIntBig buf_size = ComputeDatasetRasterIOSize (nxsize, nysize, ntypesize,
                                               band_list ? band_list : GDALGetRasterCount(self), pband_list, band_list,
                                               pixel_space, line_space, band_space, FALSE);
    if (buf_size == 0)
    {
        *buf = NULL;
        return CE_Failure;
    }

%#if PY_VERSION_HEX >= 0x03000000 
    *buf = (void *)PyBytes_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyBytes_AsString( (PyObject *)*buf ); 
%#else 
    *buf = (void *)PyString_FromStringAndSize( NULL, buf_size ); 
    if (*buf == NULL)
    {
        if( !bUseExceptions ) PyErr_Clear();
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return CE_Failure;
    }
    char *data = PyString_AsString( (PyObject *)*buf ); 
%#endif

    /* Should we clear the buffer in case there are hole in it ? */
    if( line_space != 0 && pixel_space != 0 && line_space > pixel_space * nxsize )
    {
        memset(data, 0, buf_size);
    }
    else if( band_list > 1 && band_space != 0 )
    {
        if( line_space != 0 && band_space > line_space * nysize )
            memset(data, 0, buf_size);
        else if( pixel_space != 0 && band_space < pixel_space &&
                 pixel_space != GDALGetRasterCount(self) * ntypesize )
            memset(data, 0, buf_size);
    }

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    sExtraArg.eResampleAlg = resample_alg;
    sExtraArg.pfnProgress = callback;
    sExtraArg.pProgressData = callback_data;
    CPLErr eErr = GDALDatasetRasterIOEx(self, GF_Read, xoff, yoff, xsize, ysize,
                               (void*) data, nxsize, nysize, ntype,
                               band_list, pband_list, pixel_space, line_space, band_space,
                               &sExtraArg );
    if (eErr == CE_Failure)
    {
        Py_DECREF((PyObject*)*buf);
        *buf = NULL;
    }
    return eErr;
}

%clear (GDALDataType *buf_type);
%clear (int band_list, int *pband_list );
%clear (void **buf );
%clear (int*);
%clear (GIntBig*);

%pythoncode %{

    def ReadAsArray(self, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None,
                    buf_xsize = None, buf_ysize = None, buf_type = None,
                    resample_alg = GRIORA_NearestNeighbour,
                    callback = None,
                    callback_data = None):
        """ Reading a chunk of a GDAL band into a numpy array. The optional (buf_xsize,buf_ysize,buf_type)
        parameters should generally not be specified if buf_obj is specified. The array is returned"""

        import gdalnumeric
        return gdalnumeric.DatasetReadAsArray( self, xoff, yoff, xsize, ysize, buf_obj,
                                               buf_xsize, buf_ysize, buf_type,
                                               resample_alg = resample_alg,
                                               callback = callback,
                                               callback_data = callback_data )

    def WriteRaster(self, xoff, yoff, xsize, ysize,
                    buf_string,
                    buf_xsize = None, buf_ysize = None, buf_type = None,
                    band_list = None,
                    buf_pixel_space = None, buf_line_space = None, buf_band_space = None ):

        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;
        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if buf_type is None:
            buf_type = self.GetRasterBand(1).DataType

        return _gdal.Dataset_WriteRaster(self,
                 xoff, yoff, xsize, ysize,
                buf_string, buf_xsize, buf_ysize, buf_type, band_list,
                buf_pixel_space, buf_line_space, buf_band_space )

    def ReadRaster(self, xoff = 0, yoff = 0, xsize = None, ysize = None,
                   buf_xsize = None, buf_ysize = None, buf_type = None,
                   band_list = None,
                   buf_pixel_space = None, buf_line_space = None, buf_band_space = None,
                   resample_alg = GRIORA_NearestNeighbour,
                   callback = None,
                   callback_data = None):

        if xsize is None:
            xsize = self.RasterXSize
        if ysize is None:
            ysize = self.RasterYSize
        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if buf_xsize is None:
            buf_xsize = xsize;
        if buf_ysize is None:
            buf_ysize = ysize;

        if buf_type is None:
            buf_type = self.GetRasterBand(1).DataType;

        return _gdal.Dataset_ReadRaster1(self, xoff, yoff, xsize, ysize,
                                            buf_xsize, buf_ysize, buf_type,
                                            band_list, buf_pixel_space, buf_line_space, buf_band_space,
                                          resample_alg, callback, callback_data )

    def GetVirtualMemArray(self, eAccess = gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, bufxsize=None, bufysize=None,
                           datatype = None, band_list = None, band_sequential = True,
                           cache_size = 10 * 1024 * 1024, page_size_hint = 0,
                           options = None):
        """Return a NumPy array for the dataset, seen as a virtual memory mapping.
           If there are several bands and band_sequential = True, an element is
           accessed with array[band][y][x].
           If there are several bands and band_sequential = False, an element is
           accessed with array[y][x][band].
           If there is only one band, an element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        import gdalnumeric
        if xsize is None:
            xsize = self.RasterXSize
        if ysize is None:
            ysize = self.RasterYSize
        if bufxsize is None:
            bufxsize = self.RasterXSize
        if bufysize is None:
            bufysize = self.RasterYSize
        if datatype is None:
            datatype = self.GetRasterBand(1).DataType
        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if options is None:
            virtualmem = self.GetVirtualMem(eAccess,xoff,yoff,xsize,ysize,bufxsize,bufysize,datatype,band_list,band_sequential,cache_size,page_size_hint)
        else:
            virtualmem = self.GetVirtualMem(eAccess,xoff,yoff,xsize,ysize,bufxsize,bufysize,datatype,band_list,band_sequential,cache_size,page_size_hint, options)
        return gdalnumeric.VirtualMemGetArray( virtualmem )

    def GetTiledVirtualMemArray(self, eAccess = gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, tilexsize=256, tileysize=256,
                           datatype = None, band_list = None, tile_organization = gdalconst.GTO_BSQ,
                           cache_size = 10 * 1024 * 1024, options = None):
        """Return a NumPy array for the dataset, seen as a virtual memory mapping with
           a tile organization.
           If there are several bands and tile_organization = gdal.GTO_TIP, an element is
           accessed with array[tiley][tilex][y][x][band].
           If there are several bands and tile_organization = gdal.GTO_BIT, an element is
           accessed with array[tiley][tilex][band][y][x].
           If there are several bands and tile_organization = gdal.GTO_BSQ, an element is
           accessed with array[band][tiley][tilex][y][x].
           If there is only one band, an element is accessed with array[tiley][tilex][y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        import gdalnumeric
        if xsize is None:
            xsize = self.RasterXSize
        if ysize is None:
            ysize = self.RasterYSize
        if datatype is None:
            datatype = self.GetRasterBand(1).DataType
        if band_list is None:
            band_list = range(1,self.RasterCount+1)
        if options is None:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,band_list,tile_organization,cache_size)
        else:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,band_list,tile_organization,cache_size, options)
        return gdalnumeric.VirtualMemGetArray( virtualmem )

    def GetSubDatasets(self):
        sd_list = []
        
        sd = self.GetMetadata('SUBDATASETS')
        if sd is None:
            return sd_list

        i = 1
        while 'SUBDATASET_'+str(i)+'_NAME' in sd:
            sd_list.append( ( sd['SUBDATASET_'+str(i)+'_NAME'],
                              sd['SUBDATASET_'+str(i)+'_DESC'] ) )
            i = i + 1
        return sd_list

    def BeginAsyncReader(self, xoff, yoff, xsize, ysize, buf_obj = None, buf_xsize = None, buf_ysize = None, buf_type = None, band_list = None, options=[]):
        if band_list is None:
            band_list = range(1, self.RasterCount + 1)
        if buf_xsize is None:
            buf_xsize = 0;
        if buf_ysize is None:
            buf_ysize = 0;
        if buf_type is None:
            buf_type = GDT_Byte

        if buf_xsize <= 0:
            buf_xsize = xsize
        if buf_ysize <= 0:
            buf_ysize = ysize

        if buf_obj is None:
            from sys import version_info
            nRequiredSize = int(buf_xsize * buf_ysize * len(band_list) * (_gdal.GetDataTypeSize(buf_type) / 8))
            if version_info >= (3,0,0):
                buf_obj_ar = [ None ]
                exec("buf_obj_ar[0] = b' ' * nRequiredSize")
                buf_obj = buf_obj_ar[0]
            else:
                buf_obj = ' ' * nRequiredSize
        return _gdal.Dataset_BeginAsyncReader(self, xoff, yoff, xsize, ysize, buf_obj, buf_xsize, buf_ysize, buf_type, band_list,  0, 0, 0, options)

    def GetLayer(self,iLayer=0):
        """Return the layer given an index or a name"""
        if isinstance(iLayer, str):
            return self.GetLayerByName(str(iLayer))
        elif isinstance(iLayer, int):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))

    def DeleteLayer(self, value):
        """Deletes the layer given an index or layer name"""
        if isinstance(value, str):
            for i in range(self.GetLayerCount()):
                name = self.GetLayer(i).GetName()
                if name == value:
                    return _gdal.Dataset_DeleteLayer(self, i)
            raise ValueError("Layer %s not found to delete" % value)
        elif isinstance(value, int):
            return _gdal.Dataset_DeleteLayer(self, value)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(value))
%}
}

%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata( self, domain = '' ):
    if domain[:4] == 'xml:':
      return self.GetMetadata_List( domain )
    return self.GetMetadata_Dict( domain )
%}
}

%extend GDALRasterAttributeTableShadow {
%pythoncode %{
  def WriteArray(self, array, field, start=0):
      import gdalnumeric

      return gdalnumeric.RATWriteArray(self, array, field, start)

  def ReadAsArray(self, field, start=0, length=None):
      import gdalnumeric

      return gdalnumeric.RATReadArray(self, field, start, length)
%}
}

%include "callback.i"


%pythoncode %{

def InfoOptions(options = [], format = 'text', deserialize = True,
         computeMinMax = False, reportHistograms = False, reportProj4 = False,
         stats = False, approxStats = False, computeChecksum = False,
         showGCPs = True, showMetadata = True, showRAT = True, showColorTable = True,
         listMDD = False, showFileList = True, allMetadata = False,
         extraMDDomains = None):
    """ Create a InfoOptions() object that can be passed to gdal.Info()
        options can be be an array of strings, a string or let empty and filled from other keywords."""
    import copy

    if type(options) == type(''):
        new_options = ParseCommandLine(options)
        format = 'text'
        if '-json' in new_options:
            format = 'json'
    else:
        new_options = copy.copy(options)
        if format == 'json':
            new_options += ['-json']
        if computeMinMax:
            new_options += ['-mm']
        if reportHistograms:
            new_options += ['-hist']
        if reportProj4:
            new_options += ['-proj4']
        if stats:
            new_options += ['-stats']
        if approxStats:
            new_options += ['-approx_stats']
        if computeChecksum:
            new_options += ['-checksum']
        if not showGCPs:
            new_options += ['-nogcp']
        if not showMetadata:
            new_options += ['-nomd']
        if not showRAT:
            new_options += ['-norat']
        if not showColorTable:
            new_options += ['-noct']
        if listMDD:
            new_options += ['-listmdd']
        if not showFileList:
            new_options += ['-nofl']
        if allMetadata:
            new_options += ['-mdd', 'all']
        if extraMDDomains is not None:
            for mdd in extraMDDomains:
                new_options += ['-mdd', mdd]

    return (GDALInfoOptions(new_options), format, deserialize)

def Info(ds, **kwargs):
    """ Return information on a dataset.
        Arguments are :
          ds --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.InfoOptions(), string or array of strings
          other keywords arguments of gdal.InfoOptions()
        If options is provided as a gdal.InfoOptions() object, other keywords are ignored. """
    if not 'options' in kwargs or type(kwargs['options']) == type([]) or type(kwargs['options']) == type(''):
        (opts, format, deserialize) = InfoOptions(**kwargs)
    else:
        (opts, format, deserialize) = kwargs['options']
    if type(ds) == type(''):
        ds = Open(ds)
    ret = InfoInternal(ds, opts)
    if format == 'json' and deserialize:
        import json
        ret = json.loads(ret)
    return ret


def TranslateOptions(options = [], format = 'GTiff',
              outputType = GDT_Unknown, bandList = None, maskBand = None,
              oXSizePixel = 0, oYSizePixel = 0, oXSizePct = 0.0, oYSizePct = 0.0,
              xRes = 0.0, yRes = 0.0,
              creationOptions = None, srcWin = None, projWin = None, projWinSRS = None, strict = False,
              unscale = False, scaleParams = None, exponents = None,
              assignedULLR = None, metadataOptions = None,
              outputSRS = None, GCPs = None,
              noData = None, rgbExpand = None,
              stats = False, rat = True, resampleAlg = None,
              callback = None, callback_data = None):
    """ Create a TranslateOptions() object that can be passed to gdal.Translate()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdal.GDT_Byte, etc...)
          bandList --- array of band numbers (index start at 1)
          maskBand --- mask band to generate or not ("none", "auto", "mask", 1, ...)
          oXSizePixel --- width of the output raster in pixel
          oYSizePixel --- height of the output raster in pixel
          oXSizePct --- width of the output raster in percentage (100 = original size)
          oYSizePct --- height of the output raster in percentage (100 = original size)
          xRes --- output horizontal resolution
          yRes --- output vertical resolution
          creationOptions --- list of creation options
          srcWin --- subwindow in pixels to extract: [left_x, top_y, width, height]
          projWin --- subwindow in projected coordinates to extract: [ulx, uly, lrx, lry]
          projWinSRS --- SRS in which projWin is expressed
          strict --- strict mode
          unscale --- unscale values with scale and offset metadata
          scaleParams --- list of scale parameters, each of the form [src_min,src_max] or [src_min,src_max,dst_min,dst_max]
          exponents --- list of exponentation parameters
          assignedULLR --- assigned output bounds: [ulx, uly, lrx, lry]
          metadataOptions --- list of metadata options
          outputSRS --- assigned output SRS
          GCPs --- list of GCPs
          noData --- nodata value (or "none" to unset it)
          rgbExpand --- Color palette expansion mode: "gray", "rgb", "rgba"
          stats --- whether to calcule statistics
          rat --- whether to write source RAT
          resampleAlg --- resampling mode
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if type(options) == type(''):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if outputType != GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType) ]
        if maskBand != None:
            new_options += ['-mask', str(maskBand) ]
        if bandList != None:
            for b in bandList:
                new_options += ['-b', str(b) ]
        if oXSizePixel != 0 or oYSizePixel != 0:
            new_options += ['-outsize', str(oXSizePixel), str(oYSizePixel)]
        elif oXSizePct != 0 and oYSizePct != 0:
            new_options += ['-outsize', str(oXSizePct) + '%%', str(oYSizePct) + '%%']
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if srcWin is not None:
            new_options += ['-srcwin', str(srcWin[0]), str(srcWin[1]), str(srcWin[2]), str(srcWin[3])]
        if strict:
            new_options += ['-strict']
        if unscale:
            new_options += ['-unscale']
        if scaleParams:
            for scaleParam in scaleParams:
                new_options += ['-scale']
                for v in scaleParam:
                    new_options += [ str(v) ]
        if exponents:
            for exponent in exponents:
                new_options += ['-exponent', str(exponent)]
        if assignedULLR is not None:
            new_options += ['-a_ullr', str(assignedULLR[0]), str(assignedULLR[1]), str(assignedULLR[2]), str(assignedULLR[3])]
        if metadataOptions is not None:
            for opt in metadataOptions:
                new_options += ['-mo', opt ]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS) ]
        if GCPs is not None:
            for gcp in GCPs:
                new_options += ['-gcp', str(gcp.GCPPixel), str(gcp.GCPLine), str(gcp.GCPX), str(gcp.GCPY), str(gcp.GCPZ) ]
        if projWin is not None:
            new_options += ['-projwin', str(projWin[0]), str(projWin[1]), str(projWin[2]), str(projWin[3])]
        if projWinSRS is not None:
            new_options += ['-projwin_srs', str(projWinSRS) ]
        if noData is not None:
            new_options += ['-a_nodata', str(noData) ]
        if rgbExpand is not None:
            new_options += ['-expand', str(rgbExpand) ]
        if stats:
            new_options += ['-stats']
        if not rat:
            new_options += ['-norat']
        if resampleAlg is not None:
            if resampleAlg == GRA_NearestNeighbour:
                new_options += ['-r', 'near']
            elif resampleAlg == GRA_Bilinear:
                new_options += ['-r', 'bilinear']
            elif resampleAlg == GRA_Cubic:
                new_options += ['-r', 'cubic']
            elif resampleAlg == GRA_CubicSpline:
                new_options += ['-r', 'cubicspline']
            elif resampleAlg == GRA_Lanczos:
                new_options += ['-r', 'lanczos']
            elif resampleAlg == GRRA_Average:
                new_options += ['-r', 'average']
            elif resampleAlg == GRA_Mode:
                new_options += ['-r', 'mode']
            else:
                new_options += ['-r', str(resampleAlg) ]
        if xRes != 0 and yRes != 0:
            new_options += ['-tr', str(xRes), str(yRes) ]

    return (GDALTranslateOptions(new_options), callback, callback_data)

def Translate(destName, srcDS, **kwargs):
    """ Convert a dataset.
        Arguments are :
          destName --- Output dataset name
          srcDS --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.InfoOptions(), string or array of strings
          other keywords arguments of gdal.TranslateOptions()
        If options is provided as a gdal.TranslateOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or type(kwargs['options']) == type(''):
        (opts, callback, callback_data) = TranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if type(srcDS) == type(''):
        srcDS = Open(srcDS)

    return TranslateInternal(destName, srcDS, opts, callback, callback_data)

def WarpOptions(options = [], format = 'GTiff', 
         minX = None, minY = None, maxX = None, maxY = None,
         xRes = None, yRes = None, targetAlignedPixels = False,
         oXSizePixel = 0, oYSizePixel = 0,
         srcSRS = None, dstSRS = None,
         enableSrcAlpha = False, enableDstAlpha = False, 
         warpOptions = None, errorThreshold = None,
         warpMemoryLimit = None, creationOptions = None, outputType = GDT_Unknown,
         workingType = GDT_Unknown, resampleAlg = None,
         srcNodata = None, dstNodata = None, multithread = False,
         tps = False, rpc = False, geoloc = False, polynomialOrder = None,
         transformerOptions = None, cutlineDSName = None,
         cutlineLayer = None, cutlineWhere = None, cutlineSQL = None, cropToCutline = False,
         copyMetadata = True, MDConflictValue = None,
         setColorInterpretation = False,
         callback = None, callback_data = None):
    """ Create a WarpOptions() object that can be passed to gdal.Warp()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          minX, minY, maxX, maxY --- output bounds in target SRS
          xRes, yRes --- output resolution in target SRS
          oXSizePixel --- width of the output raster in pixel
          oYSizePixel --- height of the output raster in pixel
          srcSRS --- source SRS
          dstSRS --- output SRS
          targetAlignedPixels --- whether to force output bounds to be multiple of output resolution
          enableSrcAlpha --- whether to force the last band of the input dataset to be considered as an alpha band
          enableDstAlpha --- whether to force the creation of an output alpha band
          outputType --- output type (gdal.GDT_Byte, etc...)
          workingType --- working type (gdal.GDT_Byte, etc...)
          warpOptions --- list of warping options
          errorThreshold --- error threshold for approximation transformer (in pixels)
          warpMemoryLimit --- size of working buffer in bytes
          resampleAlg --- resampling mode
          creationOptions --- list of creation options
          srcNodata --- source nodata value(s)
          dstNodata --- output nodata value(s)
          multithread --- whether to multithread computation and I/O operations
          tps --- whether to use Thin Plate Spline GCP transformer
          rpc --- whether to use RPC transformer
          geoloc --- whether to use GeoLocation array transformer
          polynomialOrder --- order of polynomial GCP interpolation
          transformerOptions --- list of transformer options
          cutlineDSName --- cutline dataset name
          cutlineLayer --- cutline layer name
          cutlineWhere --- cutline WHERE clause
          cutlineSQL --- cutline SQL statement
          cropToCutline --- whether to use cutline extent for output bounds
          copyMetadata --- whether to copy source metadata
          MDConflictValue --- metadata data conflict value
          setColorInterpretation --- whether to force color interpretation of input bands to output bands
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if type(options) == type(''):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if outputType != GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType) ]
        if workingType != GDT_Unknown:
            new_options += ['-wt', GetDataTypeName(workingType) ]
        if minX is not None and minY is not None and maxX is not None and maxY is not None:
            new_options += ['-te', str(minX), str(minY), str(maxX), str(maxY) ]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', str(xRes), str(yRes) ]
        if oXSizePixel != 0 or oYSizePixel != 0:
            new_options += ['-ts', str(oXSizePixel), str(oYSizePixel)]
        if srcSRS is not None:
            new_options += ['-s_srs', str(srcSRS) ]
        if dstSRS is not None:
            new_options += ['-t_srs', str(dstSRS) ]
        if targetAlignedPixels:
            new_options += ['-tap']
        if enableSrcAlpha:
            new_options += ['-srcalpha']
        if enableDstAlpha:
            new_options += ['-dstalpha']
        if warpOptions is not None:
            for opt in warpOptions:
                new_options += ['-wo', str(opt)]
        if errorThreshold is not None:
            new_options += ['-et', str(errorThreshold)]
        if resampleAlg is not None:
            if resampleAlg == GRIORA_NearestNeighbour:
                new_options += ['-r', 'near']
            elif resampleAlg == GRIORA_Bilinear:
                new_options += ['-rb']
            elif resampleAlg == GRIORA_Cubic:
                new_options += ['-rc']
            elif resampleAlg == GRIORA_CubicSpline:
                new_options += ['-rcs']
            elif resampleAlg == GRIORA_Lanczos:
                new_options += ['-r', 'lanczos']
            elif resampleAlg == GRIORA_Average:
                new_options += ['-r', 'average']
            elif resampleAlg == GRIORA_Mode:
                new_options += ['-r', 'mode']
            elif resampleAlg == GRIORA_Gauss:
                new_options += ['-r', 'gauss']
            else:
                new_options += ['-r', str(resampleAlg) ]
        if warpMemoryLimit is not None:
            new_options += ['-wm', str(warpMemoryLimit) ]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if srcNodata is not None:
            new_options += ['-srcnodata', str(srcNodata) ]
        if dstNodata is not None:
            new_options += ['-dstnodata', str(dstNodata) ]
        if multithread:
            new_options += ['-multi']
        if tps:
            new_options += ['-tps']
        if rpc:
            new_options += ['-rpc']
        if geoloc:
            new_options += ['-geoloc']
        if polynomialOrder is not None:
            new_options += ['-order', str(polynomialOrder)]
        if transformerOptions is not None:
            for opt in transformerOptions:
                new_options += ['-to', opt ]
        if cutlineDSName is not None:
            new_options += ['-cutline', str(cutlineDSName) ]
        if cutlineLayer is not None:
            new_options += ['-cl', str(cutlineLayer) ]
        if cutlineWhere is not None:
            new_options += ['-cwhere', str(cutlineWhere) ]
        if cutlineSQL is not None:
            new_options += ['-csql', str(cutlineSQL) ]
        if cropToCutline:
            new_options += ['-crop_to_cutline']
        if not copyMetadata:
            new_options += ['-nomd']
        if MDConflictValue:
            new_options += ['-cvmd', str(MDConflictValue) ]
        if setColorInterpretation:
            new_options += ['-setci']

    return (GDALWarpAppOptions(new_options), callback, callback_data)

def Warp(destNameOrDestDS, srcDSOrSrcDSTab, **kwargs):
    """ Warp one or several datasets.
        Arguments are :
          destNameOrDestDS --- Output dataset name or object
          srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.InfoOptions(), string or array of strings
          other keywords arguments of gdal.WarpOptions()
        If options is provided as a gdal.WarpOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or type(kwargs['options']) == type(''):
        (opts, callback, callback_data) = WarpOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if type(srcDSOrSrcDSTab) == type(''):
        srcDSTab = [Open(srcDSOrSrcDSTab)]
    elif type(srcDSOrSrcDSTab) == type([]):
        srcDSTab = []
        for elt in srcDSOrSrcDSTab:
            if type(elt) == type(''):
                srcDSTab.append(Open(elt))
            else:
                srcDSTab.append(elt)
    else:
        srcDSTab = [ srcDSOrSrcDSTab ]

    if type(destNameOrDestDS) == type(''):
        return wrapper_GDALWarpDestName(destNameOrDestDS, srcDSTab, opts, callback, callback_data)
    else:
        return wrapper_GDALWarpDestDS(destNameOrDestDS, srcDSTab, opts, callback, callback_data)

%}
