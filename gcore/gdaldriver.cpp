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
 * gdaldriver.cpp
 *
 * The GDALDriver class.  This class is mostly just a container for
 * driver specific function pointers.
 * 
 * $Log$
 * Revision 1.4  1999/01/11 15:36:50  warmerda
 * Added GDALCreate()
 *
 * Revision 1.3  1998/12/31 18:54:53  warmerda
 * Flesh out create method.
 *
 * Revision 1.2  1998/12/06 22:17:32  warmerda
 * Add stub Create() method
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver()

{
    pszShortName = NULL;
    pszLongName = NULL;

    pfnOpen = NULL;
}

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset * GDALDriver::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char ** papszParmList )

{
    /* notdef: should add a bunch of error checking here */
    
    if( pfnCreate == NULL )
    {
        CPLError( CE_Fatal, CPLE_NotSupported,
                  "GDALDriver::Create() ... not create method implemented"
                  " for this format.\n" );
        
        return NULL;
    }
    else
    {
        return( pfnCreate( pszFilename, nXSize, nYSize, nBands, eType,
                           papszParmList ) );
    }
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

GDALDatasetH CPL_DLL GDALCreate( GDALDriverH hDriver,
                                 const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eBandType,
                                 char ** papszOptions )

{
    return( ((GDALDriver *) hDriver)->Create( pszFilename,
                                              nXSize, nYSize, nBands,
                                              eBandType, papszOptions ) );
}
