/******************************************************************************
 * $Id$
 *
 * Name:     gdal_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  GDAL CSharp SWIG Interface declarations.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/
 
%include cpl_exceptions.i

%rename (GetMetadata) GetMetadata_List;
%ignore GetMetadata_Dict;

%include typemaps_csharp.i

%pragma(csharp) modulecode="public delegate int GDALProgressFuncDelegate(double Complete, IntPtr Message, IntPtr Data);"

%typemap(imtype) (GDALProgressFunc callback)  "$module.GDALProgressFuncDelegate"
%typemap(cstype) (GDALProgressFunc callback) "$module.GDALProgressFuncDelegate"
%typemap(csin) (GDALProgressFunc callback)  "$csinput"
%typemap(in) (GDALProgressFunc callback) %{ $1 = ($1_ltype)$input; %}
%typemap(imtype) (void* callback_data) "string"
%typemap(cstype) (void* callback_data) "string"
%typemap(csin) (void* callback_data) "$csinput"

%apply (void *buffer_ptr) {GDAL_GCP const *pGCPs};
%csmethodmodifiers __SetGCPs "private";
%csmethodmodifiers __GetGCPs "private";
%csmethodmodifiers GDALGCPsToGeoTransform "private";

DEFINE_EXTERNAL_CLASS(OGRLayerShadow, OSGeo.OGR.Layer)

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
  
%enddef

%typemap(cscode, noblock="1") GDALDatasetShadow {
/*! Eight bit unsigned integer */ %ds_rasterio_functions(DataType.GDT_Byte,byte)
/*! Sixteen bit signed integer */ %ds_rasterio_functions(DataType.GDT_Int16,short)
/*! Thirty two bit signed integer */ %ds_rasterio_functions(DataType.GDT_Int32,int)
/*! Thirty two bit floating point */ %ds_rasterio_functions(DataType.GDT_Float32,float)
/*! Sixty four bit floating point */ %ds_rasterio_functions(DataType.GDT_Float64,double)

public int BuildOverviews( string resampling, int[] overviewlist, $module.GDALProgressFuncDelegate callback, string callback_data) {
      int retval;
      if (overviewlist.Length <= 0)
        throw new ArgumentException("overviewlist size is small (BuildOverviews)");
        
      IntPtr ptr = Marshal.AllocHGlobal(overviewlist.Length * Marshal.SizeOf(overviewlist[0]));
      try {
          Marshal.Copy(overviewlist, 0, ptr, overviewlist.Length);
          retval = BuildOverviews(resampling, overviewlist.Length, ptr, callback, callback_data);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      GC.KeepAlive(this);
      return retval;
  }
public int BuildOverviews( string resampling, int[] overviewlist) {
      return BuildOverviews( resampling, overviewlist, null, null);
  }
  
public GCP[] GetGCPs() {
      /*hello*/
      IntPtr cPtr = __GetGCPs();
      int length = GetGCPCount();
      GCP[] ret = null;
      if (cPtr != IntPtr.Zero && length > 0)
      {
          ret = new GCP[length];
          for (int i=0; i < length; i++)
              ret[i] = __ReadCArrayItem_GDAL_GCP(cPtr, i);
      }
      GC.KeepAlive(this);
      return ret;
  }
  
public CPLErr SetGCPs(GCP[] pGCPs, string pszGCPProjection) {
     CPLErr ret = 0;
     if (pGCPs != null && pGCPs.Length > 0)
     {
         IntPtr cPtr = __AllocCArray_GDAL_GCP(pGCPs.Length);
         if (cPtr == IntPtr.Zero)
            throw new ApplicationException("Error allocating CArray with __AllocCArray_GDAL_GCP");
            
         try {
             for (int i=0; i < pGCPs.Length; i++)
                __WriteCArrayItem_GDAL_GCP(cPtr, i, pGCPs[i]);
             
             ret = __SetGCPs(pGCPs.Length, cPtr, pszGCPProjection);
         }
         finally
         {
            __FreeCArray_GDAL_GCP(cPtr);
         }
     }
     GC.KeepAlive(this);
     return ret;
  }
}

/*! Sixteen bit unsigned integer */ //%ds_rasterio_functions(DataType.GDT_UInt16,ushort)
/*! Thirty two bit unsigned integer */ //%ds_rasterio_functions(DataType.GDT_UInt32,uint)
/*! Complex Int16 */ //%ds_rasterio_functions(DataType.GDT_CInt16,int)                 
/*! Complex Int32 */ //%ds_rasterio_functions(DataType.GDT_CInt32,int)                 
/*! Complex Float32 */ //%ds_rasterio_functions(DataType.GDT_CFloat32,int)              
/*! Complex Float64 */ //%ds_rasterio_functions(DataType.GDT_CFloat64,int)

%pragma(csharp) modulecode=%{
  public static int GCPsToGeoTransform(GCP[] pGCPs, double[] argout, int bApproxOK) {
    int ret = 0;
    if (pGCPs != null && pGCPs.Length > 0)
     {
         IntPtr cPtr = __AllocCArray_GDAL_GCP(pGCPs.Length);
         if (cPtr == IntPtr.Zero)
            throw new ApplicationException("Error allocating CArray with __AllocCArray_GDAL_GCP");
            
         try {   
             for (int i=0; i < pGCPs.Length; i++)
                __WriteCArrayItem_GDAL_GCP(cPtr, i, pGCPs[i]);
             
             ret = GCPsToGeoTransform(pGCPs.Length, cPtr, argout, bApproxOK);
         }
         finally
         {
            __FreeCArray_GDAL_GCP(cPtr);
         }
     }
     return ret;
   }
%}
