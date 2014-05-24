/******************************************************************************
 * $Id$
 *
 * Project:  eCognition
 * Purpose:  Implementation of Erdas .LAN / .GIS format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_spatialref.h"

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

There is also some information on .STA and .TRL files at:

  http://www.pcigeomatics.com/cgi-bin/pcihlp/ERDASWR%7CTRAILER+FORMAT

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
    VSILFILE	*fpImage;	// image data file.
    
    char	pachHeader[ERD_HEADER_SIZE];

    char        *pszProjection;
    
    double      adfGeoTransform[6];

    CPLString   osSTAFilename;
    void        CheckForStatistics(void);

    virtual char **GetFileList();

  public:
    		LANDataset();
    	        ~LANDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef();
    virtual CPLErr SetProjection( const char * );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
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
    if( poOpenInfo->nHeaderBytes < ERD_HEADER_SIZE )
        return NULL;

    if( !EQUALN((const char *)poOpenInfo->pabyHeader,"HEADER",6)
        && !EQUALN((const char *)poOpenInfo->pabyHeader,"HEAD74",6) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    LANDataset 	*poDS;

    poDS = new LANDataset();

    poDS->eAccess = poOpenInfo->eAccess;

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

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBandCount, FALSE))
    {
        delete poDS;
        return NULL;
    }

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
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->CheckForStatistics();
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

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
        poDS->pszProjection = CPLStrdup(SRS_WKT_WGS84);
            
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
    VSILFILE *fpTRL;

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
    if( adfGeoTransform[1] != 0.0 && adfGeoTransform[5] != 0.0 )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr LANDataset::SetGeoTransform( double * padfTransform )

{
    unsigned char abyHeader[128];

    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFReadL( abyHeader, 128, 1, fpImage );

    // Upper Left X
    float f32Val;

    f32Val = (float) (adfGeoTransform[0] + 0.5 * adfGeoTransform[1]);
    memcpy( abyHeader + 112, &f32Val, 4 );
    
    // Upper Left Y
    f32Val = (float) (adfGeoTransform[3] + 0.5 * adfGeoTransform[5]);
    memcpy( abyHeader + 116, &f32Val, 4 );
    
    // width of pixel
    f32Val = (float) adfGeoTransform[1];
    memcpy( abyHeader + 120, &f32Val, 4 );
    
    // height of pixel
    f32Val = (float) fabs(adfGeoTransform[5]);
    memcpy( abyHeader + 124, &f32Val, 4 );

    if( VSIFSeekL( fpImage, 0, SEEK_SET ) != 0 
        || VSIFWriteL( abyHeader, 128, 1, fpImage ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "File IO Error writing header with new geotransform." );
        return CE_Failure;
    }
    else
        return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/*                                                                      */
/*      Use PAM coordinate system if available in preference to the     */
/*      generally poor value derived from the file itself.              */
/************************************************************************/

const char *LANDataset::GetProjectionRef()

{
    const char* pszPamPrj = GDALPamDataset::GetProjectionRef();

    if( pszProjection != NULL && strlen(pszPamPrj) == 0 )
        return pszProjection;
    else
        return pszPamPrj;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr LANDataset::SetProjection( const char * pszWKT )

{
    unsigned char abyHeader[128];

    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFReadL( abyHeader, 128, 1, fpImage );

    OGRSpatialReference oSRS( pszWKT );
    
    GUInt16 nProjCode = 0;

    if( oSRS.IsGeographic() )
        nProjCode = 0;

    else if( oSRS.GetUTMZone() != 0 )
        nProjCode = 1;

    // Too bad we have no way of recognising state plane projections. 

    else 
    {
        const char *pszProjection = oSRS.GetAttrValue("PROJECTION");

        if( pszProjection == NULL )
            ;
        else if( EQUAL(pszProjection,
                       SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
            nProjCode = 3;
        else if( EQUAL(pszProjection,
                       SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) )
            nProjCode = 4;
        else if( EQUAL(pszProjection,
                       SRS_PT_MERCATOR_1SP) )
            nProjCode = 5;
        else if( EQUAL(pszProjection,
                       SRS_PT_POLAR_STEREOGRAPHIC) )
            nProjCode = 6;
        else if( EQUAL(pszProjection,
                       SRS_PT_POLYCONIC) )
            nProjCode = 7;
        else if( EQUAL(pszProjection,
                       SRS_PT_EQUIDISTANT_CONIC) )
            nProjCode = 8;
        else if( EQUAL(pszProjection,
                       SRS_PT_TRANSVERSE_MERCATOR) )
            nProjCode = 9;
        else if( EQUAL(pszProjection,
                       SRS_PT_STEREOGRAPHIC) )
            nProjCode = 10;
        else if( EQUAL(pszProjection,
                       SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
            nProjCode = 11;
        else if( EQUAL(pszProjection,
                       SRS_PT_AZIMUTHAL_EQUIDISTANT) )
            nProjCode = 12;
        else if( EQUAL(pszProjection,
                       SRS_PT_GNOMONIC) )
            nProjCode = 13;
        else if( EQUAL(pszProjection,
                       SRS_PT_ORTHOGRAPHIC) )
            nProjCode = 14;
        // we don't have GVNP.
        else if( EQUAL(pszProjection,
                       SRS_PT_SINUSOIDAL) )
            nProjCode = 16;
        else if( EQUAL(pszProjection,
                       SRS_PT_EQUIRECTANGULAR) )
            nProjCode = 17;
        else if( EQUAL(pszProjection,
                       SRS_PT_MILLER_CYLINDRICAL) )
            nProjCode = 18;
        else if( EQUAL(pszProjection,
                       SRS_PT_VANDERGRINTEN) )
            nProjCode = 19;
        else if( EQUAL(pszProjection,
                       SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
            nProjCode = 20;
    }

    memcpy( abyHeader + 88, &nProjCode, 2 );

    VSIFSeekL( fpImage, 0, SEEK_SET );
    VSIFWriteL( abyHeader, 128, 1, fpImage );

    return GDALPamDataset::SetProjection( pszWKT );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **LANDataset::GetFileList()

{
    char **papszFileList = NULL;

    // Main data file, etc. 
    papszFileList = GDALPamDataset::GetFileList();

    if( strlen(osSTAFilename) > 0 )
        papszFileList = CSLAddString( papszFileList, osSTAFilename );

    return papszFileList;
}

/************************************************************************/
/*                         CheckForStatistics()                         */
/************************************************************************/

void LANDataset::CheckForStatistics()

{
/* -------------------------------------------------------------------- */
/*      Do we have a statistics file?                                   */
/* -------------------------------------------------------------------- */
    osSTAFilename = CPLResetExtension(GetDescription(),"sta");

    VSILFILE *fpSTA = VSIFOpenL( osSTAFilename, "r" );

    if( fpSTA == NULL && VSIIsCaseSensitiveFS(osSTAFilename) )
    {
        osSTAFilename = CPLResetExtension(GetDescription(),"STA");
        fpSTA = VSIFOpenL( osSTAFilename, "r" );
    }

    if( fpSTA == NULL )
    {
        osSTAFilename = "";
        return;
    }

/* -------------------------------------------------------------------- */
/*      Read it one band at a time.                                     */
/* -------------------------------------------------------------------- */
    GByte abyBandInfo[1152];
    int iBand;

    for( iBand = 0; iBand < nBands; iBand++ )
    {
        if( VSIFReadL( abyBandInfo, 1152, 1, fpSTA ) != 1 )
            break;

        int nBandNumber = abyBandInfo[7];
        GDALRasterBand *poBand = GetRasterBand(nBandNumber);
        if( poBand == NULL )
            break;

        float fMean, fStdDev;
        GInt16 nMin, nMax;

        if( poBand->GetRasterDataType() != GDT_Byte )
        {
            memcpy( &nMin, abyBandInfo + 28, 2 );
            memcpy( &nMax, abyBandInfo + 30, 2 );
            CPL_LSBPTR16( &nMin );
            CPL_LSBPTR16( &nMax );
        }
        else
        {
            nMin = abyBandInfo[9];
            nMax = abyBandInfo[8];
        }
        
        memcpy( &fMean, abyBandInfo + 12, 4 );
        memcpy( &fStdDev, abyBandInfo + 24, 4 );
        CPL_LSBPTR32( &fMean );
        CPL_LSBPTR32( &fStdDev );
        
        poBand->SetStatistics( nMin, nMax, fMean, fStdDev );
    }
    
    VSIFCloseL( fpSTA );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *LANDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** papszOptions )

{
    if( eType != GDT_Byte && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create .GIS file with unsupported data type '%s'.",
                  GDALGetDataTypeName( eType ) );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE	*fp;

    fp = VSIFOpenL( pszFilename, "wb" );

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
    unsigned char abyHeader[128];
    GInt16  n16Val;
    GInt32  n32Val;

    memset( abyHeader, 0, sizeof(abyHeader) );
    
    memcpy( abyHeader + 0, "HEAD74", 6 );

    // Pixel type
    if( eType == GDT_Byte ) // do we want 4bit?
        n16Val = 0;
    else
        n16Val = 2;
    memcpy( abyHeader + 6, &n16Val, 2 );

    // Number of Bands.
    n16Val = (GInt16) nBands;
    memcpy( abyHeader + 8, &n16Val, 2 );

    // Unknown (6)

    // Width
    n32Val = nXSize;
    memcpy( abyHeader + 16, &n32Val, 4 );
    
    // Height
    n32Val = nYSize;
    memcpy( abyHeader + 20, &n32Val, 4 );

    // X Start (4)
    // Y Start (4)

    // Unknown (56)

    // Coordinate System
    n16Val = 0;
    memcpy( abyHeader + 88, &n16Val, 2 );

    // Classes in coverage 
    n16Val = 0;
    memcpy( abyHeader + 90, &n16Val, 2 );

    // Unknown (14)

    // Area Unit
    n16Val = 0;
    memcpy( abyHeader + 106, &n16Val, 2 );

    // Pixel Area
    float f32Val;

    f32Val = 0.0f;
    memcpy( abyHeader + 108, &f32Val, 4 );

    // Upper Left X
    f32Val = 0.5f;
    memcpy( abyHeader + 112, &f32Val, 4 );
    
    // Upper Left Y
    f32Val = (float) (nYSize - 0.5);
    memcpy( abyHeader + 116, &f32Val, 4 );
    
    // width of pixel
    f32Val = 1.0f;
    memcpy( abyHeader + 120, &f32Val, 4 );
    
    // height of pixel
    f32Val = 1.0f;
    memcpy( abyHeader + 124, &f32Val, 4 );

    VSIFWriteL( abyHeader, sizeof(abyHeader), 1, fp );

/* -------------------------------------------------------------------- */
/*      Extend the file to the target size.                             */
/* -------------------------------------------------------------------- */
    vsi_l_offset nImageBytes;

    if( eType != GDT_Byte )
        nImageBytes = nXSize * (vsi_l_offset) nYSize * 2;
    else
        nImageBytes = nXSize * (vsi_l_offset) nYSize;

    memset( abyHeader, 0, sizeof(abyHeader) );
    
    while( nImageBytes > 0 )
    {
        vsi_l_offset nWriteThisTime = MIN(nImageBytes,sizeof(abyHeader));
        
        if( VSIFWriteL( abyHeader, 1, (size_t)nWriteThisTime, fp ) 
            != nWriteThisTime )
        {
            VSIFCloseL( fp );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to write whole Istar file." );
            return NULL;
        }
        nImageBytes -= nWriteThisTime;
    }

    VSIFCloseL( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
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
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Erdas .LAN/.GIS" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#LAN" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16" );
        
        poDriver->pfnOpen = LANDataset::Open;
        poDriver->pfnCreate = LANDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

