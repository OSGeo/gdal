/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Python Bindings
 * Purpose:  Declarations of entry points in source files other than that
 *           generated from gdal.i.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 * Revision 1.4  2006/03/21 21:54:00  fwarmerdam
 * fixup headers
 *
 * Revision 1.3  2003/09/26 15:58:58  warmerda
 * keep track if we used internal defs
 *
 * Revision 1.2  2000/12/23 02:11:50  warmerda
 * moved Python.h include outside CPL_C_START brackets
 *
 * Revision 1.1  2000/07/19 19:42:54  warmerda
 * New
 *
 */


#ifndef _GDAL_PY_INCLUDED
#define _GDAL_PY_INCLUDED

#include "Python.h"

CPL_C_START

void	GDALRegister_NUMPY(void);
PyObject *py_NumPyArrayToGDALFilename( PyObject *, PyObject * );

#ifdef HAVE_NUMPY
#  define NO_IMPORT
#  include "Numeric/arrayobject.h"
#endif

/* -------------------------------------------------------------------- */
/*      If we don't appear to have the arrayobject include file,        */
/*      then just declare the numeric types as we last knew them.       */
/* -------------------------------------------------------------------- */

#ifndef HAVE_NUMPY

#define NUMPY_DEFS_WRONG

enum PyArray_TYPES {    PyArray_CHAR, PyArray_UBYTE, PyArray_SBYTE,
		        PyArray_SHORT, PyArray_USHORT, 
		        PyArray_INT, PyArray_UINT, 
			PyArray_LONG,
			PyArray_FLOAT, PyArray_DOUBLE, 
			PyArray_CFLOAT, PyArray_CDOUBLE,
			PyArray_OBJECT,
			PyArray_NTYPES, PyArray_NOTYPE};

typedef void (PyArray_VectorUnaryFunc) Py_FPROTO((char *, int, char *, int, int));
typedef PyObject * (PyArray_GetItemFunc) Py_FPROTO((char *));
typedef int (PyArray_SetItemFunc) Py_FPROTO((PyObject *, char *));

typedef struct {
  PyArray_VectorUnaryFunc *cast[PyArray_NTYPES]; /* Functions to cast to */
					           /* all other types */
  PyArray_GetItemFunc *getitem;
  PyArray_SetItemFunc *setitem;

  int type_num, elsize;
  char *one, *zero;
  char type;

} PyArray_Descr;

typedef struct {
  PyObject_HEAD
  char *data;
  int nd;
  int *dimensions, *strides;
  PyObject *base;
  PyArray_Descr *descr;
  int flags;
#ifndef NUMPY_NOEXTRA
  PyObject* attributes; /* for user-defined information */
#endif
} PyArrayObject;

#endif /* ndef HAVE_NUMPY */

CPL_C_END

#endif /* ndef _GDAL_PY_INCLUDED */
