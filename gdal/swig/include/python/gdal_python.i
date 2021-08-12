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

%{
static int getAlignment(GDALDataType ntype)
{
    switch(ntype)
    {
        case GDT_Unknown:
            break; // shouldn't happen
        case GDT_Byte:
            return 1;
        case GDT_Int16:
        case GDT_UInt16:
            return 2;
        case GDT_Int32:
        case GDT_UInt32:
        case GDT_Float32:
            return 4;
        case GDT_Float64:
            return 8;
        case GDT_CInt16:
            return 2;
        case GDT_CInt32:
        case GDT_CFloat32:
            return 4;
        case GDT_CFloat64:
            return 8;
        case GDT_TypeCount:
            break; // shouldn't happen
    }
    // shouldn't happen
    CPLAssert(false);
    return 1;
}

static bool readraster_acquirebuffer(void** buf,
                                     void*& inputOutputBuf,
                                     size_t buf_size,
                                     GDALDataType ntype,
                                     int bUseExceptions,
                                     char*& data,
                                     Py_buffer& view)
{
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;

    if( inputOutputBuf == Py_None )
        inputOutputBuf = NULL;

    if( inputOutputBuf )
    {
        if (PyObject_GetBuffer( (PyObject*)inputOutputBuf, &view,
                                PyBUF_SIMPLE | PyBUF_WRITABLE) == 0)
        {
            if( static_cast<GUIntBig>(view.len) < buf_size )
            {
                PyBuffer_Release(&view);
                SWIG_PYTHON_THREAD_END_BLOCK;
                CPLError(CE_Failure, CPLE_AppDefined,
                    "buf_obj length is " CPL_FRMT_GUIB " bytes. "
                    "It should be at least " CPL_FRMT_GUIB,
                    static_cast<GUIntBig>(view.len),
                    static_cast<GUIntBig>(buf_size));
                return false;
            }
            data = (char*)view.buf;
            if( (reinterpret_cast<uintptr_t>(data) % getAlignment(ntype)) != 0 )
            {
                PyBuffer_Release(&view);
                SWIG_PYTHON_THREAD_END_BLOCK;
                CPLError(CE_Failure, CPLE_AppDefined,
                         "buffer has not the appropriate alignment");
                return false;
            }
        }
        else
        {
            PyErr_Clear();
            SWIG_PYTHON_THREAD_END_BLOCK;
            CPLError(CE_Failure, CPLE_AppDefined,
                     "buf_obj is not a simple writable buffer");
            return false;
        }
    }
    else
    {
        *buf = (void *)PyByteArray_FromStringAndSize( NULL, buf_size );
        if (*buf == NULL)
        {
            *buf = Py_None;
            if( !bUseExceptions )
            {
                PyErr_Clear();
            }
            SWIG_PYTHON_THREAD_END_BLOCK;
            CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
            return false;
        }
        data = PyByteArray_AsString( (PyObject *)*buf );
    }
    SWIG_PYTHON_THREAD_END_BLOCK;
    return true;
}

static void readraster_releasebuffer(CPLErr eErr,
                                     void** buf,
                                     void* inputOutputBuf,
                                     Py_buffer& view)
{
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;

    if( inputOutputBuf )
        PyBuffer_Release(&view);

    if (eErr == CE_Failure)
    {
        if( inputOutputBuf == NULL )
            Py_DECREF((PyObject*)*buf);
        *buf = NULL;
    }
    else if( inputOutputBuf )
    {
        *buf = inputOutputBuf;
        Py_INCREF((PyObject*)*buf);
    }

    SWIG_PYTHON_THREAD_END_BLOCK;
}

%}

%pythoncode %{


  have_warned = 0
  def deprecation_warn(module, sub_package=None, new_module=None):
      global have_warned

      if have_warned == 1:
          return

      have_warned = 1
      if sub_package is None or sub_package == 'utils':
          sub_package = 'osgeo_utils'
      if new_module is None:
          new_module = module
      new_module = '{}.{}'.format(sub_package, new_module)

      from warnings import warn
      warn('{}.py was placed in a namespace, it is now available as {}' .format(module, new_module),
         DeprecationWarning)


  from osgeo.gdalconst import *
  from osgeo import gdalconst


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
    err = ComputeMedianCutPCT(src_ds.GetRasterBand(1),
                              src_ds.GetRasterBand(2),
                              src_ds.GetRasterBand(3),
                              256, ct)
    if err != 0:
        return err

    gtiff_driver = GetDriverByName('GTiff')
    if gtiff_driver is None:
        return 1

    dst_ds = gtiff_driver.Create(dst_filename,
                                 src_ds.RasterXSize, src_ds.RasterYSize)
    dst_ds.GetRasterBand(1).SetRasterColorTable(ct)

    err = DitherRGB2PCT(src_ds.GetRasterBand(1),
                        src_ds.GetRasterBand(2),
                        src_ds.GetRasterBand(3),
                        dst_ds.GetRasterBand(1),
                        ct)
    dst_ds = None
    src_ds = None

    return 0

  def listdir(path, recursionLevel = -1, options = []):
    """ Iterate over a directory.

        recursionLevel = -1 means unlimited level of recursion.
    """
    dir = OpenDir(path, recursionLevel, options)
    if not dir:
        raise OSError(path + ' does not exist')
    try:
        while True:
            entry = GetNextDirEntry(dir)
            if not entry:
                break
            yield entry
    finally:
        CloseDir(dir)
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
%apply Pointer NONNULL {VSILFILE* fp};
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

    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    *buf = (void *)PyByteArray_FromStringAndSize( NULL, buf_size );
    if (*buf == NULL)
    {
        *buf = Py_None;
        if( !bUseExceptions )
        {
            PyErr_Clear();
        }
        SWIG_PYTHON_THREAD_END_BLOCK;
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate result buffer");
        return 0;
    }
    PyObject* o = (PyObject*) *buf;
    char *data = PyByteArray_AsString(o);
    SWIG_PYTHON_THREAD_END_BLOCK;
    size_t nRet = (size_t)VSIFReadL( data, nMembSize, nMembCount, fp );
    if (nRet * (size_t)nMembSize < buf_size)
    {
        SWIG_PYTHON_THREAD_BEGIN_BLOCK;
        PyByteArray_Resize(o, nRet * nMembSize);
        SWIG_PYTHON_THREAD_END_BLOCK;
        *buf = o;
    }
    return static_cast<unsigned int>(nRet);
}
%}
%clear (void **buf );
%clear VSILFILE* fp;

/* -------------------------------------------------------------------- */
/*      VSIGetMemFileBuffer_unsafe()                                    */
/* -------------------------------------------------------------------- */

%rename (VSIGetMemFileBuffer_unsafe) wrapper_VSIGetMemFileBuffer;

%typemap(in, numinputs=0) (GByte **out, vsi_l_offset *length) (GByte *out = NULL, vsi_l_offset length) {
    $1 = &out;
    $2 = &length;
}

%typemap(argout) (GByte **out, vsi_l_offset *length) {
    if (*$1 == NULL) {
        if( bUseExceptions ) {
            PyErr_SetString(PyExc_RuntimeError, "Could not find path");
            $result = NULL;
        } else {
            CPLError(CE_Failure, CPLE_AppDefined, "Could not find path");
            $result = Py_None;
            Py_INCREF($result);
        }
    } else {
      do {
        $result = PyMemoryView_FromMemory(reinterpret_cast<char *>(*$1), *$2, PyBUF_READ);
        if ($result == NULL) {
            if( bUseExceptions ) {
                PyErr_SetString(PyExc_RuntimeError, "Could not allocate result buffer");
                $result = NULL;
            } else {
                CPLError(CE_Failure, CPLE_AppDefined, "Could not allocate result buffer");
                $result = Py_None;
                Py_INCREF($result);
            }
        }
      } while(0);
    }
}

%inline %{
void wrapper_VSIGetMemFileBuffer(const char *utf8_path, GByte **out, vsi_l_offset *length)
{
    *out = VSIGetMemFileBuffer(utf8_path, length, 0);
}
%}
%clear (GByte **out, vsi_l_offset *length);



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

  def serialize(self, with_Z=0):
    base = [gdalconst.CXT_Element,'GCP']
    base.append([gdalconst.CXT_Attribute,'Id',[gdalconst.CXT_Text,self.Id]])
    pixval = '%0.15E' % self.GCPPixel
    lineval = '%0.15E' % self.GCPLine
    xval = '%0.15E' % self.GCPX
    yval = '%0.15E' % self.GCPY
    zval = '%0.15E' % self.GCPZ
    base.append([gdalconst.CXT_Attribute,'Pixel',[gdalconst.CXT_Text,pixval]])
    base.append([gdalconst.CXT_Attribute,'Line',[gdalconst.CXT_Text,lineval]])
    base.append([gdalconst.CXT_Attribute,'X',[gdalconst.CXT_Text,xval]])
    base.append([gdalconst.CXT_Attribute,'Y',[gdalconst.CXT_Text,yval]])
    if with_Z:
        base.append([gdalconst.CXT_Attribute,'Z',[gdalconst.CXT_Text,zval]])
    return base
%} /* pythoncode */
}

%extend GDALRasterBandShadow {
%apply ( void *inPythonObject ) { (void* inputOutputBuf) };
%apply ( void **outPythonObject ) { (void **buf ) };
%apply ( int *optional_int ) {(int*)};
%apply ( GDALDataType *optional_GDALDataType ) {(GDALDataType*)};
%apply ( GIntBig *optional_GIntBig ) {(GIntBig*)};
%feature( "kwargs" ) ReadRaster1;
  CPLErr ReadRaster1( double xoff, double yoff, double xsize, double ysize,
                     void **buf,
                     int *buf_xsize = 0,
                     int *buf_ysize = 0,
                     GDALDataType *buf_type = 0,
                     GIntBig *buf_pixel_space = 0,
                     GIntBig *buf_line_space = 0,
                     GDALRIOResampleAlg resample_alg = GRIORA_NearestNeighbour,
                     GDALProgressFunc callback = NULL,
                     void* callback_data=NULL,
                     void* inputOutputBuf = NULL) {

    *buf = NULL;

    int nxsize = (buf_xsize==0) ? static_cast<int>(xsize) : *buf_xsize;
    int nysize = (buf_ysize==0) ? static_cast<int>(ysize) : *buf_ysize;
    GDALDataType ntype  = (buf_type==0) ? GDALGetRasterDataType(self)
                                        : *buf_type;
    GIntBig pixel_space = (buf_pixel_space == 0) ? 0 : *buf_pixel_space;
    GIntBig line_space = (buf_line_space == 0) ? 0 : *buf_line_space;

    size_t buf_size = static_cast<size_t>(
        ComputeBandRasterIOSize( nxsize, nysize,
                                 GDALGetDataTypeSize( ntype ) / 8,
                                 pixel_space, line_space, FALSE ) );
    if (buf_size == 0)
    {
        return CE_Failure;
    }

    char *data;
    Py_buffer view;
    if( !readraster_acquirebuffer(buf, inputOutputBuf, buf_size, ntype,
                                  bUseExceptions, data, view) )
    {
        return CE_Failure;
    }

    /* Should we clear the buffer in case there are hole in it ? */
    if( inputOutputBuf == NULL &&
        line_space != 0 && pixel_space != 0 && line_space > pixel_space * nxsize )
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
                         data, nxsize, nysize, ntype,
                         pixel_space, line_space, &sExtraArg );

    readraster_releasebuffer(eErr, buf, inputOutputBuf, view);

    return eErr;
  }
%clear (void **buf );
%clean (void* inputOutputBuf);
%clear (int*);
%clear (GIntBig*);
%clear (GDALDataType *);

%apply ( void *inPythonObject ) { (void* buf_obj) };
%apply ( void **outPythonObject ) { (void **buf ) };
%feature( "kwargs" ) ReadBlock;
  CPLErr ReadBlock( int xoff, int yoff, void **buf, void* buf_obj=NULL) {

    int nBlockXSize, nBlockYSize;
    GDALGetBlockSize(self, &nBlockXSize, &nBlockYSize);
    GDALDataType ntype = GDALGetRasterDataType(self);
    int nDataTypeSize = (GDALGetDataTypeSize(ntype) / 8);
    size_t buf_size = static_cast<size_t>(nBlockXSize) *
                                                nBlockYSize * nDataTypeSize;

    *buf = NULL;

    char *data;
    Py_buffer view;

    if( !readraster_acquirebuffer(buf, buf_obj, buf_size, ntype,
                                  bUseExceptions, data, view) )
    {
        return CE_Failure;
    }

    CPLErr eErr = GDALReadBlock( self, xoff, yoff, data);

    readraster_releasebuffer(eErr, buf, buf_obj, view);

    return eErr;
  }
%clear (void **buf );
%clear (void* buf_obj);


%pythoncode %{

  def ComputeStatistics(self, *args):
    """ComputeStatistics(Band self, bool approx_ok, GDALProgressFunc callback=0, void * callback_data=None) -> CPLErr"""

    # For backward compatibility. New SWIG has stricter typing and really
    # enforces bool
    approx_ok = args[0]
    if approx_ok == 0:
        approx_ok = False
    elif approx_ok == 1:
        approx_ok = True
    new_args = [approx_ok]
    for arg in args[1:]:
        new_args.append( arg )

    return _gdal.Band_ComputeStatistics(self, *new_args)


  def ReadRaster(self, xoff=0, yoff=0, xsize=None, ysize=None,
                 buf_xsize=None, buf_ysize=None, buf_type=None,
                 buf_pixel_space=None, buf_line_space=None,
                 resample_alg=gdalconst.GRIORA_NearestNeighbour,
                 callback=None,
                 callback_data=None,
                 buf_obj=None):

      if xsize is None:
          xsize = self.XSize
      if ysize is None:
          ysize = self.YSize

      return _gdal.Band_ReadRaster1(self, xoff, yoff, xsize, ysize,
                                    buf_xsize, buf_ysize, buf_type,
                                    buf_pixel_space, buf_line_space,
                                    resample_alg, callback, callback_data,
                                    buf_obj)

  def WriteRaster(self, xoff, yoff, xsize, ysize,
                  buf_string,
                  buf_xsize=None, buf_ysize=None, buf_type=None,
                  buf_pixel_space=None, buf_line_space=None ):

      if buf_xsize is None:
          buf_xsize = xsize
      if buf_ysize is None:
          buf_ysize = ysize

      # Redirect to numpy-friendly WriteArray() if buf_string is a numpy array
      # and other arguments are compatible
      if type(buf_string).__name__ == 'ndarray' and \
         buf_xsize == xsize and buf_ysize == ysize and buf_type is None and \
         buf_pixel_space is None and buf_line_space is None:
          return self.WriteArray(buf_string, xoff=xoff, yoff=yoff)

      if buf_type is None:
          buf_type = self.DataType

      return _gdal.Band_WriteRaster(self,
               xoff, yoff, xsize, ysize,
              buf_string, buf_xsize, buf_ysize, buf_type,
              buf_pixel_space, buf_line_space )

  def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_type=None, buf_obj=None,
                  resample_alg=gdalconst.GRIORA_NearestNeighbour,
                  callback=None,
                  callback_data=None):
      """ Reading a chunk of a GDAL band into a numpy array. The optional (buf_xsize,buf_ysize,buf_type)
      parameters should generally not be specified if buf_obj is specified. The array is returned"""

      from osgeo import gdal_array

      return gdal_array.BandReadAsArray(self, xoff, yoff,
                                         win_xsize, win_ysize,
                                         buf_xsize, buf_ysize, buf_type, buf_obj,
                                         resample_alg=resample_alg,
                                         callback=callback,
                                         callback_data=callback_data)

  def WriteArray(self, array, xoff=0, yoff=0,
                 resample_alg=gdalconst.GRIORA_NearestNeighbour,
                 callback=None,
                 callback_data=None):
      from osgeo import gdal_array

      return gdal_array.BandWriteArray(self, array, xoff, yoff,
                                        resample_alg=resample_alg,
                                        callback=callback,
                                        callback_data=callback_data)

  def GetVirtualMemArray(self, eAccess=gdalconst.GF_Read, xoff=0, yoff=0,
                         xsize=None, ysize=None, bufxsize=None, bufysize=None,
                         datatype=None,
                         cache_size = 10 * 1024 * 1024, page_size_hint = 0,
                         options=None):
        """Return a NumPy array for the band, seen as a virtual memory mapping.
           An element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        from osgeo import gdal_array
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
            virtualmem = self.GetVirtualMem(eAccess, xoff, yoff, xsize, ysize, bufxsize, bufysize, datatype, cache_size, page_size_hint)
        else:
            virtualmem = self.GetVirtualMem(eAccess, xoff, yoff, xsize, ysize, bufxsize, bufysize, datatype, cache_size, page_size_hint, options)
        return gdal_array.VirtualMemGetArray(virtualmem)

  def GetVirtualMemAutoArray(self, eAccess=gdalconst.GF_Read, options=None):
        """Return a NumPy array for the band, seen as a virtual memory mapping.
           An element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        from osgeo import gdal_array
        if options is None:
            virtualmem = self.GetVirtualMemAuto(eAccess)
        else:
            virtualmem = self.GetVirtualMemAuto(eAccess,options)
        return gdal_array.VirtualMemGetArray( virtualmem )

  def GetTiledVirtualMemArray(self, eAccess=gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, tilexsize=256, tileysize=256,
                           datatype=None,
                           cache_size = 10 * 1024 * 1024, options=None):
        """Return a NumPy array for the band, seen as a virtual memory mapping with
           a tile organization.
           An element is accessed with array[tiley][tilex][y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        from osgeo import gdal_array
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
        return gdal_array.VirtualMemGetArray( virtualmem )
%}
}

%extend GDALDatasetShadow {
%feature("kwargs") ReadRaster1;
%apply ( void *inPythonObject ) { (void* inputOutputBuf) };
%apply ( GDALDataType *optional_GDALDataType ) {(GDALDataType*)};
%apply (int nList, int *pList ) { (int band_list, int *pband_list ) };
%apply ( void **outPythonObject ) { (void **buf ) };
%apply ( int *optional_int ) {(int*)};
%apply ( GIntBig *optional_GIntBig ) {(GIntBig*)};
CPLErr ReadRaster1( double xoff, double yoff, double xsize, double ysize,
                    void **buf,
                    int *buf_xsize = 0, int *buf_ysize = 0,
                    GDALDataType *buf_type = 0,
                    int band_list = 0, int *pband_list = 0,
                    GIntBig* buf_pixel_space = 0, GIntBig* buf_line_space = 0, GIntBig* buf_band_space = 0,
                    GDALRIOResampleAlg resample_alg = GRIORA_NearestNeighbour,
                    GDALProgressFunc callback = NULL,
                    void* callback_data=NULL,
                    void* inputOutputBuf = NULL )
{
    *buf = NULL;

    int nxsize = (buf_xsize==0) ? xsize : *buf_xsize;
    int nysize = (buf_ysize==0) ? ysize : *buf_ysize;
    GDALDataType ntype;
    if ( buf_type != 0 ) {
      ntype = *buf_type;
    } else {
      int lastband = GDALGetRasterCount( self ) - 1;
      if (lastband < 0)
      {
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
        return CE_Failure;
    }

    char *data;
    Py_buffer view;

    if( !readraster_acquirebuffer(buf, inputOutputBuf, buf_size, ntype,
                                  bUseExceptions, data, view) )
    {
        return CE_Failure;
    }

    if( inputOutputBuf == NULL )
    {
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

    CPLErr eErr = GDALDatasetRasterIOEx(self, GF_Read, nXOff, nYOff, nXSize, nYSize,
                               data, nxsize, nysize, ntype,
                               band_list, pband_list, pixel_space, line_space, band_space,
                               &sExtraArg );

    readraster_releasebuffer(eErr, buf, inputOutputBuf, view);

    return eErr;
}

%clear (GDALDataType *);
%clear (int band_list, int *pband_list );
%clear (void **buf );
%clear (void *inputOutputBuf);
%clear (int*);
%clear (GIntBig*);

%pythoncode %{

    def ReadAsArray(self, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None,
                    buf_xsize=None, buf_ysize=None, buf_type=None,
                    resample_alg=gdalconst.GRIORA_NearestNeighbour,
                    callback=None,
                    callback_data=None,
                    interleave='band',
                    band_list=None):
        """ Reading a chunk of a GDAL band into a numpy array. The optional (buf_xsize,buf_ysize,buf_type)
        parameters should generally not be specified if buf_obj is specified. The array is returned"""

        from osgeo import gdal_array
        return gdal_array.DatasetReadAsArray(self, xoff, yoff, xsize, ysize, buf_obj,
                                              buf_xsize, buf_ysize, buf_type,
                                              resample_alg=resample_alg,
                                              callback=callback,
                                              callback_data=callback_data,
                                              interleave=interleave,
                                              band_list=band_list)

    def WriteArray(self, array, xoff=0, yoff=0,
                   band_list=None,
                   interleave='band',
                   resample_alg=gdalconst.GRIORA_NearestNeighbour,
                   callback=None,
                   callback_data=None):
        from osgeo import gdal_array

        return gdal_array.DatasetWriteArray(self, array, xoff, yoff,
                                            band_list=band_list,
                                            interleave=interleave,
                                            resample_alg=resample_alg,
                                            callback=callback,
                                            callback_data=callback_data)

    def WriteRaster(self, xoff, yoff, xsize, ysize,
                    buf_string,
                    buf_xsize=None, buf_ysize=None, buf_type=None,
                    band_list=None,
                    buf_pixel_space=None, buf_line_space=None, buf_band_space=None ):

        if buf_xsize is None:
            buf_xsize = xsize
        if buf_ysize is None:
            buf_ysize = ysize
        if band_list is None:
            band_list = list(range(1, self.RasterCount + 1))

        # Redirect to numpy-friendly WriteArray() if buf_string is a numpy array
        # and other arguments are compatible
        if type(buf_string).__name__ == 'ndarray' and \
           buf_xsize == xsize and buf_ysize == ysize and buf_type is None and \
           buf_pixel_space is None and buf_line_space is None and buf_band_space is None:
            return self.WriteArray(buf_string, xoff=xoff, yoff=yoff,
                                   band_list=band_list)
        if buf_type is None:
            buf_type = self.GetRasterBand(1).DataType

        return _gdal.Dataset_WriteRaster(self,
                 xoff, yoff, xsize, ysize,
                buf_string, buf_xsize, buf_ysize, buf_type, band_list,
                buf_pixel_space, buf_line_space, buf_band_space )

    def ReadRaster(self, xoff=0, yoff=0, xsize=None, ysize=None,
                   buf_xsize=None, buf_ysize=None, buf_type=None,
                   band_list=None,
                   buf_pixel_space=None, buf_line_space=None, buf_band_space=None,
                   resample_alg=gdalconst.GRIORA_NearestNeighbour,
                   callback=None,
                   callback_data=None,
                   buf_obj=None):

        if xsize is None:
            xsize = self.RasterXSize
        if ysize is None:
            ysize = self.RasterYSize
        if band_list is None:
            band_list = list(range(1, self.RasterCount + 1))
        if buf_xsize is None:
            buf_xsize = xsize
        if buf_ysize is None:
            buf_ysize = ysize

        if buf_type is None:
            buf_type = self.GetRasterBand(1).DataType;

        return _gdal.Dataset_ReadRaster1(self, xoff, yoff, xsize, ysize,
                                            buf_xsize, buf_ysize, buf_type,
                                            band_list, buf_pixel_space, buf_line_space, buf_band_space,
                                          resample_alg, callback, callback_data, buf_obj )

    def GetVirtualMemArray(self, eAccess=gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, bufxsize=None, bufysize=None,
                           datatype=None, band_list=None, band_sequential = True,
                           cache_size = 10 * 1024 * 1024, page_size_hint = 0,
                           options=None):
        """Return a NumPy array for the dataset, seen as a virtual memory mapping.
           If there are several bands and band_sequential = True, an element is
           accessed with array[band][y][x].
           If there are several bands and band_sequential = False, an element is
           accessed with array[y][x][band].
           If there is only one band, an element is accessed with array[y][x].
           Any reference to the array must be dropped before the last reference to the
           related dataset is also dropped.
        """
        from osgeo import gdal_array
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
            band_list = list(range(1, self.RasterCount + 1))
        if options is None:
            virtualmem = self.GetVirtualMem(eAccess, xoff, yoff, xsize, ysize, bufxsize, bufysize, datatype, band_list, band_sequential, cache_size, page_size_hint)
        else:
            virtualmem = self.GetVirtualMem(eAccess, xoff, yoff, xsize, ysize, bufxsize, bufysize, datatype, band_list, band_sequential, cache_size, page_size_hint,  options)
        return gdal_array.VirtualMemGetArray( virtualmem )

    def GetTiledVirtualMemArray(self, eAccess=gdalconst.GF_Read, xoff=0, yoff=0,
                           xsize=None, ysize=None, tilexsize=256, tileysize=256,
                           datatype=None, band_list=None, tile_organization=gdalconst.GTO_BSQ,
                           cache_size = 10 * 1024 * 1024, options=None):
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
        from osgeo import gdal_array
        if xsize is None:
            xsize = self.RasterXSize
        if ysize is None:
            ysize = self.RasterYSize
        if datatype is None:
            datatype = self.GetRasterBand(1).DataType
        if band_list is None:
            band_list = list(range(1, self.RasterCount + 1))
        if options is None:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,band_list,tile_organization,cache_size)
        else:
            virtualmem = self.GetTiledVirtualMem(eAccess,xoff,yoff,xsize,ysize,tilexsize,tileysize,datatype,band_list,tile_organization,cache_size, options)
        return gdal_array.VirtualMemGetArray( virtualmem )

    def GetSubDatasets(self):
        sd_list = []

        sd = self.GetMetadata('SUBDATASETS')
        if sd is None:
            return sd_list

        i = 1
        while 'SUBDATASET_'+str(i)+'_NAME' in sd:
            sd_list.append((sd['SUBDATASET_'+str(i)+'_NAME'],
                            sd['SUBDATASET_'+str(i)+'_DESC']))
            i = i + 1
        return sd_list

    def BeginAsyncReader(self, xoff, yoff, xsize, ysize, buf_obj=None, buf_xsize=None, buf_ysize=None, buf_type=None, band_list=None, options=None):
        if band_list is None:
            band_list = list(range(1, self.RasterCount + 1))
        if buf_xsize is None:
            buf_xsize = 0;
        if buf_ysize is None:
            buf_ysize = 0;
        if buf_type is None:
            buf_type = gdalconst.GDT_Byte

        if buf_xsize <= 0:
            buf_xsize = xsize
        if buf_ysize <= 0:
            buf_ysize = ysize
        options = [] if options is None else options

        if buf_obj is None:
            from sys import version_info
            nRequiredSize = int(buf_xsize * buf_ysize * len(band_list) * (_gdal.GetDataTypeSize(buf_type) / 8))
            if version_info >= (3, 0, 0):
                buf_obj_ar = [None]
                exec("buf_obj_ar[0] = b' ' * nRequiredSize")
                buf_obj = buf_obj_ar[0]
            else:
                buf_obj = ' ' * nRequiredSize
        return _gdal.Dataset_BeginAsyncReader(self, xoff, yoff, xsize, ysize, buf_obj, buf_xsize, buf_ysize, buf_type, band_list,  0, 0, 0, options)

    def GetLayer(self, iLayer=0):
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

    def SetGCPs(self, gcps, wkt_or_spatial_ref):
        if isinstance(wkt_or_spatial_ref, str):
            return self._SetGCPs(gcps, wkt_or_spatial_ref)
        else:
            return self._SetGCPs2(gcps, wkt_or_spatial_ref)
%}
}

%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata(self, domain=''):
    if domain and domain[:4] == 'xml:':
      return self.GetMetadata_List(domain)
    return self.GetMetadata_Dict(domain)
%}
}

%extend GDALRasterAttributeTableShadow {
%pythoncode %{
  def WriteArray(self, array, field, start=0):
      from osgeo import gdal_array

      return gdal_array.RATWriteArray(self, array, field, start)

  def ReadAsArray(self, field, start=0, length=None):
      from osgeo import gdal_array

      return gdal_array.RATReadArray(self, field, start, length)
%}
}

%extend GDALExtendedDataTypeHS {
%pythoncode %{

  def __eq__(self, other):
    return self.Equals(other)

  def __ne__(self, other):
    return not self.__eq__(other)
%}
}

%extend GDALMDArrayHS {
%pythoncode %{
  def Read(self,
           array_start_idx = None,
           count = None,
           array_step = None,
           buffer_stride = None,
           buffer_datatype = None):
      if not array_start_idx:
        array_start_idx = [0] * self.GetDimensionCount()
      if not count:
        count = [ dim.GetSize() for dim in self.GetDimensions() ]
      if not array_step:
        array_step = [1] * self.GetDimensionCount()
      if not buffer_stride:
        stride = 1
        buffer_stride = []
        # To compute strides we must proceed from the fastest varying dimension
        # (the last one), and then reverse the result
        for cnt in reversed(count):
            buffer_stride.append(stride)
            stride *= cnt
        buffer_stride.reverse()
      if not buffer_datatype:
        buffer_datatype = self.GetDataType()
      return _gdal.MDArray_Read(self, array_start_idx, count, array_step, buffer_stride, buffer_datatype)

  def ReadAsArray(self,
                  array_start_idx = None,
                  count = None,
                  array_step = None,
                  buffer_datatype = None,
                  buf_obj = None):

      from osgeo import gdal_array
      return gdal_array.MDArrayReadAsArray(self, array_start_idx, count, array_step, buffer_datatype, buf_obj)

  def AdviseRead(self, array_start_idx = None, count = None):
      if not array_start_idx:
        array_start_idx = [0] * self.GetDimensionCount()
      if not count:
        count = [ (self.GetDimensions()[i].GetSize() - array_start_idx[i]) for i in range (self.GetDimensionCount()) ]
      return _gdal.MDArray_AdviseRead(self, array_start_idx, count)

  def __getitem__(self, item):

        def stringify(v):
            if v == Ellipsis:
                return '...'
            if isinstance(v, slice):
                return ':'.join([str(x) if x is not None else '' for x in (v.start, v.stop, v.step)])
            if isinstance(v, str):
                return v
            if isinstance(v, (int, type(12345678901234))):
                return str(v)
            try:
                import numpy as np
                if v == np.newaxis:
                    return 'newaxis'
            except:
                pass

            return str(v)

        if isinstance(item, str):
            return self.GetView('["' + item.replace('\\', '\\\\').replace('"', '\\"') + '"]')
        elif isinstance(item, slice):
            return self.GetView('[' + stringify(item) + ']')
        elif isinstance(item, tuple):
            return self.GetView('[' + ','.join([stringify(x) for x in item]) + ']')
        else:
            return self.GetView('[' + stringify(item) + ']')

  def Write(self,
           buffer,
           array_start_idx = None,
           count = None,
           array_step = None,
           buffer_stride = None,
           buffer_datatype = None):

      dimCount = self.GetDimensionCount()

      # Redirect to numpy-friendly WriteArray() if buffer is a numpy array
      # and other arguments are compatible
      if type(buffer).__name__ == 'ndarray' and \
         count is None and buffer_stride is None and buffer_datatype is None:
          return self.WriteArray(buffer, array_start_idx=array_start_idx, array_step=array_step)

      # Special case for buffer of type array and 1D arrays
      if dimCount == 1 and type(buffer).__name__ == 'array' and \
         count is None and buffer_stride is None and buffer_datatype is None:
          map_typecode_itemsize_to_gdal = {
             ('B',1): GDT_Byte,
             ('h',2): GDT_Int16,
             ('H',2): GDT_UInt16,
             ('i',4): GDT_Int32,
             ('I',4): GDT_UInt32,
             ('l',4): GDT_Int32,
             # ('l',8): GDT_Int64,
             # ('q',8): GDT_Int64,
             # ('Q',8): GDT_UInt64,
             ('f', 4): GDT_Float32,
             ('d', 8): GDT_Float64
          }
          key = (buffer.typecode, buffer.itemsize)
          if key not in map_typecode_itemsize_to_gdal:
              raise Exception("unhandled type for buffer of type array")
          buffer_datatype = ExtendedDataType.Create(map_typecode_itemsize_to_gdal[key])

      # Special case for a list of numeric values and 1D arrays
      elif dimCount == 1 and type(buffer) == type([]) and len(buffer) != 0 \
           and self.GetDataType().GetClass() != GEDTC_STRING:
          buffer_datatype = GDT_Int32
          for v in buffer:
              if isinstance(v, int):
                  if v >= (1 << 31) or v < -(1 << 31):
                      buffer_datatype = GDT_Float64
              elif isinstance(v, float):
                  buffer_datatype = GDT_Float64
              else:
                  raise ValueError('Only lists with integer or float elements are supported')
          import array
          buffer = array.array('d' if buffer_datatype == GDT_Float64 else 'i', buffer)
          buffer_datatype = ExtendedDataType.Create(buffer_datatype)

      if not buffer_datatype:
        buffer_datatype = self.GetDataType()

      is_1d_string = self.GetDataType().GetClass() == GEDTC_STRING and buffer_datatype.GetClass() == GEDTC_STRING and dimCount == 1

      if not array_start_idx:
        array_start_idx = [0] * dimCount

      if not count:
        if is_1d_string:
            assert type(buffer) == type([])
            count = [ len(buffer) ]
        else:
            count = [ dim.GetSize() for dim in self.GetDimensions() ]

      if not array_step:
        array_step = [1] * dimCount

      if not buffer_stride:
        stride = 1
        buffer_stride = []
        # To compute strides we must proceed from the fastest varying dimension
        # (the last one), and then reverse the result
        for cnt in reversed(count):
            buffer_stride.append(stride)
            stride *= cnt
        buffer_stride.reverse()

      if is_1d_string:
          return _gdal.MDArray_WriteStringArray(self, array_start_idx, count, array_step, buffer_datatype, buffer)

      return _gdal.MDArray_Write(self, array_start_idx, count, array_step, buffer_stride, buffer_datatype, buffer)

  def WriteArray(self, array,
                  array_start_idx = None,
                  array_step = None):

      from osgeo import gdal_array
      return gdal_array.MDArrayWriteArray(self, array, array_start_idx, array_step)

  def ReadAsMaskedArray(self,
                  array_start_idx = None,
                  count = None,
                  array_step = None):
      """ Return a numpy masked array of ReadAsArray() with GetMask() """
      import numpy
      mask = self.GetMask()
      if mask is not None:
          array = self.ReadAsArray(array_start_idx, count, array_step)
          mask_array = mask.ReadAsArray(array_start_idx, count, array_step)
          bool_array = ~mask_array.astype(bool)
          return numpy.ma.array(array, mask=bool_array)
      else:
          return numpy.ma.array(self.ReadAsArray(array_start_idx, count, array_step), mask=None)

  def GetShape(self):
    """ Return the shape of the array """
    if not self.GetDimensionCount():
      return None
    shp = ()
    for dim in self.GetDimensions():
      shp += (dim.GetSize(),)
    return shp

  shape = property(fget=GetShape, doc='Returns the shape of the array.')

%}
}

%extend GDALAttributeHS {
%pythoncode %{

  def Read(self):
    """ Read an attribute and return it with the most appropriate type """
    dt = self.GetDataType()
    dt_class = dt.GetClass()
    if dt_class == GEDTC_STRING:
        if self.GetTotalElementsCount() == 1:
            s = self.ReadAsString()
            if dt.GetSubType() == GEDTST_JSON:
                try:
                    import json
                    return json.loads(s)
                except:
                    pass
            return s
        return self.ReadAsStringArray()
    if dt_class == GEDTC_NUMERIC:
        if dt.GetNumericDataType() in (GDT_Byte, GDT_Int16, GDT_UInt16, GDT_Int32):
            if self.GetTotalElementsCount() == 1:
                return self.ReadAsInt()
            else:
                return self.ReadAsIntArray()
        else:
            if self.GetTotalElementsCount() == 1:
                return self.ReadAsDouble()
            else:
                return self.ReadAsDoubleArray()
    return self.ReadAsRaw()

  def Write(self, val):
    if isinstance(val, (int, type(12345678901234))):
        if val >= -0x80000000 and val <= 0x7FFFFFFF:
            return self.WriteInt(val)
        else:
            return self.WriteDouble(val)
    if isinstance(val, float):
      return self.WriteDouble(val)
    if isinstance(val, str) and self.GetDataType().GetClass() != GEDTC_COMPOUND:
      return self.WriteString(val)
    if isinstance(val, list):
      if len(val) == 0:
        if self.GetDataType().GetClass() == GEDTC_STRING:
            return self.WriteStringArray(val)
        else:
            return self.WriteDoubleArray(val)
      if isinstance(val[0], (int, type(12345678901234), float)):
        return self.WriteDoubleArray(val)
      if isinstance(val[0], str):
        return self.WriteStringArray(val)
    if isinstance(val, dict) and self.GetDataType().GetSubType() == GEDTST_JSON:
        import json
        return self.WriteString(json.dumps(val))
    return self.WriteRaw(val)

%}
}

%include "callback.i"


%pythoncode %{

def InfoOptions(options=None, format='text', deserialize=True,
         computeMinMax=False, reportHistograms=False, reportProj4=False,
         stats=False, approxStats=False, computeChecksum=False,
         showGCPs=True, showMetadata=True, showRAT=True, showColorTable=True,
         listMDD=False, showFileList=True, allMetadata=False,
         extraMDDomains=None, wktFormat=None):
    """ Create a InfoOptions() object that can be passed to gdal.Info()
        options can be be an array of strings, a string or let empty and filled from other keywords."""

    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
        format = 'text'
        if '-json' in new_options:
            format = 'json'
    else:
        new_options = options
        if format == 'json':
            new_options += ['-json']
        if '-json' in new_options:
            format = 'json'
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
        if wktFormat:
            new_options += ['-wkt_format', wktFormat]
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
    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, format, deserialize) = InfoOptions(**kwargs)
    else:
        (opts, format, deserialize) = kwargs['options']
    if isinstance(ds, str):
        ds = Open(ds)
    ret = InfoInternal(ds, opts)
    if format == 'json' and deserialize:
        import json
        ret = json.loads(ret)
    return ret


def MultiDimInfoOptions(options=None, detailed=False, array=None, arrayoptions=None, limit=None, as_text=False):
    """ Create a MultiDimInfoOptions() object that can be passed to gdal.MultiDimInfo()
        options can be be an array of strings, a string or let empty and filled from other keywords."""

    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if detailed:
            new_options += ['-detailed']
        if array:
            new_options += ['-array', array]
        if limit:
            new_options += ['-limit', str(limit)]
        if arrayoptions:
            for option in arrayoptions:
                new_options += ['-arrayoption', option]

    return GDALMultiDimInfoOptions(new_options), as_text

def MultiDimInfo(ds, **kwargs):
    """ Return information on a dataset.
        Arguments are :
          ds --- a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.MultiDimInfoOptions(), string or array of strings
          other keywords arguments of gdal.MultiDimInfoOptions()
        If options is provided as a gdal.MultiDimInfoOptions() object, other keywords are ignored. """
    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        opts, as_text = MultiDimInfoOptions(**kwargs)
    else:
        opts = kwargs['options']
        as_text = True
    if isinstance(ds, str):
        ds = OpenEx(ds, OF_VERBOSE_ERROR | OF_MULTIDIM_RASTER)
    ret = MultiDimInfoInternal(ds, opts)
    if not as_text:
        import json
        ret = json.loads(ret)
    return ret


def _strHighPrec(x):
    return x if isinstance(x, str) else '%.18g' % x

def TranslateOptions(options=None, format=None,
              outputType = gdalconst.GDT_Unknown, bandList=None, maskBand=None,
              width = 0, height = 0, widthPct = 0.0, heightPct = 0.0,
              xRes = 0.0, yRes = 0.0,
              creationOptions=None, srcWin=None, projWin=None, projWinSRS=None, strict = False,
              unscale = False, scaleParams=None, exponents=None,
              outputBounds=None, metadataOptions=None,
              outputSRS=None, nogcp=False, GCPs=None,
              noData=None, rgbExpand=None,
              stats = False, rat = True, resampleAlg=None,
              callback=None, callback_data=None):
    """ Create a TranslateOptions() object that can be passed to gdal.Translate()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdalconst.GDT_Byte, etc...)
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
          nogcp --- ignore GCP in the raster
          GCPs --- list of GCPs
          noData --- nodata value (or "none" to unset it)
          rgbExpand --- Color palette expansion mode: "gray", "rgb", "rgba"
          stats --- whether to calculate statistics
          rat --- whether to write source RAT
          resampleAlg --- resampling mode
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if maskBand != None:
            new_options += ['-mask', str(maskBand)]
        if bandList != None:
            for b in bandList:
                new_options += ['-b', str(b)]
        if width != 0 or height != 0:
            new_options += ['-outsize', str(width), str(height)]
        elif widthPct != 0 and heightPct != 0:
            new_options += ['-outsize', str(widthPct) + '%%', str(heightPct) + '%%']
        if creationOptions is not None:
            if isinstance(creationOptions, str):
                new_options += ['-co', creationOptions]
            else:
                for opt in creationOptions:
                    new_options += ['-co', opt]
        if srcWin is not None:
            new_options += ['-srcwin', _strHighPrec(srcWin[0]), _strHighPrec(srcWin[1]), _strHighPrec(srcWin[2]), _strHighPrec(srcWin[3])]
        if strict:
            new_options += ['-strict']
        if unscale:
            new_options += ['-unscale']
        if scaleParams:
            for scaleParam in scaleParams:
                new_options += ['-scale']
                for v in scaleParam:
                    new_options += [str(v)]
        if exponents:
            for exponent in exponents:
                new_options += ['-exponent', _strHighPrec(exponent)]
        if outputBounds is not None:
            new_options += ['-a_ullr', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[2]), _strHighPrec(outputBounds[3])]
        if metadataOptions is not None:
            if isinstance(metadataOptions, str):
                new_options += ['-mo', metadataOptions]
            else:
                for opt in metadataOptions:
                    new_options += ['-mo', opt]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS)]
        if nogcp:
            new_options += ['-nogcp']
        if GCPs is not None:
            for gcp in GCPs:
                new_options += ['-gcp', _strHighPrec(gcp.GCPPixel), _strHighPrec(gcp.GCPLine), _strHighPrec(gcp.GCPX), str(gcp.GCPY), _strHighPrec(gcp.GCPZ)]
        if projWin is not None:
            new_options += ['-projwin', _strHighPrec(projWin[0]), _strHighPrec(projWin[1]), _strHighPrec(projWin[2]), _strHighPrec(projWin[3])]
        if projWinSRS is not None:
            new_options += ['-projwin_srs', str(projWinSRS)]
        if noData is not None:
            new_options += ['-a_nodata', _strHighPrec(noData)]
        if rgbExpand is not None:
            new_options += ['-expand', str(rgbExpand)]
        if stats:
            new_options += ['-stats']
        if not rat:
            new_options += ['-norat']
        if resampleAlg is not None:
            if resampleAlg == gdalconst.GRA_NearestNeighbour:
                new_options += ['-r', 'near']
            elif resampleAlg == gdalconst.GRA_Bilinear:
                new_options += ['-r', 'bilinear']
            elif resampleAlg == gdalconst.GRA_Cubic:
                new_options += ['-r', 'cubic']
            elif resampleAlg == gdalconst.GRA_CubicSpline:
                new_options += ['-r', 'cubicspline']
            elif resampleAlg == gdalconst.GRA_Lanczos:
                new_options += ['-r', 'lanczos']
            elif resampleAlg == gdalconst.GRA_Average:
                new_options += ['-r', 'average']
            elif resampleAlg == gdalconst.GRA_RMS:
                new_options += ['-r', 'rms']
            elif resampleAlg == gdalconst.GRA_Mode:
                new_options += ['-r', 'mode']
            else:
                new_options += ['-r', str(resampleAlg)]
        if xRes != 0 and yRes != 0:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = TranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = Open(srcDS)

    return TranslateInternal(destName, srcDS, opts, callback, callback_data)

def WarpOptions(options=None, format=None,
         outputBounds=None,
         outputBoundsSRS=None,
         xRes=None, yRes=None, targetAlignedPixels = False,
         width = 0, height = 0,
         srcSRS=None, dstSRS=None,
         coordinateOperation=None,
         srcAlpha = False, dstAlpha = False,
         warpOptions=None, errorThreshold=None,
         warpMemoryLimit=None, creationOptions=None, outputType = gdalconst.GDT_Unknown,
         workingType = gdalconst.GDT_Unknown, resampleAlg=None,
         srcNodata=None, dstNodata=None, multithread = False,
         tps = False, rpc = False, geoloc = False, polynomialOrder=None,
         transformerOptions=None, cutlineDSName=None,
         cutlineLayer=None, cutlineWhere=None, cutlineSQL=None, cutlineBlend=None, cropToCutline = False,
         copyMetadata = True, metadataConflictValue=None,
         setColorInterpretation = False,
         overviewLevel = 'AUTO',
         callback=None, callback_data=None):
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
          coordinateOperation -- coordinate operation as a PROJ string or WKT string
          srcAlpha --- whether to force the last band of the input dataset to be considered as an alpha band
          dstAlpha --- whether to force the creation of an output alpha band
          outputType --- output type (gdalconst.GDT_Byte, etc...)
          workingType --- working type (gdalconst.GDT_Byte, etc...)
          warpOptions --- list of warping options
          errorThreshold --- error threshold for approximation transformer (in pixels)
          warpMemoryLimit --- size of working buffer in MB
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
          overviewLevel --- To specify which overview level of source files must be used
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if workingType != gdalconst.GDT_Unknown:
            new_options += ['-wt', GetDataTypeName(workingType)]
        if outputBounds is not None:
            new_options += ['-te', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[2]), _strHighPrec(outputBounds[3])]
        if outputBoundsSRS is not None:
            new_options += ['-te_srs', str(outputBoundsSRS)]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]
        if width != 0 or height != 0:
            new_options += ['-ts', str(width), str(height)]
        if srcSRS is not None:
            new_options += ['-s_srs', str(srcSRS)]
        if dstSRS is not None:
            new_options += ['-t_srs', str(dstSRS)]
        if coordinateOperation is not None:
            new_options += ['-ct', coordinateOperation]
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
            new_options += ['-et', _strHighPrec(errorThreshold)]
        if resampleAlg is not None:
            if resampleAlg == gdalconst.GRIORA_NearestNeighbour:
                new_options += ['-r', 'near']
            elif resampleAlg == gdalconst.GRIORA_Bilinear:
                new_options += ['-rb']
            elif resampleAlg == gdalconst.GRIORA_Cubic:
                new_options += ['-rc']
            elif resampleAlg == gdalconst.GRIORA_CubicSpline:
                new_options += ['-rcs']
            elif resampleAlg == gdalconst.GRIORA_Lanczos:
                new_options += ['-r', 'lanczos']
            elif resampleAlg == gdalconst.GRIORA_Average:
                new_options += ['-r', 'average']
            elif resampleAlg == gdalconst.GRIORA_RMS:
                new_options += ['-r', 'rms']
            elif resampleAlg == gdalconst.GRIORA_Mode:
                new_options += ['-r', 'mode']
            elif resampleAlg == gdalconst.GRIORA_Gauss:
                new_options += ['-r', 'gauss']
            else:
                new_options += ['-r', str(resampleAlg)]
        if warpMemoryLimit is not None:
            new_options += ['-wm', str(warpMemoryLimit)]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if srcNodata is not None:
            new_options += ['-srcnodata', str(srcNodata)]
        if dstNodata is not None:
            new_options += ['-dstnodata', str(dstNodata)]
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
                new_options += ['-to', opt]
        if cutlineDSName is not None:
            new_options += ['-cutline', str(cutlineDSName)]
        if cutlineLayer is not None:
            new_options += ['-cl', str(cutlineLayer)]
        if cutlineWhere is not None:
            new_options += ['-cwhere', str(cutlineWhere)]
        if cutlineSQL is not None:
            new_options += ['-csql', str(cutlineSQL)]
        if cutlineBlend is not None:
            new_options += ['-cblend', str(cutlineBlend)]
        if cropToCutline:
            new_options += ['-crop_to_cutline']
        if not copyMetadata:
            new_options += ['-nomd']
        if metadataConflictValue:
            new_options += ['-cvmd', str(metadataConflictValue)]
        if setColorInterpretation:
            new_options += ['-setci']

        if overviewLevel is None or isinstance(overviewLevel, str):
            pass
        elif isinstance(overviewLevel, int):
            if overviewLevel < 0:
                overviewLevel = 'AUTO' + str(overviewLevel)
            else:
                overviewLevel = str(overviewLevel)
        else:
            overviewLevel = None

        if overviewLevel:
            new_options += ['-ovr', overviewLevel]

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = WarpOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDSOrSrcDSTab, str):
        srcDSTab = [Open(srcDSOrSrcDSTab)]
    elif isinstance(srcDSOrSrcDSTab, list):
        srcDSTab = []
        for elt in srcDSOrSrcDSTab:
            if isinstance(elt, str):
                srcDSTab.append(Open(elt))
            else:
                srcDSTab.append(elt)
    else:
        srcDSTab = [srcDSOrSrcDSTab]

    if isinstance(destNameOrDestDS, str):
        return wrapper_GDALWarpDestName(destNameOrDestDS, srcDSTab, opts, callback, callback_data)
    else:
        return wrapper_GDALWarpDestDS(destNameOrDestDS, srcDSTab, opts, callback, callback_data)


def VectorTranslateOptions(options=None, format=None,
         accessMode=None,
         srcSRS=None, dstSRS=None, reproject=True,
         coordinateOperation=None,
         SQLStatement=None, SQLDialect=None, where=None, selectFields=None,
         addFields=False,
         forceNullable=False,
         emptyStrAsNull=False,
         spatFilter=None, spatSRS=None,
         datasetCreationOptions=None,
         layerCreationOptions=None,
         layers=None,
         layerName=None,
         geometryType=None,
         dim=None,
         segmentizeMaxDist= None,
         makeValid=False,
         zField=None,
         resolveDomains=False,
         skipFailures=False,
         limit=None,
         callback=None, callback_data=None):
    """ Create a VectorTranslateOptions() object that can be passed to gdal.VectorTranslate()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("ESRI Shapefile", etc...)
          accessMode --- None for creation, 'update', 'append', 'overwrite'
          srcSRS --- source SRS
          dstSRS --- output SRS (with reprojection if reproject = True)
          coordinateOperation -- coordinate operation as a PROJ string or WKT string
          reproject --- whether to do reprojection
          SQLStatement --- SQL statement to apply to the source dataset
          SQLDialect --- SQL dialect ('OGRSQL', 'SQLITE', ...)
          where --- WHERE clause to apply to source layer(s)
          selectFields --- list of fields to select
          addFields --- whether to add new fields found in source layers (to be used with accessMode == 'append')
          forceNullable --- whether to drop NOT NULL constraints on newly created fields
          emptyStrAsNull --- whether to treat empty string values as NULL
          spatFilter --- spatial filter as (minX, minY, maxX, maxY) bounding box
          spatSRS --- SRS in which the spatFilter is expressed. If not specified, it is assumed to be the one of the layer(s)
          datasetCreationOptions --- list of dataset creation options
          layerCreationOptions --- list of layer creation options
          layers --- list of layers to convert
          layerName --- output layer name
          geometryType --- output layer geometry type ('POINT', ....)
          dim --- output dimension ('XY', 'XYZ', 'XYM', 'XYZM', 'layer_dim')
          segmentizeMaxDist --- maximum distance between consecutive nodes of a line geometry
          makeValid --- run MakeValid() on geometries
          zField --- name of field to use to set the Z component of geometries
          resolveDomains --- whether to create an additional field for each field associated with a coded field domain.
          skipFailures --- whether to skip failures
          limit -- maximum number of features to read per layer
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-f', format]
        if srcSRS is not None:
            new_options += ['-s_srs', str(srcSRS)]
        if dstSRS is not None:
            if reproject:
                new_options += ['-t_srs', str(dstSRS)]
            else:
                new_options += ['-a_srs', str(dstSRS)]
        if coordinateOperation is not None:
            new_options += ['-ct', coordinateOperation]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement)]
        if SQLDialect is not None:
            new_options += ['-dialect', str(SQLDialect)]
        if where is not None:
            new_options += ['-where', str(where)]
        if accessMode is not None:
            if accessMode == 'update':
                new_options += ['-update']
            elif accessMode == 'append':
                new_options += ['-append']
            elif accessMode == 'overwrite':
                new_options += ['-overwrite']
            else:
                raise Exception('unhandled accessMode')
        if addFields:
            new_options += ['-addfields']
        if forceNullable:
            new_options += ['-forceNullable']
        if emptyStrAsNull:
            new_options += ['-emptyStrAsNull']
        if selectFields is not None:
            val = ''
            for item in selectFields:
                if val:
                    val += ','
                val += item
            new_options += ['-select', val]
        if datasetCreationOptions is not None:
            for opt in datasetCreationOptions:
                new_options += ['-dsco', opt]
        if layerCreationOptions is not None:
            for opt in layerCreationOptions:
                new_options += ['-lco', opt]
        if layers is not None:
            if isinstance(layers, str):
                new_options += [layers]
            else:
                for lyr in layers:
                    new_options += [lyr]
        if segmentizeMaxDist is not None:
            new_options += ['-segmentize', str(segmentizeMaxDist)]
        if makeValid:
            new_options += ['-makevalid']
        if spatFilter is not None:
            new_options += ['-spat', str(spatFilter[0]), str(spatFilter[1]), str(spatFilter[2]), str(spatFilter[3])]
        if spatSRS is not None:
            new_options += ['-spat_srs', str(spatSRS)]
        if layerName is not None:
            new_options += ['-nln', layerName]
        if geometryType is not None:
            if isinstance(geometryType, str):
                new_options += ['-nlt', geometryType]
            else:
                for opt in geometryType:
                    new_options += ['-nlt', opt]
        if dim is not None:
            new_options += ['-dim', dim]
        if zField is not None:
            new_options += ['-zfield', zField]
        if resolveDomains:
            new_options += ['-resolveDomains']
        if skipFailures:
            new_options += ['-skip']
        if limit is not None:
            new_options += ['-limit', str(limit)]
    if callback is not None:
        new_options += ['-progress']

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = VectorTranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    if isinstance(destNameOrDestDS, str):
        return wrapper_GDALVectorTranslateDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALVectorTranslateDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)

def DEMProcessingOptions(options=None, colorFilename=None, format=None,
              creationOptions=None, computeEdges=False, alg=None, band=1,
              zFactor=None, scale=None, azimuth=None, altitude=None,
              combined=False, multiDirectional=False, igor=False,
              slopeFormat=None, trigonometric=False, zeroForFlat=False,
              addAlpha=None, colorSelection=None,
              callback=None, callback_data=None):
    """ Create a DEMProcessingOptions() object that can be passed to gdal.DEMProcessing()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          colorFilename --- (mandatory for "color-relief") name of file that contains palette definition for the "color-relief" processing.
          format --- output format ("GTiff", etc...)
          creationOptions --- list of creation options
          computeEdges --- whether to compute values at raster edges.
          alg --- 'Horn' (default) or 'ZevenbergenThorne' for hillshade, slope or aspect. 'Wilson' (default) or 'Riley' for TRI
          band --- source band number to use
          zFactor --- (hillshade only) vertical exaggeration used to pre-multiply the elevations.
          scale --- ratio of vertical units to horizontal.
          azimuth --- (hillshade only) azimuth of the light, in degrees. 0 if it comes from the top of the raster, 90 from the east, ... The default value, 315, should rarely be changed as it is the value generally used to generate shaded maps.
          altitude ---(hillshade only) altitude of the light, in degrees. 90 if the light comes from above the DEM, 0 if it is raking light.
          combined --- (hillshade only) whether to compute combined shading, a combination of slope and oblique shading. Only one of combined, multiDirectional and igor can be specified.
          multiDirectional --- (hillshade only) whether to compute multi-directional shading. Only one of combined, multiDirectional and igor can be specified.
          igor --- (hillshade only) whether to use Igor's hillshading from Maperitive.  Only one of combined, multiDirectional and igor can be specified.
          slopeformat --- (slope only) "degree" or "percent".
          trigonometric --- (aspect only) whether to return trigonometric angle instead of azimuth. Thus 0deg means East, 90deg North, 180deg West, 270deg South.
          zeroForFlat --- (aspect only) whether to return 0 for flat areas with slope=0, instead of -9999.
          addAlpha --- adds an alpha band to the output file (only for processing = 'color-relief')
          colorSelection --- (color-relief only) Determines how color entries are selected from an input value. Can be "nearest_color_entry", "exact_color_entry" or "linear_interpolation". Defaults to "linear_interpolation"
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if computeEdges:
            new_options += ['-compute_edges']
        if alg:
            new_options += ['-alg', alg]
        new_options += ['-b', str(band)]
        if zFactor is not None:
            new_options += ['-z', str(zFactor)]
        if scale is not None:
            new_options += ['-s', str(scale)]
        if azimuth is not None:
            new_options += ['-az', str(azimuth)]
        if altitude is not None:
            new_options += ['-alt', str(altitude)]
        if combined:
            new_options += ['-combined']
        if multiDirectional:
            new_options += ['-multidirectional']
        if igor:
            new_options += ['-igor']
        if slopeFormat == 'percent':
            new_options += ['-p']
        if trigonometric:
            new_options += ['-trigonometric']
        if zeroForFlat:
            new_options += ['-zero_for_flat']
        if colorSelection is not None:
            if colorSelection == 'nearest_color_entry':
                new_options += ['-nearest_color_entry']
            elif colorSelection == 'exact_color_entry':
                new_options += ['-exact_color_entry']
            elif colorSelection == 'linear_interpolation':
                pass
            else:
                raise ValueError("Unsupported value for colorSelection")
        if addAlpha:
            new_options += ['-alpha']

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, colorFilename, callback, callback_data) = DEMProcessingOptions(**kwargs)
    else:
        (opts, colorFilename, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = Open(srcDS)

    return DEMProcessingInternal(destName, srcDS, processing, colorFilename, opts, callback, callback_data)


def NearblackOptions(options=None, format=None,
         creationOptions=None, white = False, colors=None,
         maxNonBlack=None, nearDist=None, setAlpha = False, setMask = False,
         callback=None, callback_data=None):
    """ Create a NearblackOptions() object that can be passed to gdal.Nearblack()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          creationOptions --- list of creation options
          white --- whether to search for nearly white (255) pixels instead of nearly black pixels.
          colors --- list of colors  to search for, e.g. ((0,0,0),(255,255,255)). The pixels that are considered as the collar are set to 0
          maxNonBlack --- number of non-black (or other searched colors specified with white / colors) pixels that can be encountered before the giving up search inwards. Defaults to 2.
          nearDist --- select how far from black, white or custom colors the pixel values can be and still considered near black, white or custom color.  Defaults to 15.
          setAlpha --- adds an alpha band to the output file.
          setMask --- adds a mask band to the output file.
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if white:
            new_options += ['-white']
        if colors is not None:
            for color in colors:
                color_str = ''
                for cpt in color:
                    if color_str != '':
                        color_str += ','
                    color_str += str(cpt)
                new_options += ['-color', color_str]
        if maxNonBlack is not None:
            new_options += ['-nb', str(maxNonBlack)]
        if nearDist is not None:
            new_options += ['-near', str(nearDist)]
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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = NearblackOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = OpenEx(srcDS)

    if isinstance(destNameOrDestDS, str):
        return wrapper_GDALNearblackDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALNearblackDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def GridOptions(options=None, format=None,
              outputType=gdalconst.GDT_Unknown,
              width=0, height=0,
              creationOptions=None,
              outputBounds=None,
              outputSRS=None,
              noData=None,
              algorithm=None,
              layers=None,
              SQLStatement=None,
              where=None,
              spatFilter=None,
              zfield=None,
              z_increase=None,
              z_multiply=None,
              callback=None, callback_data=None):
    """ Create a GridOptions() object that can be passed to gdal.Grid()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdalconst.GDT_Byte, etc...)
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
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if width != 0 or height != 0:
            new_options += ['-outsize', str(width), str(height)]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if outputBounds is not None:
            new_options += ['-txe', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[2]), '-tye', _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[3])]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS)]
        if algorithm is not None:
            new_options += ['-a', algorithm]
        if layers is not None:
            if isinstance(layers, (tuple, list)):
                for layer in layers:
                    new_options += ['-l', layer]
            else:
                new_options += ['-l', layers]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement)]
        if where is not None:
            new_options += ['-where', str(where)]
        if zfield is not None:
            new_options += ['-zfield', zfield]
        if z_increase is not None:
            new_options += ['-z_increase', str(z_increase)]
        if z_multiply is not None:
            new_options += ['-z_multiply', str(z_multiply)]
        if spatFilter is not None:
            new_options += ['-spat', str(spatFilter[0]), str(spatFilter[1]), str(spatFilter[2]), str(spatFilter[3])]

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = GridOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    return GridInternal(destName, srcDS, opts, callback, callback_data)

def RasterizeOptions(options=None, format=None,
         outputType=gdalconst.GDT_Unknown,
         creationOptions=None, noData=None, initValues=None,
         outputBounds=None, outputSRS=None,
         transformerOptions=None,
         width=None, height=None,
         xRes=None, yRes=None, targetAlignedPixels=False,
         bands=None, inverse=False, allTouched=False,
         burnValues=None, attribute=None, useZ=False, layers=None,
         SQLStatement=None, SQLDialect=None, where=None, optim=None,
         add=None,
         callback=None, callback_data=None):
    """ Create a RasterizeOptions() object that can be passed to gdal.Rasterize()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          outputType --- output type (gdalconst.GDT_Byte, etc...)
          creationOptions --- list of creation options
          outputBounds --- assigned output bounds: [minx, miny, maxx, maxy]
          outputSRS --- assigned output SRS
          transformerOptions --- list of transformer options
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
          optim --- optimization mode ('RASTER', 'VECTOR')
          add --- set to True to use additive mode instead of replace when burning values
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if bands is not None:
            for b in bands:
                new_options += ['-b', str(b)]
        if noData is not None:
            new_options += ['-a_nodata', str(noData)]
        if initValues is not None:
            if isinstance(initValues, (tuple, list)):
                for val in initValues:
                    new_options += ['-init', str(val)]
            else:
                new_options += ['-init', str(initValues)]
        if outputBounds is not None:
            new_options += ['-te', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[2]), _strHighPrec(outputBounds[3])]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS)]
        if transformerOptions is not None:
            for opt in transformerOptions:
                new_options += ['-to', opt]
        if width is not None and height is not None:
            new_options += ['-ts', str(width), str(height)]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]
        if targetAlignedPixels:
            new_options += ['-tap']
        if inverse:
            new_options += ['-i']
        if allTouched:
            new_options += ['-at']
        if burnValues is not None:
            if attribute is not None:
                raise Exception('burnValues and attribute option are exclusive.')
            if isinstance(burnValues, (tuple, list)):
                for val in burnValues:
                    new_options += ['-burn', str(val)]
            else:
                new_options += ['-burn', str(burnValues)]
        if attribute is not None:
            new_options += ['-a', attribute]
        if useZ:
            new_options += ['-3d']
        if layers is not None:
            if isinstance(layers, ((tuple, list))):
                for layer in layers:
                    new_options += ['-l', layer]
            else:
                new_options += ['-l', layers]
        if SQLStatement is not None:
            new_options += ['-sql', str(SQLStatement)]
        if SQLDialect is not None:
            new_options += ['-dialect', str(SQLDialect)]
        if where is not None:
            new_options += ['-where', str(where)]
        if optim is not None:
            new_options += ['-optim', str(optim)]
        if add:
            new_options += ['-add']

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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = RasterizeOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, str):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    if isinstance(destNameOrDestDS, str):
        return wrapper_GDALRasterizeDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALRasterizeDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def BuildVRTOptions(options=None,
                    resolution=None,
                    outputBounds=None,
                    xRes=None, yRes=None,
                    targetAlignedPixels=None,
                    separate=None,
                    bandList=None,
                    addAlpha=None,
                    resampleAlg=None,
                    outputSRS=None,
                    allowProjectionDifference=None,
                    srcNodata=None,
                    VRTNodata=None,
                    hideNodata=None,
                    callback=None, callback_data=None):
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
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if resolution is not None:
            new_options += ['-resolution', str(resolution)]
        if outputBounds is not None:
            new_options += ['-te', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[2]), _strHighPrec(outputBounds[3])]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]
        if targetAlignedPixels:
            new_options += ['-tap']
        if separate:
            new_options += ['-separate']
        if bandList != None:
            for b in bandList:
                new_options += ['-b', str(b)]
        if addAlpha:
            new_options += ['-addalpha']
        if resampleAlg is not None:
            if resampleAlg == gdalconst.GRIORA_NearestNeighbour:
                new_options += ['-r', 'near']
            elif resampleAlg == gdalconst.GRIORA_Bilinear:
                new_options += ['-rb']
            elif resampleAlg == gdalconst.GRIORA_Cubic:
                new_options += ['-rc']
            elif resampleAlg == gdalconst.GRIORA_CubicSpline:
                new_options += ['-rcs']
            elif resampleAlg == gdalconst.GRIORA_Lanczos:
                new_options += ['-r', 'lanczos']
            elif resampleAlg == gdalconst.GRIORA_Average:
                new_options += ['-r', 'average']
            elif resampleAlg == gdalconst.GRIORA_RMS:
                new_options += ['-r', 'rms']
            elif resampleAlg == gdalconst.GRIORA_Mode:
                new_options += ['-r', 'mode']
            elif resampleAlg == gdalconst.GRIORA_Gauss:
                new_options += ['-r', 'gauss']
            else:
                new_options += ['-r', str(resampleAlg)]
        if outputSRS is not None:
            new_options += ['-a_srs', str(outputSRS)]
        if allowProjectionDifference:
            new_options += ['-allow_projection_difference']
        if srcNodata is not None:
            new_options += ['-srcnodata', str(srcNodata)]
        if VRTNodata is not None:
            new_options += ['-vrtnodata', str(VRTNodata)]
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

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = BuildVRTOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    srcDSTab = []
    srcDSNamesTab = []
    if isinstance(srcDSOrSrcDSTab, str):
        srcDSNamesTab = [srcDSOrSrcDSTab]
    elif isinstance(srcDSOrSrcDSTab, list):
        for elt in srcDSOrSrcDSTab:
            if isinstance(elt, str):
                srcDSNamesTab.append(elt)
            else:
                srcDSTab.append(elt)
        if srcDSTab and srcDSNamesTab:
            raise Exception('Mix of names and dataset objects not supported')
    else:
        srcDSTab = [srcDSOrSrcDSTab]

    if srcDSTab:
        return BuildVRTInternalObjects(destName, srcDSTab, opts, callback, callback_data)
    else:
        return BuildVRTInternalNames(destName, srcDSNamesTab, opts, callback, callback_data)


def MultiDimTranslateOptions(options=None, format=None, creationOptions=None,
         arraySpecs=None, groupSpecs=None, subsetSpecs=None, scaleAxesSpecs=None,
         callback=None, callback_data=None):
    """ Create a MultiDimTranslateOptions() object that can be passed to gdal.MultiDimTranslate()
        Keyword arguments are :
          options --- can be be an array of strings, a string or let empty and filled from other keywords.
          format --- output format ("GTiff", etc...)
          creationOptions --- list of creation options
          arraySpecs -- list of array specifications, each of them being an array name or "name={src_array_name},dstname={dst_name},transpose=[1,0],view=[:,::-1]"
          groupSpecs -- list of group specifications, each of them being a group name or "name={src_array_name},dstname={dst_name},recursive=no"
          subsetSpecs -- list of subset specifications, each of them being like "{dim_name}({min_val},{max_val})" or "{dim_name}({slice_va})"
          scaleAxesSpecs -- list of dimension scaling specifications, each of them being like "{dim_name}({scale_factor})"
          callback --- callback method
          callback_data --- user data for callback
    """
    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        new_options = options
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            for opt in creationOptions:
                new_options += ['-co', opt]
        if arraySpecs is not None:
            for s in arraySpecs:
                new_options += ['-array', s]
        if groupSpecs is not None:
            for s in groupSpecs:
                new_options += ['-group', s]
        if subsetSpecs is not None:
            for s in subsetSpecs:
                new_options += ['-subset', s]
        if scaleAxesSpecs is not None:
            for s in scaleAxesSpecs:
                new_options += ['-scaleaxes', s]

    return (GDALMultiDimTranslateOptions(new_options), callback, callback_data)

def MultiDimTranslate(destName, srcDSOrSrcDSTab, **kwargs):
    """ MultiDimTranslate one or several datasets.
        Arguments are :
          destName --- Output dataset name
          srcDSOrSrcDSTab --- an array of Dataset objects or filenames, or a Dataset object or a filename
        Keyword arguments are :
          options --- return of gdal.MultiDimTranslateOptions(), string or array of strings
          other keywords arguments of gdal.MultiDimTranslateOptions()
        If options is provided as a gdal.MultiDimTranslateOptions() object, other keywords are ignored. """

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = MultiDimTranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDSOrSrcDSTab, str):
        srcDSTab = [OpenEx(srcDSOrSrcDSTab, OF_VERBOSE_ERROR | OF_RASTER | OF_MULTIDIM_RASTER)]
    elif isinstance(srcDSOrSrcDSTab, list):
        srcDSTab = []
        for elt in srcDSOrSrcDSTab:
            if isinstance(elt, str):
                srcDSTab.append(OpenEx(elt, OF_VERBOSE_ERROR | OF_RASTER | OF_MULTIDIM_RASTER))
            else:
                srcDSTab.append(elt)
    else:
        srcDSTab = [srcDSOrSrcDSTab]

    return wrapper_GDALMultiDimTranslateDestName(destName, srcDSTab, opts, callback, callback_data)

# Logging Helpers
def _pylog_handler(err_level, err_no, err_msg):
    if err_no != gdalconst.CPLE_None:
        typ = _pylog_handler.errcode_map.get(err_no, str(err_no))
        message = "%s: %s" % (typ, err_msg)
    else:
        message = err_msg

    level = _pylog_handler.level_map.get(err_level, 20)  # default level is INFO
    _pylog_handler.logger.log(level, message)

def ConfigurePythonLogging(logger_name='gdal', enable_debug=False):
    """ Configure GDAL to use Python's logging framework """
    import logging

    _pylog_handler.logger = logging.getLogger(logger_name)

    # map CPLE_* constants to names
    _pylog_handler.errcode_map = {_num: _name[5:] for _name, _num in gdalconst.__dict__.items() if _name.startswith('CPLE_')}

    # Map GDAL log levels to Python's
    _pylog_handler.level_map = {
        CE_None: logging.INFO,
        CE_Debug: logging.DEBUG,
        CE_Warning: logging.WARN,
        CE_Failure: logging.ERROR,
        CE_Fatal: logging.CRITICAL,
    }

    # Set CPL_DEBUG so debug messages are passed through the logger
    if enable_debug:
        SetConfigOption("CPL_DEBUG", "ON")

    # Install as the default GDAL log handler
    SetErrorHandler(_pylog_handler)

%}
