/******************************************************************************
 * $Id$
 *
 * Name:     osr_java.i
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

%include arrays_java.i
%include typemaps_java.i

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("osrjni");
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

/*
 *  Needed to make the Constructor and getCptr 'public' and not 'protected'.
 *   There is likely a better way to do this (with javamethodmodifiers) but
 *   none worked for me. 
 */ 
%typemap(javabody) OSRSpatialReferenceShadow, OSRCoordinateTransformationShadow %{
  private long swigCPtr;
  protected boolean swigCMemOwn;

  public $javaclassname(long cPtr, boolean cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = cPtr;
  }
  
  public static long getCPtr($javaclassname obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }
%}

