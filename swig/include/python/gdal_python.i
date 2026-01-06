/*
 *
 * python specific code for gdal bindings.
 */

// Disable C-style signatures in Python docstrings (see #12177)
%feature("autodoc", "0");


%include "gdal_docs.i"
%include "gdal_algorithm_docs.i"
%include "gdal_band_docs.i"
%include "gdal_dataset_docs.i"
%include "gdal_driver_docs.i"
%include "gdal_mdm_docs.i"
%include "gdal_operations_docs.i"
%include "gdal_rat_docs.i"

%init %{
  /* gdal_python.i %init code */
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
  // Will be turned on for GDAL 4.0
  // UseExceptions();

%}

%{
static int getAlignment(GDALDataType ntype)
{
    switch(ntype)
    {
        case GDT_Unknown:
            break; // shouldn't happen
        case GDT_Byte:
        case GDT_Int8:
            return 1;
        case GDT_Int16:
        case GDT_UInt16:
        case GDT_Float16:
            return 2;
        case GDT_Int32:
        case GDT_UInt32:
        case GDT_Float32:
            return 4;
        case GDT_Float64:
        case GDT_Int64:
        case GDT_UInt64:
            return 8;
        case GDT_CInt16:
        case GDT_CFloat16:
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
                                     int l_bUseExceptions,
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
            if( !l_bUseExceptions )
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

  import os
  import sys
  byteorders = {"little": "<",
                "big": ">"}
  array_modes = { gdalconst.GDT_Int16:    ("%si2" % byteorders[sys.byteorder]),
                  gdalconst.GDT_UInt16:   ("%su2" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Int32:    ("%si4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_UInt32:   ("%su4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Int64:    ("%si8" % byteorders[sys.byteorder]),
                  gdalconst.GDT_UInt64:   ("%su8" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Float16:  ("%sf2" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Float32:  ("%sf4" % byteorders[sys.byteorder]),
                  gdalconst.GDT_Float64:  ("%sf8" % byteorders[sys.byteorder]),
                  gdalconst.GDT_CFloat16: ("%sf2" % byteorders[sys.byteorder]),
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

  def where(cond_band, then_band, else_band):
      """Ternary operator. Return a band whose value is then_band if the
         corresponding pixel in cond_band is not zero, or the one from else_band
         otherwise.

         cond_band must be a band or convertible to a band. then_band or else_band
         can be band, convertible to band or numeric constants.

         The resulting band is lazily evaluated.

         :since: 3.12
      """
      cond_band = Band._get_as_band_if_possible(cond_band)
      then_band = Band._get_as_band_if_possible(then_band)
      else_band = Band._get_as_band_if_possible(else_band)

      if not isinstance(then_band, Band):
          then_band = (cond_band * 0).astype(DataTypeUnionWithValue(gdalconst.GDT_Unknown, then_band, False)) + then_band

      if not isinstance(else_band, Band):
          else_band = (cond_band * 0).astype(DataTypeUnionWithValue(gdalconst.GDT_Unknown, else_band, False)) + else_band

      return _gdal.Band_IfThenElse(cond_band, then_band, else_band)._add_parent_references([cond_band, then_band, else_band])

  Where = where

  def minimum(*args):
      """Return a band whose each pixel value is the minimum of the corresponding
         pixel values in the input arguments which may be gdal.Band or a numeric constant.

         The resulting band is lazily evaluated.

         :since: 3.12
      """
      constant = None
      band_refs = []
      band_args = []
      for arg in args:
          band_arg = Band._get_as_band_if_possible(arg)
          if isinstance(band_arg, Band):
              band_args.append(band_arg)
              band_refs.append(arg)
          elif constant is None or arg < constant:
              constant = arg
      if not band_args:
          raise RuntimeError("At least one argument should be a band (or convertible to a band)")
      res = _gdal.Band_MinimumOfNBands(band_args)._add_parent_references(band_refs)
      if constant is not None:
          res = _gdal.Band_MinConstant(res, constant)._add_parent_references([res])
      return res

  Minimum = minimum

  def maximum(*args):
      """Return a band whose each pixel value is the maximum of the corresponding
         pixel values in the input arguments which may be gdal.Band or a numeric constant.

         The resulting band is lazily evaluated.

         :since: 3.12
      """
      constant = None
      band_refs = []
      band_args = []
      for arg in args:
          band_arg = Band._get_as_band_if_possible(arg)
          if isinstance(band_arg, Band):
              band_args.append(band_arg)
              band_refs.append(arg)
          elif constant is None or arg > constant:
              constant = arg
      if not band_args:
          raise RuntimeError("At least one argument should be a band (or convertible to a band)")
      res = _gdal.Band_MaximumOfNBands(band_args)._add_parent_references(band_refs)
      if constant is not None:
          res = _gdal.Band_MaxConstant(res, constant)._add_parent_references([res])
      return res

  Maximum = maximum

  def mean(*args):
      """Return a band whose each pixel value is the arithmetic mean of the corresponding
         pixel values in the input bands.

         The resulting band is lazily evaluated.

         :since: 3.12
      """
      bands = [Band._get_as_band_if_possible(band) for band in args]
      band_refs = [band for band in args]
      return _gdal.Band_MeanOfNBands(bands)._add_parent_references(band_refs)

  Mean = mean


  def logical_and(x1, x2):
      """Perform a logical and between two objects, such objects being
         a raster band a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      x1 = Band._get_as_band_if_possible(x1)
      x2 = Band._get_as_band_if_possible(x2)
      if isinstance(x1, Band) and isinstance(x2, Band):
          return _gdal.Band_BinaryOpBand(x1, GRABO_LOGICAL_AND, x2)._add_parent_references([x1, x2])
      elif isinstance(x1, Band):
          return _gdal.Band_BinaryOpDouble(x1, GRABO_LOGICAL_AND, x2)._add_parent_references([x1])
      else:
          return _gdal.Band_BinaryOpDouble(x2, GRABO_LOGICAL_AND, x1)._add_parent_references([x2])

  LogicalAnd = logical_and


  def logical_or(x1, x2):
      """Perform a logical or between two objects, such objects being
         a raster band a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      x1 = Band._get_as_band_if_possible(x1)
      x2 = Band._get_as_band_if_possible(x2)
      if isinstance(x1, Band) and isinstance(x2, Band):
          return _gdal.Band_BinaryOpBand(x1, GRABO_LOGICAL_OR, x2)._add_parent_references([x1, x2])
      elif isinstance(x1, Band):
          return _gdal.Band_BinaryOpDouble(x1, GRABO_LOGICAL_OR, x2)._add_parent_references([x1])
      else:
          return _gdal.Band_BinaryOpDouble(x2, GRABO_LOGICAL_OR, x1)._add_parent_references([x2])

  LogicalOr = logical_or


  def logical_not(band):
      """Perform a logical not on a raster band or a numpy array.

         The resulting band is lazily evaluated.
      """
      band = Band._get_as_band_if_possible(band)
      return _gdal.Band_UnaryOp(band, GRAUO_LOGICAL_NOT)._add_parent_references([band])

  LogicalNot = logical_not

  def abs(band):
      """Return the absolute value (or module for complex data type) of a raster band or a numpy array.

         The resulting band is lazily evaluated.
      """
      band = Band._get_as_band_if_possible(band)
      return _gdal.Band_UnaryOp(band, GRAUO_ABS)._add_parent_references([band])

  Abs = abs

  def sqrt(band):
      """Return the square root of a raster band or a numpy array.

         The resulting band is lazily evaluated.
      """
      band = Band._get_as_band_if_possible(band)
      return _gdal.Band_UnaryOp(band, GRAUO_SQRT)._add_parent_references([band])

  Sqrt = sqrt

  def log10(band):
      """Return the logarithm base 10 of a raster band or a numpy array.

         The resulting band is lazily evaluated.
      """
      band = Band._get_as_band_if_possible(band)
      return _gdal.Band_UnaryOp(band, GRAUO_LOG10)._add_parent_references([band])

  Log10 = log10

  def log(band):
      """Return the natural logarithm of a raster band or a numpy array.

         The resulting band is lazily evaluated.
      """
      band = Band._get_as_band_if_possible(band)
      return _gdal.Band_UnaryOp(band, GRAUO_LOG)._add_parent_references([band])

  Log = log


  def pow(x1, x2):
      """Raise x1 to the power of x2 between two objects, such objects being
         a raster band a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      x1 = Band._get_as_band_if_possible(x1)
      x2 = Band._get_as_band_if_possible(x2)
      if isinstance(x1, Band) and isinstance(x2, Band):
          return _gdal.Band_BinaryOpBand(x1, GRABO_POW, x2)._add_parent_references([x1, x2])
      elif isinstance(x1, Band):
          return _gdal.Band_BinaryOpDouble(x1, GRABO_POW, x2)._add_parent_references([x1])
      else:
          return _gdal.Band_BinaryOpDoubleToBand(x1, GRABO_POW, x2)._add_parent_references([x2])

  Pow = pow

  class Window:
      def __init__(self, xoff, yoff, xsize, ysize):
          self.data = [xoff, yoff, xsize, ysize]

      def __copy__(self):
          return Window(*self.data)

      def __getitem__(self, i):
          return self.data[i]

      def __setitem__(self, i, value):
          self.data[i] = value

      def __iter__(self):
          return iter(self.data)

      def __eq__(self, other):
          if isinstance(other, Window):
              return self.data == other.data
          if type(other) is tuple:
              return tuple(self.data) == other
          return self.data == other

      def __repr__(self):
          return f'Window({self.data[0]}, {self.data[1]}, {self.data[2]}, {self.data[3]})'

      @property
      def xoff(self):
          return self.data[0]

      @xoff.setter
      def xoff(self, value):
          self.data[0] = value

      @property
      def yoff(self):
          return self.data[1]

      @yoff.setter
      def yoff(self, value):
          self.data[1] = value

      @property
      def xsize(self):
          return self.data[2]

      @xsize.setter
      def xsize(self, value):
          self.data[2] = value

      @property
      def ysize(self):
          return self.data[3]

      @ysize.setter
      def ysize(self, value):
          self.data[3] = value

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
        if( !GetUseExceptions() )
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
        if( GetUseExceptions() ) {
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
            if( GetUseExceptions() ) {
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


%pythonappend VSIFCloseL %{
    args[0].this = None
%}

%pythonprepend VSIFCloseL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend VSIFEofL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend VSIFFlushL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend wrapper_VSIFSeekL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend VSIFTellL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend VSIFTruncateL %{
    if args[0].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%pythonprepend wrapper_VSIFWriteL %{
    if args[3].this is None:
        raise ValueError("I/O operation on closed file.")
%}

%feature("pythonprepend") CPLSetThreadLocalConfigOption %{
    if type(args[1]) in (bool, int, float):
        args = (args[0], str(args[1]))
%}

%feature("pythonprepend") CPLSetConfigOption %{
    if type(args[1]) in (bool, int, float):
        args = (args[0], str(args[1]))
%}

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
      base = [gdalconst.CXT_Element, 'GCP']
      base.append([gdalconst.CXT_Attribute, 'Id', [gdalconst.CXT_Text, self.Id]])
      pixval = '%0.15E' % self.GCPPixel
      lineval = '%0.15E' % self.GCPLine
      xval = '%0.15E' % self.GCPX
      yval = '%0.15E' % self.GCPY
      zval = '%0.15E' % self.GCPZ
      base.append([gdalconst.CXT_Attribute, 'Pixel', [gdalconst.CXT_Text, pixval]])
      base.append([gdalconst.CXT_Attribute, 'Line', [gdalconst.CXT_Text, lineval]])
      base.append([gdalconst.CXT_Attribute, 'X', [gdalconst.CXT_Text, xval]])
      base.append([gdalconst.CXT_Attribute, 'Y', [gdalconst.CXT_Text, yval]])
      if with_Z:
          base.append([gdalconst.CXT_Attribute, 'Z', [gdalconst.CXT_Text, zval]])
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
                                 GDALGetDataTypeSizeBytes( ntype ),
                                 pixel_space, line_space, FALSE ) );
    if (buf_size == 0)
    {
        return CE_Failure;
    }

    char *data;
    Py_buffer view;
    if( !readraster_acquirebuffer(buf, inputOutputBuf, buf_size, ntype,
                                  GetUseExceptions(), data, view) )
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
%clear (void* inputOutputBuf);
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
    int nDataTypeSize = GDALGetDataTypeSizeBytes(ntype);
    size_t buf_size = static_cast<size_t>(nBlockXSize) *
                                                nBlockYSize * nDataTypeSize;

    *buf = NULL;

    char *data;
    Py_buffer view;

    if( !readraster_acquirebuffer(buf, buf_obj, buf_size, ntype,
                                  GetUseExceptions(), data, view) )
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

  def _add_parent_references(self, parents):
      if not hasattr(self, '_parent_references'):
          self._parent_references = set()
      for parent in parents:
          if hasattr(parent, '_parent_references'):
              for parent_of_parent in parent._parent_references:
                  if parent_of_parent not in self._parent_references:
                      parent_of_parent._add_child_ref(self)
                      self._parent_references.add(parent_of_parent)
          elif hasattr(parent, "_parent_ds"):
              parent_ds = parent._parent_ds()
              if parent_ds and parent_ds not in self._parent_references:
                  parent_ds._add_child_ref(self)
                  self._parent_references.add(parent_ds)
      return self

  @staticmethod
  def _get_as_band_if_possible(o):
        if hasattr(o, "shape"):
            from osgeo import gdal_array
            ds = gdal_array.OpenArray(o)
            if ds.RasterCount != 1:
                raise ValueError("numpy array must hold a single band")
            band = ds.GetRasterBand(1)
            band._hard_ref_to_parent = ds
            return band
        else:
            return o

  def __key(self):
      return str(self)

  def __hash__(self):
      return hash(self.__key())

  def __add__(self, other):
      """Add this raster band to a raster band, a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_ADD, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_ADD, other)._add_parent_references([self])

  def __radd__(self, constant):
      """Add a constant to this raster band

         The resulting band is lazily evaluated.
      """
      return _gdal.Band_BinaryOpDoubleToBand(constant, GRABO_ADD, self)._add_parent_references([self])

  def __sub__(self, other):
      """Subtract this raster band with a raster band, a numpy array or constant

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_SUB, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_SUB, other)._add_parent_references([self])

  def __rsub__(self, constant):
      """Subtract a constant with a raster band

         The resulting band is lazily evaluated.
      """
      return _gdal.Band_BinaryOpDoubleToBand(constant, GRABO_SUB, self)._add_parent_references([self])

  def __mul__(self, other):
      """Multiply this raster band with a raster band, a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_MUL, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_MUL, other)._add_parent_references([self])

  def __rmul__(self, constant):
      """Multiply a constant with this raster band

         The resulting band is lazily evaluated.
      """
      return _gdal.Band_BinaryOpDoubleToBand(constant, GRABO_MUL, self)._add_parent_references([self])

  def __truediv__(self, other):
      """Divide this raster band by a raster band, a numpy array or a constant

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_DIV, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_DIV, other)._add_parent_references([self])

  def __rtruediv__(self, constant):
      """Divide a constant by a raster band

         The resulting band is lazily evaluated.
      """
      return _gdal.Band_BinaryOpDoubleToBand(constant, GRABO_DIV, self)._add_parent_references([self])

  def __gt__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is greater than the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_GT, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_GT, other)._add_parent_references([self])

  def __ge__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is greater or equal to the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_GE, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_GE, other)._add_parent_references([self])

  def __lt__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is lesser than the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_LT, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_LT, other)._add_parent_references([self])

  def __le__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is lesser or equal to the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_LE, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_LE, other)._add_parent_references([self])

  def __eq__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is equal to the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_EQ, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_EQ, other)._add_parent_references([self])

  def __ne__(self, other):
      """Return a band whose value is 1 if the pixel value of the left operand
         is not equal to the pixel value of the right operand.

         The resulting band is lazily evaluated.
      """
      other = Band._get_as_band_if_possible(other)
      if isinstance(other, Band):
          return _gdal.Band_BinaryOpBand(self, GRABO_NE, other)._add_parent_references([self, other])
      else:
          return _gdal.Band_BinaryOpDouble(self, GRABO_NE, other)._add_parent_references([self])

  def astype(self, dt):
      """Cast this band to the specified data type

         The data type can be one of the constant of the GDAL ``GDT_`` enumeration
         or a numpy dtype.

         The resulting band is lazily evaluated.
      """
      if not isinstance(dt, int):
          try:
              from osgeo import gdal_array
              dt = gdal_array.NumericTypeCodeToGDALTypeCode(dt)
          except Exception:
              raise ValueError( "Invalid dt value")

      return _gdal.Band_AsType(self, dt)._add_parent_references([self])

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
      """
      Write the contents of a buffer to a dataset.

      """

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

  def ReadAsMaskedArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_type=None,
                  resample_alg=gdalconst.GRIORA_NearestNeighbour,
                  mask_resample_alg=gdalconst.GRIORA_NearestNeighbour,
                  callback=None,
                  callback_data=None):
      """
      Read a window of this raster band into a NumPy masked array.

      Values of the mask will be ``True`` where pixels are invalid.

      Starting in GDAL 3.11, if resampling (``buf_xsize`` != ``xsize``, or ``buf_ysize`` != ``ysize``) the mask
      band will be resampled using the algorithm specified by ``mask_resample_alg``.

      See :py:meth:`ReadAsArray` for a description of additional arguments.
      """
      import numpy
      array = self.ReadAsArray(xoff=xoff, yoff=yoff,
                               win_xsize=win_xsize, win_ysize=win_ysize,
                               buf_xsize=buf_xsize, buf_ysize=buf_ysize,
                               buf_type=buf_type,
                               resample_alg=resample_alg,
                               callback=callback, callback_data=callback_data)

      if self.GetMaskFlags() != GMF_ALL_VALID:
          mask = self.GetMaskBand()
          mask_array = ~mask.ReadAsArray(xoff=xoff,
                                         yoff=yoff,
                                         win_xsize=win_xsize,
                                         win_ysize=win_ysize,
                                         buf_xsize=buf_xsize,
                                         buf_ysize=buf_ysize,
                                         resample_alg=mask_resample_alg).astype(bool)
      else:
          mask_array = None
      return numpy.ma.array(array, mask=mask_array)


  def ReadAsArray(self, xoff=0, yoff=0, win_xsize=None, win_ysize=None,
                  buf_xsize=None, buf_ysize=None, buf_type=None, buf_obj=None,
                  resample_alg=gdalconst.GRIORA_NearestNeighbour,
                  callback=None,
                  callback_data=None):
      """
      Read a window of this raster band into a NumPy array.

      Parameters
      ----------
      xoff : float, default=0
         The pixel offset to left side of the region of the band to
         be read. This would be zero to start from the left side.
      yoff : float, default=0
         The line offset to top side of the region of the band to
         be read. This would be zero to start from the top side.
      win_xsize : float, optional
           The number of pixels to read in the x direction. By default,
           equal to the number of columns in the raster.
      win_ysize : float, optional
           The number of rows to read in the y direction. By default,
           equal to the number of bands in the raster.
      buf_xsize : int, optional
           The number of columns in the returned array. If not equal
           to ``win_xsize``, the returned values will be determined
           by ``resample_alg``.
      buf_ysize : int, optional
           The number of rows in the returned array. If not equal
           to ``win_ysize``, the returned values will be determined
           by ``resample_alg``.
      buf_type : int, optional
           The data type of the returned array
      buf_obj : np.ndarray, optional
           Optional buffer into which values will be read. If ``buf_obj``
           is specified, then ``buf_xsize``/``buf_ysize``/``buf_type``
           should generally not be specified.
      resample_alg : int, default = :py:const:`gdal.GRIORA_NearestNeighbour`.
           Specifies the resampling algorithm to use when the size of
           the read window and the buffer are not equal.
      callback : callable, optional
          A progress callback function
      callback_data : any, optional
          Optional data to be passed to callback function

      Returns
      -------
      np.ndarray

      Examples
      --------
      >>> import numpy as np
      >>> ds = gdal.GetDriverByName("GTiff").Create("test.tif", 4, 4, eType=gdal.GDT_Float32)
      >>> ds.WriteArray(np.arange(16).reshape(4, 4))
      0
      >>> band = ds.GetRasterBand(1)
      >>> # Reading an entire band
      >>> band.ReadAsArray()
      array([[ 0.,  1.,  2.,  3.],
             [ 4.,  5.,  6.,  7.],
             [ 8.,  9., 10., 11.],
             [12., 13., 14., 15.]], dtype=float32)
      >>> # Reading a window of a band
      >>> band.ReadAsArray(xoff=2, yoff=2, win_xsize=2, win_ysize=2)
      array([[10., 11.],
             [14., 15.]], dtype=float32)
      >>> # Reading a band into a new buffer at higher resolution
      >>> band.ReadAsArray(xoff=0.5, yoff=0.5, win_xsize=2.5, win_ysize=2.5, buf_xsize=5, buf_ysize=5)
      array([[ 0.,  1.,  1.,  2.,  2.],
             [ 4.,  5.,  5.,  6.,  6.],
             [ 4.,  5.,  5.,  6.,  6.],
             [ 8.,  9.,  9., 10., 10.],
             [ 8.,  9.,  9., 10., 10.]], dtype=float32)
      >>> # Reading a band into an existing buffer at lower resolution
      >>> band.ReadAsArray(buf_xsize=2, buf_ysize=2, buf_type=gdal.GDT_Float64, resample_alg=gdal.GRIORA_Average)
      array([[ 2.5,  4.5],
             [10.5, 12.5]])
      >>> buf = np.zeros((2,2))
      >>> band.ReadAsArray(buf_obj=buf)
      array([[ 5.,  7.],
             [13., 15.]])
      """
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
      """
      Write the contents of a NumPy array to a Band.

      Parameters
      ----------
      array : np.ndarray
          Two-dimensional array containing values to write
      xoff : int, default=0
         The pixel offset to left side of the region of the band to
         be written. This would be zero to start from the left side.
      yoff : int, default=0
         The line offset to top side of the region of the band to
         be written. This would be zero to start from the top side.
      resample_alg : int, default = :py:const:`gdal.GRIORA_NearestNeighbour`
         Resampling algorithm. Placeholder argument, not currently supported.
      callback : callable, optional
          A progress callback function
      callback_data : any, optional
          Optional data to be passed to callback function

      Returns
      -------
      int
          Error code, or ``gdal.CE_None`` if no error occurred.
      """
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
            virtualmem = self.GetVirtualMemAuto(eAccess, options)
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
            virtualmem = self.GetTiledVirtualMem(eAccess, xoff, yoff, xsize, ysize, tilexsize, tileysize, datatype, cache_size)
        else:
            virtualmem = self.GetTiledVirtualMem(eAccess, xoff, yoff, xsize, ysize, tilexsize, tileysize, datatype, cache_size, options)
        return gdal_array.VirtualMemGetArray( virtualmem )

  def BlockWindows(self):
       """Yield a window ``(xOff, yOff, xSize, ySize)`` corresponding to
       each block in this ``Band``. Iteration order is from left to right,
       then from top to bottom.

       Examples
       --------
       .. testsetup::
          >>> src_ds = gdal.Open("byte.tif")
          >>> dst_ds = gdal.GetDriverByName("MEM").Create("", 20, 20)
          >>> src_band = src_ds.GetRasterBand(1)
          >>> dst_band = dst_ds.GetRasterBand(1)

       >>> for window in src_band.BlockWindows():
       ...    values = src_band.ReadAsArray(*window)
       ...    dst_band.WriteArray(values + 20, window.xoff, window.yoff)
       0
       """
       import math
       blockXSize, blockYSize = self.GetBlockSize()
       nBlocksX = math.ceil(self.XSize / blockXSize)
       nBlocksY = math.ceil(self.YSize / blockYSize)
       for winrow in range(0, nBlocksY):
           yOff = winrow * blockYSize
           ySize = min(blockYSize, self.YSize - yOff)
           for wincol in range(0, nBlocksX):
               xOff = wincol * blockXSize
               xSize = min(blockXSize, self.XSize - xOff)
               yield Window(xOff, yOff, xSize, ySize)


%}

%feature("pythonappend") GetMaskBand %{
    if hasattr(self, '_parent_ds') and self._parent_ds():
        self._parent_ds()._add_child_ref(val)
%}

%feature("pythonappend") GetOverview %{
    if hasattr(self, '_parent_ds') and self._parent_ds():
        self._parent_ds()._add_child_ref(val)
%}

%feature("shadow") ComputeStatistics %{
def ComputeStatistics(self, *args, **kwargs):
    """ComputeStatistics(Band self, bool approx_ok, callback=None, callback_data=None)

    Compute image statistics.
    See :cpp:func:`GDALRasterBand::ComputeStatistics`.

    Parameters
    ----------
    approx_ok : bool
                 If ``True``, compute statistics based on overviews or a
                 subset of tiles.
    callback : callable, optional
                 A progress callback function
    callback_data : any, optional
                 Optional data to be passed to callback function

    Returns
    -------
    list
       a list with the min, max, mean, and standard deviation of values
       in the Band.

    See Also
    --------
    :py:meth:`ComputeBandStats`
    :py:meth:`ComputeRasterMinMax`
    :py:meth:`GetMaximum`
    :py:meth:`GetMinimum`
    :py:meth:`GetStatistics`
    :py:meth:`SetStatistics`
    """

    if len(args) == 1:
        kwargs["approx_ok"] = args[0]
        args = ()

    if "approx_ok" in kwargs:
        # Compatibility with older signature that used int for approx_ok
        if kwargs["approx_ok"] == 0:
            kwargs["approx_ok"] = False
        elif kwargs["approx_ok"] == 1:
            kwargs["approx_ok"] = True
        elif isinstance(kwargs["approx_ok"], int):
            raise Exception("approx_ok value should be 0/1/False/True")

    return $action(self, *args, **kwargs)
%}

%feature("shadow") GetNoDataValue %{
def GetNoDataValue(self):
    """GetNoDataValue(Band self) -> value

    Fetch the nodata value for this band.
    Unlike :cpp:func:`GDALRasterBand::GetNoDataValue`, this
    method handles 64-bit integer data types.

    Returns
    -------
    float or int
        The nodata value, or ``None`` if it has not been set.
    """

    if self.DataType == gdalconst.GDT_Int64:
        return _gdal.Band_GetNoDataValueAsInt64(self)

    if self.DataType == gdalconst.GDT_UInt64:
        return _gdal.Band_GetNoDataValueAsUInt64(self)

    return $action(self)
%}


%feature("shadow") SetNoDataValue %{
def SetNoDataValue(self, value):
    """SetNoDataValue(Band self, value)

    Set the nodata value for this band.
    Unlike :cpp:func:`GDALRasterBand::SetNoDataValue`, this
    method handles 64-bit integer types.

    Parameters
    ----------
    value : float or int
        The nodata value to set

    Returns
    -------
    int
       :py:const:`CE_None` on success or :py:const:`CE_Failure` on failure.

    """

    if self.DataType == gdalconst.GDT_Int64:
        return _gdal.Band_SetNoDataValueAsInt64(self, value)

    if self.DataType == gdalconst.GDT_UInt64:
        return _gdal.Band_SetNoDataValueAsUInt64(self, value)

    return $action(self, value)
%}

%feature("shadow") ComputeRasterMinMax %{
def ComputeRasterMinMax(self, *args, **kwargs):
    """ComputeRasterMinMax(Band self, bool approx_ok=False, bool can_return_none=False) -> (min, max) or None

    Computes the minimum and maximum values for this Band.
    See :cpp:func:`GDALComputeRasterMinMax`.

    Parameters
    ----------
    approx_ok : bool, default=False
        If ``False``, read all pixels in the band. If ``True``, check
        :py:meth:`GetMinimum`/:py:meth:`GetMaximum` or read a subsample.
    can_return_none : bool, default=False
        If ``True``, return ``None`` on error. Otherwise, return a tuple
        with NaN values.

    Returns
    -------
    tuple

    See Also
    --------
    :py:meth:`ComputeBandStats`
    :py:meth:`ComputeStatistics`
    :py:meth:`GetMaximum`
    :py:meth:`GetMinimum`
    :py:meth:`GetStatistics`
    :py:meth:`SetStatistics`
    """

    if len(args) == 1:
        kwargs["approx_ok"] = args[0]
        args = ()

    if "approx_ok" in kwargs:
        # Compatibility with older signature that used int for approx_ok
        if kwargs["approx_ok"] == 0:
            kwargs["approx_ok"] = False
        elif kwargs["approx_ok"] == 1:
            kwargs["approx_ok"] = True
        elif isinstance(kwargs["approx_ok"], int):
            raise Exception("approx_ok value should be 0/1/False/True")

    # can_return_null is used in other methods
    if "can_return_null" in kwargs:
        kwargs["can_return_none"] = kwargs["can_return_null"];
        del kwargs["can_return_null"]

    if "can_return_none" in kwargs and kwargs["can_return_none"]:
        try:
            return $action(self, *args, **kwargs)
        except Exception:
            return None
    else:
        return $action(self, *args, **kwargs)
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

    int nxsize = (buf_xsize==0) ? static_cast<int>(xsize) : *buf_xsize;
    int nysize = (buf_ysize==0) ? static_cast<int>(ysize) : *buf_ysize;
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

    int ntypesize = GDALGetDataTypeSizeBytes( ntype );
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
                                  GetUseExceptions(), data, view) )
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
                     pixel_space != (GIntBig)GDALGetRasterCount(self) * ntypesize )
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

    def ReadAsMaskedArray(self, xoff=0, yoff=0, xsize=None, ysize=None,
                    buf_xsize=None, buf_ysize=None, buf_type=None,
                    resample_alg=gdalconst.GRIORA_NearestNeighbour,
                    mask_resample_alg=gdalconst.GRIORA_NearestNeighbour,
                    callback=None,
                    callback_data=None,
                    band_list=None):
        """
        Read a window from raster bands into a NumPy masked array.

        Values of the mask will be ``True`` where pixels are invalid.

        If resampling (``buf_xsize`` != ``xsize``, or ``buf_ysize`` != ``ysize``) the mask band will be resampled
        using the algorithm specified by ``mask_resample_alg``.

        See :py:meth:`ReadAsArray` for a description of additional arguments.
        """

        import numpy as np

        arr = self.ReadAsArray(xoff=xoff, yoff=yoff, xsize=xsize, ysize=ysize,
                               buf_xsize=buf_xsize, buf_ysize=buf_ysize, buf_type=buf_type,
                               resample_alg=resample_alg, band_list=band_list)

        if band_list is None:
            band_list = [i+1 for i in range(self.RasterCount)]

        all_valid = all(self.GetRasterBand(band).GetMaskFlags() == GMF_ALL_VALID for band in band_list)

        if all_valid:
            return np.ma.masked_array(arr, False)

        masks = [self.GetRasterBand(band).GetMaskBand().ReadAsArray(
                xoff=xoff, yoff=yoff,
                win_xsize=xsize, win_ysize=ysize,
                buf_xsize=buf_xsize, buf_ysize=buf_ysize,
                resample_alg=mask_resample_alg) != 255
                 for band in band_list]

        return np.ma.masked_array(arr, np.vstack(masks))


    def ReadAsArray(self, xoff=0, yoff=0, xsize=None, ysize=None, buf_obj=None,
                    buf_xsize=None, buf_ysize=None, buf_type=None,
                    resample_alg=gdalconst.GRIORA_NearestNeighbour,
                    callback=None,
                    callback_data=None,
                    interleave='band',
                    band_list=None):
        """
        Read a window from raster bands into a NumPy array.

        Parameters
        ----------
        xoff : float, default=0
           The pixel offset to left side of the region of the band to
           be read. This would be zero to start from the left side.
        yoff : float, default=0
           The line offset to top side of the region of the band to
           be read. This would be zero to start from the top side.
        xsize : float, optional
             The number of pixels to read in the x direction. By default,
             equal to the number of columns in the raster.
        ysize : float, optional
             The number of rows to read in the y direction. By default,
             equal to the number of bands in the raster.
        buf_xsize : int, optional
             The number of columns in the returned array. If not equal
             to ``win_xsize``, the returned values will be determined
             by ``resample_alg``.
        buf_ysize : int, optional
             The number of rows in the returned array. If not equal
             to ``win_ysize``, the returned values will be determined
             by ``resample_alg``.
        buf_type : int, optional
             The data type of the returned array
        buf_obj : np.ndarray, optional
             Optional buffer into which values will be read. If ``buf_obj``
             is specified, then ``buf_xsize``/``buf_ysize``/``buf_type``
             should generally not be specified.
        resample_alg : int, default = :py:const:`gdal.GRIORA_NearestNeighbour`.
             Specifies the resampling algorithm to use when the size of
             the read window and the buffer are not equal.
        callback : callable, optional
            A progress callback function
        callback_data : any, optional
            Optional data to be passed to callback function
        band_list : list, optional
            Indexes of bands from which data should be read. By default,
            data will be read from all bands.

        Returns
        -------
        np.ndarray

        Examples
        --------
        >>> ds = gdal.GetDriverByName("GTiff").Create("test.tif", 4, 4, bands=2)
        >>> ds.WriteArray(np.arange(32).reshape(2, 4, 4))
        0
        >>> ds.ReadAsArray()
        array([[[ 0,  1,  2,  3],
                [ 4,  5,  6,  7],
                [ 8,  9, 10, 11],
                [12, 13, 14, 15]],
               [[16, 17, 18, 19],
                [20, 21, 22, 23],
                [24, 25, 26, 27],
                [28, 29, 30, 31]]], dtype=uint8)
        >>> ds.ReadAsArray(xoff=2, yoff=2, xsize=2, ysize=2)
        array([[[10, 11],
                [14, 15]],
               [[26, 27],
                [30, 31]]], dtype=uint8)
        >>> ds.ReadAsArray(buf_xsize=2, buf_ysize=2, buf_type=gdal.GDT_Float64, resample_alg=gdal.GRIORA_Average)
        array([[[ 3.,  5.],
                [11., 13.]],
               [[19., 21.],
                [27., 29.]]])
        >>> buf = np.zeros((2,2,2))
        >>> ds.ReadAsArray(buf_obj=buf)
        array([[[ 5.,  7.],
                [13., 15.]],
               [[21., 23.],
                [29., 31.]]])
        >>> ds.ReadAsArray(band_list=[2,1])
        array([[[16, 17, 18, 19],
                [20, 21, 22, 23],
                [24, 25, 26, 27],
                [28, 29, 30, 31]],
               [[ 0,  1,  2,  3],
                [ 4,  5,  6,  7],
                [ 8,  9, 10, 11],
                [12, 13, 14, 15]]], dtype=uint8)
        """

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
        """
        Write the contents of a NumPy array to a Dataset.

        Parameters
        ----------
        array : np.ndarray
            Two- or three-dimensional array containing values to write
        xoff : int, default=0
           The pixel offset to left side of the region of the band to
           be written. This would be zero to start from the left side.
        yoff : int, default=0
           The line offset to top side of the region of the band to
           be written. This would be zero to start from the top side.
        band_list : list, optional
            Indexes of bands to which data should be written. By default,
            it is assumed that the Dataset contains the same number of
            bands as levels in ``array``.
        interleave : str, default="band"
            Interleaving, "band" or "pixel". For band-interleaved writing,
            ``array`` should have shape ``(nband, ny, nx)``. For pixel-
            interleaved-writing, ``array`` should have shape
            ``(ny, nx, nbands)``.
        resample_alg : int, default = :py:const:`gdal.GRIORA_NearestNeighbour`
            Resampling algorithm. Placeholder argument, not currently supported.
        callback : callable, optional
            A progress callback function
        callback_data : any, optional
            Optional data to be passed to callback function

        Returns
        -------
        int
            Error code, or ``gdal.CE_None`` if no error occurred.

        Examples
        --------

        >>> import numpy as np
        >>>
        >>> nx = 4
        >>> ny = 3
        >>> nbands = 2
        >>> with gdal.GetDriverByName("GTiff").Create("band3_px.tif", nx, ny, bands=nbands) as ds:
        ...     data = np.arange(nx*ny*nbands).reshape(ny,nx,nbands)
        ...     ds.WriteArray(data, interleave="pixel")
        ...     ds.ReadAsArray()
        ...
        0
        array([[[ 0,  2,  4,  6],
                [ 8, 10, 12, 14],
                [16, 18, 20, 22]],
               [[ 1,  3,  5,  7],
                [ 9, 11, 13, 15],
                [17, 19, 21, 23]]], dtype=uint8)
        >>> with gdal.GetDriverByName("GTiff").Create("band3_band.tif", nx, ny, bands=nbands) as ds:
        ...     data = np.arange(nx*ny*nbands).reshape(nbands, ny, nx)
        ...     ds.WriteArray(data, interleave="band")
        ...     ds.ReadAsArray()
        ...
        0
        array([[[ 0,  1,  2,  3],
                [ 4,  5,  6,  7],
                [ 8,  9, 10, 11]],
               [[12, 13, 14, 15],
                [16, 17, 18, 19],
                [20, 21, 22, 23]]], dtype=uint8)
        """

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
            virtualmem = self.GetTiledVirtualMem(eAccess, xoff, yoff, xsize, ysize, tilexsize, tileysize, datatype, band_list, tile_organization, cache_size)
        else:
            virtualmem = self.GetTiledVirtualMem(eAccess, xoff, yoff, xsize, ysize, tilexsize, tileysize, datatype, band_list, tile_organization, cache_size, options)
        return gdal_array.VirtualMemGetArray( virtualmem )

    def GetSubDatasets(self):
        """
        Return a list of Subdatasets.


        Returns
        -------
        list

        """
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
            nRequiredSize = int(buf_xsize * buf_ysize * len(band_list) * _gdal.GetDataTypeSizeBytes(buf_type))
            if version_info >= (3, 0, 0):
                buf_obj_ar = [None]
                exec("buf_obj_ar[0] = b' ' * nRequiredSize")
                buf_obj = buf_obj_ar[0]
            else:
                buf_obj = ' ' * nRequiredSize
        return _gdal.Dataset_BeginAsyncReader(self, xoff, yoff, xsize, ysize, buf_obj, buf_xsize, buf_ysize, buf_type, band_list,  0, 0, 0, options)

    def GetLayer(self, iLayer=0):
        """
        Get the indicated layer from the Dataset

        Parameters
        ----------
        value : int or str
                Name or 0-based index of the layer to delete.

        Returns
        -------
        Layer
            A layer if successful, or ``None`` on error.
        """

        _WarnIfUserHasNotSpecifiedIfUsingOgrExceptions()

        if isinstance(iLayer, str):
            return self.GetLayerByName(str(iLayer))
        elif isinstance(iLayer, int):
            return self.GetLayerByIndex(iLayer)
        else:
            raise TypeError("Input %s is not of String or Int type" % type(iLayer))

    def DeleteLayer(self, value):
        """
        Delete the indicated layer from the Dataset.

        Parameters
        ----------
        value : int or str
                Name or 0-based index of the layer to delete.

        Returns
        -------
        int
            :py:const:`ogr.OGRERR_NONE` on success or
            :py:const:`ogr.OGRERR_UNSUPPORTED_OPERATION` if DeleteLayer is not supported
            for this dataset.
        """
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
        """
        Assign GCPs.

        See :cpp:func:`GDALSetGCPs`.

        Parameters
        ----------
        gcps : list
               a list of :py:class:`GCP` objects
        wkt_or_spatial_ref : str or SpatialReference
               spatial reference of the GCPs as a string, or a :py:class:`osr.SpatialReference`
        """

        if isinstance(wkt_or_spatial_ref, str):
            return self._SetGCPs(gcps, wkt_or_spatial_ref)
        else:
            return self._SetGCPs2(gcps, wkt_or_spatial_ref)

    def Destroy(self):
        import warnings
        warnings.warn("Destroy() is deprecated; use a context manager or Close() instead", DeprecationWarning)
        self._invalidate_children()
        try:
            return _gdal.Dataset_Close(self)
        finally:
            self.thisown = 0
            self.this = None

    def Release(self):
        import warnings
        warnings.warn("Release() is deprecated; use a context manager or Close() instead", DeprecationWarning)
        self._invalidate_children()
        try:
            return _gdal.Dataset_Close(self)
        finally:
            self.thisown = 0
            self.this = None

    def SyncToDisk(self):
        return self.FlushCache()

    def GetName(self):
        return self.GetDescription()

    def _add_child_ref(self, child):
        if child is None:
            return

        import weakref

        if not hasattr(self, '_child_references'):
            self._child_references = weakref.WeakSet()

        self._child_references.add(child)
        child._parent_ds = weakref.ref(self)

    def _invalidate_children(self):
        if hasattr(self, '_child_references'):
            for child in self._child_references:
                child.this = None

    def __del__(self):
        self._invalidate_children()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.Close()

    def __bool__(self):
        return True

    def __len__(self):
        return self.RasterCount + self.GetLayerCount()

    def __iter__(self):
        if self.RasterCount:
            for band in range(1, self.RasterCount + 1):
                yield self[band]
        else:
            for layer in range(self.GetLayerCount()):
                yield self[layer]

    def __getitem__(self, value):
        """Support dictionary, list, and slice -like access to the datasource.
        ds[0] would return the first layer on the datasource.
        ds['aname'] would return the layer named "aname".
        ds[0:4] would return a list of the first four layers."""

        if self.RasterCount and self.GetLayerCount():
            raise ValueError("Cannot access slice of Dataset with both raster bands and vector layers")

        if self.GetLayerCount():
            get = self.GetLayer
            min = 0
            max = self.GetLayerCount() - 1
        else:
            get = self.GetRasterBand
            min = 1
            max = self.RasterCount

        if isinstance(value, slice):
            output = []
            step = value.step if value.step else 1
            for i in range(value.start, value.stop, step):
                lyr = self.GetLayer(i)
                if lyr is None:
                    return output
                output.append(lyr)
            return output

        if value < min or value > max:
            # Exception needed to make for _ in loop finish
            raise IndexError(value)

        return get(value)
%}

%feature("pythonappend") GetThreadSafeDataset %{
    if val:
        val._parent_ds = self

        import weakref
        if not hasattr(self, '_child_references'):
            self._child_references = weakref.WeakSet()
        self._child_references.add(val)
%}

%feature("shadow") Close %{
    def Close(self, callback=None, callback_data=None):
        r"""
        Close(Dataset self, callback=Callable|None, callback_data=any|None) -> CPLErr

        Closes opened dataset and releases allocated resources.

        This method can be used to force the dataset to close
        when one more references to the dataset are still
        reachable. If :py:meth:`Close` is never called, the dataset will
        be closed automatically during garbage collection.

        It is illegal to call any method on the dataset or objects derived
        from it (bands, layers, etc.) afterwards.

        In most cases, it is preferable to open or create a dataset
        using a context manager instead of calling :py:meth:`Close`
        directly.

        This function may report progress if a progress
        callback if provided and if the dataset returns True for
        GetCloseReportsProgress()

        Parameters
        ----------
        callback: Callable|None
            Callable that accepts (pct: float, message: str, user_data) and returns bool
        callback_data: any|None
            User data to pass to the callback
        """

        self._invalidate_children()
        if self.GetRefCount() == 1 and self.thisown:
            try:
                return _gdal.Dataset_Close(self, callback, callback_data)
            finally:
                self.thisown = 0
                self.this = None
        else:
            return _gdal.Dataset__RunCloseWithoutDestroying(self, callback, callback_data)
%}

%feature("shadow") ExecuteSQL %{
def ExecuteSQL(self, statement, spatialFilter=None, dialect="", keep_ref_on_ds=False):
    """ExecuteSQL(self, statement, spatialFilter: ogr.Geometry = None, dialect: Optional[str] = "", keep_ref_on_ds=False) -> ogr.Layer

    Execute a SQL statement against the dataset

    The result of a SQL query is:
      - None (or an exception if exceptions are enabled) for statements
        that are in error
      - or None for statements that have no results set,
      - or a :py:class:`ogr.Layer` handle representing a results set from the query.

    Note that this :py:class:`ogr.Layer` is in addition to the layers in the data store
    and must be released with :py:meth:`ReleaseResultSet` before the data source is closed
    (destroyed).

    Starting with GDAL 3.7, this method can also be used as a context manager,
    as a convenient way of automatically releasing the returned result layer.

    For more information on the SQL dialect supported internally by OGR
    review the OGR SQL document (:ref:`ogr_sql_sqlite_dialect`)
    Some drivers (i.e. Oracle and PostGIS) pass the SQL directly through to the
    underlying RDBMS.

    The SQLITE dialect can also be used (:ref:`sql_sqlite_dialect`)

    Parameters
    ----------
    statement : str
        the SQL statement to execute (e.g "SELECT * FROM layer")
    spatialFilter : any
        a geometry which represents a spatial filter. Can be None
    dialect : str
        allows control of the statement dialect. If set to None or empty string,
        the OGR SQL engine will be used, except for RDBMS drivers that will
        use their dedicated SQL engine, unless OGRSQL is explicitly passed as
        the dialect. The SQLITE dialect can also be used.
    keep_ref_on_ds : bool
        whether the returned layer should keep a (strong) reference on
        the current dataset. Cf example 2 for a use case.

    Returns
    -------
    Layer
        a layer containing the results of the query, that will be
        automatically released when the context manager goes out of scope.

    Examples
    --------

    .. testsetup::

       >>> src_ds = gdal.OpenEx("poly.shp", gdal.OF_VECTOR)
       >>> ds = gdal.GetDriverByName("MEM").CreateVector("")
       >>> _ = ds.CopyLayer(src_ds.GetLayer(0), "layer")

    1. Use as a context manager:

    >>> with ds.ExecuteSQL("SELECT * FROM layer") as lyr:
    ...     print(lyr.GetFeatureCount())
    10

    2. Use keep_ref_on_ds=True to return an object that keeps a reference to its dataset:

    >>> def get_sql_lyr():
    ...     return gdal.OpenEx("poly.shp", gdal.OF_VECTOR).ExecuteSQL("SELECT * FROM poly", keep_ref_on_ds=True)
    ...
    >>> with get_sql_lyr() as lyr:
    ...     print(lyr.GetFeatureCount())
    10
    """

    class MyHandler:
        def __init__(self):
            self.errors = []

        def callback(self, err_type, err_no, err_msg):
            self.errors.append([err_type, err_no, err_msg])

    my_error_handler = MyHandler()
    if GetUseExceptions():
        with ExceptionMgr(useExceptions=False):
            PushErrorHandler(my_error_handler.callback)
            try:
                sql_lyr = $action(self, statement, spatialFilter, dialect)
            finally:
                PopErrorHandler()
    else:
        sql_lyr = $action(self, statement, spatialFilter, dialect)
    if sql_lyr:
        import weakref
        sql_lyr._to_release = True
        sql_lyr._dataset_weak_ref = weakref.ref(self)
        if keep_ref_on_ds:
            sql_lyr._dataset_strong_ref = self

    if my_error_handler.errors and my_error_handler.errors[-1][0] == CE_Failure:
        if sql_lyr:
            self.ReleaseResultSet(sql_lyr)
        raise RuntimeError(my_error_handler.errors[-1][2])

    elif my_error_handler.errors:
        for err_type, err_no, err_msg in my_error_handler.errors:
            if err_type == CE_Warning:
                Error(err_type, err_no, err_msg)

    return sql_lyr
%}

%feature("shadow") ReleaseResultSet %{
def ReleaseResultSet(self, sql_lyr):
    """ReleaseResultSet(self, sql_lyr: ogr.Layer)

    Release :py:class:`ogr.Layer` returned by :py:meth:`ExecuteSQL` (when not called as a context manager)

    The sql_lyr object is invalidated after this call.

    Parameters
    ----------
    sql_lyr : Layer
        :py:class:`ogr.Layer` got with :py:meth:`ExecuteSQL`
    """

    if sql_lyr and not hasattr(sql_lyr, "_to_release"):
        raise Exception("This layer was not returned by ExecuteSQL() and should not be released with ReleaseResultSet()")
    $action(self, sql_lyr)
    # Invalidates the layer
    if sql_lyr:
        sql_lyr.thisown = None
        sql_lyr.this = None
%}

%feature("shadow") GetInterBandCovarianceMatrix %{
def GetInterBandCovarianceMatrix(self,
                             band_list=None,
                             approx_ok=False,
                             force=False,
                             write_into_metadata=True,
                             delta_degree_of_freedom=1,
                             callback=None,
                             callback_data=None):
    """
    Fetch or compute the covariance matrix between bands of this dataset.

    The covariance indicates the level to which two bands vary together.

    If we call :math:`v_i[y,x]` the value of pixel at row=y and column=x for band i,
    and :math:`mean_i` the mean value of all pixels of band i, then

    .. math::

        \\mathrm{cov}\\left[i,j\\right] =
        \\frac{
            \\sum_{y,x} \\left( v_{i}[y,x] - \\mathrm{mean}_{i} \\right)
            \\left( v_{j}[y,x] - \\mathrm{mean}_{j} \\right)
        }{
            \\mathrm{pixel\\_count} - \\mathrm{delta\\_degree\\_of\\_freedom}
        }

    When there are no nodata values, :math:`pixel\\_count = self.RasterXSize * self.RasterYSize`.
    We can see that :math:`cov[i,j] = cov[j,i]`, and consequently the returned matrix
    is symmetric.

    A value of delta_degree_of_freedom=1 (the default) will return a unbiased estimate
    if the pixels in bands are considered to be a sample of the whole population.
    This is consistent with the default of
    https://numpy.org/doc/stable/reference/generated/numpy.cov.html and the returned
    matrix is consistent with what can be obtained with

    .. code-block:: python

       numpy.cov(
          [ds.GetRasterBand(band_nr).ReadAsArray().ravel() for band_nr in band_list]
       )

    Otherwise a value of delta_degree_of_freedom=0 can be used if they are considered
    to be the whole population.

    If STATISTICS_COVARIANCES metadata items are available in band metadata,
    this method uses them.
    Otherwise, if bForce is true, :py:meth:`ComputeInterBandCovarianceMatrix` is called.
    Otherwise, if bForce is false, an empty vector is returned

    Parameters
    ----------
    band_list: list[int], optional
        If not specified, compute the covariance matrix of all bands of the dataset.
        Otherwise compute it on the subset of bands specified by band_list.
        Values in band_list must be between 1 and self.RasterCount.
    approx_ok : bool, optional
        Whether it is acceptable to use a subsample of values in
        :py:meth:`ComputeInterBandCovarianceMatrix`
    force : bool | None, optional
        Whether :py:meth:`ComputeInterBandCovarianceMatrix` should be called
        when the STATISTICS_COVARIANCES metadata items are missing.
    write_into_metadata : bool, optional
        Whether :py:meth:`ComputeInterBandCovarianceMatrix` must
        write STATISTICS_COVARIANCES band metadata items.
    delta_degree_of_freedom : int, optional
        Correction term to subtract in the final averaging phase of the covariance computation.
    callback : callable, optional
        A progress callback function
    callback_data : any, optional
        Optional data to be passed to callback function

    Returns
    -------
    List[List[float]]
        a list of len(band_list) of lists of len(band_list) values (where len(band_list) == self.RasterCount if band_list not set)

    Examples
    --------
    .. testsetup::
       >>> ds = gdal.Open('rgbsmall.tif')

    >>> print(ds.GetInterBandCovarianceMatrix(force=True))
    [[2241.7045363745387, 2898.8196128051163, 1009.979953581434], [2898.8196128051163, 3900.269159023618, 1248.65396718687], [1009.979953581434, 1248.65396718687, 602.4703641456648]] # rtol: 1e-6
    """

    if band_list is None:
        band_list = list(range(1, self.RasterCount + 1))
    if not band_list:
        return []
    return _gdal.Dataset_GetInterBandCovarianceMatrix(
              self,
              nBandCount=band_list,
              approx_ok=approx_ok,
              force=force,
              write_into_metadata=write_into_metadata,
              delta_degree_of_freedom=delta_degree_of_freedom,
              callback=callback,
              callback_data=callback_data)
%}


%feature("shadow") ComputeInterBandCovarianceMatrix %{
def ComputeInterBandCovarianceMatrix(self,
                                     band_list=None,
                                     approx_ok=False,
                                     write_into_metadata=True,
                                     delta_degree_of_freedom=1,
                                     callback=None,
                                     callback_data=None):
    """
    Compute the covariance matrix between bands of this dataset.

    The covariance indicates the level to which two bands vary together.

    If we call :math:`v_i[y,x]` the value of pixel at row=y and column=x for band i,
    and :math:`mean_i` the mean value of all pixels of band i, then

    .. math::

        \\mathrm{cov}\\left[i,j\\right] =
        \\frac{
            \\sum_{y,x} \\left( v_{i}[y,x] - \\mathrm{mean}_{i} \\right)
            \\left( v_{j}[y,x] - \\mathrm{mean}_{j} \\right)
        }{
            \\mathrm{pixel\\_count} - \\mathrm{delta\\_degree\\_of\\_freedom}
        }

    When there are no nodata values, :math:`pixel\\_count = self.RasterXSize * self.RasterYSize`.
    We can see that :math:`cov[i,j] = cov[j,i]`, and consequently the returned matrix
    is symmetric.

    A value of delta_degree_of_freedom=1 (the default) will return a unbiased estimate
    if the pixels in bands are considered to be a sample of the whole population.
    This is consistent with the default of
    https://numpy.org/doc/stable/reference/generated/numpy.cov.html and the returned
    matrix is consistent with what can be obtained with

    .. code-block:: python

       numpy.cov(
          [ds.GetRasterBand(band_nr).ReadAsArray().ravel() for band_nr in band_list]
       )

    Otherwise a value of delta_degree_of_freedom=0 can be used if they are considered
    to be the whole population.

    If STATISTICS_COVARIANCES metadata items are available in band metadata,
    this method uses them.
    Otherwise, if bForce is true, :py:meth:`ComputeInterBandCovarianceMatrix` is called.
    Otherwise, if bForce is false, an empty vector is returned

    Parameters
    ----------
    band_list: list[int], optional
        If not specified, compute the covariance matrix of all bands of the dataset.
        Otherwise compute it on the subset of bands specified by band_list.
        Values in band_list must be between 1 and self.RasterCount.
    approx_ok : bool, optional
        Whether it is acceptable to use a subsample of values
    write_into_metadata : bool, optional
        Whether this method must write STATISTICS_COVARIANCES band metadata items.
    delta_degree_of_freedom : int, optional
        Correction term to subtract in the final averaging phase of the covariance computation.
    callback : callable, optional
        A progress callback function
    callback_data : any, optional
        Optional data to be passed to callback function

    Returns
    -------
    List[List[float]]
        a list of len(band_list) of lists of len(band_list) values (where len(band_list) == self.RasterCount if band_list not set)

    Examples
    --------
    .. testsetup::
       >>> ds = gdal.Open('rgbsmall.tif')

    >>> print(ds.ComputeInterBandCovarianceMatrix())
    [[2241.7045363745387, 2898.8196128051163, 1009.979953581434], [2898.8196128051163, 3900.269159023618, 1248.65396718687], [1009.979953581434, 1248.65396718687, 602.4703641456648]] # rtol: 1e-6
    """

    if band_list is None:
        band_list = list(range(1, self.RasterCount + 1))
    if not band_list:
        return []
    return _gdal.Dataset_ComputeInterBandCovarianceMatrix(
              self,
              nBandCount=band_list,
              approx_ok=approx_ok,
              write_into_metadata=write_into_metadata,
              delta_degree_of_freedom=delta_degree_of_freedom,
              callback=callback,
              callback_data=callback_data)
%}

%feature("pythonappend") GetRasterBand %{
    self._add_child_ref(val)
%}

%feature("pythonappend") GetLayerByName %{
    self._add_child_ref(val)
%}

%feature("pythonappend") GetLayerByIndex %{
    self._add_child_ref(val)
%}

%feature("pythonappend") CreateLayer %{
    self._add_child_ref(val)
%}

%feature("pythonappend") CopyLayer %{
    self._add_child_ref(val)
%}

}

%extend GDALMajorObjectShadow {
%pythoncode %{
  def GetMetadata(self, domain=''):
    if domain and (domain[:4] == 'xml:' or domain[:5] == 'json:'):
      return self.GetMetadata_List(domain)
    return self.GetMetadata_Dict(domain)
%}
}

%extend GDALRasterAttributeTableShadow {
%pythoncode %{


  def GetValueAsDateTime(self, iRow, iCol):
      """
      Fetch field value as a datetime.

      The value of the requested column in the requested row is returned
      as a Python datetime. Besides being called on a GFT_DateTime field, it
      is also possible to call this method on a string field that contains a
      ISO-8601 encoded datetime.

      Parameters
      ----------
      iRow : int
          The index of the row to read (starting at 0)
      iCol : int
          The index of the column to read (starting at 0)

      Returns
      -------
      datetime
          Datetime value, or None if it is invalid
      """

      import datetime
      import math
      RAT_dt = _gdal.RasterAttributeTable_GetValueAsDateTime(self, iRow, iCol)
      if not RAT_dt.bIsValid:
          return None
      delta = RAT_dt.nTimeZoneHour * 3600 + RAT_dt.nTimeZoneMinute * 60
      if not RAT_dt.bPositiveTimeZone:
          delta = -delta
      tz = datetime.timezone(datetime.timedelta(seconds=delta))
      return datetime.datetime(RAT_dt.nYear, RAT_dt.nMonth, RAT_dt.nDay,
                               RAT_dt.nHour, RAT_dt.nMinute, int(RAT_dt.fSecond),
                               int(math.fmod(RAT_dt.fSecond, 1) * 1e6 + 0.5),
                               tz)

  def SetValueAsDateTime(self, iRow, iCol, dt):
      """
      Set field value from a datetime.

      The indicated field (column) on the indicated row is set from the
      passed value.  The value will be automatically converted for other field
      types, with a possible loss of precision.

      Parameters
      ----------
      iRow : int
          The index of the row to read (starting at 0)
      iCol : int
          The index of the column to read (starting at 0)
      dt : datetime | RATDateTime | None
          The datetime value
      """

      if dt is None:
          RAT_dt = RATDateTime()
          RAT_dt.bIsValid = False
      elif isinstance(dt, RATDateTime):
          RAT_dt = dt
      else:
          import datetime
          if not isinstance(dt, datetime.datetime):
              raise ValueError("dt is not a datetime.datetime instance")
          RAT_dt = RATDateTime()
          RAT_dt.nYear = dt.year
          RAT_dt.nMonth = dt.month
          RAT_dt.nDay = dt.day
          RAT_dt.nHour = dt.hour
          RAT_dt.nMinute = dt.minute
          RAT_dt.fSecond = dt.second + dt.microsecond * 1e-6
          RAT_dt.bPositiveTimeZone = False
          RAT_dt.nTimeZoneHour = 0
          RAT_dt.nTimeZoneMinute = 0
          RAT_dt.bIsValid = True
          tzinfo = dt.tzinfo
          if tzinfo:
              offset = tzinfo.utcoffset(dt)
              delta_minutes = offset.days * 24 * 60 + offset.seconds // 60
              if delta_minutes >= 0:
                  RAT_dt.bPositiveTimeZone = True
              else:
                  RAT_dt.bPositiveTimeZone = False
                  delta_minutes = -delta_minutes
              RAT_dt.nTimeZoneHour = delta_minutes // 60;
              RAT_dt.nTimeZoneMinute = delta_minutes % 60;

      _gdal.RasterAttributeTable_SetValueAsDateTime(self, iRow, iCol, RAT_dt)

  def WriteArray(self, array, field, start=0):
      """
      Write a NumPy array to a single column of a RAT.

      Parameters
      ----------
      array : np.ndarray
          One-dimensional array of values to write
      field : int
          The index of the column to write (starting at 0)
      start : int, default = 0
          The index of the first row to write (starting at 0)

      Returns
      -------
      int
          Error code, or ``gdal.CE_None`` if no error occurred.
      """
      from osgeo import gdal_array

      return gdal_array.RATWriteArray(self, array, field, start)

  def ReadAsArray(self, field, start=0, length=None):
      """
      Read a single column of a RAT into a NumPy array.

      Parameters
      ----------
      field : int
          The index of the column to read (starting at 0)
      start : int, default = 0
          The index of the first row to read (starting at 0)
      length : int, default = None
          The number of rows to read


      Returns
      -------
      np.ndarray

      Examples
      --------

      .. testsetup::
         >>> pytest.skip()

      >>> ds = gdal.Open('clc2018_v2020_20u1.tif')
      >>> rat = ds.GetRasterBand(1).GetDefaultRAT()
      >>> rat.ReadAsArray(0)
      array([ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
             18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
             35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 48], dtype=int32)
      >>> rat.ReadAsArray(0, 5, 3)
      array([6, 7, 8], dtype=int32)
      """

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

%extend GDALGroupHS {

%feature("shadow") GetGroupNames %{
def GetGroupNames(self, options = []) -> "list[str]":
    ret = $action(self, options)
    if ret is None:
        ret = []
    return ret
%}

%feature("shadow") GetMDArrayNames %{
def GetMDArrayNames(self, options = []) -> "list[str]":
    ret = $action(self, options)
    if ret is None:
        ret = []
    return ret
%}

%pythoncode %{
  def GetDataTypes(self):
      """Return data types associated with that group (typically enumerations)
      """
      return [self.GetDataType(i) for i in range(self.GetDataTypeCount())]
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
        if buffer_datatype.GetClass() == GEDTC_NUMERIC and buffer_datatype.GetNumericDataType() == gdalconst.GDT_Float16:
          buffer_datatype = ExtendedDataType.Create(GDT_Float32)
        elif buffer_datatype.GetClass() == GEDTC_NUMERIC and buffer_datatype.GetNumericDataType() == gdalconst.GDT_CFloat16:
          buffer_datatype = ExtendedDataType.Create(GDT_CFloat32)
      return _gdal.MDArray_Read(self, array_start_idx, count, array_step, buffer_stride, buffer_datatype)

  def ReadAsArray(self,
                  array_start_idx = None,
                  count = None,
                  array_step = None,
                  buffer_datatype = None,
                  buf_obj = None):

      from osgeo import gdal_array
      return gdal_array.MDArrayReadAsArray(self, array_start_idx, count, array_step, buffer_datatype, buf_obj)

  def AdviseRead(self, array_start_idx = None, count = None, options = []):
      if not array_start_idx:
        array_start_idx = [0] * self.GetDimensionCount()
      if not count:
        count = [ (self.GetDimensions()[i].GetSize() - array_start_idx[i]) for i in range (self.GetDimensionCount()) ]
      return _gdal.MDArray_AdviseRead(self, array_start_idx, count, options)

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
             ('B', 1): GDT_Byte,
             ('b', 1): GDT_Int8,
             ('h', 2): GDT_Int16,
             ('H', 2): GDT_UInt16,
             ('i', 4): GDT_Int32,
             ('I', 4): GDT_UInt32,
             ('l', 4): GDT_Int32,
             ('q', 8): GDT_Int64,
             ('Q', 8): GDT_UInt64,
             ('e', 2): GDT_Float16,
             ('f', 4): GDT_Float32,
             ('d', 8): GDT_Float64,
             # ('E', 2): GDT_CFloat16,
             ('F', 4): GDT_CFloat32,
             ('D', 8): GDT_CFloat64
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

      is_0d_or_1d_string = self.GetDataType().GetClass() == GEDTC_STRING and buffer_datatype.GetClass() == GEDTC_STRING and dimCount <= 1

      if not array_start_idx:
        array_start_idx = [0] * dimCount

      if not count:
        if is_0d_or_1d_string:
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

      if is_0d_or_1d_string:
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


  def GetNoDataValue(self):
    """GetNoDataValue(MDArray self) -> value """

    dt = self.GetDataType()
    if dt.GetClass() == GEDTC_NUMERIC and dt.GetNumericDataType() == gdalconst.GDT_Int64:
        return _gdal.MDArray_GetNoDataValueAsInt64(self)

    if dt.GetClass() == GEDTC_NUMERIC and dt.GetNumericDataType() == gdalconst.GDT_UInt64:
        return _gdal.MDArray_GetNoDataValueAsUInt64(self)

    return _gdal.MDArray_GetNoDataValueAsDouble(self)


  def SetNoDataValue(self, value):
    """SetNoDataValue(MDArray self, value) -> CPLErr"""

    dt = self.GetDataType()
    if dt.GetClass() == GEDTC_NUMERIC and dt.GetNumericDataType() == gdalconst.GDT_Int64:
        return _gdal.MDArray_SetNoDataValueInt64(self, value)

    if dt.GetClass() == GEDTC_NUMERIC and dt.GetNumericDataType() == gdalconst.GDT_UInt64:
        return _gdal.MDArray_SetNoDataValueUInt64(self, value)

    return _gdal.MDArray_SetNoDataValueDouble(self, value)
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
        if dt.GetNumericDataType() in (GDT_Byte, GDT_UInt16,
                                       GDT_Int8, GDT_Int16, GDT_Int32):
            if self.GetTotalElementsCount() == 1:
                return self.ReadAsInt()
            return self.ReadAsIntArray()
        if dt.GetNumericDataType() in (GDT_UInt32, GDT_Int64):
            if self.GetTotalElementsCount() == 1:
                return self.ReadAsInt64()
            return self.ReadAsInt64Array()
        if self.GetTotalElementsCount() == 1:
            return self.ReadAsDouble()
        return self.ReadAsDoubleArray()
    return self.ReadAsRaw()

  def Write(self, val):
    if isinstance(val, (int, type(12345678901234))):
        if val >= -0x80000000 and val <= 0x7FFFFFFF:
            return self.WriteInt(val)
        if val >= -0x8000000000000000 and val <= 0x7FFFFFFFFFFFFFFF:
            return self.WriteInt64(val)
        return self.WriteDouble(val)
    if isinstance(val, float):
        return self.WriteDouble(val)
    if isinstance(val, str) and self.GetDataType().GetClass() != GEDTC_COMPOUND:
        return self.WriteString(val)
    if isinstance(val, list):
        if len(val) == 0:
            if self.GetDataType().GetClass() == GEDTC_STRING:
                return self.WriteStringArray(val)
            return self.WriteDoubleArray(val)
        if isinstance(val[0], (int, type(12345678901234))):
            if all(v >= -0x80000000 and v <= 0x7FFFFFFF for v in val):
                return self.WriteIntArray(val)
            if all(v >= -0x8000000000000000 and v <= 0x7FFFFFFFFFFFFFFF
                   for v in val):
                return self.WriteInt64Array(val)
            return self.WriteDoubleArray(val)
        if isinstance(val[0], float):
            return self.WriteDoubleArray(val)
        if isinstance(val[0], str):
            return self.WriteStringArray(val)
    if (isinstance(val, dict) and
        self.GetDataType().GetSubType() == GEDTST_JSON):
        import json
        return self.WriteString(json.dumps(val))
    return self.WriteRaw(val)

%}
}

%include "callback.i"

// Start: to be removed in GDAL 4.0

// Issue a FutureWarning in a number of functions and methods that will
// be impacted when exceptions are enabled by default

%pythoncode %{

def _WarnIfUserHasNotSpecifiedIfUsingExceptions():
    from . import gdal
    if not hasattr(gdal, "hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions") and not _UserHasSpecifiedIfUsingExceptions():
        gdal.hasWarnedAboutUserHasNotSpecifiedIfUsingExceptions = True
        import warnings
        warnings.warn(
            "Neither gdal.UseExceptions() nor gdal.DontUseExceptions() has been explicitly called. " +
            "In GDAL 4.0, exceptions will be enabled by default.", FutureWarning)

def _WarnIfUserHasNotSpecifiedIfUsingOgrExceptions():
    from . import ogr
    ogr._WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend GeneralCmdLineProcessor %{
    for i in range(len(args[0])):
        if isinstance(args[0][i], (os.PathLike, int)):
            args[0][i] = str(args[0][i])
%}

%pythonprepend Open %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend OpenEx %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend OpenShared %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend Unlink %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%extend GDALDatasetShadow {

%pythonprepend GetLayerByName %{
    _WarnIfUserHasNotSpecifiedIfUsingOgrExceptions()
%}

%pythonprepend ExecuteSQL %{
    _WarnIfUserHasNotSpecifiedIfUsingOgrExceptions()
%}

}

%extend GDALDriverShadow {

%pythonprepend Create %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend CreateMultiDimensional %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythonprepend CreateCopy %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if len(args) >= 2 and isinstance(args[1], Band):
        ds = args[1].GetDataset()
        if ds:
            args = [arg for arg in args]
            args[1] = ds
            args = tuple(args)

%}

%pythonprepend Delete %{
    _WarnIfUserHasNotSpecifiedIfUsingExceptions()
%}

%pythoncode %{

def CreateDataSource(self, utf8_path, options=None):
    """
    Synonym for :py:meth:`CreateVector`.
    """
    return self.Create(utf8_path, 0, 0, 0, GDT_Unknown, options or [])

def CopyDataSource(self, ds, utf8_path, options=None):
    """
    Synonym for :py:meth:`CreateCopy`.
    """
    return self.CreateCopy(utf8_path, ds, options = options or [])

def DeleteDataSource(self, utf8_path):
    """
    Synonym for :py:meth:`Delete`.
    """
    return self.Delete(utf8_path)

def Open(self, utf8_path, update=False):
    """
    Attempt to open a specified path with this driver.

    Parameters
    ----------
    utf8_path : str
       The path to open
    update : bool, default = False
       Whether to open the dataset in update mode.

    Returns
    -------
    Dataset or None
        ``None`` on error
    """
    return OpenEx(utf8_path,
                  OF_VECTOR | (OF_UPDATE if update else 0),
                  [self.GetDescription()])

def GetName(self):
    """
    Synonym for :py:meth:`GetDescription`.
    """
    return self.GetDescription()
%}

}

// End: to be removed in GDAL 4.0


%pythoncode %{

def InfoOptions(options=None, format='text', deserialize=True,
         computeMinMax=False, reportHistograms=False, reportProj4=False,
         stats=False, approxStats=False, computeChecksum=False,
         showGCPs=True, showMetadata=True, showRAT=True, showColorTable=True,
         showNodata=True, showMask=True,
         listMDD=False, showFileList=True, allMetadata=False,
         extraMDDomains=None, wktFormat=None):
    """ Create a InfoOptions() object that can be passed to gdal.Info()
        options can be an array of strings, a string or let empty and filled from other keywords."""

    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
        format = 'text'
        if '-json' in new_options:
            format = 'json'
    else:
        import copy
        new_options = copy.copy(options)
        if format == 'json':
            new_options += ['-json']
        elif format != "text":
            raise Exception("Invalid value for format")
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
        if not showNodata:
            new_options += ['-nonodata']
        if not showMask:
            new_options += ['-nomask']
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
    """Return information on a raster dataset.

    Parameters
    ----------
    ds : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.InfoOptions(), string or array of strings
        other keywords arguments of gdal.InfoOptions().
        If options is provided as a gdal.InfoOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, format, deserialize) = InfoOptions(**kwargs)
    else:
        (opts, format, deserialize) = kwargs['options']

    if isinstance(ds, (str, os.PathLike)):
        ds = Open(ds)
    ret = InfoInternal(ds, opts)
    if format == 'json' and deserialize:
        import json
        ret = json.loads(ret)
    return ret


def VectorInfoOptions(options=None,
                      format='text',
                      deserialize=True,
                      layers=None,
                      dumpFeatures=False,
                      limit=None,
                      featureCount=True,
                      extent=True,
                      SQLStatement=None,
                      SQLDialect=None,
                      where=None,
                      wktFormat=None):
    """ Create a VectorInfoOptions() object that can be passed to gdal.VectorInfo()
        options can be an array of strings, a string or let empty and filled from other keywords.

        Parameters
        ----------
        options : any
            can be an array of strings, a string or let empty and filled from other keywords.
        format : any
            "text" or "json"
        deserialize : any
            if JSON output should be returned as a Python dictionary. Otherwise as a serialized representation.
        SQLStatement : str
            SQL statement to apply to the source dataset
        SQLDialect : str
            SQL dialect ('OGRSQL', 'SQLITE', ...)
        where : str
            WHERE clause to apply to source layer(s)
        layers : any
            list of layers of interest
        featureCount : any
            whether to compute and display the feature count
        extent : any
            whether to compute and display the layer extent. Can also be set to the string '3D' to request a 3D extent
        dumpFeatures : any
            set to True to get the dump of all features
        limit : int
            maximum number of features to read per layer
    """

    options = [] if options is None else options
    deserialize=True

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
        format = 'text'
        if '-json' in new_options:
            format = 'json'
    else:
        import copy
        new_options = copy.copy(options)
        if format == 'json':
            new_options += ['-json']
        elif format != "text":
            raise Exception("Invalid value for format")
        if '-json' in new_options:
            format = 'json'
        if SQLStatement:
            new_options += ['-sql', SQLStatement]
        if SQLDialect:
            new_options += ['-dialect', SQLDialect]
        if where:
            new_options += ['-where', where]
        if wktFormat:
            new_options += ['-wkt_format', wktFormat]
        if not featureCount:
            new_options += ['-nocount']
        if extent in ('3d', '3D'):
            new_options += ['-extent3D']
        elif not extent:
            new_options += ['-noextent']
        if layers:
            new_options += ["dummy_dataset_name"]
            for layer in layers:
                new_options += [layer]
        else:
            new_options += ["-al"]
        if format == 'json':
            if dumpFeatures:
                new_options += ["-features"]
        else:
            if not dumpFeatures:
                new_options += ["-so"]
        if limit:
            new_options += ["-limit", str(limit)]

    return (GDALVectorInfoOptions(new_options), format, deserialize)


def VectorInfo(ds, **kwargs):
    """Return information on a vector dataset.

    Parameters
    ----------
    ds : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.VectorInfoOptions(), string or array of strings
        other keywords arguments of gdal.VectorInfoOptions().
        If options is provided as a gdal.VectorInfoOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, format, deserialize) = VectorInfoOptions(**kwargs)
    else:
        (opts, format, deserialize) = kwargs['options']

    if isinstance(ds, (str, os.PathLike)):
        ds = OpenEx(ds, OF_VERBOSE_ERROR | OF_VECTOR)
    ret = VectorInfoInternal(ds, opts)
    if format == 'json' and deserialize:
        import json
        ret = json.loads(ret)
    return ret


def MultiDimInfoOptions(options=None, detailed=False, array=None, arrayoptions=None, limit=None, as_text=False):
    """ Create a MultiDimInfoOptions() object that can be passed to gdal.MultiDimInfo()
        options can be an array of strings, a string or let empty and filled from other keywords."""

    options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
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
    """Return information on a dataset.

    Parameters
    ----------
    ds : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.MultiDimInfoOptions(), string or array of strings
        other keywords arguments of gdal.MultiDimInfoOptions().
        If options is provided as a gdal.MultiDimInfoOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        opts, as_text = MultiDimInfoOptions(**kwargs)
    else:
        opts = kwargs['options']
        as_text = True

    if isinstance(ds, (str, os.PathLike)):
        ds = OpenEx(ds, OF_VERBOSE_ERROR | OF_MULTIDIM_RASTER)
    ret = MultiDimInfoInternal(ds, opts)
    if not as_text:
        import json
        ret = json.loads(ret)
    return ret


def _strHighPrec(x):
    return ('%.18g' % x) if isinstance(x, float) else str(x)

mapGRIORAMethodToString = {
    gdalconst.GRIORA_NearestNeighbour: 'near',
    gdalconst.GRIORA_Bilinear: 'bilinear',
    gdalconst.GRIORA_Cubic: 'cubic',
    gdalconst.GRIORA_CubicSpline: 'cubicspline',
    gdalconst.GRIORA_Lanczos: 'lanczos',
    gdalconst.GRIORA_Average: 'average',
    gdalconst.GRIORA_RMS: 'rms',
    gdalconst.GRIORA_Mode: 'mode',
    gdalconst.GRIORA_Gauss: 'gauss',
}

def _addOptions(new_options, arg, options):
    if isinstance(options, str):
        new_options += [arg, options]
    elif isinstance(options, dict):
        for k, v in options.items():
            new_options += [arg, f'{k}={v}']
    else:
        for opt in options:
            new_options += [arg, opt]

def _addCreationOptions(new_options, creationOptions):
    """Update new_options with creationOptions formatted as expected by utilities"""
    _addOptions(new_options, '-co', creationOptions)

def TranslateOptions(options=None, format=None,
              outputType = gdalconst.GDT_Unknown, bandList=None, maskBand=None,
              width = 0, height = 0, widthPct = 0.0, heightPct = 0.0,
              xRes = 0.0, yRes = 0.0,
              creationOptions=None, srcWin=None, projWin=None, projWinSRS=None, strict = False,
              unscale = False, scaleParams=None, exponents=None,
              outputBounds=None, outputGeotransform=None, metadataOptions=None,
              outputSRS=None, nogcp=False, GCPs=None,
              noData=None, rgbExpand=None,
              stats = False, rat = True, xmp = True, resampleAlg=None,
              overviewLevel = 'AUTO',
              colorInterpretation=None,
              callback=None, callback_data=None,
              domainMetadataOptions = None,
              errorIfWindowOutsideSource = False):
    """Create a TranslateOptions() object that can be passed to gdal.Translate()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : str
        output format ("GTiff", etc...)
    outputType : any
        output type (gdalconst.GDT_Byte, etc...)
    bandList : any
        array of band numbers (index start at 1)
    maskBand : any
        mask band to generate or not ("none", "auto", "mask", 1, ...)
    width : int
        width of the output raster in pixel
    height : int
        height of the output raster in pixel
    widthPct : any
        width of the output raster in percentage (100 = original width)
    heightPct : any
        height of the output raster in percentage (100 = original height)
    xRes : int
        output horizontal resolution
    yRes : int
        output vertical resolution
    creationOptions : list or dict
        list or dict of creation options
    srcWin : any
        subwindow in pixels to extract: [left_x, top_y, width, height]
    projWin : any
        subwindow in projected coordinates to extract: [ulx, uly, lrx, lry]
    projWinSRS : any
        SRS in which projWin is expressed
    strict : any
        strict mode
    unscale : any
        unscale values with scale and offset metadata
    scaleParams : any
        list of scale parameters, each of the form [src_min,src_max] or [src_min,src_max,dst_min,dst_max]
    exponents : any
        list of exponentiation parameters
    outputBounds : any
        assigned output bounds: [ulx, uly, lrx, lry]
    outputGeotransform : any
        assigned geotransform matrix (array of 6 values) (mutually exclusive with outputBounds)
    metadataOptions : any
        list or dict of metadata options
    outputSRS : any
        assigned output SRS
    nogcp : any
        ignore GCP in the raster
    GCPs : any
        list of GCPs
    noData : any
        nodata value (or "none" to unset it)
    rgbExpand : any
        Color palette expansion mode: "gray", "rgb", "rgba"
    stats : any
        whether to calculate statistics
    rat : any
        whether to write source RAT
    xmp : any
        whether to copy XMP metadata
    resampleAlg : any
        resampling mode
    overviewLevel : any
        To specify which overview level of source files must be used
    colorInterpretation : any
        Band color interpretation, as a single value or a list, of the following values ("red", "green", "blue", "alpha", "grey", "undefined", etc.) or their GCI_xxxx symbolic names
    callback : any
        callback method
    callback_data : any
        user data for callback
    domainMetadataOptions : any
        list or dict of domain-specific metadata options
    errorIfWindowOutsideSource : {True, False, "partially", "completely"}, default=True
         raise an error if the requested window is partially or completely outside the source dataset. ("True" is a synonym for "partially"). This corresponds to the ``-epo`` and ``-eco`` options of ``gdal_translate``.
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
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
            new_options += ['-outsize', str(widthPct) + '%', str(heightPct) + '%']
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
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
        if outputGeotransform:
            if outputBounds:
                raise Exception("outputBounds and outputGeotransform are mutually exclusive")
            assert len(outputGeotransform) == 6
            new_options += ['-a_gt']
            for val in outputGeotransform:
                new_options += [_strHighPrec(val)]
        if metadataOptions is not None:
            _addOptions(new_options, '-mo', metadataOptions)
        if domainMetadataOptions is not None:
            if isinstance(domainMetadataOptions, str):
                new_options += ['-dmo', domainMetadataOptions]
            elif isinstance(domainMetadataOptions, dict):
                for d in domainMetadataOptions:
                  things = domainMetadataOptions[d]
                  for k, v in things.items():
                    new_options += ['-dmo', f'{d}:{k}={v}']
            else:
                for opt in domainMetadataOptions:
                    new_options += ['-dmo', opt]
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
        if not xmp:
            new_options += ['-noxmp']
        if resampleAlg is not None:
            if resampleAlg in mapGRIORAMethodToString:
                new_options += ['-r', mapGRIORAMethodToString[resampleAlg]]
            else:
                new_options += ['-r', str(resampleAlg)]
        if xRes != 0 and yRes != 0:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]

        if overviewLevel is None or isinstance(overviewLevel, str):
            pass
        elif isinstance(overviewLevel, int):
            if overviewLevel < 0:
                overviewLevel = 'AUTO' + str(overviewLevel)
            else:
                overviewLevel = str(overviewLevel)
        else:
            overviewLevel = None
        if colorInterpretation is not None:
            def colorInterpAsString(x):
                return GetColorInterpretationName(x) if isinstance(x, int) else x
            if isinstance(colorInterpretation, list):
                new_options += ['-colorinterp', ','.join([colorInterpAsString(x) for x in colorInterpretation])]
            else:
                new_options += ['-colorinterp', colorInterpAsString(colorInterpretation)]

        if overviewLevel is not None and overviewLevel != 'AUTO':
            new_options += ['-ovr', overviewLevel]

        if errorIfWindowOutsideSource is not False:
            if errorIfWindowOutsideSource in ("partially", True):
                new_options.append('-epo')
            elif errorIfWindowOutsideSource == "completely":
                new_options.append('-eco')
            else:
                raise RuntimeError("errorIfWindowOutsideSource must be one of True / 'partially', False, or 'completely'")


    if return_option_list:
        return new_options

    return (GDALTranslateOptions(new_options), callback, callback_data)

def Translate(destName, srcDS, **kwargs):
    """Convert a dataset.

    Parameters
    ----------
    destName : str
        Output dataset name
    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.TranslateOptions(), string or array of strings
        other keywords arguments of gdal.TranslateOptions().
        If options is provided as a gdal.TranslateOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    filenamePrefix = ""
    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = TranslateOptions(**kwargs)
        if "format" in kwargs and kwargs["format"].upper() == "ZARR" and "creationOptions" in kwargs:
            for opt in kwargs["creationOptions"]:
                if opt.upper() in ("CONVERT_TO_KERCHUNK_PARQUET_REFERENCE=YES", "CONVERT_TO_KERCHUNK_PARQUET_REFERENCE=ON", "CONVERT_TO_KERCHUNK_PARQUET_REFERENCE=TRUE"):
                    filenamePrefix = "ZARR_DUMMY:"

    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = Open(filenamePrefix + str(srcDS))

    return TranslateInternal(destName, srcDS, opts, callback, callback_data)

def WarpOptions(options=None, format=None,
         srcBands=None,
         dstBands=None,
         outputBounds=None,
         outputBoundsSRS=None,
         xRes=None, yRes=None, targetAlignedPixels = False,
         width = 0, height = 0,
         srcSRS=None, dstSRS=None,
         coordinateOperation=None,
         srcAlpha = None, dstAlpha = False,
         warpOptions=None, errorThreshold=None,
         warpMemoryLimit=None, creationOptions=None, outputType = gdalconst.GDT_Unknown,
         workingType = gdalconst.GDT_Unknown, resampleAlg=None,
         srcNodata=None, dstNodata=None, multithread = False,
         tps = False, rpc = False, geoloc = False, polynomialOrder=None,
         transformerOptions=None, cutlineDSName=None,
         cutlineWKT=None,
         cutlineSRS=None,
         cutlineLayer=None, cutlineWhere=None, cutlineSQL=None, cutlineBlend=None, cropToCutline = False,
         copyMetadata = True, metadataConflictValue=None,
         setColorInterpretation = False,
         overviewLevel = 'AUTO',
         callback=None, callback_data=None):
    """Create a WarpOptions() object that can be passed to gdal.Warp()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : str
        output format ("GTiff", etc...)
    srcBands : any
        list of source band numbers (between 1 and the number of input bands)
    dstBands : any
        list of output band numbers
    outputBounds : any
        output bounds as (minX, minY, maxX, maxY) in target SRS
    outputBoundsSRS : any
        SRS in which output bounds are expressed, in the case they are not expressed in dstSRS
    xRes : any
        output resolution in target SRS
    yRes : any
        output resolution in target SRS
    targetAlignedPixels : any
        whether to force output bounds to be multiple of output resolution
    width : int
        width of the output raster in pixel
    height : int
        height of the output raster in pixel
    srcSRS : any
        source SRS
    dstSRS : any
        output SRS
    coordinateOperation : any
        coordinate operation as a PROJ string or WKT string
    srcAlpha : any
        whether to force the last band of the input dataset to be considered as an alpha band.
        If set to False, source alpha warping will be disabled.
    dstAlpha : any
        whether to force the creation of an output alpha band
    outputType : any
        output type (gdalconst.GDT_Byte, etc...)
    workingType : any
        working type (gdalconst.GDT_Byte, etc...)
    warpOptions : any
        list or dict of warping options. For a list of available options, see :cpp:member:`GDALWarpOptions::papszWarpOptions`.
    errorThreshold : any
        error threshold for approximation transformer (in pixels)
    warpMemoryLimit : any
        size of working buffer in MB
    resampleAlg : any
        resampling mode
    creationOptions : list or dict
        list or dict of creation options
    srcNodata : any
        source nodata value(s)
    dstNodata : any
        output nodata value(s)
    multithread : any
        whether to multithread computation and I/O operations
    tps : any
        whether to use Thin Plate Spline GCP transformer
    rpc : any
        whether to use RPC transformer
    geoloc : any
        whether to use GeoLocation array transformer
    polynomialOrder : any
        order of polynomial GCP interpolation
    transformerOptions : any
        list or dict of transformer options
    cutlineDSName : any
        cutline dataset name (mutually exclusive with cutlineWKT)
    cutlineWKT : any
        cutline WKT geometry (POLYGON or MULTIPOLYGON) (mutually exclusive with cutlineDSName)
    cutlineSRS : any
        set/override cutline SRS
    cutlineLayer : any
        cutline layer name
    cutlineWhere : any
        cutline WHERE clause
    cutlineSQL : any
        cutline SQL statement
    cutlineBlend : any
        cutline blend distance in pixels
    cropToCutline : any
        whether to use cutline extent for output bounds
    copyMetadata : any
        whether to copy source metadata
    metadataConflictValue : any
        metadata data conflict value
    setColorInterpretation : any
        whether to force color interpretation of input bands to output bands
    overviewLevel : any
        To specify which overview level of source files must be used
    callback : any
        callback method
    callback_data : any
        user data for callback
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if srcBands:
            for b in srcBands:
                new_options += ['-srcband', str(b)]
        if dstBands:
            for b in dstBands:
                new_options += ['-dstband', str(b)]
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
        elif srcAlpha is not None:
            new_options += ['-nosrcalpha']
        if dstAlpha:
            new_options += ['-dstalpha']
        if warpOptions is not None:
            _addOptions(new_options, '-wo', warpOptions)
        if errorThreshold is not None:
            new_options += ['-et', _strHighPrec(errorThreshold)]
        if resampleAlg is not None:

            mapMethodToString = {
                gdalconst.GRA_NearestNeighbour: 'near',
                gdalconst.GRA_Bilinear: 'bilinear',
                gdalconst.GRA_Cubic: 'cubic',
                gdalconst.GRA_CubicSpline: 'cubicspline',
                gdalconst.GRA_Lanczos: 'lanczos',
                gdalconst.GRA_Average: 'average',
                gdalconst.GRA_RMS: 'rms',
                gdalconst.GRA_Mode: 'mode',
                gdalconst.GRA_Max: 'max',
                gdalconst.GRA_Min: 'min',
                gdalconst.GRA_Med: 'med',
                gdalconst.GRA_Q1: 'q1',
                gdalconst.GRA_Q3: 'q3',
                gdalconst.GRA_Sum: 'sum',
            }

            if resampleAlg in mapMethodToString:
                new_options += ['-r', mapMethodToString[resampleAlg]]
            else:
                new_options += ['-r', str(resampleAlg)]
        if warpMemoryLimit is not None:
            new_options += ['-wm', str(warpMemoryLimit)]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
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
            _addOptions(new_options, '-to', transformerOptions)
        if cutlineDSName is not None:
            if cutlineWKT is not None:
                raise Exception("cutlineDSName and cutlineWKT are mutually exclusive")
            new_options += ['-cutline', str(cutlineDSName)]
        if cutlineWKT is not None:
            new_options += ['-cutline', str(cutlineWKT)]
        if cutlineSRS is not None:
            new_options += ['-cutline_srs', str(cutlineSRS)]
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

        if overviewLevel is not None and overviewLevel != 'AUTO':
            new_options += ['-ovr', overviewLevel]

    if return_option_list:
        return new_options

    return (GDALWarpAppOptions(new_options), callback, callback_data)

def Warp(destNameOrDestDS, srcDSOrSrcDSTab, **kwargs):
    """Warp one or several datasets.

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object.

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object.

    srcDSOrSrcDSTab : any
        an array of Dataset objects or filenames, or a Dataset object or a filename
    **kwargs : any
        options: return of gdal.WarpOptions(), string or array of strings,
        other keywords arguments of gdal.WarpOptions().
        If options is provided as a gdal.WarpOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = WarpOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDSOrSrcDSTab, (str, os.PathLike)):
        srcDSTab = [Open(srcDSOrSrcDSTab)]
    elif isinstance(srcDSOrSrcDSTab, list):
        srcDSTab = []
        for elt in srcDSOrSrcDSTab:
            if isinstance(elt, (str, os.PathLike)):
                srcDSTab.append(Open(elt))
            else:
                srcDSTab.append(elt)
    else:
        srcDSTab = [srcDSOrSrcDSTab]

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
        return wrapper_GDALWarpDestName(destNameOrDestDS, srcDSTab, opts, callback, callback_data)
    else:
        return wrapper_GDALWarpDestDS(destNameOrDestDS, srcDSTab, opts, callback, callback_data)


def VectorTranslateOptions(options=None, format=None,
         accessMode=None,
         srcSRS=None, dstSRS=None, reproject=True,
         coordinateOperation=None,
         coordinateOperationOptions=None,
         SQLStatement=None, SQLDialect=None, where=None, selectFields=None,
         addFields=False,
         relaxedFieldNameMatch=False,
         forceNullable=False,
         emptyStrAsNull=False,
         spatFilter=None, spatSRS=None,
         datasetCreationOptions=None,
         layerCreationOptions=None,
         layers=None,
         layerName=None,
         geometryType=None,
         dim=None,
         transactionSize=None,
         clipSrc=None,
         clipSrcSQL=None,
         clipSrcLayer=None,
         clipSrcWhere=None,
         clipDst=None,
         clipDstSQL=None,
         clipDstLayer=None,
         clipDstWhere=None,
         preserveFID=False,
         simplifyTolerance=None,
         segmentizeMaxDist=None,
         makeValid=False,
         skipInvalid=False,
         mapFieldType=None,
         explodeCollections=False,
         zField=None,
         resolveDomains=False,
         skipFailures=False,
         limit=None,
         xyRes=None,
         zRes=None,
         mRes=None,
         setCoordPrecision=True,
         callback=None, callback_data=None):
    """
    Create a VectorTranslateOptions() object that can be passed to
    gdal.VectorTranslate()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other
        keywords.
    format : str
        format ("ESRI Shapefile", etc...)
    accessMode : any
        None for creation, 'update', 'append', 'upsert', 'overwrite'
    srcSRS : any
        source SRS
    dstSRS : any
        output SRS (with reprojection if reproject = True)
    coordinateOperation : any
        coordinate operation as a PROJ string or WKT string
    coordinateOperationOptions : any
        list or dict of coordinate operation options (ALLOW_BALLPARK=NO, ONLY_BEST=YES, WARN_ABOUT_DIFFERENT_COORD_OP=NO)
    reproject : any
        whether to do reprojection
    SQLStatement : any
        SQL statement to apply to the source dataset
    SQLDialect : any
        SQL dialect ('OGRSQL', 'SQLITE', ...)
    where : any
        WHERE clause to apply to source layer(s)
    selectFields : any
        list of fields to select
    addFields : any
        whether to add new fields found in source layers (to be used with
        accessMode == 'append' or 'upsert')
    relaxedFieldNameMatch : any
        Do field name matching between source and existing target layer in a more relaxed way if the target driver has an implementation for it.
    forceNullable : any
        whether to drop NOT NULL constraints on newly created fields
    emptyStrAsNull : any
        whether to treat empty string values as NULL
    spatFilter : any
        spatial filter as (minX, minY, maxX, maxY) bounding box
    spatSRS : any
        SRS in which the spatFilter is expressed. If not specified, it is assumed to be
        the one of the layer(s)
    datasetCreationOptions : any
        list or dict of dataset creation options
    layerCreationOptions : any
        list or dict of layer creation options
    layers : any
        list of layers to convert
    layerName : any
        output layer name
    geometryType : any
        output layer geometry type ('POINT', ....). May be an array of strings
        when using a special value like 'PROMOTE_TO_MULTI', 'CONVERT_TO_LINEAR',
        'CONVERT_TO_CURVE' combined with another one or a geometry type.
    dim : any
        output dimension ('XY', 'XYZ', 'XYM', 'XYZM', 'layer_dim')
    transactionSize : any
        number of features to save per transaction (default 100 000). Increase the value
        for better performance when writing into DBMS drivers that have transaction
        support. Set to "unlimited" to load the data into a single transaction.
    clipSrc : any
        clip geometries to the specified bounding box (expressed in source SRS),
        WKT geometry (POLYGON or MULTIPOLYGON), from a datasource or to the spatial
        extent of the -spat option if you use the "spat_extent" keyword. When
        specifying a datasource, you will generally want to use it in combination with
        the clipSrcLayer, clipSrcWhere or clipSrcSQL options.
    clipSrcSQL : any
        select desired geometries using an SQL query instead.
    clipSrcLayer : any
        select the named layer from the source clip datasource.
    clipSrcWhere : any
        restrict desired geometries based on attribute query.
    clipDst : any
        clip geometries after reprojection to the specified bounding box (expressed in
        dest SRS), WKT geometry (POLYGON or MULTIPOLYGON) or from a datasource. When
        specifying a datasource, you will generally want to use it in combination of
        the clipDstLayer, clipDstWhere or clipDstSQL options.
    clipDstSQL : any
        select desired geometries using an SQL query instead.
    clipDstLayer : any
        select the named layer from the destination clip datasource.
    clipDstWhere : any
        restrict desired geometries based on attribute query.
    simplifyTolerance : any
        distance tolerance for simplification. The algorithm used preserves topology per
        feature, in particular for polygon geometries, but not for a whole layer.
    segmentizeMaxDist : any
        maximum distance between consecutive nodes of a line geometry
    makeValid : any
        run MakeValid() on geometries
    skipInvalid : any
        whether to skip features with invalid geometries regarding the rules of
        the Simple Features specification.
    mapFieldType : any
        converts any field of the specified type to another type. Valid types are:
        Integer, Integer64, Real, String, Date, Time, DateTime, Binary, IntegerList,
        Integer64List, RealList, StringList. Types can also include subtype between
        parenthesis, such as Integer(Boolean), Real(Float32),... Special value All can
        be used to convert all fields to another type. This is an alternate way to using
        the CAST operator of OGR SQL, that may avoid typing a long SQL query.
        Note that this does not influence the field types used by the source driver,
        and is only an afterwards conversion.
    explodeCollections : any
        produce one feature for each geometry in any kind of geometry collection in the
        source file, applied after any -sql option. This option is not compatible with
        preserveFID but a SQLStatement (e.g. SELECT fid AS original_fid, * FROM ...)
        can be used to store the original FID if needed.
    preserveFID : any
        Use the FID of the source features instead of letting the output driver automatically
        assign a new one (for formats that require a FID). If not in append mode, this behavior
        is the default if the output driver has a FID  layer  creation  option,  in which case
        the name of the source FID column will be used and source feature IDs will be attempted
        to be preserved. This behavior can be disabled by setting -unsetFid.
        This option is not compatible with explodeCollections
    zField : any
        name of field to use to set the Z component of geometries
    resolveDomains : any
        whether to create an additional field for each field associated with a coded
        field domain.
    skipFailures : any
        whether to skip failures
    limit : int
        maximum number of features to read per layer
    xyRes : any
        Geometry X,Y coordinate resolution. Numeric value, or numeric value suffixed with " m", " mm" or "deg".
    zRes : any
        Geometry Z coordinate resolution. Numeric value, or numeric value suffixed with " m" or " mm".
    mRes : any
        Geometry M coordinate resolution. Numeric value.
    setCoordPrecision : any
        Set to False to unset the geometry coordinate precision.
    callback : any
        callback method
    callback_data : any
        user data for callback
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
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

        if coordinateOperationOptions is not None:
            _addOptions(new_options,'-ct_opt', coordinateOperationOptions)
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
            elif accessMode == 'upsert':
                new_options += ['-upsert']
            else:
                raise Exception('unhandled accessMode')
        if addFields:
            new_options += ['-addfields']
        if relaxedFieldNameMatch:
            new_options += ['-relaxedFieldNameMatch']
        if forceNullable:
            new_options += ['-forceNullable']
        if emptyStrAsNull:
            new_options += ['-emptyStrAsNull']
        if selectFields is not None:
            val = ''
            for item in selectFields:
                if val:
                    val += ','
                if ',' in item or ' ' in item or '"' in item:
                    val += '"' + item.replace('"', '\\"') + '"'
                else:
                    val += item
            new_options += ['-select', val]

        if datasetCreationOptions is not None:
            _addOptions(new_options, '-dsco', datasetCreationOptions)

        if layerCreationOptions is not None:
            _addOptions(new_options, '-lco', layerCreationOptions)

        if layers is not None:
            if isinstance(layers, str):
                new_options += [layers]
            else:
                for lyr in layers:
                    new_options += [lyr]

        if transactionSize is not None:
            new_options += ['-gt', str(transactionSize)]

        if clipSrc is not None:
            import os

            new_options += ['-clipsrc']
            if isinstance(clipSrc, str):
                new_options += [clipSrc]
            elif isinstance(clipSrc, os.PathLike):
                new_options += [str(clipSrc)]
            else:
                try:
                    new_options += [
                        str(clipSrc[0]),
                        str(clipSrc[1]),
                        str(clipSrc[2]),
                        str(clipSrc[3])
                    ]
                except Exception as ex:
                    raise ValueError(f"invalid value for clipSrc: {clipSrc}") from ex
        if clipSrcSQL is not None:
            new_options += ['-clipsrcsql', str(clipSrcSQL)]
        if clipSrcLayer is not None:
            new_options += ['-clipsrclayer', str(clipSrcLayer)]
        if clipSrcWhere is not None:
            new_options += ['-clipsrcwhere', str(clipSrcWhere)]

        if clipDst is not None:
            import os

            new_options += ['-clipdst']
            if isinstance(clipDst, str):
                new_options += [clipDst]
            elif isinstance(clipDst, os.PathLike):
                new_options += [str(clipDst)]
            else:
                try:
                    new_options += [
                        str(clipDst[0]),
                        str(clipDst[1]),
                        str(clipDst[2]),
                        str(clipDst[3])
                    ]
                except Exception as ex:
                    raise ValueError(f"invalid value for clipDst: {clipDst}") from ex
        if clipDstSQL is not None:
            new_options += ['-clipdstsql', str(clipDstSQL)]
        if clipDstLayer is not None:
            new_options += ['-clipdstlayer', str(clipDstLayer)]
        if clipDstWhere is not None:
            new_options += ['-clipdstwhere', str(clipDstWhere)]

        if simplifyTolerance is not None:
            new_options += ['-simplify', str(simplifyTolerance)]
        if segmentizeMaxDist is not None:
            new_options += ['-segmentize', str(segmentizeMaxDist)]
        if makeValid:
            new_options += ['-makevalid']
        if skipInvalid:
            new_options += ['-skipinvalid']
        if mapFieldType is not None:
            new_options += ['-mapFieldType']
            if isinstance(mapFieldType, str):
                new_options += [mapFieldType]
            else:
                new_options += [",".join(mapFieldType)]
        if explodeCollections:
            new_options += ['-explodecollections']
        if preserveFID:
            new_options += ['-preserve_fid']
        if spatFilter is not None:
            new_options += [
                '-spat',
                str(spatFilter[0]),
                str(spatFilter[1]),
                str(spatFilter[2]),
                str(spatFilter[3])
            ]
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
        if xyRes is not None:
            new_options += ['-xyRes', str(xyRes)]
        if zRes is not None:
            new_options += ['-zRes', str(zRes)]
        if mRes is not None:
            new_options += ['-mRes', str(mRes)]
        if setCoordPrecision is False:
            new_options += ["-unsetCoordPrecision"]

    if callback is not None:
        new_options += ['-progress']

    if return_option_list:
        return new_options

    return (GDALVectorTranslateOptions(new_options), callback, callback_data)


def VectorTranslate(destNameOrDestDS, srcDS, **kwargs):
    """Convert one vector dataset

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object. Note that the accessMode parameter
        also controls, at the layer level, if existing layers must be overwritten
        or updated.

    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.VectorTranslateOptions(), string or array of strings,
        other keywords arguments of gdal.VectorTranslateOptions().
        If options is provided as a gdal.VectorTranslateOptions() object,
        other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = VectorTranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
        return wrapper_GDALVectorTranslateDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALVectorTranslateDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)

def DEMProcessingOptions(options=None, colorFilename=None, format=None,
              creationOptions=None, computeEdges=False, alg=None, band=1,
              zFactor=None, scale=None, xscale=None, yscale=None, azimuth=None, altitude=None,
              combined=False, multiDirectional=False, igor=False,
              slopeFormat=None, trigonometric=False, zeroForFlat=False,
              addAlpha=None, colorSelection=None,
              callback=None, callback_data=None):
    """Create a DEMProcessingOptions() object that can be passed to gdal.DEMProcessing()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    colorFilename : any
        (mandatory for "color-relief") name of file that contains palette definition for the "color-relief" processing.
    format : any
        output format ("GTiff", etc...)
    creationOptions : any
        list or dict of creation options
    computeEdges : any
        whether to compute values at raster edges.
    alg : any
        'Horn' (default) or 'ZevenbergenThorne' for hillshade, slope or aspect. 'Wilson' (default) or 'Riley' for TRI
    band : any
        source band number to use
    zFactor : any
        (hillshade only) vertical exaggeration used to pre-multiply the elevations.
    scale : any
        ratio of vertical units to horizontal.
    xscale : any
        Ratio of vertical units to horizontal X axis units.
    yscale : any
        Ratio of vertical units to horizontal Y axis units.
    azimuth : any
        (hillshade only) azimuth of the light, in degrees. 0 if it comes from the top of the raster, 90 from the east, ... The default value, 315, should rarely be changed as it is the value generally used to generate shaded maps.
    altitude : any
        (hillshade only) altitude of the light, in degrees. 90 if the light comes from above the DEM, 0 if it is raking light.
    combined : any
        (hillshade only) whether to compute combined shading, a combination of slope and oblique shading. Only one of combined, multiDirectional and igor can be specified.
    multiDirectional : any
        (hillshade only) whether to compute multi-directional shading. Only one of combined, multiDirectional and igor can be specified.
    igor : any
        (hillshade only) whether to use Igor's hillshading from Maperitive.  Only one of combined, multiDirectional and igor can be specified.
    slopeFormat : any
        (slope only) "degree" or "percent".
    trigonometric : any
        (aspect only) whether to return trigonometric angle instead of azimuth. Thus 0deg means East, 90deg North, 180deg West, 270deg South.
    zeroForFlat : any
        (aspect only) whether to return 0 for flat areas with slope=0, instead of -9999.
    addAlpha : any
        adds an alpha band to the output file (only for processing = 'color-relief')
    colorSelection : any
        (color-relief only) Determines how color entries are selected from an input value. Can be "nearest_color_entry", "exact_color_entry" or "linear_interpolation". Defaults to "linear_interpolation"
    callback : any
        callback method
    callback_data : any
        user data for callback
    """
    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
        if computeEdges:
            new_options += ['-compute_edges']
        if alg:
            new_options += ['-alg', alg]
        new_options += ['-b', str(band)]
        if zFactor is not None:
            new_options += ['-z', str(zFactor)]
        if scale is not None:
            new_options += ['-s', str(scale)]
        if xscale is not None:
            new_options += ['-xscale', str(xscale)]
        if yscale is not None:
            new_options += ['-yscale', str(yscale)]
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

    if return_option_list:
        return new_options

    return (GDALDEMProcessingOptions(new_options), colorFilename, callback, callback_data)

def DEMProcessing(destName, srcDS, processing, **kwargs):
    """Apply a DEM processing.

    Parameters
    ----------
    destName : any
        Output dataset name
    srcDS : any
        a Dataset object or a filename
    processing : any
        one of "hillshade", "slope", "aspect", "color-relief", "TRI", "TPI", "Roughness"
    **kwargs : any
        options: return of gdal.DEMProcessingOptions(), string or array of strings,
        other keywords arguments of gdal.DEMProcessingOptions().
        If options is provided as a gdal.DEMProcessingOptions() object,
        other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, colorFilename, callback, callback_data) = DEMProcessingOptions(**kwargs)
    else:
        (opts, colorFilename, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = Open(srcDS)

    if isinstance(colorFilename, os.PathLike):
        colorFilename = str(colorFilename)

    return DEMProcessingInternal(destName, srcDS, processing, colorFilename, opts, callback, callback_data)


def NearblackOptions(options=None, format=None,
         creationOptions=None, white = False, colors=None,
         maxNonBlack=None, nearDist=None, setAlpha = False, setMask = False,
         alg=None,
         callback=None, callback_data=None):
    """Create a NearblackOptions() object that can be passed to gdal.Nearblack()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("GTiff", etc...)
    creationOptions : any
        list or dict of creation options
    white : any
        whether to search for nearly white (255) pixels instead of nearly black pixels.
    colors : any
        list of colors  to search for, e.g. ((0,0,0),(255,255,255)). The pixels that are considered as the collar are set to 0
    maxNonBlack : any
        number of non-black (or other searched colors specified with white / colors) pixels that can be encountered before the giving up search inwards. Defaults to 2.
    nearDist : any
        select how far from black, white or custom colors the pixel values can be and still considered near black, white or custom color.  Defaults to 15.
    setAlpha : any
        adds an alpha band to the output file.
    setMask : any
        adds a mask band to the output file.
    alg : any
        "twopasses" (default), or "floodfill"
    callback : any
        callback method
    callback_data : any
        user data for callback
    """
    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
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
        if alg:
            new_options += ['-alg', alg]

    if return_option_list:
        return new_options

    return (GDALNearblackOptions(new_options), callback, callback_data)

def Nearblack(destNameOrDestDS, srcDS, **kwargs):
    """Convert nearly black/white borders to exact value.

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object.

    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.NearblackOptions(), string or array of strings,
        other keywords arguments of gdal.NearblackOptions().
        If options is provided as a gdal.NearblackOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = NearblackOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS)

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
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

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("GTiff", etc...)
    outputType : any
        output type (gdalconst.GDT_Byte, etc...)
    width : any
        width of the output raster in pixel
    height : any
        height of the output raster in pixel
    creationOptions : any
        list or dict of creation options
    outputBounds : any
        assigned output bounds:
        [ulx, uly, lrx, lry]
    outputSRS : any
        assigned output SRS
    noData : any
        nodata value
    algorithm : any
        e.g "invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0"
    layers : any
        list of layers to convert
    SQLStatement : any
        SQL statement to apply to the source dataset
    where : any
        WHERE clause to apply to source layer(s)
    spatFilter : any
        spatial filter as (minX, minY, maxX, maxY) bounding box
    zfield : any
        Identifies an attribute field on the features to be used to get a Z value from.
        This value overrides Z value read from feature geometry record.
    z_increase : any
        Addition to the attribute field on the features to be used to get a Z value from.
        The addition should be the same unit as Z value. The result value will be
        Z value + Z increase value. The default value is 0.
    z_multiply : any
        Multiplication ratio for Z field. This can be used for shift from e.g. foot to meters
        or from  elevation to deep. The result value will be
        (Z value + Z increase value) * Z multiply value. The default value is 1.
    callback : any
        callback method
    callback_data : any
        user data for callback
    """
    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if width != 0 or height != 0:
            new_options += ['-outsize', str(width), str(height)]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
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

    if return_option_list:
        return new_options

    return (GDALGridOptions(new_options), callback, callback_data)

def Grid(destName, srcDS, **kwargs):
    """ Create raster from the scattered data.

    Parameters
    ----------
    destName : any
        Output dataset name
    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.GridOptions(), string or array of strings,
        other keywords arguments of gdal.GridOptions()
        If options is provided as a gdal.GridOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = GridOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    return GridInternal(destName, srcDS, opts, callback, callback_data)

def ContourOptions(
    options=None,
    format=None,
    band=1,
    elevationName=None,
    minName=None,
    maxName=None,
    with3d=False,
    srcNodata=None,
    offset=None,
    datasetCreationOptions=None,
    layerCreationOptions=None,
    interval=None,
    fixedLevels=None,
    exponentialBase=None,
    layerName="contour",
    polygonize=False,
    groupTransactions=100000,
    callback=None,
    callback_data=None):
    """Create a ContourOptions() object that can be passed to gdal.Contour()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("ESRI Shapefile", etc...)
    band : any
        band number to use (default = 1)
    elevationName : any
        name of the attribute in which to put the elevation.
        If not provided no elevation attribute is attached.
        Ignored in polygonal contouring (polygonize) mode.
    minName : any
        name for the attribute in which to put the minimum elevation of contour polygon.
        If not provided no minimum elevation attribute is attached.
        Ignored in default line contouring mode.
    maxName : any
        name for the attribute in which to put the maximum elevation of contour polygon.
        If not provided no maximum elevation attribute is attached.
        Ignored in default line contouring mode.
    with3d : any
        Force production of 3D vectors instead of 2D. Includes elevation at every vertex.
    srcNodata : any
        Input pixel value to treat as "nodata".
    offset : any
        Offset to apply to the elevation values.
    datasetCreationOptions : any
        List or dict of dataset creation options.
    layerCreationOptions : any
        List or dict of layer creation options.
    interval : any
        Elevation interval between contours. Must specify either "interval" or "fixedLevels" or "exponentialBase".
    fixedLevels : any
        Name one or more "fixed levels" to extract. Must specify either "interval" or "fixedLevels" or "exponentialBase".
    exponentialBase : any
        Generate levels on an exponential scale: base ^ k, for k an integer. Must specify either. Must specify either "interval" or "fixedLevels" or "exponentialBase".
    layerName : any
        Name for the output vector layer, defaults to "contour".
    polygonize : any
        Produce polygons instead of lines (default = False).
    groupTransactions : any
        Group n features per transaction (default 100 000). Increase the value for better performance when writing into
        DBMS drivers that have transaction support. n can be set to unlimited to load the data into a single transaction.
        If set to 0, no explicit transaction is done.
    callback : any
        Callback method.
    callback_data : any
        User data for callback.
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)

        if format is not None:
            new_options += ['-of', format]
        if elevationName is not None:
            new_options += ['-a', elevationName]
        if minName is not None:
            new_options += ['-amin', minName]
        if maxName is not None:
            new_options += ['-amax', maxName]
        if with3d:
            new_options += ['-3d']
        if srcNodata is not None:
            new_options += ['-snodata', str(srcNodata)]
        if offset is not None:
            new_options += ['-off', str(offset)]
        if datasetCreationOptions is not None:
            _addOptions(new_options, '-dsco', datasetCreationOptions)

        if layerCreationOptions is not None:
            _addOptions(new_options, '-lco', layerCreationOptions)

        if interval is not None:
            new_options += ['-i', str(interval)]
        if fixedLevels is not None:
            for level in fixedLevels:
                new_options += ['-fl', str(level)]
        if exponentialBase is not None:
            new_options += ['-e', str(exponentialBase)]
        if layerName is not None:
            new_options += ['-nln', layerName]
        if polygonize:
            new_options += ['-p']
        if groupTransactions is not None:
            new_options += ['-gt', str(groupTransactions)]

    if return_option_list:
        return new_options

    return (GDALContourOptions(new_options), callback, callback_data)


def Contour(destNameOrDestDS, srcDS, **kwargs):
    """Create contour lines or polygons from raster data.

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object.

    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.ContourOptions(), string or array of strings,
        other keywords arguments of gdal.ContourOptions().
        If options is provided as a gdal.ContourOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = ContourOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS)

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
        return wrapper_GDALContourDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALContourDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


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
    """Create a RasterizeOptions() object that can be passed to gdal.Rasterize()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("GTiff", etc...)
    outputType : any
        output type (gdalconst.GDT_Byte, etc...)
    creationOptions : any
        list or dict of creation options
    outputBounds : any
        assigned output bounds : any
        [minx, miny, maxx, maxy]
    outputSRS : any
        assigned output SRS
    transformerOptions : any
        list or dict of transformer options
    width : any
        width of the output raster in pixel
    height : any
        height of the output raster in pixel
    xRes : any
        output resolution in target SRS
    yRes : any
        output resolution in target SRS
    targetAlignedPixels : any
        whether to force output bounds to be multiple of output resolution
    noData : any
        nodata value
    initValues : any
        Value or list of values to pre-initialize the output image bands with.
         However, it is not marked as the nodata value in the output file.
          If only one value is given, the same value is used in all the bands.
    bands : any
        list of output bands to burn values into
    inverse : any
        whether to invert rasterization, i.e. burn the fixed burn value, or the
        burn value associated with the first feature into all parts of the image
        not inside the provided a polygon.
    allTouched : any
        whether to enable the ALL_TOUCHED rasterization option so that all pixels
        touched by lines or polygons will be updated, not just those on the line
        render path, or whose center point is within the polygon.
    burnValues : any
        list of fixed values to burn into each band for all objects.
        Exclusive with attribute.
    attribute : any
        identifies an attribute field on the features to be used for a burn-in value.
        The value will be burned into all output bands. Exclusive with burnValues.
    useZ : any
        whether to indicate that a burn value should be extracted from the "Z" values
        of the feature. These values are added to the burn value given by burnValues
        or attribute if provided. As of now, only points and lines are drawn in 3D.
    layers : any
        list of layers from the datasource that will be used for input features.
    SQLStatement : any
        SQL statement to apply to the source dataset
    SQLDialect : any
        SQL dialect ('OGRSQL', 'SQLITE', ...)
    where : any
        WHERE clause to apply to source layer(s)
    optim : any
        optimization mode ('RASTER', 'VECTOR')
    add : any
        set to True to use additive mode instead of replace when burning values
    callback : any
        callback method
    callback_data : any
        user data for callback
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if outputType != gdalconst.GDT_Unknown:
            new_options += ['-ot', GetDataTypeName(outputType)]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
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
            _addOptions(new_options, '-to', transformerOptions)
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

    if return_option_list:
        return new_options

    return (GDALRasterizeOptions(new_options), callback, callback_data)

def Rasterize(destNameOrDestDS, srcDS, **kwargs):
    """Burns vector geometries into a raster

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object.

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object.

    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.RasterizeOptions(), string or array of strings,
        other keywords arguments of gdal.RasterizeOptions()
        If options is provided as a gdal.RasterizeOptions() object, other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = RasterizeOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']
    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS, gdalconst.OF_VECTOR)

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
        return wrapper_GDALRasterizeDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALRasterizeDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def FootprintOptions(options=None,
                     format=None,
                     bands=None,
                     combineBands=None,
                     srcNodata=None,
                     ovr=None,
                     targetCoordinateSystem=None,
                     dstSRS=None,
                     splitPolys=None,
                     convexHull=None,
                     densify=None,
                     simplify=None,
                     maxPoints=None,
                     minRingArea=None,
                     layerName=None,
                     locationFieldName="location",
                     writeAbsolutePath=False,
                     layerCreationOptions=None,
                     datasetCreationOptions=None,
                     callback=None, callback_data=None):
    """Create a FootprintOptions() object that can be passed to gdal.Footprint()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("GeoJSON", etc...)
    bands : any
        list of output bands to burn values into
    combineBands : any
        how to combine bands: "union" (default) or "intersection"
    srcNodata : any
        source nodata value(s).
    ovr : any
        overview index.
    targetCoordinateSystem : any
        "pixel" or "georef"
    dstSRS : any
        output SRS
    datasetCreationOptions : any
        list or dict of dataset creation options
    layerCreationOptions : any
        list or dict of layer creation options
    splitPolys : any
        whether to split multipolygons as several polygons
    convexHull : any
        whether to compute the convex hull of polygons/multipolygons
    densify : any
        tolerance value for polygon densification
    simplify : any
        tolerance value for polygon simplification
    maxPoints : any
        maximum number of points (100 by default, "unlimited" for unlimited)
    minRingArea : any
        Minimum value for the area of a ring The unit of the area is in square pixels if targetCoordinateSystem equals "pixel", or otherwise in georeferenced units of the target vector dataset. This option is applied after the reprojection implied by dstSRS
    locationFieldName : any
        Specifies the name of the field in the resulting vector dataset where the path of the input dataset will be stored. The default field name is "location". Can be set to None to disable creation of such field.
    writeAbsolutePath : any
        Enables writing the absolute path of the input dataset. By default, the filename is written in the location field exactly as the dataset name.
    layerName : any
        output layer name
    callback : any
        callback method
    callback_data : any
        user data for callback
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if bands is not None:
            for b in bands:
                new_options += ['-b', str(b)]
        if combineBands:
            new_options += ["-combine_bands", combineBands]
        if targetCoordinateSystem:
            new_options += ["-t_cs", targetCoordinateSystem]
        if dstSRS:
            new_options += ["-t_srs", dstSRS]
        if srcNodata is not None:
            new_options += ['-srcnodata', str(srcNodata)]
        if ovr is not None:
            new_options += ['-ovr', str(ovr)]
        if splitPolys:
            new_options += ["-split_polys"]
        if convexHull:
            new_options += ["-convex_hull"]
        if densify is not None:
            new_options += ['-densify', str(densify)]
        if simplify is not None:
            new_options += ['-simplify', str(simplify)]
        if maxPoints is not None:
            new_options += ['-max_points', str(maxPoints)]
        if minRingArea is not None:
            new_options += ['-min_ring_area', str(minRingArea)]
        if layerName is not None:
            new_options += ['-lyr_name', layerName]
        if datasetCreationOptions is not None:
            _addOptions(new_options, '-dsco', datasetCreationOptions)
        if layerCreationOptions is not None:
            _addOptions(new_options, '-lco', layerCreationOptions)
        if locationFieldName is not None:
            new_options += ['-location_field_name', locationFieldName]
        else:
            new_options += ['-no_location']
        if writeAbsolutePath:
            new_options += ['-write_absolute_path']

    if return_option_list:
        return new_options

    return (GDALFootprintOptions(new_options), callback, callback_data)

def Footprint(destNameOrDestDS, srcDS, **kwargs):
    """Compute the footprint of a raster

    Parameters
    ----------
    destNameOrDestDS : any
        Output dataset name or object

        If passed as a dataset name, a potentially existing output dataset of
        the same name will be overwritten. To update an existing output dataset,
        it must be passed as a dataset object.

    srcDS : any
        a Dataset object or a filename
    **kwargs : any
        options: return of gdal.FootprintOptions(), string or array of strings,
        other keywords arguments of gdal.FootprintOptions()
        If options is provided as a gdal.FootprintOptions() object, other keywords are ignored.

    Examples
    --------

    .. testsetup::

        >>> src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    1. Special mode to get deserialized GeoJSON (in EPSG:4326 if dstSRS not specified):

    >>> deserialized_geojson = gdal.Footprint(None, src_ds, format="GeoJSON")

    2. Special mode to get WKT:

    >>> wkt = gdal.Footprint(None, src_ds, format="WKT")

    3. Get result in a GeoPackage

    >>> gdal.Footprint("out.gpkg", src_ds, format="GPKG")
    <osgeo.gdal.Dataset; proxy of <Swig Object of type 'GDALDatasetShadow *' at 0x...> >
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    inline_geojson_requested = (destNameOrDestDS is None or destNameOrDestDS == "") and \
        "format" in kwargs and kwargs["format"] == "GeoJSON"
    if inline_geojson_requested and "dstSRS" not in kwargs:
        import copy
        kwargs = copy.copy(kwargs)
        kwargs["dstSRS"] = "EPSG:4326"

    wkt_requested = (destNameOrDestDS is None or destNameOrDestDS == "") and \
        "format" in kwargs and kwargs["format"] == "WKT"
    if wkt_requested:
        import copy
        kwargs = copy.copy(kwargs)
        kwargs["format"] = "GeoJSON"

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = FootprintOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    if isinstance(srcDS, (str, os.PathLike)):
        srcDS = OpenEx(srcDS, gdalconst.OF_RASTER)

    if inline_geojson_requested or wkt_requested:
        import uuid
        temp_filename = "/vsimem/" + str(uuid.uuid4())
        try:
            if not wrapper_GDALFootprintDestName(temp_filename, srcDS, opts, callback, callback_data):
                return None
            if inline_geojson_requested:
                f = VSIFOpenL(temp_filename, "rb")
                assert f
                VSIFSeekL(f, 0, 2) # SEEK_END
                size = VSIFTellL(f)
                VSIFSeekL(f, 0, 0) # SEEK_SET
                data = VSIFReadL(1, size, f)
                VSIFCloseL(f)
                import json
                return json.loads(data)
            else:
                assert wkt_requested
                ds = OpenEx(temp_filename)
                lyr = ds.GetLayer(0)
                wkts = []
                for f in lyr:
                    wkts.append(f.GetGeometryRef().ExportToWkt())
                if len(wkts) == 1:
                    return wkts[0]
                else:
                    return wkts
        finally:
            if VSIStatL(temp_filename):
                Unlink(temp_filename)

    if isinstance(destNameOrDestDS, (str, os.PathLike)):
        return wrapper_GDALFootprintDestName(destNameOrDestDS, srcDS, opts, callback, callback_data)
    else:
        return wrapper_GDALFootprintDestDS(destNameOrDestDS, srcDS, opts, callback, callback_data)


def BuildVRTOptions(options=None,
                    resolution=None,
                    outputBounds=None,
                    xRes=None,
                    yRes=None,
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
                    nodataMaxMaskThreshold=None,
                    strict=False,
                    writeAbsolutePath=False,
                    pixelFunction=None,
                    pixelFunctionArgs=None,
                    creationOptions=None,
                    callback=None, callback_data=None):
    """Create a BuildVRTOptions() object that can be passed to gdal.BuildVRT()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    resolution : any
        'highest', 'lowest', 'average', 'user'.
    outputBounds : any
        output bounds as (minX, minY, maxX, maxY) in target SRS.
    xRes : any
        output resolution in target SRS.
    yRes : any
        output resolution in target SRS.
    targetAlignedPixels : any
        whether to force output bounds to be multiple of output resolution.
    separate : any
        whether each source file goes into a separate stacked band in the VRT band.
    bandList : any
        array of band numbers (index start at 1).
    addAlpha : any
        whether to add an alpha mask band to the VRT when the source raster have none.
    resampleAlg : any
        resampling mode.
    outputSRS : any
        assigned output SRS.
    allowProjectionDifference : any
        whether to accept input datasets have not the same projection.
        Note: they will *not* be reprojected.
    srcNodata : any
        source nodata value(s).
    VRTNodata : any
        nodata values at the VRT band level.
    hideNodata : any
        whether to make the VRT band not report the NoData value.
    nodataMaxMaskThreshold : any
        value of the mask band of a source below which the source band values should be replaced by VRTNodata (or 0 if not specified)
    strict : any
        set to True if warnings should be failures
    pixelFunction : any
        a pixel function to use to calculate output pixel values when multiple
        sources overlap. For a list of available pixel functions, see
        :ref:`builtin_pixel_functions`.
    pixelFunctionArgs : any
        list or dict of pixel function arguments
    creationOptions : any
        list or dict of creation options
    writeAbsolutePath : any
        Enables writing the absolute path of the input datasets. By default, input filenames are written in a relative way with respect to the VRT filename (when possible)
    callback : any
        callback method.
    callback_data : any
        user data for callback.
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
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
            if resampleAlg in mapGRIORAMethodToString:
                new_options += ['-r', mapGRIORAMethodToString[resampleAlg]]
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
        if nodataMaxMaskThreshold is not None:
            new_options += ['-nodata_max_mask_threshold', str(nodataMaxMaskThreshold)]
        if hideNodata:
            new_options += ['-hidenodata']
        if strict:
            new_options += ['-strict']
        if writeAbsolutePath:
            new_options += ['-write_absolute_path']
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
        if pixelFunction:
            new_options += ['-pixel-function', pixelFunction]
        if pixelFunctionArgs:
            _addOptions(new_options, '-pixel-function-arg', pixelFunctionArgs)

    if return_option_list:
        return new_options

    return (GDALBuildVRTOptions(new_options), callback, callback_data)

def BuildVRT(destName, srcDSOrSrcDSTab, **kwargs):
    """Build a VRT from a list of datasets.

    Parameters
    ----------
    destName : any
        Output dataset name.
    srcDSOrSrcDSTab : any
        An array of Dataset objects or filenames, or a Dataset object or a filename.
    **kwargs : any
        options: return of gdal.BuildVRTOptions(), string or array of strings,
        other keywords arguments of gdal.BuildVRTOptions().
        If options is provided as a gdal.BuildVRTOptions() object,
        other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = BuildVRTOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    srcDSTab = []
    srcDSNamesTab = []

    if isinstance(srcDSOrSrcDSTab, (str, os.PathLike)):
        srcDSNamesTab = [str(srcDSOrSrcDSTab)]
    elif isinstance(srcDSOrSrcDSTab, list):
        for elt in srcDSOrSrcDSTab:
            if isinstance(elt, (str, os.PathLike)):
                srcDSNamesTab.append(str(elt))
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


def TileIndexOptions(options=None,
                     overwrite=None,
                     recursive=None,
                     filenameFilter=None,
                     minPixelSize=None,
                     maxPixelSize=None,
                     format=None,
                     layerName=None,
                     layerCreationOptions=None,
                     locationFieldName="location",
                     outputSRS=None,
                     writeAbsolutePath=None,
                     skipDifferentProjection=None,
                     gtiFilename=None,
                     xRes=None,
                     yRes=None,
                     outputBounds=None,
                     colorInterpretation=None,
                     noData=None,
                     bandCount=None,
                     mask=None,
                     metadataOptions=None,
                     fetchMD=None):
    """Create a TileIndexOptions() object that can be passed to gdal.TileIndex()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    overwrite : any
        Whether to overwrite the existing tile index
    recursive : any
        Whether directories specified in source filenames should be explored recursively
    filenameFilter : any
        Pattern that the filenames contained in directories pointed by <file_or_dir> should follow. '*' and '?' wildcard can be used. String or list of strings.
    minPixelSize : any
        Minimum pixel size in term of geospatial extent per pixel (resolution) that a raster should have to be selected.
    maxPixelSize : any
        Maximum pixel size in term of geospatial extent per pixel (resolution) that a raster should have to be selected.
    format : any
        output format ("ESRI Shapefile", "GPKG", etc...)
    layerName : any
        output layer name
    layerCreationOptions : any
        list or dict of layer creation options
    locationFieldName : any
        Specifies the name of the field in the resulting vector dataset where the path of the input dataset will be stored. The default field name is "location". Can be set to None to disable creation of such field.
    outputSRS : any
        assigned output SRS
    writeAbsolutePath : any
        Enables writing the absolute path of the input dataset. By default, the filename is written in the location field exactly as the dataset name.
    skipDifferentProjection : any
        Whether to skip sources that have a different SRS
    gtiFilename : any
        Filename of the GDAL XML Tile Index file
    xRes : any
        output horizontal resolution
    yRes : any
        output vertical resolution
    outputBounds : any
        output bounds as [minx, miny, maxx, maxy]
    colorInterpretation : any
        Tile color interpretation, as a single value or a list, of the following values ("red", "green", "blue", "alpha", "grey", "undefined", etc.) or their GCI_xxxx symbolic names
    noData : any
        tile nodata value, as a single value or a list
    bandCount : any
        number of band of tiles in the index
    mask : any
        whether tiles have a band mask
    metadataOptions : any
        list or dict of metadata options
    fetchMD : any
        Fetch a metadata item from the raster tile and write it as a field in the
        tile index.
        Tuple (raster metadata item name, target field name, target field type), or list of such tuples, with target field type in "String", "Integer", "Integer64", "Real", "Date", "DateTime";
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'
    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if overwrite:
            new_options += ['-overwrite']
        if recursive:
            new_options += ['-recursive']
        if filenameFilter is not None:
            if isinstance(filenameFilter, list):
                for filter in filenameFilter:
                    new_options += ['-filename_filter', filter]
            else:
                new_options += ['-filename_filter', filenameFilter]
        if minPixelSize is not None:
            new_options += ['-min_pixel_size', _strHighPrec(minPixelSize)]
        if maxPixelSize is not None:
            new_options += ['-max_pixel_size', _strHighPrec(maxPixelSize)]
        if format:
            new_options += ['-f', format]
        if layerName is not None:
            new_options += ['-lyr_name', layerName]

        if layerCreationOptions is not None:
            _addOptions(new_options, '-lco', layerCreationOptions)

        if locationFieldName is not None:
            new_options += ['-tileindex', locationFieldName]
        if outputSRS is not None:
            new_options += ['-t_srs', str(outputSRS)]
        if writeAbsolutePath:
            new_options += ['-write_absolute_path']
        if skipDifferentProjection:
            new_options += ['-skip_different_projection']
        if gtiFilename is not None:
            new_options += ['-gti_filename', gtiFilename]
        if xRes is not None and yRes is not None:
            new_options += ['-tr', _strHighPrec(xRes), _strHighPrec(yRes)]
        elif xRes is not None:
            raise Exception("yRes should also be specified")
        elif yRes is not None:
            raise Exception("xRes should also be specified")
        if outputBounds is not None:
            new_options += ['-te', _strHighPrec(outputBounds[0]), _strHighPrec(outputBounds[1]), _strHighPrec(outputBounds[2]), _strHighPrec(outputBounds[3])]
        if colorInterpretation is not None:
            def colorInterpAsString(x):
                return GetColorInterpretationName(x) if isinstance(x, int) else x
            if isinstance(colorInterpretation, list):
                new_options += ['-colorinterp', ','.join([colorInterpAsString(x) for x in colorInterpretation])]
            else:
                new_options += ['-colorinterp', colorInterpAsString(colorInterpretation)]
        if noData is not None:
            if isinstance(noData, list):
                new_options += ['-nodata', ','.join([_strHighPrec(x) for x in noData])]
            else:
                new_options += ['-nodata', _strHighPrec(noData)]
        if bandCount is not None:
            new_options += ['-bandcount', str(bandCount)]
        if mask:
            new_options += ['-mask']
        if metadataOptions is not None:
            _addOptions(new_options, '-mo', metadataOptions)
        if fetchMD is not None:
            if isinstance(fetchMD, list):
                for mdItemName, fieldName, fieldType in fetchMD:
                    new_options += ['-fetch_md', mdItemName, fieldName, fieldType]
            else:
                new_options += ['-fetch_md', fetchMD[0], fetchMD[1], fetchMD[2]]

    if return_option_list:
        return new_options

    callback = None
    callback_data = None
    return (GDALTileIndexOptions(new_options), callback, callback_data)

def TileIndex(destName, srcFilenames, **kwargs):
    """Build a tileindex from a list of datasets.

    Parameters
    ----------
    destName : any
        Output dataset name.
    srcFilenames : any
        An array of filenames.
    **kwargs  : any
        options: return of gdal.TileIndexOptions(), string or array of strings,
        other keywords arguments of gdal.TileIndexOptions().
        If options is provided as a gdal.TileIndexOptions() object,
        other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = TileIndexOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    srcDSNamesTab = []

    if isinstance(srcFilenames, (str, os.PathLike)):
        srcDSNamesTab = [str(srcFilenames)]
    elif isinstance(srcFilenames, list):
        for elt in srcFilenames:
            srcDSNamesTab.append(str(elt))
    else:
        raise Exception("Unexpected type for srcFilenames")

    return TileIndexInternalNames(destName, srcDSNamesTab, opts, callback, callback_data)


def MultiDimTranslateOptions(options=None, format=None, creationOptions=None,
         arraySpecs=None, arrayOptions=None, groupSpecs=None, subsetSpecs=None, scaleAxesSpecs=None,
         callback=None, callback_data=None):
    """Create a MultiDimTranslateOptions() object that can be passed to gdal.MultiDimTranslate()

    Parameters
    ----------
    options : any
        can be an array of strings, a string or let empty and filled from other keywords.
    format : any
        output format ("GTiff", etc...)
    creationOptions : any
        list or dict of creation options
    arraySpecs : any
        list of array specifications, each of them being an array name or
        "name={src_array_name},dstname={dst_name},transpose=[1,0],view=[:,::-1]"
    arrayOptions : any
        list of options passed to `GDALGroup.GetMDArrayNames` to filter reported arrays.
    groupSpecs : any
        list of group specifications, each of them being a group name or
        "name={src_array_name},dstname={dst_name},recursive=no"
    subsetSpecs : any
        list of subset specifications, each of them being like
        "{dim_name}({min_val},{max_val})" or "{dim_name}({slice_va})"
    scaleAxesSpecs : any
        list of dimension scaling specifications, each of them being like
        "{dim_name}({scale_factor})"
    callback : any
        callback method
    callback_data : any
        user data for callback
    """

    # Only used for tests
    return_option_list = options == '__RETURN_OPTION_LIST__'

    if return_option_list:
        options = []
    else:
        options = [] if options is None else options

    if isinstance(options, str):
        new_options = ParseCommandLine(options)
    else:
        import copy
        new_options = copy.copy(options)
        if format is not None:
            new_options += ['-of', format]
        if creationOptions is not None:
            _addCreationOptions(new_options, creationOptions)
        if arraySpecs is not None:
            for s in arraySpecs:
                new_options += ['-array', s]
        if arrayOptions:
            for option in arrayOptions:
                new_options += ['-arrayoption', option]
        if groupSpecs is not None:
            for s in groupSpecs:
                new_options += ['-group', s]
        if subsetSpecs is not None:
            for s in subsetSpecs:
                new_options += ['-subset', s]
        if scaleAxesSpecs is not None:
            for s in scaleAxesSpecs:
                new_options += ['-scaleaxes', s]

    if return_option_list:
        return new_options

    return (GDALMultiDimTranslateOptions(new_options), callback, callback_data)

def MultiDimTranslate(destName, srcDSOrSrcDSTab, **kwargs):
    """MultiDimTranslate one or several datasets.

    Parameters
    ----------
    destName : any
        Output dataset name
    srcDSOrSrcDSTab : any
        an array of Dataset objects or filenames, or a Dataset object or a filename
    **kwargs : any
        options: return of gdal.MultiDimTranslateOptions(), string or array of strings
        other keywords arguments of gdal.MultiDimTranslateOptions().
        If options is provided as a gdal.MultiDimTranslateOptions() object,
        other keywords are ignored.
    """

    _WarnIfUserHasNotSpecifiedIfUsingExceptions()

    if 'options' not in kwargs or isinstance(kwargs['options'], (list, str)):
        (opts, callback, callback_data) = MultiDimTranslateOptions(**kwargs)
    else:
        (opts, callback, callback_data) = kwargs['options']

    import os

    if isinstance(srcDSOrSrcDSTab, (str, os.PathLike)):
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


def EscapeString(*args, **kwargs):
    """EscapeString(string_or_bytes, scheme = gdal.CPLES_SQL)"""
    if isinstance(args[0], bytes):
        return _gdal.EscapeBinary(*args, **kwargs)
    else:
        return _gdal.wrapper_EscapeString(*args, **kwargs)


def ApplyVerticalShiftGrid(*args, **kwargs):
    """ApplyVerticalShiftGrid(Dataset src_ds, Dataset grid_ds, bool inverse=False, double srcUnitToMeter=1.0, double dstUnitToMeter=1.0, char ** options=None) -> Dataset"""

    from warnings import warn
    warn('ApplyVerticalShiftGrid() will be removed in GDAL 4.0', DeprecationWarning)
    return _ApplyVerticalShiftGrid(*args, **kwargs)


import contextlib
@contextlib.contextmanager
def config_options(options, thread_local=True):
    """Temporarily define a set of configuration options.

       Parameters
       ----------
       options : dict
            Dictionary of configuration options passed as key, value
       thread_local : bool, default=True
            Whether the configuration options should be only set on the current
            thread.

       Returns
       -------
       contextlib.contextmanager
           A context manager

       Examples
       --------

       >>> with gdal.config_options({"GDAL_NUM_THREADS": "ALL_CPUS"}):
       ...     gdal.Warp("out.tif", "in.tif", dstSRS="EPSG:4326")
       <osgeo.gdal.Dataset; proxy of <Swig Object of type 'GDALDatasetShadow *' at 0x...> >
    """
    get_config_option = GetThreadLocalConfigOption if thread_local else GetGlobalConfigOption
    set_config_option = SetThreadLocalConfigOption if thread_local else SetConfigOption

    oldvals = {key: get_config_option(key) for key in options}

    for key in options:
        set_config_option(key, options[key])
    try:
        yield
    finally:
        for key in options:
            set_config_option(key, oldvals[key])


def config_option(key, value, thread_local=True):
    """Temporarily define a configuration option.

       Parameters
       ----------
       key : str
            Name of the configuration option
       value : str
            Value of the configuration option
       thread_local : bool, default=True
            Whether the configuration option should be only set on the current
            thread.

       Returns
       -------
       contextlib.contextmanager
           A context manager

       Examples
       --------

       >>> with gdal.config_option("GDAL_NUM_THREADS", "ALL_CPUS"):
       ...     gdal.Warp("out.tif", "in.tif", dstSRS="EPSG:4326")
       <osgeo.gdal.Dataset; proxy of <Swig Object of type 'GDALDatasetShadow *' at 0x...> >
    """
    return config_options({key: value}, thread_local=thread_local)


@contextlib.contextmanager
def quiet_errors():
    """Temporarily install an error handler that silences all warnings and errors.

       Returns
       -------
       contextlib.contextmanager
           A context manager

       Examples
       --------

       >>> with gdal.ExceptionMgr(useExceptions=False), gdal.quiet_errors():
       ...     gdal.Error(gdal.CE_Failure, gdal.CPLE_AppDefined, "you will never see me")
    """
    PushErrorHandler("CPLQuietErrorHandler")
    try:
        yield
    finally:
        PopErrorHandler()

@contextlib.contextmanager
def quiet_warnings():
    """Temporarily install an error handler that silences all warnings.

       .. versionadded:: 3.11

       Returns
       -------
       contextlib.contextmanager
           A context manager

       Examples
       --------

       >>> with gdal.ExceptionMgr(useExceptions=False), gdal.quiet_warnings():
       ...     gdal.Error(gdal.CE_Warning, gdal.CPLE_AppDefined, "you will never see me")
    """
    PushErrorHandler("CPLQuietWarningsErrorHandler")
    try:
        yield
    finally:
        PopErrorHandler()


def Run(*alg, arguments={}, progress=None, **kwargs):
    """Run a GDAL algorithm and return it.

       .. versionadded:: 3.11

       This method can also be used within a context manager, in which case
       :py:meth:`osgeo.gdal.Algorithm.Finalize` will be called at the exit of the
       context manager.  An exception will be raised if the algorithm fails,
       even if :py:meth:`osgeo.gdal.UseExceptions()` has not been called.

       Parameters
       ----------
       alg : str, list[str], tuple[str] or Algorithm
            Path to the algorithm or algorithm instance itself. For example "raster info", ["raster", "info"] or "raster", "info".
       arguments : dict
            Input arguments of the algorithm. For example {"format": "json", "input": "byte.tif"}
       progress : callable
            Progress function whose arguments are a progress ratio, a string and a user data
       **kwargs : any
            Instead of using the ``arguments`` parameter, it is possible to pass
            algorithm arguments directly as named parameters of gdal.Run().
            If the named argument has dash characters in it, the corresponding
            parameter must replace them with an underscore character.
            For example ``dst_crs`` as a a parameter of gdal.Run(), instead of
            ``dst-crs`` which is the name to use on the command line.

       Returns
       -------
       Algorithm

       Examples
       --------

       >>> alg = gdal.Run(["raster", "info"], {"input": "byte.tif"})
       >>> print(alg.Output()["bands"])
       [{'band': 1, 'block': [20, 20], 'type': 'Byte', 'colorInterpretation': 'Gray', 'metadata': {}}]

       >>> with gdal.Run("raster", "reproject", input="byte.tif", output_format="MEM", dst_crs="EPSG:4326") as alg:
       ...     print(alg.Output().ReadAsArray().shape)
       (18, 22)
    """

    new_alg = []
    for v in alg:
        if isinstance(v, dict):
            arguments = v
            break
        new_alg.append(v)
    alg = new_alg

    if len(alg) == 1 and isinstance(alg[0], Algorithm):
        alg = alg[0]
    elif len(alg) >= 1 and (isinstance(alg[0], list) or isinstance(alg[0], str)):
        alg = Algorithm(*alg)
    else:
        raise RuntimeError("Wrong type for alg. Expected string, list of strings or Algorithm")

    for k in arguments:
        alg[k.replace('_', '-')] = arguments[k]

    for k in kwargs:
        alg[k.replace('_', '-')] = kwargs[k]

    if not alg.Run(progress):
        # We go here only if gdal.UseExceptions() has not been called
        raise RuntimeError("Algorithm.Run() failed: %s" % GetLastErrorMsg())

    return alg

%}


%feature("pythonappend") IsLineOfSightVisible %{
    is_visible, col_intersection, row_intersection = val
    import collections
    tuple = collections.namedtuple('IsLineOfSightVisibleResult', ['is_visible', 'col_intersection', 'row_intersection'])
    tuple.is_visible = is_visible
    tuple.col_intersection = col_intersection
    tuple.row_intersection = row_intersection
    val = tuple
%}


%feature("pythonappend") MultipartUploadGetCapabilities %{
    if val:
        non_sequential_upload_supported, parallel_upload_supported, abort_supported, min_part_size, max_part_size, max_part_count = val
        import collections
        tuple = collections.namedtuple('MultipartUploadGetCapabilitiesResult',
            ['non_sequential_upload_supported',
             'parallel_upload_supported',
             'abort_supported',
             'min_part_size',
             'max_part_size',
             'max_part_count',
             ])
        tuple.non_sequential_upload_supported = non_sequential_upload_supported
        tuple.parallel_upload_supported = parallel_upload_supported
        tuple.abort_supported = abort_supported
        tuple.min_part_size = min_part_size
        tuple.max_part_size = max_part_size
        tuple.max_part_count = max_part_count
        val = tuple
%}

%feature("shadow") InterpolateAtPoint %{
def InterpolateAtPoint(self, *args, **kwargs):
    """Return the interpolated value at pixel and line raster coordinates.
       See :cpp:func:`GDALRasterBand::InterpolateAtPoint`.

       Parameters
       ----------
       pixel : float
       line : float
       interpolation : str
           Resampling algorithm to use. One of:

           - ``nearest``
           - ``bilinear``
           - ``cubic``
           - ``cubicspline``

       Returns
       -------
       float
           Interpolated value, or ``None`` if it has any error.
    """

    ret = $action(self, *args, **kwargs)
    if ret[0] != CE_None:
        return None

    from . import gdal
    if gdal.DataTypeIsComplex(self.DataType):
        return complex(ret[1], ret[2])
    else:
        return ret[1]
%}

%feature("shadow") InterpolateAtGeolocation %{
def InterpolateAtGeolocation(self, *args, **kwargs):
    """Return the interpolated value at georeferenced coordinates.
       See :cpp:func:`GDALRasterBand::InterpolateAtGeolocation`.

       When srs is None, those georeferenced coordinates (geolocX, geolocY)
       must be in the "natural" SRS of the dataset, that is the one returned by
       GetSpatialRef() if there is a geotransform, GetGCPSpatialRef() if there are
       GCPs, WGS 84 if there are RPC coefficients, or the SRS of the geolocation
       array (generally WGS 84) if there is a geolocation array.
       If that natural SRS is a geographic one, geolocX must be a longitude, and
       geolocY a latitude. If that natural SRS is a projected one, geolocX must
       be a easting, and geolocY a northing.

       When srs is set to a non-None value, (geolocX, geolocY) must be
       expressed in that CRS, and that tuple must be conformant with the
       data-axis-to-crs-axis setting of srs, that is the one returned by
       the :py:func:`osgeo.osr.SpatialReference.GetDataAxisToSRSAxisMapping`.
       If you want to be sure of the axis order, then make sure to call
       ``srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)``
       before calling this method, and in that case, geolocX must be a longitude
       or an easting value, and geolocX a latitude or a northing value.

       Parameters
       ----------
       geolocX : float
           X coordinate of the position where interpolation should be done.
           Longitude or easting in "natural" CRS if `srs` is None,
           otherwise consistent with first axis of `srs`,
           taking into account the data-axis-to-crs-axis mapping
       geolocY : float
           Y coordinate of the position where interpolation should be done.
           Latitude or northing in "natural" CRS if `srs` is None,
           otherwise consistent with second axis of `srs`,
           taking into account the data-axis-to-crs-axis mapping
       srs : object
           :py:class:`osr.SpatialReference`. If set, override the natural CRS in which geolocX, geolocY are expressed
       interpolation : str
           Resampling algorithm to use. One of:

           - ``nearest``
           - ``bilinear``
           - ``cubic``
           - ``cubicspline``

       Returns
       -------
       float
           Interpolated value, or ``None`` if it has any error.

       Examples
       --------

       >>> longitude_degree = -117.64
       >>> latitude_degree = 33.90
       >>> with gdal.Open("byte.tif") as ds:
       ...    wgs84_srs = osr.SpatialReference("WGS84")
       ...    wgs84_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
       ...    ds.GetRasterBand(1).InterpolateAtGeolocation(longitude_degree, \
                                                           latitude_degree, \
                                                           wgs84_srs, \
                                                           gdal.GRIORA_Bilinear)
       135.62  # interpolated value, rtol: 1e-3
    """

    ret = $action(self, *args, **kwargs)
    if ret[0] != CE_None:
        return None

    from . import gdal
    if gdal.DataTypeIsComplex(self.DataType):
        return complex(ret[1], ret[2])
    else:
        return ret[1]
%}

%feature("shadow") ComputeMinMaxLocation %{
def ComputeMinMaxLocation(self, *args, **kwargs):
    """Compute the min/max values for a band, and their location.

       Pixels whose value matches the nodata value or are masked by the mask
       band are ignored.

       If the minimum or maximum value is hit in several locations, it is not
       specified which one will be returned.

       This is a mapping of :cpp:func:`GDALRasterBand::ComputeRasterMinMaxLocation`.

       Parameters
       ----------
       None

       Returns
       -------
       tuple or None
           a named tuple (min, max, minX, minY, maxX, maxY) or ``None``
           in case of error or no valid pixel.
    """

    ret = $action(self, *args, **kwargs)
    if ret[0] != CE_None:
        return None

    import collections
    tuple = collections.namedtuple('ComputeMinMaxLocationResult',
            ['min',
             'max',
             'minX',
             'minY',
             'maxX',
             'maxY',
             ])
    tuple.min = ret[1]
    tuple.max = ret[2]
    tuple.minX = ret[3]
    tuple.minY = ret[4]
    tuple.maxX = ret[5]
    tuple.maxY = ret[6]
    return tuple
%}

%pythoncode %{

# VSIFile: Copyright (c) 2024, Dan Baston <dbaston at gmail.com>

from io import BytesIO

class VSIFile(BytesIO):
    """Class wrapping a GDAL VSILFILE instance as a Python BytesIO instance

       :since: GDAL 3.11
    """

    def __init__(self, path, mode, encoding="utf-8", options = {}):
        self._path = path
        self._mode = mode

        self._binary = "b" in mode
        self._encoding = encoding

        self._closed = True
        self._fp = None

        self._fp = VSIFOpenExL(self._path, self._mode, True, options)
        if self._fp is None:
            raise OSError(VSIGetLastErrorMsg())

        self._closed = False

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __iter__(self):
        return self

    def __next__(self):
        line = CPLReadLineL(self._fp)
        if line is None:
            raise StopIteration
        if self._binary:
            return line.encode()
        return line

    def close(self):
        if self._closed:
            return

        self._closed = True
        VSIFCloseL(self._fp)

    def read(self, size=-1):
        if size == -1:
            pos = self.tell()
            self.seek(0, 2)
            size = self.tell()
            self.seek(pos)

        raw = VSIFReadL(1, size, self._fp)

        if self._binary:
            return bytes(raw)
        else:
            return raw.decode(self._encoding)

    def write(self, x):

        if self._binary:
            assert type(x) in (bytes, bytearray, memoryview)
        else:
            assert type(x) is str
            x = x.encode(self._encoding)

        planned_write = len(x)
        actual_write = VSIFWriteL(x, 1, planned_write, self._fp)

        if planned_write != actual_write:
            raise OSError(
                f"Expected to write {planned_write} bytes but {actual_write} were written"
            )

    def seek(self, offset, whence=0):
        # We redefine the docstring since otherwise breathe would complain on the one coming from BytesIO.seek()
        """Change stream position.

           Seek to byte offset pos relative to position indicated by whence:

           - 0: Start of stream (the default).  pos should be >= 0;
           - 1: Current position - pos may be negative;
           - 2: End of stream - pos usually negative.

           Returns the new absolute position.
        """

        if VSIFSeekL(self._fp, offset, whence) != 0:
            raise OSError(VSIGetLastErrorMsg())

    def tell(self):
        return VSIFTellL(self._fp)
%}


/* -------------------------------------------------------------------- */
/* GDALAlgorithmRegistryHS                                              */
/* -------------------------------------------------------------------- */

%extend GDALAlgorithmRegistryHS {
%pythoncode %{

    def __getitem__(self, key):
        """Instantiate an algorithm

           Shortcut for self.InstantiateAlg(key)

           Examples
           --------
           >>> gdal.GetGlobalAlgorithmRegistry()["raster"]
           <osgeo.gdal.Algorithm; proxy of <Swig Object of type 'GDALAlgorithmHS *' at 0x...> >
        """

        alg = self.InstantiateAlg(key)
        if not alg:
            raise RuntimeError(f"'{key}' is not a valid algorithm")
        return alg
%}
}

/* -------------------------------------------------------------------- */
/* GDALAlgorithmHS                                                      */
/* -------------------------------------------------------------------- */

%extend GDALAlgorithmHS {
%pythoncode %{

    def __init__(self, *path):

        """Instantiate an existing GDAL algorithm from its path.

           .. versionadded:: 3.11

           Parameters
           ----------
           path : str, list[str] or tuple[str]
                Path to the algorithm. For example "raster info", ["raster", "info"] or "raster", "info"

           Returns
           -------
           any
                An algorithm

           Examples
           --------

           >>> alg = gdal.Algorithm("raster", "info")
           >>> # or alg = gdal.Algorithm(["raster", "info"])
           >>> # or alg = gdal.Algorithm("raster info")
           >>> alg["input"] = "byte.tif"
           >>> alg.Run()
           True
           >>> print(alg.Output()["bands"])
           [{'band': 1, 'block': [20, 20], 'type': 'Byte', 'colorInterpretation': 'Gray', 'metadata': {}}]
        """

        alg = None
        if len(path) == 1:
            if isinstance(path[0], list):
                alg = GetGlobalAlgorithmRegistry()
                alg_is_registry = True
                for i, v in enumerate(path[0]):
                    if i == 0 and v == "gdal":
                        continue
                    alg_is_registry = False
                    alg = alg[v]
                if alg_is_registry:
                    alg = alg["gdal"]
            elif isinstance(path[0], str):
                alg = GetGlobalAlgorithmRegistry()
                alg_is_registry = True
                for v in path[0].lstrip("gdal ").split(' '):
                    alg_is_registry = False
                    alg = alg[v]
                if alg_is_registry:
                    alg = alg["gdal"]
        elif len(path) > 1:
            alg = GetGlobalAlgorithmRegistry()
            for i, v in enumerate(path):
                if i == 0 and v == "gdal":
                    continue
                alg = alg[v]
        if not alg:
            raise RuntimeError("Wrong type for algorithm path. Expected string or list of strings")

        self.this = alg.this
        self.thisown = True


    def __enter__(self):
        return self

    def __exit__(self, *args):
        if hasattr(self, "has_run") and not self.Finalize():
            # We go here only if gdal.UseExceptions() has not been called
            raise RuntimeError("Algorithm.Finalize() failed: %s" % GetLastErrorMsg())

    def _get_arg_value(self, arg, parse_json):
        val = arg.Get()
        if parse_json and arg.GetType() == GAAT_STRING and \
           ((val.startswith('{') and (val.endswith('}') or val.endswith('}\n'))) or \
           (val.startswith('[') and (val.endswith(']') or val.endswith(']\n')))):
            import json
            try:
                return json.loads(val)
            except Exception:
                return val
        elif arg.GetType() == GAAT_DATASET:
            return val.GetDataset()
        else:
            return val


    def Output(self, parse_json=True):
        """Return the single output value of this algorithm, after it has been run.

           If there are multiple output values, this method will raise an exception,
           and the :py:meth:`Outputs` (plural) method should be called instead.

           Arguments of type GAAT_DATASET are returned as a
           :py:class:`osgeo.gdal.Dataset` instance.

           Parameters
           ----------
           parse_json : bool, default=True
               Whether a JSON string should be returned as a dict or list (instead of a string).

           Returns
           -------
           any
               The single output argument value.

           Examples
           --------
           >>> with gdal.Run("raster", "info", input="byte.tif") as alg:
           ...    print(alg.Output()["bands"])
           [{'band': 1, 'block': [20, 20], 'type': 'Byte', 'colorInterpretation': 'Gray', 'metadata': {}}]
        """

        if not hasattr(self, "has_run"):
            raise RuntimeError("Algorithm.Run() must be called before")

        output_args = []
        output_args_set = []
        for name in self.GetArgNames():
            arg = self.GetArg(name)
            if arg.IsOutput():
                output_args.append(arg)
                if arg.IsExplicitlySet():
                    output_args_set.append(arg)

        if len(output_args) == 1:
            return self._get_arg_value(output_args[0], parse_json)
        elif len(output_args_set) == 1:
            return self._get_arg_value(output_args_set[0], parse_json)
        elif len(output_args) >= 2:
            raise RuntimeError("Cannot use 'output' method on this algorithm as it supports multiple output arguments. Use 'Outputs' (plural) instead")
        else:
            return None


    def Outputs(self, parse_json=True):
        """Return the output value(s) of this algorithm as a dict, after it has been run.

           Most algorithms only return a single output, in which case the :py:meth:`Output`
           method (singular) is preferable for easier use.

           Arguments of type GAAT_DATASET are returned as a
           :py:class:`osgeo.gdal.Dataset` instance.

           Parameters
           ----------
           parse_json : bool, default=True
               Whether a JSON string should be returned as a dict or list (instead of a string).

           Returns
           -------
           dict
               A dict whose keys are arguments that have outputs and whose values
               are the argument values.

           Examples
           --------
           >>> with gdal.Run("raster", "reproject", input="byte.tif", output_format="MEM", dst_crs="EPSG:4326") as alg:
           ...    print(alg.Outputs()["output"].ReadAsArray())
           [[107 123 132 ... 115 99 107]]
        """

        if not hasattr(self, "has_run"):
            raise RuntimeError("Algorithm.Run() must be called before")

        res = {}
        for name in self.GetArgNames():
            arg = self.GetArg(name)
            if arg.IsOutput():
                res[name] = self._get_arg_value(arg, parse_json)
        return res


    def __getitem__(self, key):
        """Get the value of an argument.

           Shortcut for self.GetActualAlgorithm().GetArg(key).Get()
           or self.InstantiateSubAlgorithm(key) for a non-leaf algorithm

           Parameters
           ----------
           key : str
               Name of a known argument of the algorithm
           value : any
               Value of the argument

           Examples
           --------
           >>> alg = gdal.Algorithm("raster convert")
           >>> alg["output"].GetName()
           ''
           >>> alg["output"].GetDataset()
        """

        if self.HasSubAlgorithms():
            subalg = self.InstantiateSubAlgorithm(key.replace('_', '-'))
            if not subalg:
                raise RuntimeError(f"'{key}' is not a valid sub-algorithm of '{self.GetName()}'")
            return subalg
        else:
            actual_alg = self.GetActualAlgorithm()
            arg = actual_alg.GetArg(key.replace('_', '-'))
            if not arg:
                raise RuntimeError(f"'{key}' is not a valid argument of '{actual_alg.GetName()}'")
            return arg.Get()


    def __setitem__(self, key, value):
        """Set the value of an argument.

           Shortcut for self.GetArg(key).Set(value)

           Parameters
           ----------
           key : str
               Name of a known argument of the algorithm
           value : any
               Value of the argument

           Examples
           --------
           >>> alg = gdal.Algorithm("vector clip")
           >>> alg["bbox"] = [2, 49, 3, 50]

           >>> alg = gdal.Algorithm("vector filter")
           >>> alg["where"] = "country = 'France'"

           >>> alg = gdal.Algorithm("raster reproject")
           >>> alg["input"] = "byte.tif"
           >>> alg["input"] = gdal.Open("byte.tif")
           >>> alg["target-aligned-pixels"] = True

           >>> # Multiple input datasets
           >>> alg = gdal.Algorithm("raster mosaic")
           >>> alg["input"] = ["one.tif", "two.tif"]
           >>> one_ds = gdal.Open("byte.tif")
           >>> two_ds = gdal.Open("byte.tif")
           >>> alg["input"] = [one_ds, two_ds]
        """

        arg = self.GetArgNonConst(key.replace('_', '-'))
        if not arg:
            raise RuntimeError(f"'{key}' is not a valid argument of '{self.GetName()}'")
        if not arg.Set(value):
            raise RuntimeError(f"Cannot set argument '{key}' to '{value}'")
%}
}

%pythonprepend GDALAlgorithmHS::Run %{
    self.has_run = True
%}

%pythonprepend GDALAlgorithmHS::ParseCommandLineArguments %{
    # Convert PathLike to str
    import copy
    args = copy.deepcopy(args)
    if isinstance(args[0], list):
        for i in range(len(args[0])):
            args[0][i] = str(args[0][i])

%}

%pythonprepend GDALAlgorithmHS::ParseRunAndFinalize %{
    # Convert PathLike to str
    import copy
    args = copy.deepcopy(args)
    if isinstance(args[0], list):
        for i in range(len(args[0])):
            args[0][i] = str(args[0][i])

%}

/* -------------------------------------------------------------------- */
/* GDALAlgorithmArgHS                                                   */
/* -------------------------------------------------------------------- */

%extend GDALAlgorithmArgHS {
%pythoncode %{

    def Get(self):
        """Return the argument value in its native type.

           Note: using the ``[]`` operator of Algorithm is also a convenient
           way of getting the value of an argument.

           Examples
           --------
           >>> alg = gdal.Algorithm("raster convert")
           >>> arg = alg.GetArg("output")
           >>> arg.Get()
           <osgeo.gdal.ArgDatasetValue; proxy of <Swig Object of type 'GDALArgDatasetValueHS *' at 0x...> >
        """

        type = self.GetType()
        if type == GAAT_BOOLEAN:
            return self.GetAsBoolean()
        if type == GAAT_STRING:
            return self.GetAsString()
        if type == GAAT_INTEGER:
            return self.GetAsInteger()
        if type == GAAT_REAL:
            return self.GetAsDouble()
        if type == GAAT_DATASET:
            return self.GetAsDatasetValue()
        if type == GAAT_STRING_LIST:
            return self.GetAsStringList()
        if type == GAAT_INTEGER_LIST:
            return self.GetAsIntegerList()
        if type == GAAT_REAL_LIST:
            return self.GetAsDoubleList()

        # should not happen
        raise RuntimeError("Unhandled algorithm argument data type")

    def Set(self, value):
        """Sets the value of an argument.

           Note: using the ``[]`` operator of Algorithm is also a convenient
           way of setting the value of an argument.

           Returns
           -------
           bool
               ``True`` if the argument was successfully set

           Examples
           --------
           >>> alg = gdal.Algorithm("raster info")
           >>> arg = alg.GetArg("input")
           >>> arg.Set("in.tif")
           True
        """

        arg_type = self.GetType()

        def ToInt(v):
            if int(v) == v:
                return int(v)
            raise TypeError(f"{v} is not an integer")

        if arg_type == GAAT_BOOLEAN:
            if value in (1, "1", "yes", "YES", "true", "True", "TRUE", "on", "ON"):
                return self.SetAsBoolean(True)
            elif value in (0, "0", "no", "NO", "false", "False", "FALSE", "off", "OFF"):
                return self.SetAsBoolean(False)
            else:
                return self.SetAsBoolean(value)

        if arg_type == GAAT_STRING:
            if isinstance(value, int):
                metadata_item = self.GetMetadataItem("type")
                if metadata_item and ("GDALDataType" in metadata_item) and value >= GDT_Byte and value < GDT_TypeCount:
                    return self.SetAsString(GetDataTypeName(value))
                else:
                    return self.SetAsString(str(value))
            elif isinstance(value, str) or isinstance(value, float) or isinstance(value, os.PathLike):
                return self.SetAsString(str(value))
            elif isinstance(value, osr.SpatialReference):
                return self.SetAsString(value.ExportToWkt(["FORMAT=WKT2_2019"]))
            elif isinstance(value, list) and len(value) >= 1 and (isinstance(value[0], str) or isinstance(value[0], int) or isinstance(value[0], float) or isinstance(value[0], os.PathLike)):
                if len(value) > 1:
                    raise RuntimeError("Only one value supported for an argument of type String")
                return self.Set(value[0])
            raise TypeError("Unexpected value type %s for an argument of type String" % str(type(value)))

        if arg_type == GAAT_INTEGER:
            if isinstance(value, int):
                return self.SetAsInteger(value)
            elif isinstance(value, str):
                return self.SetAsInteger(int(value))
            elif isinstance(value, float):
                return self.SetAsInteger(ToInt(value))
            elif isinstance(value, list) and len(value) >= 1 and (isinstance(value[0], str) or isinstance(value[0], int) or isinstance(value[0], float)):
                if len(value) > 1:
                    raise RuntimeError("Only one value supported for an argument of type Integer")
                return self.Set(value[0])
            raise TypeError("Unexpected value type %s for an argument of type Integer" % str(type(value)))

        if arg_type == GAAT_REAL:
            if isinstance(value, str):
                return self.SetAsDouble(float(value))
            elif isinstance(value, int) or isinstance(value, float):
                return self.SetAsDouble(value)
            elif isinstance(value, list) and len(value) >= 1 and (isinstance(value[0], str) or isinstance(value[0], int) or isinstance(value[0], float)):
                if len(value) > 1:
                        raise RuntimeError("Only one value supported for an argument of type Real")
                return self.Set(value[0])
            raise TypeError("Unexpected value type %s for an argument of type Real" % str(type(value)))

        if arg_type == GAAT_DATASET:
            if isinstance(value, str) or isinstance(value, os.PathLike):
                self.GetAsDatasetValue().SetName(str(value))
                return True
            elif isinstance(value, Dataset):
                self.GetAsDatasetValue().SetDataset(value)
                return True
            elif isinstance(value, list) and len(value) >= 1 and (isinstance(value[0], str) or isinstance(value[0], os.PathLike) or isinstance(value[0], Dataset) or isinstance(value[0], ArgDatasetValue)):
                if len(value) > 1:
                    raise RuntimeError("Only one value supported for an argument of type Dataset")
                return self.Set(value[0])
            elif isinstance(value, ArgDatasetValue):
                return self.SetAsDatasetValue(value)
            raise TypeError("Unexpected value type %s for an argument of type Dataset" % str(type(value)))

        if arg_type == GAAT_STRING_LIST:
            if isinstance(value, list):
                if self.GetName() == "gcp" and len(value) >= 1 and \
                   isinstance(value[0], list) and len(value[0]) >= 4 and \
                   (isinstance(value[0][0], int) or isinstance(value[0][0], float)):
                    return self.SetAsStringList([','.join(["%.17g" % x for x in v]) for v in value])
                elif self.GetName() == "gcp" and len(value) >= 1 and isinstance(value[0], GCP):
                    return self.SetAsStringList(["%.17g,%.17g,%.17g,%.17g,%.17g" % (gcp.GCPPixel, gcp.GCPLine, gcp.GCPX, gcp.GCPY, gcp.GCPZ) for gcp in value])
                elif self.GetName() == "kernel" and len(value) >= 1 and isinstance(value[0], list) and len(value[0]) >= 1 and (isinstance(value[0][0], int) or isinstance(value[0][0], float)):
                    return self.SetAsStringList(["[" + ",".join(["[" + ",".join([str(v) for v in row]) + "]" for row in value]) + "]"])
                elif self.GetName() == "kernel" and len(value) >= 1 and isinstance(value[0], list) and len(value[0]) >= 1 and isinstance(value[0][0], list) and len(value[0][0]) >= 1 and (isinstance(value[0][0][0], int) or isinstance(value[0][0][0], float)):
                    return self.SetAsStringList([("[" + ",".join(["[" + ",".join([str(v) for v in row]) + "]" for row in it]) + "]") for it in value])
                else:
                    return self.SetAsStringList([str(v) for v in value])
            elif isinstance(value, dict):
                return self.SetAsStringList([f"{k}={str(value[k])}" for k in value])
            else:
                return self.SetAsStringList([str(value)])

        if arg_type == GAAT_INTEGER_LIST:
            if isinstance(value, int):
                return self.SetAsIntegerList([value])
            elif isinstance(value, float):
                return self.SetAsIntegerList([ToInt(value)])
            elif isinstance(value, str):
                return self.SetAsIntegerList([int(value)])
            elif isinstance(value, list) and len(value) >= 1 and isinstance(value[0], str):
                return self.SetAsIntegerList([int(v) for v in value])
            elif isinstance(value, list) and len(value) >= 1 and isinstance(value[0], float):
                return self.SetAsIntegerList([ToInt(v) for v in value])
            else:
                return self.SetAsIntegerList(value)

        if arg_type == GAAT_REAL_LIST:
            if isinstance(value, int) or isinstance(value, float) or isinstance(value, str):
                return self.SetAsDoubleList([float(value)])
            elif isinstance(value, list) and len(value) >= 1 and isinstance(value[0], str):
                return self.SetAsDoubleList([float(v) for v in value])
            else:
                return self.SetAsDoubleList(value)

        if arg_type == GAAT_DATASET_LIST:
            if isinstance(value, list) and len(value) > 0 and (isinstance(value[0], str) or isinstance(value[0], os.PathLike)):
                return self.SetDatasetNames([str(v) for v in value])
            elif isinstance(value, list) and (len(value) == 0 or isinstance(value[0], Dataset)):
                return self.SetDatasets(value)
            elif isinstance(value, str) or isinstance(value, os.PathLike):
                return self.SetDatasetNames([str(value)])
            elif isinstance(value, Dataset):
                return self.SetDatasets([value])
            else:
                raise TypeError("Unexpected value type %s for an argument of type DatasetList" % str(type(value)))

        # should not happen
        raise RuntimeError("Unhandled algorithm argument data type")

%}
}
