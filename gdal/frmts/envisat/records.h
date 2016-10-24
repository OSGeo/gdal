/******************************************************************************
 * $Id$
 *
 * Project:  APP ENVISAT Support
 * Purpose:  Low Level Envisat file access (read/write) API.
 * Author:   Antonio Valentino <antonio.valentino@tiscali.it>
 *
 ******************************************************************************
 * Copyright (c) 2011, Antonio Valentino
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

#ifndef RECORDS_H_
#define RECORDS_H_

#include "gdal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MJD_FIELD_SIZE 12

/*! Field data types */
typedef enum {
    /*! Unknown or unspecified type */      EDT_Unknown = GDT_Unknown,
    /*! Eight bit unsigned integer */       EDT_UByte = GDT_Byte,
    /*! Eight bit signed integer */         EDT_SByte = GDT_TypeCount + 0,
    /*! Sixteen bit unsigned integer */     EDT_UInt16 = GDT_UInt16,
    /*! Sixteen bit signed integer */       EDT_Int16 = GDT_Int16,
    /*! Thirty two bit unsigned integer */  EDT_UInt32 = GDT_UInt32,
    /*! Thirty two bit signed integer */    EDT_Int32 = GDT_Int32,
    /*! Thirty two bit floating point */    EDT_Float32 = GDT_Float32,
    /*! Sixty four bit floating point */    EDT_Float64 = GDT_Float64,
    /*! Complex Int16 */                    EDT_CInt16 = GDT_CInt16,
    /*! Complex Int32 */                    EDT_CInt32 = GDT_CInt32,
    /*! Complex Float32 */                  EDT_CFloat32 = GDT_CFloat32,
    /*! Complex Float64 */                  EDT_CFloat64 = GDT_CFloat64,
    /*! Modified Julian Dated */            EDT_MJD = GDT_TypeCount + 1,
    /*! ASCII characters */                 EDT_Char = GDT_TypeCount + 2,
    EDT_TypeCount = GDT_TypeCount + 3       /* maximum type # + 1 */
} EnvisatDataType;

typedef struct {
    const char* szName;
    int nOffset;
    EnvisatDataType eType;
    int nCount;
} EnvisatFieldDescr;

typedef struct {
    const char* szName;
    const EnvisatFieldDescr *pFields;
} EnvisatRecordDescr;

const EnvisatRecordDescr* EnvisatFile_GetRecordDescriptor(const char* pszProduct,
                                              const char* pszDataset);

CPLErr EnvisatFile_GetFieldAsString(const void*, int, const EnvisatFieldDescr*, char*, size_t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RECORDS_H_ */
