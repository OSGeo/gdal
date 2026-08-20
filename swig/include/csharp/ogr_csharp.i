/******************************************************************************
 *
 * Name:     ogr_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  OGR CSharp SWIG Interface declarations.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%include cpl_exceptions.i

%rename (GetFieldType) GetType;
%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

/* C# string encoder is located inside OSR (See csharp_string_encoder.i) */
%include typemaps_csharp.i

DEFINE_EXTERNAL_CLASS(OSRSpatialReferenceShadow, OSGeo.OSR.SpatialReference)
DEFINE_EXTERNAL_CLASS(OSRCoordinateTransformationShadow, OSGeo.OSR.CoordinateTransformation)
DEFINE_EXTERNAL_CLASS(GDALMajorObjectShadow, OSGeo.GDAL.MajorObject)


%typemap(cscode, noblock="1") OGRGeometryShadow {
  public int ExportToWkb( byte[] buffer, wkbByteOrder byte_order ) {
      int retval;
      long size = WkbSize();
      if (size > Int32.MaxValue)
        throw new ArgumentException("Too big geometry (ExportToWkb)");
      if (buffer.Length < size)
        throw new ArgumentException("Buffer size is small (ExportToWkb)");

      IntPtr ptr = Marshal.AllocHGlobal((int)size * Marshal.SizeOf(buffer[0]));
      try {
          retval = ExportToWkb((int)size, ptr, byte_order);
          Marshal.Copy(ptr, buffer, 0, (int)size);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      GC.KeepAlive(this);
      return retval;
  }
  public int ExportToWkb( byte[] buffer ) {
      return ExportToWkb( buffer, wkbByteOrder.wkbXDR);
  }

  public static $csclassname CreateFromWkb(byte[] wkb){
     if (wkb.Length == 0)
        throw new ArgumentException("Buffer size is small (CreateFromWkb)");
     $csclassname retval;
     IntPtr ptr = Marshal.AllocHGlobal(wkb.Length * Marshal.SizeOf(wkb[0]));
     try {
         Marshal.Copy(wkb, 0, ptr, wkb.Length);
         retval =  new $csclassname(wkbGeometryType.wkbUnknown, null, wkb.Length, ptr, null);
      } finally {
          Marshal.FreeHGlobal(ptr);
      }
      return retval;
  }

  public static $csclassname CreateFromWkt(string wkt){
     return new $csclassname(wkbGeometryType.wkbUnknown, wkt, 0, IntPtr.Zero, null);
  }

  public static $csclassname CreateFromGML(string gml){
     return new $csclassname(wkbGeometryType.wkbUnknown, null, 0, IntPtr.Zero, gml);
  }

  public Geometry(wkbGeometryType type) : this(OgrPINVOKE.new_Geometry((int)type, null, 0, IntPtr.Zero, null), true, null) {
    if (OgrPINVOKE.SWIGPendingException.Pending) throw OgrPINVOKE.SWIGPendingException.Retrieve();
  }
}

/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

%typemap(cscode, noblock="1") OGRFeatureShadow {
  [Obsolete("Use SetFieldDoubleList(int id, double[] nList) instead.")]
  public void SetFieldDoubleList(int id, int nList, double[] pList)
    => SetFieldDoubleList(id, pList);
  [Obsolete("Use SetFieldIntegerList(int id, int[] nList) instead.")]
  public void SetFieldIntegerList(int id, int nList, int[] pList)
    => SetFieldIntegerList(id, pList);
  [Obsolete("Use SetFromWithMap(Feature other, int forgiving, int[] nList) instead.")]
  public int SetFromWithMap(Feature other, int forgiving, int nList, int[] pList)
    => SetFromWithMap(other, forgiving, pList);
}

%typemap(cscode, noblock="1") OGRLayerShadow {
  [Obsolete("Use ReorderFields(int[] nList) instead.")]
  public int ReorderFields(int nList, int[] pList)
    => ReorderFields(pList);
  [Obsolete("Use UpdateFeature(Feature feature, int[] nUpdatedFieldsCount, int[] nUpdatedGeomFieldsCount, bool bUpdateStyleString) instead.")]
  public int UpdateFeature(Feature feature, int nUpdatedFieldsCount, int[] panUpdatedFieldsIdx, int nUpdatedGeomFieldsCount, int[] panUpdatedGeomFieldsIdx, bool bUpdateStyleString)
    => UpdateFeature(feature, panUpdatedFieldsIdx, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}
