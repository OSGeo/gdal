/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Free standing functions for GDAL.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.3  1999/05/17 02:00:45  vgough
 * made pure_virtual C linkage
 *
 * Revision 1.2  1999/05/16 19:32:13  warmerda
 * Added __pure_virtual.
 *
 * Revision 1.1  1998/12/06 02:50:16  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                           __pure_virtual()                           */
/*                                                                      */
/*      The following is a gross hack to remove the last remaining      */
/*      dependency on the GNU C++ standard library.                     */
/************************************************************************/

#ifdef __GNUC__

extern "C"
void __pure_virtual()

{
}

#endif

/************************************************************************/
/*                        GDALGetDataTypeSize()                         */
/************************************************************************/
int GDALGetDataTypeSize( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Byte:
        return 8;

      case GDT_UInt16:
      case GDT_Int16:
        return 16;

      case GDT_UInt32:
      case GDT_Int32:
      case GDT_Float32:
        return 32;

      case GDT_Float64:
        return 64;

      default:
        CPLAssert( FALSE );
        return 0;
    }
}
