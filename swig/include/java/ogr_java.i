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
%include java_exceptions.i

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("ogrjni");
      available = true;
      
      if (org.gdal.gdal.gdal.HasThreadSupport() == 0)
      {
        System.err.println("WARNING : GDAL should be compiled with thread support for safe execution in Java.");
      }
      
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

%typemap(javacode) OGRDataSourceShadow %{

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = ((($javaclassname)obj).swigCPtr == this.swigCPtr);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }

  public Layer GetLayer(int index)
  {
      return GetLayerByIndex(index);
  }

  public Layer GetLayer(String layerName)
  {
      return GetLayerByName(layerName);
  }

%}

%typemap(javacode) OGRLayerShadow %{
  private Object parentReference;

  protected static long getCPtrAndDisown($javaclassname obj) {
    if (obj != null)
    {
        obj.swigCMemOwn= false;
        obj.parentReference = null;
    }
    return getCPtr(obj);
  }

  /* Ensure that the GC doesn't collect any parent instance set from Java */
  protected void addReference(Object reference) {
    parentReference = reference;
  }

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = ((($javaclassname)obj).swigCPtr == this.swigCPtr);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }

  public double[] GetExtent(boolean force)
  {
      double[] argout = new double[4];
      try
      {
          GetExtent(argout, (force) ? 1 : 0);
          return argout;
      }
      catch(RuntimeException e)
      {
          return null;
      }
  }

  public double[] GetExtent()
  {
      return GetExtent(true);
  }
%}

%typemap(javaimports) OGRGeometryShadow %{
import org.gdal.ogr.ogr;
import org.gdal.ogr.GeometryNative;
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;
%}


%typemap(javainterfaces) OGRGeometryShadow "Cloneable"

%typemap(javacode) OGRGeometryShadow %{
  private Object parentReference;

  protected static long getCPtrAndDisown($javaclassname obj) {
    if (obj != null)
    {
        if (obj.nativeObject == null)
            throw new RuntimeException("Cannot disown an object that was not owned...");
        obj.nativeObject.dontDisposeNativeResources();
        obj.nativeObject = null;
        obj.parentReference = null;
    }
    return getCPtr(obj);
  }

  /* Ensure that the GC doesn't collect any parent instance set from Java */
  protected void addReference(Object reference) {
    parentReference = reference;
  }

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = Equal(($javaclassname)obj);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }

  public Object clone()
  {
      return Clone();
  }

  public double[] GetPoint_2D(int iPoint)
  {
      double[] coords = new double[2];
      GetPoint_2D(iPoint, coords);
      return coords;
  }

  public double[] GetPoint(int iPoint)
  {
      double[] coords = new double[3];
      GetPoint(iPoint, coords);
      return coords;
  }

  public static Geometry CreateFromWkt(String wkt)
  {
      return ogr.CreateGeometryFromWkt(wkt);
  }

  public static Geometry CreateFromWkb(byte[] wkb)
  {
      return ogr.CreateGeometryFromWkb(wkb);
  }

  public static Geometry CreateFromGML(String gml)
  {
      return ogr.CreateGeometryFromGML(gml);
  }

  public static Geometry CreateFromJson(String json)
  {
      return ogr.CreateGeometryFromJson(json);
  }

  public int ExportToWkb(byte[] wkbArray, int byte_order)
  {
      if (wkbArray == null)
          throw new NullPointerException();
      byte[] srcArray = ExportToWkb(byte_order);
      if (wkbArray.length < srcArray.length)
          throw new RuntimeException("Array too small");

      System.arraycopy( srcArray, 0, wkbArray, 0, srcArray.length );

      return 0;
  }

%}

/* Keep the container object alive while the contained */
/* is alive */
%typemap(javaout) int AddGeometryDirectly {
    int ret = $jnicall;
    if (other_disown != null)
        other_disown.addReference(this);
    return ret;
  }

/* Keep the container object alive while the contained */
/* is alive */
%typemap(javaout) int SetGeometryDirectly {
    int ret = $jnicall;
    if (geom != null)
        geom.addReference(this);
    return ret;
  }

// Add a Java reference to prevent premature garbage collection and resulting use
// of dangling C++ pointer. Intended for methods that return pointers or
// references to a member variable.
%typemap(javaout) OGRGeometryShadow* GetSpatialFilter,
                  OGRFeatureDefnShadow* GetLayerDefn,
                  OGRLayerShadow* CreateLayer,
                  OGRLayerShadow* CopyLayer,
                  OGRLayerShadow* GetLayerByIndex,
                  OGRLayerShadow* GetLayerByName,
                  OGRLayerShadow* ExecuteSQL,
                  OGRFeatureDefnShadow* GetDefnRef,
                  OGRFieldDefnShadow* GetFieldDefnRef,
                  OGRFieldDefnShadow* GetFieldDefn,
                  OGRGeometryShadow* GetGeometryRef {
    long cPtr = $jnicall;
    $javaclassname ret = null;
    if (cPtr != 0) {
      ret = new $javaclassname(cPtr, $owner);
      ret.addReference(this);
    }
    return ret;
  }

/* Could be disabled as do nothing, but we keep them for backwards compatibility */
//%typemap(javadestruct, methodname="delete", methodmodifiers="public") OGRDriverShadow ""
//%typemap(javadestruct, methodname="delete", methodmodifiers="public") OGRLayerShadow ""

%typemap(javainterfaces) OGRFeatureShadow "Cloneable"

/* ------------------------------------------------------------------- */
/* Below an advanced technique to avoid the use of a finalize() method */
/* in the Feature object, that prevents efficient garbarge collection. */
/* This is loosely based on ideas from an article at                   */
/* http://java.sun.com/developer/technicalArticles/javase/finalization */
/* ------------------------------------------------------------------- */

%define SMART_FINALIZER(type)
%typemap(javabody) type ## Native %{
  private long swigCPtr;

  static private ReferenceQueue refQueue = new ReferenceQueue();
  static private Set refList = Collections.synchronizedSet(new HashSet());
  static private Thread cleanupThread = null;

  /* We start a cleanup thread in daemon mode */
  /* If we can't, we'll cleanup garbaged features at creation time */
  static
  {
    cleanupThread = new Thread() {
        public void run()
        {
            while(true)
            {
                try
                {
                    type ## Native nativeObject =
                        (type ## Native) refQueue.remove();
                    if (nativeObject != null)
                        nativeObject.delete();
                }
                catch(InterruptedException ie) {}
            }
        }
    };
    try
    {
        cleanupThread.setName(#type + "NativeObjectsCleaner");
        cleanupThread.setDaemon(true);
        cleanupThread.start();
    }
    catch (SecurityException se)
    {
        //System.err.println("could not start daemon thread");
        cleanupThread = null;
    }
  }

  public $javaclassname(type javaObject, long cPtr) {
    super(javaObject, refQueue);

    if (cleanupThread == null)
    {
        /* We didn't manage to have a daemon cleanup thread */
        /* so let's clean manually */
        while(true)
        {
            type ## Native nativeObject =
                (type ## Native) refQueue.poll();
            if (nativeObject != null)
                nativeObject.delete();
            else
                break;
        }
    }

    refList.add(this);

    swigCPtr = cPtr;
  }

  public void dontDisposeNativeResources()
  {
      refList.remove(this);
      swigCPtr = 0;
  }
%}

%typemap(javadestruct, methodname="delete", methodmodifiers="public") type ## Native %{
  {
    refList.remove(this);
    if(swigCPtr != 0) {
      ogrJNI.delete_ ## type(swigCPtr);
    }
    swigCPtr = 0;
  }
%}

%typemap(javaimports) type ## Native %{
import java.lang.ref.WeakReference;
import java.lang.ref.ReferenceQueue;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;

import org.gdal.ogr. ## type;
%}


%typemap(javabase) type ## Native "WeakReference";

%typemap(javaclassmodifiers) type ## Native %{
/* This class enables to finalize native resources associated with the object */
/* without needing a finalize() method */

class%}

%typemap(javacode) type ## Native ""

%typemap(javabody) OGR ## type ## Shadow %{
  private long swigCPtr;
  private type ## Native nativeObject;

  protected $javaclassname(long cPtr, boolean cMemoryOwn) {
    if (cPtr == 0)
        throw new RuntimeException();
    swigCPtr = cPtr;
    if (cMemoryOwn)
        nativeObject = new type ## Native(this, cPtr);
  }
  
  protected static long getCPtr($javaclassname obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }
%}

%typemap(javadestruct, methodname="delete", methodmodifiers="public") OGR ## type ## Shadow %{
   {
      if (nativeObject != null)
      {
        nativeObject.delete();
        nativeObject = null;
      }
   }
%}

%typemap(javafinalize) OGR ## type ## Shadow ""

%enddef

SMART_FINALIZER(Feature)
%typemap(javaimports) OGRFeatureShadow %{
import org.gdal.ogr.FeatureNative;
%}
%typemap(javacode) OGRFeatureShadow %{

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = Equal(($javaclassname)obj);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }

  public Object clone()
  {
      return Clone();
  }
%}

SMART_FINALIZER(Geometry)

/* ----------------------------------------------------------------- */
/* End of smart finalizer mechanism                                  */
/* ----------------------------------------------------------------- */

%include typemaps_java.i
