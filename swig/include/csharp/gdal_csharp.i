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
%apply (int **pList) {int **ppanHistogram};
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
 public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = ReadRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = WriteRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE, pixelSpace, lineSpace);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace, RasterIOExtraArg extraArg) {
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
  public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize, int pixelSpace, int lineSpace, RasterIOExtraArg extraArg) {
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
/*! Eight bit unsigned integer */ %rasterio_functions(DataType.GDT_Byte,byte)
/*! Sixteen bit signed integer */ %rasterio_functions(DataType.GDT_Int16,short)
/*! Thirty two bit signed integer */ %rasterio_functions(DataType.GDT_Int32,int)
/*! Thirty two bit floating point */ %rasterio_functions(DataType.GDT_Float32,float)
/*! Sixty four bit floating point */ %rasterio_functions(DataType.GDT_Float64,double)
}

/*! Sixteen bit unsigned integer */ //%rasterio_functions(DataType.GDT_UInt16,ushort)
/*! Thirty two bit unsigned integer */ //%rasterio_functions(DataType.GDT_UInt32,uint)
/*! Complex Int16 */ //%rasterio_functions(DataType.GDT_CInt16,int)
/*! Complex Int32 */ //%rasterio_functions(DataType.GDT_CInt32,int)
/*! Complex Float32 */ //%rasterio_functions(DataType.GDT_CFloat32,int)
/*! Complex Float64 */ //%rasterio_functions(DataType.GDT_CFloat64,int)

%define %ds_rasterio_functions(GDALTYPE,CSTYPE)
 public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
     int bandCount, int[] bandMap, int pixelSpace, int lineSpace, int bandSpace) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = ReadRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE,
                               bandCount, bandMap, pixelSpace, lineSpace, bandSpace);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr WriteRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
     int bandCount, int[] bandMap, int pixelSpace, int lineSpace, int bandSpace) {
      CPLErr retval;
      GCHandle handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
      try {
          retval = WriteRaster(xOff, yOff, xSize, ySize, handle.AddrOfPinnedObject(), buf_xSize, buf_ySize, GDALTYPE,
                               bandCount, bandMap, pixelSpace, lineSpace, bandSpace);
      } finally {
          handle.Free();
      }
      GC.KeepAlive(this);
      return retval;
  }
  public CPLErr ReadRaster(int xOff, int yOff, int xSize, int ySize, CSTYPE[] buffer, int buf_xSize, int buf_ySize,
     int bandCount, int[] bandMap, int pixelSpace, int lineSpace, int bandSpace, RasterIOExtraArg extraArg) {
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
     int bandCount, int[] bandMap, int pixelSpace, int lineSpace, int bandSpace, RasterIOExtraArg extraArg) {
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
/*! Eight bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_Byte,byte)
/*! Sixteen bit signed integer */ %ds_rasterio_functions(DataType.GDT_Int16,short)
/*! Thirty two bit signed integer */ %ds_rasterio_functions(DataType.GDT_Int32,int)
/*! Thirty two bit floating point */ %ds_rasterio_functions(DataType.GDT_Float32,float)
/*! Sixty four bit floating point */ %ds_rasterio_functions(DataType.GDT_Float64,double)

public int BuildOverviews( string resampling, int[] overviewlist, $module.GDALProgressFuncDelegate callback, string callback_data) {
      return BuildOverviews( resampling, overviewlist, callback, callback_data, null);
  }

public int BuildOverviews( string resampling, int[] overviewlist) {
      return BuildOverviews( resampling, overviewlist, null, null);
  }

public GCP[] GetGCPs() {
    GetGCPs(out GCP[] gcps);
    return gcps;
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
