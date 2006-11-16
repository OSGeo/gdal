/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Python Bindings
 * Purpose:  Declarations of entry points in source files other than that
 *           generated from gdal.i.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 */

#include "Python.h"

#include "gdal_array.h"

/************************************************************************/
/*                          GDALRegister_NUMPY()                        */
/************************************************************************/

static PyObject *GDALArrayError;



PyObject* GetArrayFilename(PyObject *self, PyObject *args){
    GDALRegister_NUMPY();
    char      szString[128];
    
    PyObject  *psArray;
    
    if(!PyArg_ParseTuple(args,"O:GetArrayFilename",&psArray) ) {
        PyErr_SetString(GDALArrayError, "Unable to read in array!");
        return NULL;
        }

    
    /* I wish I had a safe way of checking the type */        
    sprintf( szString, "NUMPY:::%p", psArray );
    return Py_BuildValue( "s", szString );

}
    
PyObject* GDALRegister_NUMPY(void)

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NUMPY" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NUMPY" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Numeric Python Array" );
        
        poDriver->pfnOpen = NUMPYDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );

    }
    Py_INCREF(Py_None);
    return Py_None;
}


/************************************************************************/
/*                          Initialization()                            */
/************************************************************************/

static PyMethodDef GDALPythonArrayMethods[] = {

    {"GDALRegister_NUMPY", (PyCFunction)GDALRegister_NUMPY, METH_VARARGS,
        "Registers the NUMPY driver"},        
    {"GetArrayFilename", (PyCFunction)GetArrayFilename, METH_VARARGS,
        "Returns filename hack for GDALOpen"},  
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
init_gdal_array(void)
{
    PyObject* module;
    module = Py_InitModule3("_gdal_array", GDALPythonArrayMethods,
                       "GDAL numpy helper module");
    if (module == NULL)
      return;
    GDALArrayError = PyErr_NewException("_gdal_array.GDALArrayError", NULL, NULL);
    Py_INCREF(GDALArrayError);
    PyModule_AddObject(module, "GDALArrayError", GDALArrayError);
    

}
