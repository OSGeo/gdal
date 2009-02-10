/******************************************************************************
 * $Id$
 *
 * Name:     ogr_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
 *
 * $Log$
 * Revision 1.2  2006/02/16 17:21:12  collinsb
 * Updates to Java bindings to keep the code from halting execution if the native libraries cannot be found.
 *
 * Revision 1.1  2006/02/02 20:56:07  collinsb
 * Added Java specific typemap code
 *
 *
*/
#ifdef SWIGJAVA
%{
typedef char retStringAndCPLFree;
%}
#endif

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("ogrjni");
      available = true;
    } catch (UnsatisfiedLinkError e) {
      available = false;
      System.err.println("Native library load failed.");
      System.err.println(e);
    }
  }
  
  public static boolean isAvailable() {
    return available;
  }
%}


%rename (GetFieldType) GetType;
%rename (GetDriverCount) OGRGetDriverCount;
%rename (GetOpenDSCount) OGRGetOpenDSCount;
%rename (SetGenerate_DB2_V72_BYTE_ORDER) OGRSetGenerate_DB2_V72_BYTE_ORDER;
%rename (RegisterAll) OGRRegisterAll();

/*
 *
 */ 
 
%pragma(java) jniclassimports=%{
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;
%}
 
%pragma(java) moduleimports=%{
import org.gdal.osr.SpatialReference;
%}

%typemap(javaimports) OGRLayerShadow %{
import org.gdal.osr.SpatialReference;
%}
%typemap(javaimports) OGRDataSourceShadow %{
import org.gdal.osr.SpatialReference;
%}
%typemap(javaimports) OGRGeometryShadow %{
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;
%}

// Add a Java reference to prevent premature garbage collection and resulting use
// of dangling C++ pointer. Intended for methods that return pointers or
// references to a member variable.
%typemap(javaout) OGRGeometryShadow* GetSpatialFilter {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRFeatureDefnShadow* GetLayerDefn {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRLayerShadow* CreateLayer {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRLayerShadow* CopyLayer {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRLayerShadow* GetLayerByIndex {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRLayerShadow* GetLayerByName {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRLayerShadow* ExecuteSQL {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRFeatureDefnShadow* GetDefnRef {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRFieldDefnShadow* GetFieldDefnRef {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRFieldDefnShadow* GetFieldDefn {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%typemap(javaout) OGRGeometryShadow* GetGeometryRef {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

%include typemaps_java.i
