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
 * gdalopen.c
 *
 * GDALOpen() function, and supporting functions.
 *
 * 
 * $Log$
 * Revision 1.1  1998/12/03 18:31:45  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/* ==================================================================== */
/*                             GDALOpenInfo                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::GDALOpenInfo( const char * pszFilenameIn, GDALAccess eAccessIn )

{
    pszFilename = VSIStrdup( pszFilenameIn );

    nHeaderBytes = 0;
    pabyHeader = NULL;
    bStatOK = FALSE;
    eAccess = eAccessIn;
    
/* -------------------------------------------------------------------- */
/*      Collect information about the file.                             */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszFilename, &sStat ) )
    {
        bStatOK = TRUE;

        if( VSI_ISREG( sStat.st_mode ) )
        {
            nHeaderBytes = MAX(1024,sStat.st_size);
            pabyHeader = (GByte *) VSICalloc(nHeaderBytes+1,1);

            fp = VSIFOpen( pszFilename, "rb" );

            if( fp != NULL )
            {
                nHeaderBytes = VSIFRead( pabyHeader, nHeaderBytes, 1, fp );

                VSIRewind( fp );
            }
        }
    }
}

/************************************************************************/
/*                           ~GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::~GDALOpenInfo()

{
    VSIFree( pabyHeader );
    VSIFree( pszFilename );

    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                              GDALOpen()                              */
/*                                                                      */
/*      Attempt to open a dataset.                                      */
/************************************************************************/

GDALDatasetH GDALOpen( const char * pszFilename, GDALAccess eAccess )

{
    int		iDriver;
    GDALDriverManager *poDM = GetGDALDriverManager();
    GDALOpenInfo oOpenInfo( pszFilename, eAccess );

    CPLErrorReset();
    
    for( iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++ )
    {
        GDALDriver	*poDriver = poDM->GetDriver( iDriver );
        GDALDataset	*poDS;

        poDS = poDriver->pfnOpen( &oOpenInfo );
        if( poDS != NULL )
            return (GDALDatasetH) poDS;

        if( CPLGetLastErrorNo() != 0 )
            return NULL;
    }

    CPLError( CE_Failure, CPLE_OpenFailed,
              "`%s' not recognised as a supported file format.\n",
              pszFilename );
              
    return NULL;
}

