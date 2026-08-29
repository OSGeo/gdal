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
  public byte[] ExportToWkb(wkbByteOrder byte_order = wkbByteOrder.wkbNDR) {
    if (WkbLongSize > int.MaxValue)
	  throw new InvalidOperationException("The geometry is too larget to fit in a .NET array.");
    var buffer = new byte[WkbLongSize];
	if (ExportToWkb(buffer, byte_order) != 0)
	  throw new ApplicationException("ExportToWkb return code did not indicate success.");
	return buffer;
  }
  public int ExportToWkb(byte[] buffer, wkbByteOrder byte_order = wkbByteOrder.wkbNDR) {
	var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
	try {
	  return ExportToWkb(new IntPtr(buffer.Length), handle.AddrOfPinnedObject(), byte_order);
	} finally {
	  handle.Free();
	}
  }
  public byte[] ExportToIsoWkb(wkbByteOrder byte_order = wkbByteOrder.wkbNDR) {
    if (WkbLongSize > int.MaxValue)
	  throw new InvalidOperationException("The geometry is too larget to fit in a .NET array.");
    var buffer = new byte[WkbLongSize];
	if (ExportToIsoWkb(buffer, byte_order) != 0)
	  throw new ApplicationException("ExportToIsoWkb return code did not indicate success.");
	return buffer;
  }
  public int ExportToIsoWkb(byte[] buffer, wkbByteOrder byte_order = wkbByteOrder.wkbNDR) {
	var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
	try {
	  return ExportToIsoWkb(new IntPtr(buffer.Length), handle.AddrOfPinnedObject(), byte_order);
	} finally {
	  handle.Free();
	}
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
 
  [Obsolete("Use ExportToWkb(IntPtr length, IntPtr buffer_ptr, wkbByteOrder byte_order) instead.")]
  public int ExportToWkb(int bufLen, IntPtr buffer, wkbByteOrder byte_order)
    => ExportToWkb(new IntPtr(bufLen), buffer, byte_order);
	
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
  public static Geometry CreateGeometryFromEnvelope(Envelope envelope, OSR.SpatialReference reference = null)
    => CreateGeometryFromEnvelope(envelope.MinX, envelope.MinY, envelope.MaxX, envelope.MaxY, reference);

  public static Geometry CreateGeometryFromWkb(byte[] buffer, OSR.SpatialReference reference = null) {
    var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
	try {
	  return CreateGeometryFromWkb(new IntPtr(buffer.Length), handle.AddrOfPinnedObject(), reference);
	} finally {
	  handle.Free();
	}
  }
  
  [Obsolete("Use CreateGeometryFromWkb(IntPtr len, IntPtr bin_string, SpatialReference reference) instead.")]
  public static Geometry CreateGeometryFromWkb(uint len, IntPtr bin_string, OSR.SpatialReference reference)
    => CreateGeometryFromWkb(new IntPtr(len), bin_string, reference);
%}

/*****************************************************************************
 * Enable C# default arguments all OGR methods                               *
 * Apply fixes to specific methods to translate C++ default values to C#     *
 ****************************************************************************/

#if SWIG_VERSION >= 0x040200 && !defined(FROM_GDAL_I)
%feature("cs:defaultargs");

%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Intersection;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Union;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::SymDifference;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Identity;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Update;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Clip;
%feature("cs:defaultargs", options="null", callback="null", callback_data="null") OGRLayerShadow::Erase;

%feature("cs:defaultargs", options="null") OGRFeatureShadow::GetFieldAsISO8601DateTime;
%feature("cs:defaultargs", options="null") OGRFeatureShadow::GetFieldAsISO8601DateTime;
%feature("cs:defaultargs", options="null") OGRFeatureShadow::DumpReadableAsString;
%feature("cs:defaultargs", flags=0x7FFFFFEF, bEmitError=1) OGRFeatureShadow::Validate; //0x7FFFFFEF = OGR_F_VAL_ALL
%feature("cs:defaultargs", bNotNullableOnly=0, options="null") OGRFeatureShadow::FillUnsetWithDefault;

%feature("cs:defaultargs", type="wkbGeometryType.wkbUnknown", wkt="null", wkb="null", gml="null") OGRGeometryShadow::OGRGeometryShadow;
%feature("cs:defaultargs", byte_order="wkbByteOrder.wkbNDR") OGRGeometryShadow::ExportToWkb;
%feature("cs:defaultargs", byte_order="wkbByteOrder.wkbNDR") OGRGeometryShadow::ExportToIsoWkb;
%feature("cs:defaultargs", altitude_mode="null") OGRGeometryShadow::ExportToKML;
%feature("cs:defaultargs", options="null") OGRGeometryShadow::ExportToJson;
%feature("cs:defaultargs", options="null") OGRGeometryShadow::MakeValid;
%feature("cs:defaultargs", options="null") OGRGeometryShadow::GetLinearGeometry;
%feature("cs:defaultargs", options="null") OGRGeometryShadow::GetCurveGeometry;
%feature("cs:defaultargs", argout="null") OGRGeometryShadow::GetPoint;
%feature("cs:defaultargs", argout="null") OGRGeometryShadow::GetPointZM;
%feature("cs:defaultargs", argout="null") OGRGeometryShadow::GetPoint_2D;
%feature("cs:defaultargs", bOnlyEdges=0) OGRGeometryShadow::DelaunayTriangulation;
%feature("cs:defaultargs", bLookForCircular=0) OGRGeometryShadow::HasCurveGeometry;

%feature("cs:defaultargs", field_type="FieldType.OFTString") OGRFieldDefnShadow::OGRFieldDefnShadow;

%feature("cs:defaultargs", srs="null", geom_type="wkbGeometryType.wkbUnknown", options="null") OGRDataSourceShadow::CreateLayer;
%feature("cs:defaultargs", options="null") OGRDataSourceShadow::CopyLayer;
%feature("cs:defaultargs", spatialFilter="null") OGRDataSourceShadow::ExecuteSQL;
%feature("cs:defaultargs", force=0) OGRDataSourceShadow::StartTransaction;

%feature("cs:defaultargs", options="null") OGRDriverShadow::CreateDataSource;
%feature("cs:defaultargs", options="null") OGRDriverShadow::CopyDataSource;

%feature("cs:defaultargs", field_type="wkbGeometryType.wkbUnknown") OGRGeomFieldDefnShadow::OGRGeomFieldDefnShadow;

%feature("cs:defaultargs", name_null_ok="null") OGRFeatureDefnShadow::OGRFeatureDefnShadow;

%feature("cs:defaultargs", options="null") OGRGeomTransformerShadow::OGRGeomTransformerShadow;

%feature("cs:defaultargs", reference="null") CreateGeometryFromWkb;
%feature("cs:defaultargs", reference="null") CreateGeometryFromWkt;
%feature("cs:defaultargs", reference="null") CreateGeometryFromEnvelope;
%feature("cs:defaultargs", options="null") ForceTo;
%feature("cs:defaultargs", bSetM=0) GT_SetModifier;

#endif //SWIG_VERSION >= 0x040200 && !defined(FROM_GDAL_I)
