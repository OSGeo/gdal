/******************************************************************************
 * $Id$
 *
 * Project:  PCRaster Integration
 * Purpose:  Radarsat 2 - XML Products (product.xml) driver
 * Author:   Kor de Jong, k.dejong at geog.uu.nl
 *
 ******************************************************************************
 * Copyright (c) 2004, Kor de Jong
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
 * Revision 1.1  2004/10/22 14:19:27  fwarmerdam
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_PCRaster(void);
CPL_C_END

/************************************************************************/
/*                       GDALRegister_PCRaster()                        */
/************************************************************************/

void GDALRegister_PCRaster()

{
#ifdef notdef
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PCRaster" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PCRaster" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "PCRaster Image File" );

        poDriver->pfnOpen = PCRasterDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
#endif
}

