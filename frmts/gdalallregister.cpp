/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * gdalallregister.cpp
 *
 * Main format registration function.
 * 
 * $Log$
 * Revision 1.9  1999/06/03 14:05:33  warmerda
 * added SDTS support
 *
 * Revision 1.8  1999/05/17 01:51:43  warmerda
 * Removed unused variable.
 *
 * Revision 1.7  1999/05/13 15:28:19  warmerda
 * Added elas format.
 *
 * Revision 1.6  1999/05/05 17:32:53  warmerda
 * added ceos
 *
 * Revision 1.5  1999/02/04 22:14:46  warmerda
 * added aigrid format
 *
 * Revision 1.4  1999/01/27 18:33:45  warmerda
 * Use FMRT_ macros to test if format avail
 *
 * Revision 1.3  1999/01/11 15:30:16  warmerda
 * added OGDI
 *
 * Revision 1.2  1998/12/03 18:37:26  warmerda
 * Drop GDB, add geotiff.
 *
 * Revision 1.1  1998/11/29 22:22:14  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_C_START
void GDALRegister_GDB(void);
void GDALRegister_GTiff(void);
void GDALRegister_GXF(void);
void GDALRegister_OGDI(void);
void GDALRegister_HFA(void);
void GDALRegister_AIGrid(void);
void GDALRegister_CEOS(void);
void GDALRegister_SDTS(void);
void GDALRegister_ELAS(void);
CPL_C_END

#ifdef notdef
// we may have a use for this some day
static char *szConfiguredFormats = "GDAL_FORMATS";
#endif

/************************************************************************/
/*                          GDALAllRegister()                           */
/*                                                                      */
/*      Register all identifiably supported formats.                    */
/************************************************************************/

void GDALAllRegister()

{
#ifdef FRMT_gdb    
    GDALRegister_GDB();
#endif    

#ifdef FRMT_gtiff    
    GDALRegister_GTiff();
#endif    

#ifdef FRMT_gxf
    GDALRegister_GXF();
#endif    

#ifdef FRMT_hfa
    GDALRegister_HFA();
#endif
    
#ifdef FRMT_ceos
    GDALRegister_CEOS();
#endif
    
#ifdef FRMT_elas
    GDALRegister_ELAS();
#endif
    
#ifdef FRMT_aigrid
    GDALRegister_AIGrid();
#endif

#ifdef FRMT_sdts
    GDALRegister_SDTS();
#endif

#ifdef FRMT_ogdi
    GDALRegister_OGDI();
#endif
}
