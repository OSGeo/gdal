/******************************************************************************
 * $Id$
 *
 * Project:  PNM Driver
 * Purpose:  Implementation of PNMDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * Revision 1.7  2002/09/19 21:10:02  warmerda
 * Fixed GetDriverByName call.
 *
 * Revision 1.6  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.5  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.4  2002/04/16 17:52:35  warmerda
 * Initialize variables.
 *
 * Revision 1.3  2001/07/18 19:04:10  warmerda
 * Removed debug printf.
 *
 * Revision 1.2  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.1  2001/01/03 18:53:36  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_PNM(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				PNMDataset				*/
/* ==================================================================== */
/************************************************************************/

class PNMDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
  public:
    		PNMDataset();
    	        ~PNMDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            PNMDataset()                             */
/************************************************************************/

PNMDataset::PNMDataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~PNMDataset()                            */
/************************************************************************/

PNMDataset::~PNMDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFClose( fpImage );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PNMDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Verify that this is a _raw_ ppm or pgm file.  Note, we don't    */
/*      support ascii files, or pbm (1bit) files.                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 10 || poOpenInfo->fp == NULL )
        return NULL;

    if( poOpenInfo->pabyHeader[0] != 'P' || poOpenInfo->pabyHeader[2] != 10 )
        return NULL;

    if( poOpenInfo->pabyHeader[1] != '5' 
        && poOpenInfo->pabyHeader[1] != '6' )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Parse out the tokens from the header.                           */
/* -------------------------------------------------------------------- */
    const char  *pszSrc = (const char *) poOpenInfo->pabyHeader;
    char	szToken[512];
    int		iIn, iOut, iToken = 0, nWidth=-1, nHeight=-1;

    iIn = 2;
    while( iIn < poOpenInfo->nHeaderBytes && iToken < 3 )
    {
        iOut = 0;
        szToken[0] = '\0';
        while( iIn < poOpenInfo->nHeaderBytes )
        {
            if( pszSrc[iIn] == '#' )
            {
                while( pszSrc[iIn] != 10 && pszSrc[iIn] != 13 
                       && iIn < poOpenInfo->nHeaderBytes-1 )
                    iIn++;
            }

            if( iOut != 0 && isspace(pszSrc[iIn]) )
            {
                szToken[iOut] = '\0';

                if( iToken == 0 )
                    nWidth = atoi(szToken);
                else if( iToken == 1 )
                    nHeight = atoi(szToken);

                iToken++;
                iIn++;
                break;
            }
            
            else if( !isspace(pszSrc[iIn]) )
            {
                szToken[iOut++] = pszSrc[iIn];
            }

            iIn++;
        }
    }

    if( iToken != 3 || nWidth < 1 || nHeight < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PNMDataset 	*poDS;

    poDS = new PNMDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    
/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    VSIFClose( poOpenInfo->fp );
    poOpenInfo->fp = NULL;

    if( poOpenInfo->eAccess == GA_Update )
        poDS->fpImage = VSIFOpen( poOpenInfo->pszFilename, "rb+" );
    else
        poDS->fpImage = VSIFOpen( poOpenInfo->pszFilename, "rb" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to re-open %s within PNM driver.\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->pabyHeader[1] == '5' )
    {
        poDS->SetBand( 
            1, new RawRasterBand( poDS, 1, poDS->fpImage,
                                  iIn, 1, nWidth, GDT_Byte, TRUE ));
    }
    else 
    {
        poDS->SetBand( 
            1, new RawRasterBand( poDS, 1, poDS->fpImage,
                                  iIn, 3, nWidth*3, GDT_Byte, TRUE ));
        poDS->SetBand( 
            2, new RawRasterBand( poDS, 2, poDS->fpImage,
                                  iIn+1, 3, nWidth*3, GDT_Byte, TRUE ));
        poDS->SetBand( 
            3, new RawRasterBand( poDS, 3, poDS->fpImage,
                                  iIn+2, 3, nWidth*3, GDT_Byte, TRUE ));
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PNMDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create PNM dataset with an illegal\n"
              "data type (%s), only Byte supported.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

    if( nBands != 1 && nBands != 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create PNM dataset with an illegal number\n"
                  "of bands (%d).  Must be 1 (greyscale) or 3 (RGB).\n",
                  nBands );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Write out the header.                                           */
/* -------------------------------------------------------------------- */
    char	szHeader[500];

    memset( szHeader, 0, sizeof(szHeader) );

    if( nBands == 3 )
        sprintf( szHeader, "P6\n%d %d\n%d\n", nXSize, nYSize, 255 );
    else
        sprintf( szHeader, "P5\n%d %d\n%d\n", nXSize, nYSize, 255 );

    VSIFWrite( (void *) szHeader, strlen(szHeader)+2, 1, fp );
    VSIFClose( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_PNM()                          */
/************************************************************************/

void GDALRegister_PNM()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PNM" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PNM" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Portable Pixmap Format (netpbm)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#PNM" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pnm" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, 
                                   "image/x-portable-anymap" );

        poDriver->pfnOpen = PNMDataset::Open;
        poDriver->pfnCreate = PNMDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
