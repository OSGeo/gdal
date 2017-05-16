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

%{
#define MODULE_NAME           "gdal"
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
unsigned int wrapper_VSIFReadL( void **buf, unsigned int nMembSize, unsigned int nMembCount, VSILFILE *fp)
{
    size_t buf_size = static_cast<size_t>(nMembSize) * nMembCount;
    if( buf_size > 0xFFFFFFFFU )
   {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big request");
        *buf = NULL;
        return 0;
    }

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
    size_t nRet = (size_t)VSIFReadL( data, nMembSize, nMembCount, fp );
    if (nRet * (size_t)nMembSize < buf_size)
    {
        _PyBytes_Resize(&o, nRet * nMembSize);
        *buf = o;
    }
    return static_cast<unsigned int>(nRet);
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
    size_t nRet = (size_t)VSIFReadL( data, nMembSize, nMembCount, fp );
    if (nRet * (size_t)nMembSize < buf_size)
    {
        _PyString_Resize(&o, nRet * nMembSize);
        *buf = o;
    }
    return static_cast<unsigned int>(nRet);
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
    int nxsize = (buf_xsize==0) ? static_cast<int>(xsize) : *buf_xsize;
    int nysize = (buf_ysize==0) ? static_cast<int>(ysize) : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : (GDALDataType)*buf_type;
    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;

    size_t buf_size = static_cast<size_t>(
        ComputeBandRasterIOSize( nxsize, nysize,
                                 GDALGetDataTypeSize( ntype ) / 8,
                                 pixel_space, line_space, FALSE ) );
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
    size_t buf_size = static_cast<size_t>(nBlockXSize) *
                                                nBlockYSize * nDataTypeSize;

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

  def ComputeStatistics(self, approx_ok):
    """ComputeStatistics(Band self, bool approx_ok, GDALProgressFunc callback=0, void * callback_data=None) -> CPLErr"""

    # For backward compatibility. New SWIG has stricter typing and really
    # enforces bool
    if approx_ok == 0:
        approx_ok = False
    elif approx_ok == 1:
        approx_ok = True

    return _gdal.Band_ComputeStatistics(self, approx_ok)


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
    size_t buf_size = static_cast<size_t>(
        ComputeDatasetRasterIOSize (nxsize, nysize, ntypesize,
                                    band_list ? band_list :
                                        GDALGetRasterCount(self),
                                    pband_list, band_list,
                                    pixel_space, line_space, band_space,
                                    FALSE));
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

def _is_str_or_unicode(o):
    return isinstance(o, str) or str(type(o)) == "<type 'unicode'>"

def InfoOptions(options = [], format = 'text', deserialize = True,
         computeMinMax = False, reportHistograms = False, reportProj4 = False,
         stats = False, approxStats = False, computeChecksum = False,
         showGCPs = True, showMetadata = True, showRAT = True, showColorTable = True,
         listMDD = False, showFileList = True, allMetadata = False,
         extraMDDomains = None):
    """ Create a InfoOptions() object that can be passed to gdal.Info()
        options can be be an array of strings, a string or let empty and filled from other keywords."""
    import copy

    if _is_str_or_unicode(options):
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
    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, format, deserialize) = InfoOptions(**kwargs)
    else:
        (opts, format, deserialize) = kwargs['options']
    if _is_str_or_unicode(ds):
        ds = Open(ds)
    ret = InfoInternal(ds, opts)
    if format == 'json' and deserialize:
        import json
        ret = json.loads(ret)
    return ret


def TranslateOptions(options = [], format = 'GTiff',
              outputType = GDT_Unknown, bandList = None, maskBand = None,
              width = 0, height = 0, widthPct = 0.0, heightPct = 0.0,
              xRes = 0.0, yRes = 0.0,
              creationOptions = None, srcWin = None, projWin = None, projWinSRS = None, strict = False,
              unscale = False, scaleParams = None, exponents = None,
              outputBounds = None, metadataOptions = None,
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
          width --- width of the output raster in pixel
          height --- height of the output raster in pixel
          widthPct --- width of the output raster in percentage (100 = original width)
          heightPct --- height of the output raster in percentage (100 = original height)
          xRes --- output horizontal resolution
          yRes --- output vertical resolution
          creationOptions --- list of creation options
          srcWin --- subwindow in pixels to extract: [left_x, top_y, width, height]
          projWin --- subwindow in projected coordinates to extract: [ulx, uly, lrx, lry]
          projWinSRS --- SRS in which projWin is expressed
          strict --- strict mode
          unscale --- unscale values with scale and offset metadata
          scaleParams --- list of scale parameters, each of the form [src_min,src_max] or [src_min,src_max,dst_min,dst_max]
          exponents --- list of exponentiation parameters
          outputBounds --- assigned output bounds: [ulx, uly, lrx, lry]
          metadataOptions --- list of metadata options
          outputSRS --- assigned output SRS
          GCPs --- list of GCPs
          noData --- nodata value (or "none" to unset it)
          rgbExpand --- Color palette expansion mode: "gray", "rgb", "rgba"
          stats --- whether to calculate statistics
          rat --- whether to write source RAT
          resampleAlg --- resampling mode
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
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
        if width != 0 or height != 0:
            new_options += ['-outsize', str(width), str(height)]
        elif widthPct != 0 and heightPct != 0:
            new_options += ['-outsize', str(widthPct) + '%%', str(heightPct) + '%%']
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
        if outputBounds is not None:
            new_options += ['-a_ullr', str(outputBounds[0]), str(outputBounds[1]), str(outputBounds[2]), str(outputBounds[3])]
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
            elif resampleAlg == GRA_Average:
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
          options --- return of gdal.TranslateOptions(), string or array of strings
          other keywords arguments of gdal.TranslateOptions()
        If options is provided as a gdal.TranslateOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = TranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = Open(srcDS)

    return TranslateInternal(destName, srcDS, opts, callback, callback_data)

def WarpOptions(options = [], format = 'GTiff',
         outputBounds = None,
         outputBoundsSRS = None,
         xRes = None, yRes = None, targetAlignedPixels = False,
         width = 0, height = 0,
         srcSRS = None, dstSRS = None,
         srcAlpha = False, dstAlpha = False,
         warpOptions = None, errorThreshold = None,
         warpMemoryLimit = None, creationOptions = None, outputType = GDT_Unknown,
         workingType = GDT_Unknown, resampleAlg = None,
         srcNodata = None, dstNodata = None, multithread = False,
         tps = False, rpc = False, geoloc = False, polynomialOrder = None,
         transformerOptions = None, cutlineDSName = None,
         cutlineLayer = None, cutlineWhere = None, cutlineSQL = None, cutlineBlend = None, cropToCutline = False,
         copyMetadata = True, metadataConflictValue = None,
         setColorInterpretation = False,
         callback = None, callback_data = None):
    """ Create a WarpOptions() object that can be passed to gdal.Warp()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputBounds --- output bounds as (minX, minY, maxX, maxY) in target SRS
          outputBoundsSRS --- SRS in which output bounds are expressed, in the case they are not expressed in dstSRS
          xRes, yRes --- output resolution in target SRS
          targetAlignedPixels --- whether to force output bounds to be multiple of output resolution
          width --- width of the output raster in pixel
          height --- height of the output raster in pixel
          srcSRS --- source SRS
          dstSRS --- output SRS
          srcAlpha --- whether to force the last band of the input dataset to be considered as an alpha band
          dstAlpha --- whether to force the creation of an output alpha band
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
          cutlineBlend --- cutline blend distance in pixels
          cropToCutline --- whether to use cutline extent for output bounds
          copyMetadata --- whether to copy source metadata
          metadataConflictValue --- metadata data conflict value
          setColorInterpretation --- whether to force color interpretation of input bands to output bands
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if outputType != GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType) ]
        if workingType != GDT_Unknown:
            new_options += ['-wt', GetDataTypeName(workingType) ]
        if outputBounds is not None:
            new_options += ['-te', str(outputBounds[0]), str(outputBounds[1]), str(outputBounds[2]), str(outputBounds[3]) ]
        if outputBoundsSRS is not None:
            new_options += ['-te_srs', str(outputBoundsSRS) ]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', str(xRes), str(yRes) ]
        if width != 0 or height != 0:
            new_options += ['-ts', str(width), str(height)]
        if srcSRS is not None:
            new_options += ['-s_srs', str(srcSRS) ]
        if dstSRS is not None:
            new_options += ['-t_srs', str(dstSRS) ]
        if targetAlignedPixels:
            new_options += ['-tap']
        if srcAlpha:
            new_options += ['-srcalpha']
        if dstAlpha:
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
        if cutlineBlend is not None:
            new_options += ['-cblend', str(cutlineBlend) ]
        if cropToCutline:
            new_options += ['-crop_to_cutline']
        if not copyMetadata:
            new_options += ['-nomd']
        if metadataConflictValue:
            new_options += ['-cvmd', str(metadataConflictValue) ]
        if setColorInterpretation:
            new_options += ['-setci']

    return (GDALWarpAppOptions(new_options), callback, callback_data)

def Warp(destNameOrDestDS, srcDSOrSrcDSTab, **kwargs):
    """ Warp one or several datasets.
        Arguments are :
          destNameOrDestDS --- Output dataset name or object
          srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.WarpOptions(), string or array of strings
          other keywords arguments of gdal.WarpOptions()
        If options is provided as a gdal.WarpOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = WarpOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDSOrSrcDSTab):
        srcDSTab = [Open(srcDSOrSrcDSTab)]
    elif type(srcDSOrSrcDSTab) == type([]):
        srcDSTab = []
        for elt in srcDSOrSrcDSTab:
            if _is_str_or_unicode(elt):
                srcDSTab.append(Open(elt))
            else:
                srcDSTab.append(elt)
    else:
        srcDSTab = [ srcDSOrSrcDSTab ]

    if _is_str_or_unicode(destNameOrDestDS):
        return wrapper_GDALWarpDestName(destNameOrDestDS, srcDSTab, opts, callback, callback_data)
    else:
        return wrapper_GDALWarpDestDS(destNameOrDestDS, srcDSTab, opts, callback, callback_data)


def VectorTranslateOptions(options = [], format = 'ESRI Shapefile',
         accessMode = None,
         srcSRS = None, dstSRS = None, reproject = True,
         SQLStatement = None, SQLDialect = None, where = None, selectFields = None,
         spatFilter = None, spatSRS = None,
         datasetCreationOptions = None,
         layerCreationOptions = None,
         layers = None,
         layerName = None,
         geometryType = None,
         dim = None,
         segmentizeMaxDist= None,
         zField = None,
         skipFailures = False,
         limit = None,
         callback = None, callback_data = None):
    """ Create a VectorTranslateOptions() object that can be passed to gdal.VectorTranslate()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("ESRI Shapefile", etc...)
          accessMode --- None for creation, 'update', 'append', 'overwrite'
          srcSRS --- source SRS
          dstSRS --- output SRS (with reprojection if reproject = True)
          reproject --- whether to do reprojection
          SQLStatement --- SQL statement to apply to the source dataset
          SQLDialect --- SQL dialect ('OGRSQL', 'SQLITE', ...)
          where --- WHERE clause to apply to source layer(s)
          selectFields --- list of fields to select
          spatFilter --- spatial filter as (minX, minY, maxX, maxY) bounding box
          spatSRS --- SRS in which the spatFilter is expressed. If not specified, it is assumed to be the one of the layer(s)
          datasetCreationOptions --- list of dataset creation options
          layerCreationOptions --- list of layer creation options
          layers --- list of layers to convert
          layerName --- output layer name
          geometryType --- output layer geometry type ('POINT', ....)
          dim --- output dimension ('XY', 'XYZ', 'XYM', 'XYZM', 'layer_dim')
          segmentizeMaxDist --- maximum distance between consecutive nodes of a line geometry
          zField --- name of field to use to set the Z component of geometries
          skipFailures --- whether to skip failures
          limit -- maximum number of features to read per layer
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-f', format]
        if srcSRS is not None:
            new_options += ['-s_srs', str(srcSRS) ]
        if dstSRS is not None:
            if reproject:
                new_options += ['-t_srs', str(dstSRS) ]
            else:
                new_options += ['-a_srs', str(dstSRS) ]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement) ]
        if SQLDialect is not None:
            new_options += ['-dialect', str(SQLDialect) ]
        if where is not None:
            new_options += ['-where', str(where) ]
        if accessMode is not None:
            if accessMode == 'update':
                new_options += ['-update']
            elif accessMode == 'append':
                new_options += ['-append']
            elif accessMode == 'overwrite':
                new_options += ['-overwrite']
            else:
                raise Exception('unhandled accessMode')
        if selectFields is not None:
            val = ''
            for item in selectFields:
                if len(val)>0:
                    val += ','
                val += item
            new_options += ['-select', val]
        if datasetCreationOptions is not None:
            for opt in datasetCreationOptions:
                new_options += ['-dsco', opt ]
        if layerCreationOptions is not None:
            for opt in layerCreationOptions:
                new_options += ['-lco', opt ]
        if layers is not None:
            if _is_str_or_unicode(layers):
                new_options += [ layers ]
            else:
                for lyr in layers:
                    new_options += [ lyr ]
        if segmentizeMaxDist is not None:
            new_options += ['-segmentize', str(segmentizeMaxDist) ]
        if spatFilter is not None:
            new_options += ['-spat', str(spatFilter[0]), str(spatFilter[1]), str(spatFilter[2]), str(spatFilter[3]) ]
        if spatSRS is not None:
            new_options += ['-spat_srs', str(spatSRS) ]
        if layerName is not None:
            new_options += ['-nln', layerName]
        if geometryType is not None:
            new_options += ['-nlt', geometryType]
        if dim is not None:
            new_options += ['-dim', dim]
        if zField is not None:
            new_options += ['-zfield', zField]
        if skipFailures:
            new_options += ['-skip']
        if limit is not None:
            new_options += ['-limit', str(limit)]
    if callback is not None:
        new_options += [ '-progress' ]

    return (GDALVectorTranslateOptions(new_options), callback, callback_data)

def VectorTranslate(destNameOrDestDS, srcDS, **kwargs):
    """ Convert one vector dataset
        Arguments are :
          destNameOrDestDS --- Output dataset name or object
          srcDS --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.VectorTranslateOptions(), string or array of strings
          other keywords arguments of gdal.VectorTranslateOptions()
        If options is provided as a gdal.VectorTranslateOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = VectorTranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = OpenEx(srcDS)

    if _is_str_or_unicode(destNameOrDestDS):
        return wrapper_GDALVectorTranslateDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALVectorTranslateDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)

def DEMProcessingOptions(options = [], colorFilename = None, format = 'GTiff',
              creationOptions = None, computeEdges = False, alg = 'Horn', band = 1,
              zFactor = None, scale = None, azimuth = None, altitude = None,
              combined = False, multiDirectional = False,
              slopeFormat = None, trigonometric = False, zeroForFlat = False,
              callback = None, callback_data = None):
    """ Create a DEMProcessingOptions() object that can be passed to gdal.DEMProcessing()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          colorFilename --- (mandatory for "color-relief") name of file that contains palette definition for the "color-relief" processing.
          format --- output format ("GTiff", etc...)
          creationOptions --- list of creation options
          computeEdges --- whether to compute values at raster edges.
          alg --- 'ZevenbergenThorne' or 'Horn'
          band --- source band number to use
          zFactor --- (hillshade only) vertical exaggeration used to pre-multiply the elevations.
          scale --- ratio of vertical units to horizontal.
          azimuth --- (hillshade only) azimuth of the light, in degrees. 0 if it comes from the top of the raster, 90 from the east, ... The default value, 315, should rarely be changed as it is the value generally used to generate shaded maps.
          altitude ---(hillshade only) altitude of the light, in degrees. 90 if the light comes from above the DEM, 0 if it is raking light.
          combined --- (hillshade only) whether to compute combined shading, a combination of slope and oblique shading.
          multiDirectional --- (hillshade only) whether to compute multi-directional shading
          slopeformat --- (slope only) "degree" or "percent".
          trigonometric --- (aspect only) whether to return trigonometric angle instead of azimuth. Thus 0deg means East, 90deg North, 180deg West, 270deg South.
          zeroForFlat --- (aspect only) whether to return 0 for flat areas with slope=0, instead of -9999.
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if computeEdges:
            new_options += ['-compute_edges' ]
        if alg ==  'ZevenbergenThorne':
            new_options += ['-alg', 'ZevenbergenThorne']
        new_options += ['-b', str(band) ]
        if zFactor is not None:
            new_options += ['-z', str(zFactor) ]
        if scale is not None:
            new_options += ['-s', str(scale) ]
        if azimuth is not None:
            new_options += ['-az', str(azimuth) ]
        if altitude is not None:
            new_options += ['-alt', str(altitude) ]
        if combined:
            new_options += ['-combined' ]
        if multiDirectional:
            new_options += ['-multidirectional' ]
        if slopeFormat == 'percent':
            new_options += ['-p' ]
        if trigonometric:
            new_options += ['-trigonometric' ]
        if zeroForFlat:
            new_options += ['-zero_for_flat' ]

    return (GDALDEMProcessingOptions(new_options), colorFilename, callback, callback_data)

def DEMProcessing(destName, srcDS, processing, **kwargs):
    """ Apply a DEM processing.
        Arguments are :
          destName --- Output dataset name
          srcDS --- a Dataset object or a filename
          processing --- one of "hillshade", "slope", "aspect", "color-relief", "TRI", "TPI", "Roughness"
        Keyword arguments are :
          options --- return of gdal.DEMProcessingOptions(), string or array of strings
          other keywords arguments of gdal.DEMProcessingOptions()
        If options is provided as a gdal.DEMProcessingOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, colorFilename, callback, callback_data) = DEMProcessingOptions(**kwargs)
    else:
        (opts, colorFilename, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = Open(srcDS)

    return DEMProcessingInternal(destName, srcDS, processing, colorFilename, opts, callback, callback_data)


def NearblackOptions(options = [], format = 'GTiff',
         creationOptions = None, white = False, colors = None,
         maxNonBlack = None, nearDist = None, setAlpha = False, setMask = False,
         callback = None, callback_data = None):
    """ Create a NearblackOptions() object that can be passed to gdal.Nearblack()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          creationOptions --- list of creation options
          white --- whether to search for nearly white (255) pixels instead of nearly black pixels.
          colors --- list of colors  to search for, e.g. ((0,0,0),(255,255,255)). The pixels that are considered as the collar are set to 0
          maxNonBlack --- number of non-black (or other searched colors specified with white / colors) pixels that can be encountered before the giving up search inwards. Defaults to 2.
          nearDist --- select how far from black, white or custom colors the pixel values can be and still considered near black, white or custom color.  Defaults to 15.
          setAlpha --- adds an alpha band if the output file.
          setMask --- adds a mask band to the output file.
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if white:
            new_options += ['-white']
        if colors is not None:
            for color in colors:
                color_str = ''
                for cpt in color:
                    if color_str != '':
                        color_str += ','
                    color_str += str(cpt)
                new_options += ['-color',color_str]
        if maxNonBlack is not None:
            new_options += ['-nb', str(maxNonBlack) ]
        if nearDist is not None:
            new_options += ['-near', str(nearDist) ]
        if setAlpha:
            new_options += ['-setalpha']
        if setMask:
            new_options += ['-setmask']

    return (GDALNearblackOptions(new_options), callback, callback_data)

def Nearblack(destNameOrDestDS, srcDS, **kwargs):
    """ Convert nearly black/white borders to exact value.
        Arguments are :
          destNameOrDestDS --- Output dataset name or object
          srcDS --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.NearblackOptions(), string or array of strings
          other keywords arguments of gdal.NearblackOptions()
        If options is provided as a gdal.NearblackOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = NearblackOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = OpenEx(srcDS)

    if _is_str_or_unicode(destNameOrDestDS):
        return wrapper_GDALNearblackDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALNearblackDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def GridOptions(options = [], format = 'GTiff',
              outputType = GDT_Unknown,
              width = 0, height = 0,
              creationOptions = None,
              outputBounds = None,
              outputSRS = None,
              noData = None,
              algorithm = None,
              layers = None,
              SQLStatement = None,
              where = None,
              spatFilter = None,
              zfield = None,
              z_increase = None,
              z_multiply = None,
              callback = None, callback_data = None):
    """ Create a GridOptions() object that can be passed to gdal.Grid()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdal.GDT_Byte, etc...)
          width --- width of the output raster in pixel
          height --- height of the output raster in pixel
          creationOptions --- list of creation options
          outputBounds --- assigned output bounds: [ulx, uly, lrx, lry]
          outputSRS --- assigned output SRS
          noData --- nodata value
          algorithm --- e.g "invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0"
          layers --- list of layers to convert
          SQLStatement --- SQL statement to apply to the source dataset
          where --- WHERE clause to apply to source layer(s)
          spatFilter --- spatial filter as (minX, minY, maxX, maxY) bounding box
          zfield --- Identifies an attribute field on the features to be used to get a Z value from. This value overrides Z value read from feature geometry record.
          z_increase --- Addition to the attribute field on the features to be used to get a Z value from. The addition should be the same unit as Z value. The result value will be Z value + Z increase value. The default value is 0.
          z_multiply - Multiplication ratio for Z field. This can be used for shift from e.g. foot to meters or from  elevation to deep. The result value will be (Z value + Z increase value) * Z multiply value.  The default value is 1.
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        new_options += ['-of', format]
        if outputType != GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType) ]
        if width != 0 or height != 0:
            new_options += ['-outsize', str(width), str(height)]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if outputBounds is not None:
            new_options += ['-txe', str(outputBounds[0]), str(outputBounds[2]), '-tye', str(outputBounds[1]), str(outputBounds[3])]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS) ]
        if algorithm is not None:
            new_options += ['-a', algorithm ]
        if layers is not None:
            if type(layers) == type(()) or type(layers) == type([]):
                for layer in layers:
                    new_options += ['-l', layer]
            else:
                new_options += ['-l', layers]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement) ]
        if where is not None:
            new_options += ['-where', str(where) ]
        if zfield is not None:
            new_options += ['-zfield', zfield ]
        if z_increase is not None:
            new_options += ['-z_increase', str(z_increase) ]
        if z_multiply is not None:
            new_options += ['-z_multiply', str(z_multiply) ]
        if spatFilter is not None:
            new_options += ['-spat', str(spatFilter[0]), str(spatFilter[1]), str(spatFilter[2]), str(spatFilter[3]) ]

    return (GDALGridOptions(new_options), callback, callback_data)

def Grid(destName, srcDS, **kwargs):
    """ Create raster from the scattered data.
        Arguments are :
          destName --- Output dataset name
          srcDS --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.GridOptions(), string or array of strings
          other keywords arguments of gdal.GridOptions()
        If options is provided as a gdal.GridOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = GridOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = OpenEx(srcDS, OF_VECTOR)

    return GridInternal(destName, srcDS, opts, callback, callback_data)

def RasterizeOptions(options = [], format = None,
         outputType = GDT_Unknown, 
         creationOptions = None, noData = None, initValues = None,
         outputBounds = None, outputSRS = None,
         width = None, height = None,
         xRes = None, yRes = None, targetAlignedPixels = False,
         bands = None, inverse = False, allTouched = False,
         burnValues = None, attribute = None, useZ = False, layers = None,
         SQLStatement = None, SQLDialect = None, where = None,
         callback = None, callback_data = None):
    """ Create a RasterizeOptions() object that can be passed to gdal.Rasterize()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdal.GDT_Byte, etc...)
          creationOptions --- list of creation options
          outputBounds --- assigned output bounds: [minx, miny, maxx, maxy]
          outputSRS --- assigned output SRS
          width --- width of the output raster in pixel
          height --- height of the output raster in pixel
          xRes, yRes --- output resolution in target SRS
          targetAlignedPixels --- whether to force output bounds to be multiple of output resolution
          noData --- nodata value
          initValues --- Value or list of values to pre-initialize the output image bands with.  However, it is not marked as the nodata value in the output file.  If only one value is given, the same value is used in all the bands.
          bands --- list of output bands to burn values into
          inverse --- whether to invert rasterization, i.e. burn the fixed burn value, or the burn value associated  with the first feature into all parts of the image not inside the provided a polygon.
          allTouched -- whether to enable the ALL_TOUCHED rasterization option so that all pixels touched by lines or polygons will be updated, not just those on the line render path, or whose center point is within the polygon.
          burnValues -- list of fixed values to burn into each band for all objects. Excusive with attribute.
          attribute --- identifies an attribute field on the features to be used for a burn-in value. The value will be burned into all output bands. Excusive with burnValues.
          useZ --- whether to indicate that a burn value should be extracted from the "Z" values of the feature. These values are added to the burn value given by burnValues or attribute if provided. As of now, only points and lines are drawn in 3D.
          layers --- list of layers from the datasource that will be used for input features.
          SQLStatement --- SQL statement to apply to the source dataset
          SQLDialect --- SQL dialect ('OGRSQL', 'SQLITE', ...)
          where --- WHERE clause to apply to source layer(s)
          callback --- callback method
          callback_data --- user data for callback
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if outputType != GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType) ]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt ]
        if bands is not None:
            for b in bands:
                new_options += ['-b', str(b) ]
        if noData is not None:
            new_options += ['-a_nodata', str(noData) ]
        if initValues is not None:
            if type(initValues) == type(()) or type(initValues) == type([]):
                for val in initValues:
                    new_options += ['-init', str(val) ]
            else:
                new_options += ['-init', str(initValues) ]
        if outputBounds is not None:
            new_options += ['-te', str(outputBounds[0]), str(outputBounds[1]), str(outputBounds[2]), str(outputBounds[3])]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS) ]
        if width is not None and height is not None:
            new_options += ['-ts', str(width), str(height)]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', str(xRes), str(yRes)]
        if targetAlignedPixels:
            new_options += ['-tap']
        if inverse:
            new_options += ['-i']
        if allTouched:
            new_options += ['-at']
        if burnValues is not None:
            if attribute is not None:
                raise Exception('burnValues and attribute option are exclusive.')
            if type(burnValues) == type(()) or type(burnValues) == type([]):
                for val in burnValues:
                    new_options += ['-burn', str(val) ]
            else:
                new_options += ['-burn', str(burnValues) ]
        if attribute is not None:
            new_options += ['-a', attribute]
        if useZ:
            new_options += ['-3d']
        if layers is not None:
            if type(layers) == type(()) or type(layers) == type([]):
                for layer in layers:
                    new_options += ['-l', layer]
            else:
                new_options += ['-l', layers]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement) ]
        if SQLDialect is not None:
            new_options += ['-dialect', str(SQLDialect) ]
        if where is not None:
            new_options += ['-where', str(where) ]

    return (GDALRasterizeOptions(new_options), callback, callback_data)

def Rasterize(destNameOrDestDS, srcDS, **kwargs):
    """ Burns vector geometries into a raster
        Arguments are :
          destNameOrDestDS --- Output dataset name or object
          srcDS --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.RasterizeOptions(), string or array of strings
          other keywords arguments of gdal.RasterizeOptions()
        If options is provided as a gdal.RasterizeOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = RasterizeOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if _is_str_or_unicode(srcDS):
        srcDS = OpenEx(srcDS)

    if _is_str_or_unicode(destNameOrDestDS):
        return wrapper_GDALRasterizeDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALRasterizeDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def BuildVRTOptions(options = [],
                    resolution = None,
                    outputBounds = None,
                    xRes = None, yRes = None,
                    targetAlignedPixels = None,
                    separate = None,
                    bandList = None,
                    addAlpha = None,
                    resampleAlg = None,
                    outputSRS = None,
                    allowProjectionDifference = None,
                    srcNodata = None,
                    VRTNodata = None,
                    hideNodata = None,
                    callback = None, callback_data = None):
    """ Create a BuildVRTOptions() object that can be passed to gdal.BuildVRT()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords..
          resolution --- 'highest', 'lowest', 'average', 'user'.
          outputBounds --- output bounds as (minX, minY, maxX, maxY) in target SRS.
          xRes, yRes --- output resolution in target SRS.
          targetAlignedPixels --- whether to force output bounds to be multiple of output resolution.
          separate --- whether each source file goes into a separate stacked band in the VRT band.
          bandList --- array of band numbers (index start at 1).
          addAlpha --- whether to add an alpha mask band to the VRT when the source raster have none.
          resampleAlg --- resampling mode.
          outputSRS --- assigned output SRS.
          allowProjectionDifference --- whether to accept input datasets have not the same projection. Note: they will *not* be reprojected.
          srcNodata --- source nodata value(s).
          VRTNodata --- nodata values at the VRT band level.
          hideNodata --- whether to make the VRT band not report the NoData value.
          callback --- callback method.
          callback_data --- user data for callback.
    """
    import copy

    if _is_str_or_unicode(options):
        new_options = ParseCommandLine(options)
    else:
        new_options = copy.copy(options)
        if resolution is not None:
            new_options += ['-resolution', str(resolution) ]
        if outputBounds is not None:
            new_options += ['-te', str(outputBounds[0]), str(outputBounds[1]), str(outputBounds[2]), str(outputBounds[3])]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', str(xRes), str(yRes)]
        if targetAlignedPixels:
            new_options += ['-tap']
        if separate:
            new_options += ['-separate']
        if bandList != None:
            for b in bandList:
                new_options += ['-b', str(b) ]
        if addAlpha:
            new_options += ['-addalpha']
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
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS) ]
        if allowProjectionDifference:
            new_options += ['-allow_projection_difference']
        if srcNodata is not None:
            new_options += ['-srcnodata', str(srcNodata) ]
        if VRTNodata is not None:
            new_options += ['-vrtnodata', str(VRTNodata) ]
        if hideNodata:
            new_options += ['-hidenodata']

    return (GDALBuildVRTOptions(new_options), callback, callback_data)

def BuildVRT(destName, srcDSOrSrcDSTab, **kwargs):
    """ Build a VRT from a list of datasets.
        Arguments are :
          destName --- Output dataset name
          srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.BuildVRTOptions(), string or array of strings
          other keywords arguments of gdal.BuildVRTOptions()
        If options is provided as a gdal.BuildVRTOptions() object, other keywords are ignored. """

    if not 'options' in kwargs or type(kwargs['options']) == type([]) or _is_str_or_unicode(kwargs['options']):
        (opts, callback, callback_data) = BuildVRTOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    srcDSTab = []
    srcDSNamesTab = []
    if _is_str_or_unicode(srcDSOrSrcDSTab):
        srcDSNamesTab = [ srcDSOrSrcDSTab ]
    elif type(srcDSOrSrcDSTab) == type([]):
        for elt in srcDSOrSrcDSTab:
            if _is_str_or_unicode(elt):
                srcDSNamesTab.append(elt)
            else:
                srcDSTab.append(elt)
        if len(srcDSTab) != 0 and len(srcDSNamesTab) != 0:
            raise Exception('Mix of names and dataset objects not supported')
    else:
        srcDSTab = [ srcDSOrSrcDSTab ]

    if len(srcDSTab) > 0:
        return BuildVRTInternalObjects(destName, srcDSTab, opts, callback, callback_data)
    else:
        return BuildVRTInternalNames(destName, srcDSNamesTab, opts, callback, callback_data)

%}
