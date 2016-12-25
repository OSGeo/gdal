/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Java/JNI wrappers for PROJ.4 API.
 * Author:   Martin Desruisseaux
 *
 ******************************************************************************
 * Copyright (c) 2011, Open Geospatial Consortium, Inc.
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
 *****************************************************************************
 * This file is a copy of a file developed in the GeoAPI "Proj.4 binding"
 * module (http://www.geoapi.org/geoapi-proj4/index.html). If this file is
 * modified, please consider synchronizing the changes with GeoAPI.
 */

/**
 * Wrappers for the <a href="http://proj.osgeo.org/">Proj4</a> library. The {@link org.proj4.PJ}
 * class contains only native methods delegating their work to the Proj.4 library.
 * For higher-level methods making use of those native methods, see for example the
 * <a href="http://www.geoapi.org/geoapi-proj4/index.html">GeoAPI bindings for Proj.4</a>.
 *
 * @author  Martin Desruisseaux (Geomatys)
 */
package org.proj4;
