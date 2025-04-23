/******************************************************************************
 *
 * Project:  GNM Core SWIG Interface declarations.
 * Purpose:  GNM declarations.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2016, Dmitry Baryshnikov
 * Copyright (c) 2016, NextGIS <info@nextgis.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef FROM_GDAL_I
%include java_exceptions.i
#endif

%pragma(java) jniclasscode=%{

  static {
    gdalJNI.isAvailable();   // force gdalJNI static initializer to run and load library
  }

  public static boolean isAvailable() {
    return gdalJNI.isAvailable();
  }
%}

%pragma(java) jniclassimports=%{
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;
import org.gdal.gdal.MajorObject;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Feature;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
import org.gdal.gdal.gdalJNI;
%}

%pragma(java) moduleimports=%{
import org.gdal.osr.SpatialReference;
import org.gdal.gdal.MajorObject;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Feature;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
%}

%typemap(javaimports) GNMNetworkShadow %{
import org.gdal.osr.SpatialReference;
import org.gdal.gdal.MajorObject;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Feature;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
%}

%typemap(javaimports) GNMGenericNetworkShadow %{
import org.gdal.osr.SpatialReference;
import org.gdal.gdal.MajorObject;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Feature;
import org.gdal.ogr.StyleTable;
import org.gdal.ogr.Layer;
%}

%typemap(javacode) GNMNetworkShadow %{

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = ((($javaclassname)obj).swigCPtr == this.swigCPtr);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }
%}

%typemap(javacode) GNMGenericNetworkShadow %{

  public boolean equals(Object obj) {
    boolean equal = false;
    if (obj instanceof $javaclassname)
      equal = ((($javaclassname)obj).swigCPtr == this.swigCPtr);
    return equal;
  }

  public int hashCode() {
     return (int)swigCPtr;
  }
%}

#ifndef FROM_GDAL_I
%include callback.i
#endif

%include typemaps_java.i
