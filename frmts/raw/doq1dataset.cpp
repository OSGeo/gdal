/******************************************************************************
 * $Id$
 *
 * Project:  USGS DOQ Driver (First Generation Format)
 * Purpose:  Implementation of DOQ1Dataset
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
 * Revision 1.1  1999/12/29 20:43:05  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

static GDALDriver	*poDOQ1Driver = NULL;

static int DOQGetField( unsigned char *, int );

CPL_C_START
void	GDALRegister_DOQ1(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				DOQ1Dataset				*/
/* ==================================================================== */
/************************************************************************/

class DOQ1Dataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
    
  public:
    		DOQ1Dataset();
    	        ~DOQ1Dataset();
    
    CPLErr 	GetGeoTransform( double * padfTransform );
    const char  *GetProjectionString( void );
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            DOQ1Dataset()                             */
/************************************************************************/

DOQ1Dataset::DOQ1Dataset()
{
    fpImage = NULL;
}

/************************************************************************/
/*                            ~DOQ1Dataset()                            */
/************************************************************************/

DOQ1Dataset::~DOQ1Dataset()

{
    if( fpImage != NULL )
        VSIFClose( fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DOQ1Dataset::GetGeoTransform( double * padfTransform )

{
    return( CE_None );
}

/************************************************************************/
/*                        GetProjectionString()                         */
/************************************************************************/

const char *DOQ1Dataset::GetProjectionString()

{
    return "";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DOQ1Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		nWidth, nHeight, nBandStorage, nBandTypes;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 180 && poOpenInfo->fp != NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Attempt to extract a few key values from the header.		*/
/* -------------------------------------------------------------------- */
    nWidth = DOQGetField(poOpenInfo->pabyHeader + 150, 6);
    nHeight = DOQGetField(poOpenInfo->pabyHeader + 144, 6);
    nBandStorage = DOQGetField(poOpenInfo->pabyHeader + 162, 3);
    nBandTypes = DOQGetField(poOpenInfo->pabyHeader + 156, 3);

/* -------------------------------------------------------------------- */
/*      Do these values look coherent for a DOQ file?  It would be      */
/*      nice to do a more comprehensive test than this!                 */
/* -------------------------------------------------------------------- */
    if( nWidth < 500 || nWidth > 25000
        || nHeight < 500 || nHeight > 25000
        || nBandStorage < 0 || nBandStorage > 4
        || nBandTypes < 1 || nBandTypes > 9 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check the configuration.  We don't currently handle all         */
/*      variations, only the common ones.                               */
/* -------------------------------------------------------------------- */
    if( nBandTypes > 5 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DOQ Data Type (%d) is not a supported configuration.\n",
                  nBandTypes );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DOQ1Dataset 	*poDS;

    poDS = new DOQ1Dataset();

    poDS->poDriver = poDOQ1Driver;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;
    
/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Compute layout of data.                                         */
/* -------------------------------------------------------------------- */
    int		nSkipBytes, nBytesPerPixel, nBytesPerLine, i;

    if( nBandTypes < 5 )
        nBytesPerPixel = 1;
    else if( nBandTypes == 5 )
        nBytesPerPixel = 3;

    nBytesPerLine = nBytesPerPixel * nWidth;
    nSkipBytes = 3 * nBytesPerLine;
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBytesPerPixel;
    poDS->papoBands = (GDALRasterBand **)
        VSICalloc(sizeof(GDALRasterBand *),poDS->nBands);

    for( i = 0; i < poDS->nBands; i++ )
    {
        poDS->papoBands[i] =
            new RawRasterBand( poDS, i+1, poDS->fpImage,
                               nSkipBytes + i, nBytesPerPixel, nBytesPerLine,
                               GDT_Byte, TRUE );
    }

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_DOQ1()                          */
/************************************************************************/

void GDALRegister_DOQ1()

{
    GDALDriver	*poDriver;

    if( poDOQ1Driver == NULL )
    {
        poDOQ1Driver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "DOQ1";
        poDriver->pszLongName = "USGS DOQ (Old Style)";
        
        poDriver->pfnOpen = DOQ1Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                            DOQGetField()                             */
/************************************************************************/

static int DOQGetField( unsigned char *pabyData, int nBytes )

{
    char	szWork[128];

    strncpy( szWork, (const char *) pabyData, nBytes );
    szWork[nBytes] = '\0';

    return atoi(szWork);
}
