/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Python Bindings
 * Purpose:  Custom bound GDAL functions for Python.  
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2000/03/06 02:24:48  warmerda
 * New
 *
 */

#include "gdal.h"
#include "Python.h"

/* -------------------------------------------------------------------- */
/*      For reasons I don't fathom, the fstat() call comes out as       */
/*      missing in Python.                                              */
/* -------------------------------------------------------------------- */
/* void fstat() {} */

/************************************************************************/
/*                         py_GDALReadRaster()                          */
/************************************************************************/
/*
  Arguments:
  
  hBand
  xoff, yoff, xsize, ysize
  buf_xsize, buf_ysize, buf_type
 */
#ifdef notdef
PyObject *py_GDALReadRaster(PyObject *self, PyObject *args) {
    PyObject * _resultobj;
    CPLErr * _result;
    GDALRasterBandH  _arg0;
    int  _arg1;
    int  _arg2;
    int  _arg3;
    int  _arg4;
    int  _arg5;
    int  _arg6;
    GDALDataType  _arg7;

    self = self;
    if(!PyArg_ParseTuple(args,"siiiiiii:GDALReadRaster",
                         &_argc0,&_arg1,&_arg2,&_arg3,&_arg4,
                         &_arg5,&_argc6,&_argc7)) 
        return NULL;

    if (_argc0) {
        if (SWIG_GetPtr(_argc0,(void **) &_arg0,(char *) 0 )) {
            PyErr_SetString(PyExc_TypeError,"Type error in argument 1 of GDALRasterIO. Expected _GDALRasterBandH.");
        return NULL;
        }
    }
    _result = (CPLErr *) malloc(sizeof(CPLErr ));
    *(_result) = GDALRasterIO(_arg0,_arg1,_arg2,_arg3,_arg4,_arg5,_arg6,_arg7,_arg8,_arg9,_arg10,_arg11);
    SWIG_MakePtr(_ptemp, (void *) _result,"_CPLErr_p");
    _resultobj = Py_BuildValue("s",_ptemp);
    return _resultobj;
}

#endif
