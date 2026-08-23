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
  public byte[] ExportToWkb(wkbByteOrder byte_order = wkbByteOrder.wkbXDR) {
    byte[] buffer = new byte[WkbSize()];
	ExportToWkb(buffer, byte_order);
	return buffer;
  }

  public static $csclassname CreateFromWkb(byte[] wkb){
    return wkb.Length > 0 ? new Geometry(wkbGeometryType.wkbUnknown, null, wkb, null)
           : throw new ArgumentException("Buffer size is small (CreateFromWkb)");
  }

  public static $csclassname CreateFromWkt(string wkt){
     return new $csclassname(wkbGeometryType.wkbUnknown, wkt, null, null);
  }

  public static $csclassname CreateFromGML(string gml){
     return new $csclassname(wkbGeometryType.wkbUnknown, null, null, gml);
  }

/*
 * Overload to maintain backwards compatibility with multi-argument typemaps
 */
 
  [Obsolete("Use ExportToWkb(byte[] buffer, wkbByteOrder byte_order) instead.", error: true)]
  public int ExportToWkb(int bufLen, IntPtr buffer, wkbByteOrder byte_order)
    => throw new NotSupportedException();
	
  [Obsolete("Use $csclassname(wkbGeometryType type, string wkt, byte[] wkb, string gml) instead.")]
  public $csclassname(wkbGeometryType type, string wkt, int wkb, IntPtr wkb_buf, string gml)
    : this(type, wkt, PtrToByteArray(wkb, wkb_buf), gml) { }  

  private static byte[] PtrToByteArray(int size, IntPtr ptr) {
    if (size <= 0 || ptr == IntPtr.Zero) return null;
    byte[] ret = new byte[size];
    Marshal.Copy(ptr, ret, 0, size);
    return ret;
  }
}

%typemap(cscode, noblock="1") OGRFeatureShadow {

  public DateTime GetFieldAsDateTime(int id) {
    GetFieldAsDateTime(id, out int year, out int month, out int day, out int hour, out int minute, out float second, out int tz);
    return DateTimeFromParts(year, month, day, hour, minute, second, tz);
  }
  public DateTime GetFieldAsDateTime(string field_name) {
    GetFieldAsDateTime(field_name, out int year, out int month, out int day, out int hour, out int minute, out float second, out int tz);
    return DateTimeFromParts(year, month, day, hour, minute, second, tz);
  }
  public void SetField(int id, DateTime dateTime) {
    DateTimeToParts(dateTime, out int year, out int month, out int day, out int hour, out int minute, out float second, out int tz);
    SetField(id, year, month, day, hour, minute, second, tz);
  }   
  public void SetField(string field_name, DateTime dateTime) {
    DateTimeToParts(dateTime, out int year, out int month, out int day, out int hour, out int minute, out float second, out int tz);
    SetField(field_name, year, month, day, hour, minute, second, tz);
  }

  private static DateTime DateTimeFromParts(int year, int month, int day, int hour, int minute, float second, int tz) {
    var sec = (int)second;
    var ms = (int)Math.Round((second - sec) * 1000);
    DateTimeKind kind = tz == 100 ? DateTimeKind.Utc : tz == 1 ? DateTimeKind.Local : DateTimeKind.Unspecified;
    return new DateTime(year, month, day, hour, minute, sec, ms, kind);
  }
  private static void DateTimeToParts(DateTime dateTime, out int year, out int month, out int day, out int hour, out int minute, out float second, out int tz) {
    year = dateTime.Year;
    month = dateTime.Month;
    day = dateTime.Day;
    hour = dateTime.Hour;
    minute = dateTime.Minute;
    second = dateTime.Second + dateTime.Millisecond / 1000f;
    tz = dateTime.Kind == DateTimeKind.Utc ? 100 : dateTime.Kind == DateTimeKind.Local ? 1 : 0;
  }

/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */
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

%pragma(csharp) modulecode=%{
  [Obsolete("Use CreateGeometryFromWkb(byte[] len, SpatialReference reference) instead.", error: true)]
  public static Geometry CreateGeometryFromWkb(uint len, IntPtr bin_string, OSR.SpatialReference reference)
    => throw new NotSupportedException();
%}
