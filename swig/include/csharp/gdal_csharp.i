/******************************************************************************
 *
 * Name:     gdal_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  GDAL CSharp SWIG Interface declarations.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%include cpl_exceptions.i

%rename (GetMetadata) GetMetadata_List;
%ignore GetMetadata_Dict;

/* C# string encoder is located inside OSR (See csharp_string_encoder.i) */
%include typemaps_csharp.i

DEFINE_EXTERNAL_CLASS(OSRSpatialReferenceShadow, OSGeo.OSR.SpatialReference)
DEFINE_EXTERNAL_CLASS(OGREnvelope, OSGeo.OGR.Envelope)
DEFINE_EXTERNAL_CLASS(OGRFieldDomainShadow, OSGeo.OGR.FieldDomain)
DEFINE_EXTERNAL_CLASS(GDALSubdatasetInfoShadow, OSGeo.GDAL.SubdatasetInfo)

%apply (int *pList) {int *band_list, int *panHistogram_in};
%apply (double *OUTPUT) {double *min_ret, double *max_ret};
%apply (int *nLen) {int *buckets_ret};
%apply (double *pList) {double *burn_values_list, double *fixedLevels};
%apply (void *buffer_ptr) {void *pfnTransformer, void *pTransformArg};

VALUE_LIST_INOUT(GDAL_GCP, GCP)
%apply (int nList, GDAL_GCP *pList)  { (int nGCPs, GDAL_GCP const *pGCPs) };
%apply (int* nList, GDAL_GCP **pList)  { (int *nGCPs, GDAL_GCP const **pGCPs) };

%apply (GDALProgressFunc callback) {GDALProgressFunc pfnProgress};
%apply (void *buffer_ptr) {void *pProgressData};

%typemap(cscode, noblock=1) GDALRasterIOExtraArg %{
  private $module.GDALProgressFuncDelegate pfnProgressManaged;
  public void SetProgressDelegate(Func<double, string, IntPtr, bool> progressFunc, IntPtr progressData = default(IntPtr)) {
    pProgressData = progressData;
    pfnProgress = pfnProgressManaged = progressFunc == null ? default($module.GDALProgressFuncDelegate)
      : (p, m, d) => progressFunc(p, $module.StringEncoder?.FromNullTerminated(m), d) ? 1 : 0;
  }
  public void SetProgressDelegate<TData>(Func<double, string, TData, bool> progressFunc, TData progressData) {
    pfnProgress = pfnProgressManaged = progressFunc == null ? default($module.GDALProgressFuncDelegate)
      : (p, m, _) => progressFunc(p, $module.StringEncoder?.FromNullTerminated(m), progressData) ? 1 : 0;
  }
%}
%rename (RasterIOExtraArg) GDALRasterIOExtraArg;
struct GDALRasterIOExtraArg
{
  %mutable;
    int                 nVersion;
    GDALRIOResampleAlg  eResampleAlg;
    GDALProgressFunc    pfnProgress;
    void               *pProgressData;
    int                 bFloatingPointWindowValidity;
    double              dfXOff;
    double              dfYOff;
    double              dfXSize;
    double              dfYSize;
};

DEFINE_EXTERNAL_CLASS(OGRLayerShadow, OSGeo.OGR.Layer)
DEFINE_EXTERNAL_CLASS(OGRFeatureShadow, OSGeo.OGR.Feature)


%define %rasterio_functions(GDALTYPE,CSTYPE)
  public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
    long pixelSpace = 0, long lineSpace = 0, RasterIOExtraArg extraArg = null) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = ReadRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace, extraArg);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
    long pixelSpace = 0, long lineSpace = 0, RasterIOExtraArg extraArg = null) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = WriteRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace, extraArg);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }

%enddef

%typemap(cscode, noblock="1") GDALRasterBandShadow {
/*!  8-bit unsigned integer */ %rasterio_functions(DataType.GDT_Byte,    byte)
/*! 16-bit signed integer   */ %rasterio_functions(DataType.GDT_Int16,   short)
/*! 32-bit signed integer   */ %rasterio_functions(DataType.GDT_Int32,   int)
/*! 64-bit signed integer   */ %rasterio_functions(DataType.GDT_Int64,   long)
/*! 32-bit floating point   */ %rasterio_functions(DataType.GDT_Float32, float)
/*! 64-bit floating point   */ %rasterio_functions(DataType.GDT_Float64, double)
/*! 16-bit unsigned integer */ %rasterio_functions(DataType.GDT_UInt16,  ushort)
/*! 32-bit unsigned integer */ %rasterio_functions(DataType.GDT_UInt32,  uint)
/*! 64-bit unsigned integer */ %rasterio_functions(DataType.GDT_UInt64,  ulong)
/*!  8-bit signed integer   */ %rasterio_functions(DataType.GDT_Int8,    sbyte)


/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

[Obsolete("Use Feature MaximumOfNBands(Band[] band_count) instead.")]
public static ComputedBand MaximumOfNBands(int band_count, Band[] bands) => MaximumOfNBands(bands);
[Obsolete("Use Feature MeanOfNBands(Band[] band_count) instead.")]
public static ComputedBand MeanOfNBands(int band_count, Band[] bands) => MeanOfNBands(bands);
[Obsolete("Use Feature MinimumOfNBands(Band[] band_count) instead.")]
public static ComputedBand MinimumOfNBands(int band_count, Band[] bands) => MinimumOfNBands(bands);
}

/*! Sixteen bit unsigned integer */ //%rasterio_functions(DataType.GDT_UInt16,ushort)
/*! Thirty two bit unsigned integer */ //%rasterio_functions(DataType.GDT_UInt32,uint)
/*! Complex Int16 */ //%rasterio_functions(DataType.GDT_CInt16,int)
/*! Complex Int32 */ //%rasterio_functions(DataType.GDT_CInt32,int)
/*! Complex Float32 */ //%rasterio_functions(DataType.GDT_CFloat32,int)
/*! Complex Float64 */ //%rasterio_functions(DataType.GDT_CFloat64,int)

%define %ds_rasterio_functions(GDALTYPE,CSTYPE)
  public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
     int bandCount, int[] bandMap = null, long pixelSpace = 0, long lineSpace = 0, long bandSpace = 0, RasterIOExtraArg extraArg = null) {
      if (bandMap != null && bandMap.Length < bandCount)
        throw new ArgumentException("Array bandMap must be at least bandCount elements long.");
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = ReadRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE,
                               bandCount, bandMap, pixelSpace, lineSpace, bandSpace, extraArg);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
     int bandCount, int[] bandMap = null, long pixelSpace = 0, long lineSpace = 0, long bandSpace = 0, RasterIOExtraArg extraArg = null) {
      if (bandMap != null && bandMap.Length < bandCount)
        throw new ArgumentException("Array bandMap must be at least bandCount elements long.");
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = WriteRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE,
                               bandCount, bandMap, pixelSpace, lineSpace, bandSpace, extraArg);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }

%enddef

%csmethodmodifiers GetGCPs "private";
%typemap(csimports) GDALDatasetShadow %{
  using System;
  using System.Runtime.InteropServices;
  using OSGeo.OGR;
%}
%typemap(cscode, noblock="1") GDALDatasetShadow {
/*!  8-bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_Byte,    byte)
/*! 16-bit signed integer   */ %ds_rasterio_functions(DataType.GDT_Int16,   short)
/*! 32-bit signed integer   */ %ds_rasterio_functions(DataType.GDT_Int32,   int)
/*! 64-bit signed integer   */ %ds_rasterio_functions(DataType.GDT_Int64,   long)
/*! 32-bit floating point   */ %ds_rasterio_functions(DataType.GDT_Float32, float)
/*! 64-bit floating point   */ %ds_rasterio_functions(DataType.GDT_Float64, double)
/*! 16-bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_UInt16,  ushort)
/*! 32-bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_UInt32,  uint)
/*! 64-bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_UInt64,  ulong)
/*!  8-bit signed integer   */ %ds_rasterio_functions(DataType.GDT_Int8,    sbyte)

public GCP[] GetGCPs() {
    GetGCPs(out GCP[] gcps);
    return gcps;
  }

/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

  [Obsolete("Use AdviseRead(int xoff, int yoff, int xsize, int ysize, ref int buf_xsize, ref int buf_ysize, ref int buf_type, int[] band_list, string[] options) instead.")]
  public CPLErr AdviseRead(int xoff, int yoff, int xsize, int ysize, ref int buf_xsize, ref int buf_ysize, ref int buf_type, int band_list, int[] pband_list, string[] options)
    => AdviseRead(xoff, yoff, xsize, ysize, buf_xsize, buf_ysize, (DataType)buf_type, pband_list, options);
  
  [Obsolete("Use BuildOverviews(string resampling, int[] overviewlist, Gdal.GDALProgressFuncDelegate callback, string callback_data, string[] options) instead.", error: true)]
  public int BuildOverviews(string resampling, int overviewlist, IntPtr pOverviews, Gdal.GDALProgressFuncDelegate callback, string callback_data, string[] options)
    => throw new NotSupportedException();

  [Obsolete("Use GetNextFeature(out Layer ppoBelongingLayer, out double pdfProgressPct, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public Feature GetNextFeature(ref IntPtr ppoBelongingLayer, ref double pdfProgressPct, Gdal.GDALProgressFuncDelegate callback, string callback_data) {
    Feature feature = GetNextFeature(out Layer belongingLayer, out double progressPct, callback, callback_data);
    ppoBelongingLayer = Layer.getCPtr(belongingLayer).Handle;
    pdfProgressPct = progressPct;
    return feature;
  }
}

/*! Sixteen bit unsigned integer */ //%ds_rasterio_functions(DataType.GDT_UInt16,ushort)
/*! Thirty two bit unsigned integer */ //%ds_rasterio_functions(DataType.GDT_UInt32,uint)
/*! Complex Int16 */ //%ds_rasterio_functions(DataType.GDT_CInt16,int)
/*! Complex Int32 */ //%ds_rasterio_functions(DataType.GDT_CInt32,int)
/*! Complex Float32 */ //%ds_rasterio_functions(DataType.GDT_CFloat32,int)
/*! Complex Float64 */ //%ds_rasterio_functions(DataType.GDT_CFloat64,int)

%pragma(csharp) modulecode=%{

  /*
   *  Keep this seemingly redundant overload to maintain
   * compatibility with old FileFromMemBuffer definition
   */
  public static bool FileFromMemBuffer(string utf8_string, byte[] bytes)
    => FileFromMemBuffer(utf8_string, bytes, 0, bytes.LongLength);

  private static void ValidateBufferArgs(byte[] buffer, long offset, long count) {
    if (buffer == null)
      throw new ArgumentNullException(nameof(buffer));
    else if (offset < 0)
      throw new ArgumentOutOfRangeException(nameof(offset), "Non-negative number required");
    else if (count < 0)
      throw new ArgumentOutOfRangeException(nameof(count), "Non-negative number required");
    else if (count > buffer.LongLength - offset)
      throw new ArgumentOutOfRangeException(nameof(offset), "Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.");
  }

  private static IntPtr AddOffset(IntPtr ptr, long offset)
    => IntPtr.Size == sizeof(long) ? new IntPtr(checked(ptr.ToInt64() + offset))
     : new IntPtr(checked(ptr.ToInt32() + (int)offset));

  public sealed class VsiMemoryFile : IDisposable {
    private readonly GCHandle m_dataHandle;
    private int m_disposed;
    public string Filename { get; }
    public bool VsiOwned { get; }
    public bool IsDisposed => m_disposed != 0;
    internal VsiMemoryFile(string filename, GCHandle dataHandle) {
      Filename = filename;
      m_dataHandle = dataHandle;
    }
    internal VsiMemoryFile(string filename) {
      Filename = filename;
      VsiOwned = true;
    }
    public void Dispose() {
      if (System.Threading.Interlocked.CompareExchange(ref m_disposed, 1, 0) == 0) {
        Unlink(Filename);
        if (!VsiOwned)
          m_dataHandle.Free();
      }
      GC.SuppressFinalize(this);
    }
    ~VsiMemoryFile() => Dispose();
  }

  public static VsiMemoryFile FileFromMemBuffer(string utf8_string, byte[] buffer, long offset, long count, bool vsiTakeOwnership) {
    if (vsiTakeOwnership) {
      return FileFromMemBuffer(utf8_string, buffer, offset, count) ? new VsiMemoryFile(utf8_string) : null;
    } else {
      ValidateBufferArgs(buffer, offset, count);
      GCHandle dataHandle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
        IntPtr ptr = AddOffset(dataHandle.AddrOfPinnedObject(), offset);
        IntPtr fp = VSIFileFromMemBuffer(utf8_string, ptr, (ulong)count, bTakeOwnership: 0);
        if (fp == IntPtr.Zero) {
          dataHandle.Free();
          return null;
        }
        VSIFCloseL(fp);
        return new VsiMemoryFile(utf8_string, dataHandle);
      } catch {
        dataHandle.Free();
        throw;
      }
    }
  }

  public static bool FileFromMemBuffer(string utf8_string, byte[] buffer, long offset, long count) {
    ValidateBufferArgs(buffer, offset, count);
    GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
    try {
      IntPtr addr = AddOffset(handle.AddrOfPinnedObject(), offset);
      return FileFromMemBuffer(utf8_string, count, addr) == 0;
    } finally {
      handle.Free();
    }
  }

  public static bool VSIFSeekL(IntPtr fp, long offset, System.IO.SeekOrigin origin) {
    return VSIFSeekL(fp, offset, (int)origin) == 0;
  }

  /* To maintain compatibility with old VSIFWriteL definition */
  [Obsolete("Use VSIFWriteL(byte[] buffer, int offset, int count, IntPtr fp)")]
  public static int VSIFWriteL(string data, int objectSize, int numObjects, IntPtr fp) {
    IntPtr handle = Marshal.StringToHGlobalAnsi(data);
    try {
      return VSIFWriteL(handle, (IntPtr)1, (IntPtr)data.Length, fp).ToInt32();
    } finally {
      Marshal.FreeHGlobal(handle);
    }
  }

  public static int VSIFReadL(byte[] buffer, long offset, int count, IntPtr fp) {
    ValidateBufferArgs(buffer, offset, count);
    GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
    try {
      IntPtr addr = AddOffset(handle.AddrOfPinnedObject(), offset);
      return VSIFReadL(addr, (IntPtr)sizeof(byte), (IntPtr)count, fp).ToInt32();
    } finally {
      handle.Free();
    }
  }

  public static int VSIFWriteL(byte[] buffer, long offset, int count, IntPtr fp) {
    ValidateBufferArgs(buffer, offset, count);
    GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
    try {
      IntPtr addr = AddOffset(handle.AddrOfPinnedObject(), offset);
      return VSIFWriteL(addr, (IntPtr)sizeof(byte), (IntPtr)count, fp).ToInt32();
    } finally {
      handle.Free();
    }
  }
  public static Dataset[] GetOpenDatasets() {
    wrapper_GetOpenDatasets(out Dataset[] datasets);
    return datasets;
  }
%}

%rename (GetMemFileBuffer) wrapper_VSIGetMemFileBuffer;

%typemap(cstype) (vsi_l_offset *pnDataLength) "out ulong";
%typemap(imtype) (vsi_l_offset *pnDataLength) "out ulong";
%apply (unsigned long long *OUTPUT) {(vsi_l_offset *pnDataLength)}
%typemap(cstype) (int bUnlinkAndSeize) "bool";
%typemap(csin) (int bUnlinkAndSeize) "$csinput ? 1 : 0";

%inline {
GByte* wrapper_VSIGetMemFileBuffer(const char *utf8_string, vsi_l_offset *pnDataLength, int bUnlinkAndSeize)
{
    return VSIGetMemFileBuffer(utf8_string, pnDataLength, bUnlinkAndSeize);
}
}

%clear (vsi_l_offset *pnDataLength);

/* expose exception message setters for testing */
%csmethodmodifiers TestSwigSetException "private";
%csmethodmodifiers TestSwigSetArgumentException "private";
%inline %{
  void TestSwigSetException(int code, const char *message) {
    SWIG_CSharpSetPendingException(static_cast<SWIG_CSharpExceptionCodes>(code), message);
  }
  void TestSwigSetArgumentException(int code, const char *message, const char *param_name) {
    SWIG_CSharpSetPendingExceptionArgument(static_cast<SWIG_CSharpExceptionArgumentCodes>(code), message, param_name);
  }
%}


%typemap(cscode, noblock="1") GDALGroupHS {
  [Obsolete("Use CreateAttribute(string name, ulong[] dimensions, ExtendedDataType data_type, string[] options) instead.", error: true)]
  public Attribute CreateAttribute(string name, int dimensions, uint[] sizes, ExtendedDataType data_type, string[] options)
    => throw new NotSupportedException();
}

%typemap(cscode, noblock="1") GDALAlgorithmArgHS {
  [Obsolete("Use SetAsIntegerList(int[] nList) instead.")]
  public bool SetAsIntegerList(int nList, int[] pList) => SetAsIntegerList(pList);
  [Obsolete("Use SetAsDoubleList(double[] nList) instead.")]
  public bool SetAsDoubleList(int nList, double[] pList) => SetAsDoubleList(pList);
}

%typemap(cscode, noblock="1") GDALTransformerInfoShadow {
  [Obsolete("Use TransformPoints(int bDstToSrc, int nCount, double[] x, double[] y, double[] z, int[] panSuccess) instead.", error: true)]
  public int TransformPoints(int bDstToSrc, int nCount, double[] x, double[] y, double[] z, double[] panSuccess)
    => throw new NotSupportedException();
}

%typemap(cscode, noblock="1") GDALMDArrayHS {
  [Obsolete("Use CreateAttribute(string name, ulong[] dimensions, ExtendedDataType data_type, string[] options) instead.", error: true)]
  public Attribute CreateAttribute(string name, int dimensions, uint[] sizes, ExtendedDataType data_type, string[] options)
    => throw new NotSupportedException();

  [Obsolete("Use Resize(ulong[] newDimensions, string[] options) instead.", error: true)]
  public CPLErr Resize(int newDimensions, uint[] newSizes, string[] options) => throw new NotSupportedException();

  [Obsolete("Use Transpose(int[] axisMap) instead.")]
  public MDArray Transpose(int axisMap, int[] mapInts) => Transpose(mapInts);
}


%typemap(cscode, noblock="1") GDALRATDateTime {

  public static implicit operator RATDateTime(DateTimeOffset value) => FromDateTimeOffset(value);
  public static implicit operator DateTimeOffset(RATDateTime value) => value.ToDateTimeOffset();
  public DateTimeOffset ToDateTimeOffset() {
    var sec = (int)fSecond;
    var ms = (int)Math.Round((fSecond - sec) * 1000);
	var offsetMins = nTimeZoneHour * 60 + nTimeZoneMinute;
	if (!bPositiveTimeZone)
	  offsetMins *= -1;
    var offset = TimeSpan.FromMinutes(offsetMins);
    return new DateTimeOffset(nYear, nMonth, nDay, nHour, nMinute, sec, ms, offset);
  }
  public static RATDateTime FromDateTimeOffset(DateTimeOffset value) => new RATDateTime(
    value.Year, value.Month, value.Day,
    value.Hour, value.Minute, value.Second + value.Millisecond / 1000f,
    Math.Abs(value.Offset.Hours), Math.Abs(value.Offset.Minutes), value.Offset.Ticks >= 0
  );
}

/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

%pragma(csharp) modulecode=%{
  [Obsolete("Use GCPsToHomography(GCP[] nGCPs, double[] argout) instead.", error: true)]
  public static int GCPsToHomography(int nGCPs, IntPtr pGCPs, double[] argout)
    => throw new NotSupportedException();

  [Obsolete("Use RasterizeLayer(Dataset dataset, int[] bands, Layer layer, IntPtr pfnTransformer, IntPtr pTransformArg, double[] burn_values, string[] options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int RasterizeLayer(Dataset dataset, int bands, int[] band_list, OSGeo.OGR.Layer layer, IntPtr pfnTransformer, IntPtr pTransformArg, int burn_values, double[] burn_values_list, string[] options, GDALProgressFuncDelegate callback, string callback_data)
    => RasterizeLayer(dataset, band_list, layer, pfnTransformer, pTransformArg, burn_values_list, options, callback, callback_data);

  [Obsolete("Use RegenerateOverviews(Band srcBand, Band[] overviewBandCount, string resampling, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int RegenerateOverviews(Band srcBand, int overviewBandCount, Band[] overviewBands, string resampling, GDALProgressFuncDelegate callback, string callback_data)
    => RegenerateOverviews(srcBand, overviewBands, resampling, callback, callback_data);

  [Obsolete("Use ContourGenerate(Band srcBand, double contourInterval, double contourBase, double[] fixedLevelCount, bool useNoData, double noDataValue, Layer dstLayer, int idField, int elevField, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int ContourGenerate(Band srcBand, double contourInterval, double contourBase, int fixedLevelCount, double[] fixedLevels, int useNoData, double noDataValue, OSGeo.OGR.Layer dstLayer, int idField, int elevField, GDALProgressFuncDelegate callback, string callback_data)
    => ContourGenerate(srcBand, contourInterval, contourBase, fixedLevels, useNoData != 0, noDataValue, dstLayer, idField, elevField, callback, callback_data);

  [Obsolete("Use CreatePansharpenedVRT(string pszXML, Band panchroBand, Band[] nInputSpectralBands) instead.")]
  public static Dataset CreatePansharpenedVRT(string pszXML, Band panchroBand, int nInputSpectralBands, Band[] ahInputSpectralBands)
    => CreatePansharpenedVRT(pszXML, panchroBand, ahInputSpectralBands);

  [Obsolete("Use BuildVRT(string dest, Dataset[] object_list_count, GDALBuildVRTOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.", error: true)]
  public static Dataset wrapper_GDALBuildVRT_objects(string dest, int object_list_count, IntPtr poObjects, GDALBuildVRTOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => throw new NotSupportedException();

  [Obsolete("Use MultiDimTranslate(string dest, Dataset[] object_list_count, GDALMultiDimTranslateOptions multiDimTranslateOptions, GDALProgressFuncDelegate callback, string callback_data) instead.", error: true)]
  public static Dataset wrapper_GDALMultiDimTranslateDestName(string dest, int object_list_count, IntPtr poObjects, GDALMultiDimTranslateOptions multiDimTranslateOptions, GDALProgressFuncDelegate callback, string callback_data)
    => throw new NotSupportedException();

  [Obsolete("Use Contour(Dataset dstDS, Dataset srcDS, GDALContourOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int wrapper_GDALContourDestDS(Dataset dstDS, Dataset srcDS, GDALContourOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Contour(dstDS, srcDS, options, callback, callback_data);

  [Obsolete("Use Contour(string dest, Dataset srcDS, GDALContourOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALContourDestName(string dest, Dataset srcDS, GDALContourOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Contour(dest, srcDS, options, callback, callback_data);

  [Obsolete("Use DEMProcessing(string dest, Dataset dataset, string pszProcessing, string pszColorFilename, GDALDEMProcessingOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALDEMProcessing(string dest, Dataset dataset, string pszProcessing, string pszColorFilename, GDALDEMProcessingOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => DEMProcessing(dest, dataset, pszProcessing, pszColorFilename, options, callback, callback_data);

  [Obsolete("Use Footprint(Dataset dstDS, Dataset srcDS, GDALFootprintOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int wrapper_GDALFootprintDestDS(Dataset dstDS, Dataset srcDS, GDALFootprintOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Footprint(dstDS, srcDS, options, callback, callback_data);

  [Obsolete("Use Footprint(string dest, Dataset srcDS, GDALFootprintOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALFootprintDestName(string dest, Dataset srcDS, GDALFootprintOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Footprint(dest, srcDS, options, callback, callback_data);

  [Obsolete("Use Grid(string dest, Dataset dataset, GDALGridOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALGrid(string dest, Dataset dataset, GDALGridOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Grid(dest, dataset, options, callback, callback_data);

  [Obsolete("Use Nearblack(Dataset dstDS, Dataset srcDS, GDALNearblackOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int wrapper_GDALNearblackDestDS(Dataset dstDS, Dataset srcDS, GDALNearblackOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Nearblack(dstDS, srcDS, options, callback, callback_data);

  [Obsolete("Use Nearblack(string dest, Dataset srcDS, GDALNearblackOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALNearblackDestName(string dest, Dataset srcDS, GDALNearblackOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Nearblack(dest, srcDS, options, callback, callback_data);

  [Obsolete("Use Rasterize(Dataset dstDS, Dataset srcDS, GDALRasterizeOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int wrapper_GDALRasterizeDestDS(Dataset dstDS, Dataset srcDS, GDALRasterizeOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Rasterize(dstDS, srcDS, options, callback, callback_data);

  [Obsolete("Use Rasterize(string dest, Dataset srcDS, GDALRasterizeOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALRasterizeDestName(string dest, Dataset srcDS, GDALRasterizeOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => Rasterize(dest, srcDS, options, callback, callback_data);

  [Obsolete("Use Translate(string dest, Dataset dataset, GDALTranslateOptions translateOptions, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALTranslate(string dest, Dataset dataset, GDALTranslateOptions translateOptions, GDALProgressFuncDelegate callback, string callback_data)
    => Translate(dest, dataset, translateOptions, callback, callback_data);

  [Obsolete("Use VectorTranslate(Dataset dstDS, Dataset srcDS, GDALVectorTranslateOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static int wrapper_GDALVectorTranslateDestDS(Dataset dstDS, Dataset srcDS, GDALVectorTranslateOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => VectorTranslate(dstDS, srcDS, options, callback, callback_data);

  [Obsolete("Use VectorTranslate(string dest, Dataset srcDS, GDALVectorTranslateOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_GDALVectorTranslateDestName(string dest, Dataset srcDS, GDALVectorTranslateOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => VectorTranslate(dest, srcDS, options, callback, callback_data);

  [Obsolete("Use TileIndex(string dest, string[] source_filenames, GDALTileIndexOptions options, GDALProgressFuncDelegate callback, string callback_data) instead.")]
  public static Dataset wrapper_TileIndex_names(string dest, string[] source_filenames, GDALTileIndexOptions options, GDALProgressFuncDelegate callback, string callback_data)
    => TileIndex(dest, source_filenames, options, callback, callback_data);

  [Obsolete("Use the GCP.GCPX property instead.")]
  public static double GDAL_GCP_GCPX_get(GCP gcp) => gcp.GCPX;
  [Obsolete("Use the GCP.GCPX property instead.")]
  public static void GDAL_GCP_GCPX_set(GCP gcp, double dfGCPX) => gcp.GCPX = dfGCPX;
  [Obsolete("Use the GCP.GCPY property instead.")]
  public static double GDAL_GCP_GCPY_get(GCP gcp) => gcp.GCPY;
  [Obsolete("Use the GCP.GCPY property instead.")]
  public static void GDAL_GCP_GCPY_set(GCP gcp, double dfGCPY) => gcp.GCPY = dfGCPY;
  [Obsolete("Use the GCP.GCPZ property instead.")]
  public static double GDAL_GCP_GCPZ_get(GCP gcp) => gcp.GCPZ;
  [Obsolete("Use the GCP.GCPZ property instead.")]
  public static void GDAL_GCP_GCPZ_set(GCP gcp, double dfGCPZ) => gcp.GCPZ = dfGCPZ;
  [Obsolete("Use the GCP.GCPPixel property instead.")]
  public static double GDAL_GCP_GCPPixel_get(GCP gcp) => gcp.GCPPixel;
  [Obsolete("Use the GCP.GCPPixel property instead.")]
  public static void GDAL_GCP_GCPPixel_set(GCP gcp, double dfGCPPixel) => gcp.GCPPixel = dfGCPPixel;
  [Obsolete("Use the GCP.GCPLine property instead.")]
  public static double GDAL_GCP_GCPLine_get(GCP gcp) => gcp.GCPLine;
  [Obsolete("Use the GCP.GCPLine property instead.")]
  public static void GDAL_GCP_GCPLine_set(GCP gcp, double dfGCPLine) => gcp.GCPLine = dfGCPLine;
  [Obsolete("Use the GCP.Info property instead.")]
  public static string GDAL_GCP_Info_get(GCP gcp) => gcp.Info;
  [Obsolete("Use the GCP.Info property instead.")]
  public static void GDAL_GCP_Info_set(GCP gcp, string pszInfo) => gcp.Info = pszInfo;
  [Obsolete("Use the GCP.Id property instead.")]
  public static string GDAL_GCP_Id_get(GCP gcp) => gcp.Id;
  [Obsolete("Use the GCP.Id property instead.")]
  public static void GDAL_GCP_Id_set(GCP gcp, string pszId) => gcp.Id = pszId;
  [Obsolete("Use the GCP.GCPX property instead.")]
  public static double GDAL_GCP_get_GCPX(GCP gcp) => gcp.GCPX;
  [Obsolete("Use the GCP.GCPX property instead.")]
  public static void GDAL_GCP_set_GCPX(GCP gcp, double dfGCPX) => gcp.GCPX = dfGCPX;
  [Obsolete("Use the GCP.GCPY property instead.")]
  public static double GDAL_GCP_get_GCPY(GCP gcp) => gcp.GCPY;
  [Obsolete("Use the GCP.GCPY property instead.")]
  public static void GDAL_GCP_set_GCPY(GCP gcp, double dfGCPY) => gcp.GCPY = dfGCPY;
  [Obsolete("Use the GCP.GCPZ property instead.")]
  public static double GDAL_GCP_get_GCPZ(GCP gcp) => gcp.GCPZ;
  [Obsolete("Use the GCP.GCPZ property instead.")]
  public static void GDAL_GCP_set_GCPZ(GCP gcp, double dfGCPZ) => gcp.GCPZ = dfGCPZ;
  [Obsolete("Use the GCP.GCPPixel property instead.")]
  public static double GDAL_GCP_get_GCPPixel(GCP gcp) => gcp.GCPPixel;
  [Obsolete("Use the GCP.GCPPixel property instead.")]
  public static void GDAL_GCP_set_GCPPixel(GCP gcp, double dfGCPPixel) => gcp.GCPPixel = dfGCPPixel;
  [Obsolete("Use the GCP.GCPLine property instead.")]
  public static double GDAL_GCP_get_GCPLine(GCP gcp) => gcp.GCPLine;
  [Obsolete("Use the GCP.GCPLine property instead.")]
  public static void GDAL_GCP_set_GCPLine(GCP gcp, double dfGCPLine) => gcp.GCPLine = dfGCPLine;
  [Obsolete("Use the GCP.Info property instead.")]
  public static string GDAL_GCP_get_Info(GCP gcp) => gcp.Info;
  [Obsolete("Use the GCP.Info property instead.")]
  public static void GDAL_GCP_set_Info(GCP gcp, string pszInfo) => gcp.Info = pszInfo;
  [Obsolete("Use the GCP.Id property instead.")]
  public static string GDAL_GCP_get_Id(GCP gcp) => gcp.Id;
  [Obsolete("Use the GCP.Id property instead.")]
  public static void GDAL_GCP_set_Id(GCP gcp, string pszId) => gcp.Id = pszId;
%}

/*
 * C# extension method class
 */
%{
typedef struct {} GdalExtensions;
%}
%typemap(csclassmodifiers) GdalExtensions "public static class";
%typemap(csinterfaces)     GdalExtensions "";
%typemap(csdisposing)      GdalExtensions "";
%typemap(csdispose)        GdalExtensions "";
%typemap(csbody)           GdalExtensions %{
  public static Dataset GetDataset(this OGR.Layer layer) => Gdal.GetDatasetFromLayer(layer);
%}

%ignore GdalExtensions::GdalExtensions();
struct  GdalExtensions{};
