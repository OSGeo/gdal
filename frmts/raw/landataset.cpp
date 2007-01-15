/******************************************************************************
 * $Id$
 *
 * Project:  eCognition
 * Purpose:  Implementation of Erdas .LAN / .GIS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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
 * Revision 1.10  2005/12/14 20:03:57  fwarmerdam
 * Corrected byte swapping decision for 16bit images per report from Maciek.
 *
 * Revision 1.9  2005/09/27 18:13:04  fwarmerdam
 * Only return 16 colors for 4bit files.
 *
 * Revision 1.8  2005/09/27 17:40:05  fwarmerdam
 * Make sure we notify RawRasterBand we are using large file api.
 *
 * Revision 1.7  2005/09/27 17:37:20  fwarmerdam
 * Derived 4bit band from GDALPamRasterBand, and added colortable support.
 * Also use large file API consistently for file access.
 *
 * Revision 1.6  2005/05/05 13:55:42  fwarmerdam
 * PAM Enable
 *
 * Revision 1.5  2004/05/28 18:15:58  warmerda
 * added .TRL colortable support
 *
 * Revision 1.4  2004/05/28 16:06:27  warmerda
 * added pre7.4 handling and world file support
 *
 * Revision 1.3  2004/05/26 20:25:05  warmerda
 * Added 4bit support.
 *
 * Revision 1.2  2004/05/26 18:19:15  warmerda
 * Implement
 *
 * Revision 1.1  2004/05/26 17:45:16  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_LAN(void);
CPL_C_END

/**

Erdas Header format: "HEAD74"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (ie. HEAD74). 
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands. 
10         6     char      Unknown.
16         4    Int32      Width
20         4    Int32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area. 
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

Erdas Header format: "HEADER"

Offset   Size    Type      Description
------   ----    ----      -----------
0          6     char      magic cookie / version (ie. HEAD74). 
6          2    Int16      Pixel type, 0=8bit, 1=4bit, 2=16bit
8          2    Int16      Number of Bands. 
10         6     char      Unknown.
16         4  Float32      Width
20         4  Float32      Height
24         4    Int32      X Start (offset in original file?)
28         4    Int32      Y Start (offset in original file?)
32        56     char      Unknown.
88         2    Int16      0=LAT, 1=UTM, 2=StatePlane, 3- are projections?
90         2    Int16      Classes in coverage.
92        14     char      Unknown.
106        2    Int16      Area Unit (0=none, 1=Acre, 2=Hectare, 3=Other)
108        4  Float32      Pixel area. 
112        4  Float32      Upper Left corner X (center of pixel?)
116        4  Float32      Upper Left corner Y (center of pixel?)
120        4  Float32      Width of a pixel.
124        4  Float32      Height of a pixel.

All binary fields are in the same byte order but it may be big endian or
little endian depending on what platform the file was written on.  Usually
this can be checked against the number of bands though this test won't work
if there are more than 255 bands. 

**/

#define ERD_HEADER_SIZE  128

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class LANDataset;

class LAN4BitRasterBand : public GDALPamRasterBand
{
    GDALColorTable *poCT;
    GDALColorInterp eInterp;

  public:
                   LAN4BitRasterBand( LANDataset *, int );
                  ~LAN4BitRasterBand();

    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual CPLErr SetColorInterpretation( GDALColorInterp );

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/* ==================================================================== */
/*				LANDataset				*/
/* ==================================================================== */
/************************************************************************/

class LANDataset : public RawDataset
{
  public:
    FILE	*fpImage;	// image data file.
    
    char	pachHeader[ERD_HEADER_SIZE];

    char        *pszProjection;
    
    double      adfGeoTransform[6];

  public:
    		LANDataset();
    	        ~LANDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         LAN4BitRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         LAN4BitRasterBand()                          */
/************************************************************************/

LAN4BitRasterBand::LAN4BitRasterBand( LANDataset *poDS, int nBandIn )

{
    this->poDS = poDS;
    this->nBand = nBandIn;
    this->eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();;
    nBlockYSize = 1;

    poCT = NULL;
    eInterp = GCI_Undefined;
}

/************************************************************************/
/*                         ~LAN4BitRasterBand()                         */
/************************************************************************/

LAN4BitRasterBand::~LAN4BitRasterBand()

{
    if( poCT )
        delete poCT;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr LAN4BitRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    LANDataset *poLAN_DS = (LANDataset *) poDS;
    CPLAssert( nBlockXOff == 0  );
    
/* -------------------------------------------------------------------- */
/*      Seek to profile.                                                */
/* -------------------------------------------------------------------- */
    int nOffset;

    nOffset = 
        ERD_HEADER_SIZE
        + (nBlockYOff * nRasterXSize * poLAN_DS->GetRasterCount()) / 2
        + ((nBand - 1) * nRasterXSize) / 2;

    if( VSIFSeekL( poLAN_DS->fpImage, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "LAN Seek failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the profile.                                               */
/* -------------------------------------------------------------------- */
    if( VSIFReadL( pImage, 1, nRasterXSize/2, poLAN_DS->fpImage ) != 
        (size_t) nRasterXSize / 2 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "LAN Read failed:%s", VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Convert 4bit to 8bit.                                           */
/* -------------------------------------------------------------------- */
    int i;

    for( i = nRasterXSize-1; i >= 0; i-- )
    {
        if( (i & 0x01) != 0 )
            ((GByte *) pImage)[i] = ((GByte *) pImage)[i/2] & 0x0f;
        else
            ((GByte *) pImage)[i] = (((GByte *) pImage)[i/2] & 0xf0)/16;
    }

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorTable( GDALColorTable *poNewCT )

{
    if( poCT )
        delete poCT;
    if( poNewCT == NULL )
        poCT = NULL;
    else
        poCT = poNewCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *LAN4BitRasterBand::GetColorTable()

{
    if( poCT != NULL )
        return poCT;
    else
        return GDALPamRasterBand::GetColorTable();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr LAN4BitRasterBand::SetColorInterpretation( GDALColorInterp eNewInterp )

{
    eInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp LAN4BitRasterBand::GetColorInterpretation()

{
    return eInterp;
}

/************************************************************************/
/* ==================================================================== */
/*				LANDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             LANDataset()                             */
/************************************************************************/

LANDataset::LANDataset()
{
    fpImage = NULL;
    pszProjection = NULL;
}

/************************************************************************/
/*                            ~LANDataset()                             */
/************************************************************************/

LANDataset::~LANDataset()

{
    FlushCache();

    if( fpImage != NULL )
        VSIFCloseL( fpImage );

    CPLFree( pszProjection );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *LANDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header (.pcb) file.       */
/*      Does this appear to be a pcb file?                              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < ERD_HEADER_SIZE || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"HEADER",6)
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"HEAD74",6) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LANDataset 	*poDS;

    poDS = new LANDataset();

/* -------------------------------------------------------------------- */
/*      Adopt the openinfo file pointer for use with this file.         */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb+" );
    if( poDS->fpImage == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Do we need to byte swap the headers to local machine order?     */
/* -------------------------------------------------------------------- */
    int bBigEndian = poOpenInfo->pabyHeader[8] == 0;
    int bNeedSwap;

    memcpy( poDS->pachHeader, poOpenInfo->pabyHeader, ERD_HEADER_SIZE );

#ifdef CPL_LSB
    bNeedSwap = bBigEndian;
#else
    bNeedSwap = !bBigEndian;
#endif
        
    if( bNeedSwap )
    {
        CPL_SWAP16PTR( poDS->pachHeader + 6 );
        CPL_SWAP16PTR( poDS->pachHeader + 8 );

        CPL_SWAP32PTR( poDS->pachHeader + 16 );
        CPL_SWAP32PTR( poDS->pachHeader + 20 );
        CPL_SWAP32PTR( poDS->pachHeader + 24 );
        CPL_SWAP32PTR( poDS->pachHeader + 28 );

        CPL_SWAP16PTR( poDS->pachHeader + 88 );
        CPL_SWAP16PTR( poDS->pachHeader + 90 );

        CPL_SWAP16PTR( poDS->pachHeader + 106 );
        CPL_SWAP32PTR( poDS->pachHeader + 108 );
        CPL_SWAP32PTR( poDS->pachHeader + 112 );
        CPL_SWAP32PTR( poDS->pachHeader + 116 );
        CPL_SWAP32PTR( poDS->pachHeader + 120 );
        CPL_SWAP32PTR( poDS->pachHeader + 124 );
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int  nBandCount, nPixelOffset;
    GDALDataType eDataType;

    if( EQUALN(poDS->pachHeader,"HEADER",7) )
    {
        poDS->nRasterXSize = (int) *((float *) (poDS->pachHeader + 16));
        poDS->nRasterYSize = (int) *((float *) (poDS->pachHeader + 20));
    }
    else
    {
        poDS->nRasterXSize = *((GInt32 *) (poDS->pachHeader + 16));
        poDS->nRasterYSize = *((GInt32 *) (poDS->pachHeader + 20));
    }

    if( *((GInt16 *) (poDS->pachHeader + 6)) == 0 )
    {
        eDataType = GDT_Byte;
        nPixelOffset = 1;
    }
    else if( *((GInt16 *) (poDS->pachHeader + 6)) == 1 ) /* 4bit! */
    {
        eDataType = GDT_Byte;
        nPixelOffset = -1;
    }
    else if( *((GInt16 *) (poDS->pachHeader + 6)) == 2 )
    {
        nPixelOffset = 2;
        eDataType = GDT_Int16;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported pixel type (%d).", 
                  *((GInt16 *) (poDS->pachHeader + 6)) );
                  
        delete poDS;
        return NULL;
    }

    nBandCount = *((GInt16 *) (poDS->pachHeader + 8));

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= nBandCount; iBand++ )
    {
        if( nPixelOffset == -1 ) /* 4 bit case */
            poDS->SetBand( iBand, 
                           new LAN4BitRasterBand( poDS, iBand ) );
        else
            poDS->SetBand( 
                iBand, 
                new RawRasterBand( poDS, iBand, poDS->fpImage, 
                                   ERD_HEADER_SIZE + (iBand-1) 
                                   * nPixelOffset * poDS->nRasterXSize,
                                   nPixelOffset, 
                                   poDS->nRasterXSize*nPixelOffset*nBandCount,
                                   eDataType, !bNeedSwap, TRUE ));
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Try to interprete georeferencing.                               */
/* -------------------------------------------------------------------- */
    poDS->adfGeoTransform[0] = *((float *) (poDS->pachHeader + 112));
    poDS->adfGeoTransform[1] = *((float *) (poDS->pachHeader + 120));
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[3] = *((float *) (poDS->pachHeader + 116));
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = - *((float *) (poDS->pachHeader + 124));

    // adjust for center of pixel vs. top left corner of pixel.
    poDS->adfGeoTransform[0] -= poDS->adfGeoTransform[1] * 0.5;
    poDS->adfGeoTransform[3] -= poDS->adfGeoTransform[5] * 0.5;

/* -------------------------------------------------------------------- */
/*      If we didn't get any georeferencing, try for a worldfile.       */
/* -------------------------------------------------------------------- */
    if( poDS->adfGeoTransform[1] == 0.0
        || poDS->adfGeoTransform[5] == 0.0 )
    {
        if( !GDALReadWorldFile( poOpenInfo->pszFilename, NULL, 
                                poDS->adfGeoTransform ) )
            GDALReadWorldFile( poOpenInfo->pszFilename, ".wld", 
                               poDS->adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Try to come up with something for the coordinate system.        */
/* -------------------------------------------------------------------- */
    int nCoordSys = *((GInt16 *) (poDS->pachHeader + 88));

    if( nCoordSys == 0 )
    {
        poDS->pszProjection = CPLStrdup("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]");
            
    }
    else if( nCoordSys == 1 )
    {
        poDS->pszProjection = 
            CPLStrdup("LOCAL_CS[\"UTM - Zone Unknown\",UNIT[\"Meter\",1]]");
    }
    else if( nCoordSys == 2 )
    {
        poDS->pszProjection = CPLStrdup("LOCAL_CS[\"State Plane - Zone Unknown\",UNIT[\"US survey foot\",0.3048006096012192]]");
    }
    else 
    {
        poDS->pszProjection = 
            CPLStrdup("LOCAL_CS[\"Unknown\",UNIT[\"Meter\",1]]");
    }

/* -------------------------------------------------------------------- */
/*      Check for a trailer file with a colormap in it.                 */
/* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    char *pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));
    const char *pszTRLFilename = 
        CPLFormCIFilename( pszPath, pszBasename, "trl" );
    FILE *fpTRL;

    fpTRL = VSIFOpenL( pszTRLFilename, "rb" );
    if( fpTRL != NULL )
    {
        char szTRLData[896];
        int iColor;
        GDALColorTable *poCT;

        VSIFReadL( szTRLData, 1, 896, fpTRL );
        VSIFCloseL( fpTRL );
        
        poCT = new GDALColorTable();
        for( iColor = 0; iColor < 256; iColor++ )
        {
            GDALColorEntry sEntry;

            sEntry.c2 = ((GByte *) szTRLData)[iColor+128];
            sEntry.c1 = ((GByte *) szTRLData)[iColor+128+256];
            sEntry.c3 = ((GByte *) szTRLData)[iColor+128+512];
            sEntry.c4 = 255;
            poCT->SetColorEntry( iColor, &sEntry );

            // only 16 colors in 4bit files.
            if( nPixelOffset == -1 && iColor == 15 )
                break;
        }

        poDS->GetRasterBand(1)->SetColorTable( poCT );
        poDS->GetRasterBand(1)->SetColorInterpretation( GCI_PaletteIndex );
        
        delete poCT;
    }

    CPLFree( pszPath );
    CPLFree( pszBasename );

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::GetGeoTransform( double * padfTransform )

{
    if( adfGeoTransform[1] == 0.0 || adfGeoTransform[5] == 0.0 )
        return CE_Failure;
    else
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *LANDataset::GetProjectionRef()

{
    if( pszProjection == NULL )
        return "";
    else
        return pszProjection;
}

/************************************************************************/
/*                          GDALRegister_LAN()                          */
/************************************************************************/

void GDALRegister_LAN()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "LAN" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "LAN" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Erdas .LAN/.GIS" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#LAN" );
        
        poDriver->pfnOpen = LANDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

