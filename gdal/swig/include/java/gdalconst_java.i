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

  static {
      gdalJNI.isAvailable();   // force gdalJNI static initializer to run and load library
  }

  public static boolean isAvailable() {
    return gdalJNI.isAvailable();
  }
%}

%pragma(java) jniclassimports=%{
import org.gdal.gdal.gdalJNI;
%}

%pragma(java) modulecode=%{

    /* Uninstanciable class */
    private gdalconst()
    {
    }
%}

%include typemaps_java.i
