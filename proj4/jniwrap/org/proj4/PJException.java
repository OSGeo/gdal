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
package org.proj4;


/**
 * Exception thrown when a call to {@link PJ#transform(PJ, int, double[], int, int)} failed.
 *
 * @author  Martin Desruisseaux (Geomatys)
 */
public class PJException extends Exception {
    /**
     * For cross-version compatibility.
     */
    private static final long serialVersionUID = -2580747577812829763L;

    /**
     * Constructs a new exception with no message.
     */
    public PJException() {
        super();
    }

    /**
     * Constructs a new exception with the given message.
     *
     * @param message A message that describe the cause for the failure.
     */
    public PJException(final String message) {
        super(message);
    }
}
