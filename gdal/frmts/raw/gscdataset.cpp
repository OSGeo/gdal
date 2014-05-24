/******************************************************************************
 * $Id$
 *
 * Project:  GSC Geogrid format driver.
 * Purpose:  Implements support for reading and writing GSC Geogrid format.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*				GSCDataset				*/
/* ==================================================================== */
/************************************************************************/

class GSCDataset : public RawDataset
{
    VSILFILE	*fpImage;	// image data file.
    
    double	adfGeoTransform[6];

  public:
    		GSCDataset();
    	        ~GSCDataset();

    CPLErr 	GetGeoTransform( double * padfTransform );
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            GSCDataset()                             */
/************************************************************************/

GSCDataset::GSCDataset()
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    fpImage = NULL;
}

/************************************************************************/
/*                            ~GSCDataset()                            */
/************************************************************************/

GSCDataset::~GSCDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GSCDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GSCDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		nPixels, nLines, i, nRecordLen;

/* -------------------------------------------------------------------- */
/*      Does this plausible look like a GSC Geogrid file?               */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 20 )
        return NULL;

    if( poOpenInfo->pabyHeader[12] != 0x02
        || poOpenInfo->pabyHeader[13] != 0x00
        || poOpenInfo->pabyHeader[14] != 0x00
        || poOpenInfo->pabyHeader[15] != 0x00 )
        return NULL;

    nRecordLen = CPL_LSBWORD32(((GInt32 *) poOpenInfo->pabyHeader)[0]);
    nPixels = CPL_LSBWORD32(((GInt32 *) poOpenInfo->pabyHeader)[1]);
    nLines  = CPL_LSBWORD32(((GInt32 *) poOpenInfo->pabyHeader)[2]);

    if( nPixels < 1 || nLines < 1 || nPixels > 100000 || nLines > 100000 )
        return NULL;

    if( nRecordLen != nPixels * 4 )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GSC driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    nRecordLen += 8; /* for record length markers */

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GSCDataset 	*poDS;

    poDS = new GSCDataset();

    poDS->nRasterXSize = nPixels;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (poDS->fpImage == NULL)
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header information in the second record. 		*/
/* -------------------------------------------------------------------- */
    float	afHeaderInfo[8];

    if( VSIFSeekL( poDS->fpImage, nRecordLen + 12, SEEK_SET ) != 0
        || VSIFReadL( afHeaderInfo, sizeof(float), 8, poDS->fpImage ) != 8 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failure reading second record of GSC file with %d record length.",
                  nRecordLen );
        delete poDS;
        return NULL;
    }

    for( i = 0; i < 8; i++ )
    {
        CPL_LSBPTR32( afHeaderInfo + i );
    }

    poDS->adfGeoTransform[0] = afHeaderInfo[2];
    poDS->adfGeoTransform[1] = afHeaderInfo[0];
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = afHeaderInfo[5];
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -afHeaderInfo[1];
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    RawRasterBand *poBand;
#ifdef CPL_LSB
    int	bNative = TRUE;
#else
    int bNative = FALSE;
#endif

    poBand = new RawRasterBand( poDS, 1, poDS->fpImage,
                                nRecordLen * 2 + 4,
                                sizeof(float), nRecordLen,
                                GDT_Float32, bNative, TRUE );
    poDS->SetBand( 1, poBand );

    poBand->SetNoDataValue( -1.0000000150474662199e+30 );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          GDALRegister_GSC()                          */
/************************************************************************/

void GDALRegister_GSC()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GSC" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GSC" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GSC Geogrid" );
//        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
//                                   "frmt_various.html#GSC" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GSCDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

