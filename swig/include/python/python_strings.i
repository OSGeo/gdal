/******************************************************************************
 *
 * Name:     python_strings.i
 * Project:  GDAL Python Interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
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
        PyObject* pyObj = PyUnicode_DecodeUTF8(pszStr, strlen(pszStr), "strict");
        if (pyObj != NULL && !PyErr_Occurred())
            return pyObj;
        PyErr_Clear();
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
  else if( PyBytes_Check(pyObject) )
  {
      char* ret = PyBytes_AsString(pyObject);

      // Check if there are \0 bytes inside the string
      const Py_ssize_t size = PyBytes_Size(pyObject);
      for( Py_ssize_t i = 0; i < size; i++ )
      {
          if( ret[i] == 0 )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "bytes object cast as string contains a zero-byte.");
              return NULL;
          }
      }

      return ret;
  }
  else
  {
      CPLError(CE_Failure, CPLE_AppDefined,
               "Passed object is neither of type string nor bytes");
      return NULL;
  }
}

static char * GDALPythonPathToCStr(PyObject* pyObject, int* pbToFree) CPL_UNUSED;
static char * GDALPythonPathToCStr(PyObject* pyObject, int* pbToFree)
{
    PyObject* os = PyImport_ImportModule("os");
    if (os == NULL)
    {
        return NULL;
    }

    PyObject* pathLike = PyObject_GetAttrString(os, "PathLike");
    if (pathLike == NULL)
    {
        Py_DECREF(os);
        return NULL;
    }

    if (!PyObject_IsInstance(pyObject, pathLike))
    {
        Py_DECREF(pathLike);
        Py_DECREF(os);
        return NULL;
    }

    PyObject* str = PyObject_Str(pyObject);
    char* ret = NULL;
    if (str != NULL)
    {
        ret = GDALPythonObjectToCStr(str, pbToFree);
        Py_DECREF(str);
    }

    Py_DECREF(pathLike);
    Py_DECREF(os);

    return ret;
}


static void GDALPythonFreeCStr(void* ptr, int bToFree) CPL_UNUSED;
static void GDALPythonFreeCStr(void* ptr, int bToFree)
{
   if (bToFree)
       free(ptr);
}

%}
