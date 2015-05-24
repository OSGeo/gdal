/******************************************************************************
 * $Id$
 *
 * Name:     typedefs.i
 * Project:  GDAL Swig Interface
 * Purpose:  GDAL Typedefs for the interface.
 * Author:   Ari Jolma, ari.jolma at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Ari Jolma
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

/*
  Typedefs for GDAL types for swig, some extracted from GDAL headers.
*/

%include "../../port/cpl_config.h"

%include <windows.i>

typedef unsigned char   GByte;

#if defined(WIN32) && defined(_MSC_VER)

typedef __int64          GIntBig;
typedef unsigned __int64 GUIntBig;

#elif HAVE_LONG_LONG

typedef long long        GIntBig;
typedef unsigned long long GUIntBig;

#else

typedef long             GIntBig;
typedef unsigned long    GUIntBig;

#endif

/* Language specifics should not be here. 
   To do: include generic lang_typedefs.i, which comes from 
   included (swig command line) lang dir */
#ifdef SWIGCSHARP
typedef enum
{
    CE_None = 0,
    CE_Log = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
} CPLErr;
#else
/* why not for CSHARP? */
typedef void VSILFILE;
#endif

/*
  Typedefs for new types for both the bindings and swig.
*/

/*
  First some things needed by the bindings.
*/
%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "cpl_http.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdalwarper.h"

typedef void GDALMajorObjectShadow;
typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;
typedef void GDALColorTableShadow;
typedef void GDALRasterAttributeTableShadow;
typedef void GDALTransformerInfoShadow;
typedef void GDALAsyncReaderShadow;
%}

%inline %{
    typedef char retStringAndCPLFree;

    /* return value type that is used for VSI methods which return -1 on error (and set errno) */
    typedef int VSI_RETVAL;
    /* return value type that is used for some methods which return FALSE on error */
    typedef int GDAL_SUCCESS;
%}
