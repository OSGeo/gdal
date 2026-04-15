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
