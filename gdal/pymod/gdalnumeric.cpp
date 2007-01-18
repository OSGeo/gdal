/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Python Bindings
 * Purpose:  GDAL/numpy bindings (all custom)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ****************************************************************************/

#include "gdal.h"
#include "Python.h"
#include "gdal_py.h"

CPL_CVSID("$Id$");

PyObject *py_NumPyArrayToGDALFilename( PyObject *self, PyObject *args )

{
    PyObject  *psArray;
    char      szString[128];

    self = self;
    if(!PyArg_ParseTuple(args,"O:NumPyArrayToGDALFilename",&psArray) )
        return NULL;
    
    /* I wish I had a safe way of checking the type */
    sprintf( szString, "NUMPY:::%p", psArray );

    return Py_BuildValue( "s", szString );
}

