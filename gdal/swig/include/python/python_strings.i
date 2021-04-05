/******************************************************************************
 * $Id$
 *
 * Name:     python_strings.i
 * Project:  GDAL Python Interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

%{

/* Return a PyObject* from a NULL terminated C String */
static PyObject* GDALPythonObjectFromCStr(const char *pszStr) CPL_UNUSED;
static PyObject* GDALPythonObjectFromCStr(const char *pszStr)
{
  const unsigned char* pszIter = (const unsigned char*) pszStr;
  while(*pszIter != 0)
  {
    if (*pszIter > 127)
    {
        PyObject* pyObj = PyUnicode_DecodeUTF8(pszStr, strlen(pszStr), "ignore");
        if (pyObj != NULL)
            return pyObj;
        return PyBytes_FromString(pszStr);
    }
    pszIter ++;
  }
  return PyUnicode_FromString(pszStr);
}

/* Return a NULL terminated c String from a PyObject */
/* Result must be freed with GDALPythonFreeCStr */
static char* GDALPythonObjectToCStr(PyObject* pyObject, int* pbToFree) CPL_UNUSED;
static char* GDALPythonObjectToCStr(PyObject* pyObject, int* pbToFree)
{
  *pbToFree = 0;
  if (PyUnicode_Check(pyObject))
  {
      char *pszStr;
      char *pszNewStr;
      Py_ssize_t nLen;
      PyObject* pyUTF8Str = PyUnicode_AsUTF8String(pyObject);
      if( pyUTF8Str == NULL )
        return NULL;
      PyBytes_AsStringAndSize(pyUTF8Str, &pszStr, &nLen);
      pszNewStr = (char *) malloc(nLen+1);
      if( pszNewStr == NULL )
      {
          CPLError(CE_Failure, CPLE_OutOfMemory, "Failed to allocate %llu bytes",
                   (unsigned long long)(nLen + 1));
          Py_XDECREF(pyUTF8Str);
          return NULL;
      }
      memcpy(pszNewStr, pszStr, nLen+1);
      Py_XDECREF(pyUTF8Str);
      *pbToFree = 1;
      return pszNewStr;
  }
  else
  {
      return PyBytes_AsString(pyObject);
  }
}

static void GDALPythonFreeCStr(void* ptr, int bToFree) CPL_UNUSED;
static void GDALPythonFreeCStr(void* ptr, int bToFree)
{
   if (bToFree)
       free(ptr);
}

%}
