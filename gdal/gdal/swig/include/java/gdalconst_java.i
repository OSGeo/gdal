/******************************************************************************
 * $Id$
 *
 * Name:     gdalconst_java.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for Java bindings
 * Author:   Benjamin Collins, The MITRE Corporation
 *
*/

%pragma(java) jniclasscode=%{
  private static boolean available = false;

  static {
    try {
      System.loadLibrary("gdalconstjni");
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

/* This hacks turns the gdalconstJNI class into a package private class */
%pragma(java) jniclassimports=%{
%}

%pragma(java) modulecode=%{

    /* Uninstanciable class */
    private gdalconst()
    {
    }
%}

%include typemaps_java.i
