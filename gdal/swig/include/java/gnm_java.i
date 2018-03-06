/******************************************************************************
 * $Id$
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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
