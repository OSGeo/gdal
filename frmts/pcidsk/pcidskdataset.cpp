/******************************************************************************
 * $Id$
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.3  2003/09/17 22:20:48  gwalter
 * Added CreateCopy() function.
 *
 * Revision 1.2  2003/09/11 20:07:36  warmerda
 * avoid casting warning
 *
 * Revision 1.1  2003/09/09 08:31:28  dron
 * New.
 *
 */

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_PCIDSK(void);
CPL_C_END

typedef enum
{
    PDI_PIXEL,
    PDI_BAND,
    PDI_FILE
} PCIDSKInterleaving;

/************************************************************************/
/* ==================================================================== */
/*                              PCIDSKDataset                           */
/* ==================================================================== */
/************************************************************************/

class PCIDSKDataset : public RawDataset
{
    friend class PCIDSKRasterBand;

    const char          *pszFilename;
    FILE                *fp;

    struct tm           *poCreatTime;   // Date/time of the database creation

    vsi_l_offset        nGeoPtrOffset;  // Offset in bytes to the pointer
                                        // to GEO segment
    vsi_l_offset        nGeoOffset;     // Offset in bytes to the GEO segment

    double              adfGeoTransform[6];
    char                *pszProjection;

    GDALDataType        PCIDSKTypeToGDAL( const char *);
    void                WriteGeoSegment();

  public:
                PCIDSKDataset();
                ~PCIDSKDataset();

    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char **papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );

    virtual void        FlushCache( void );

    CPLErr              GetGeoTransform( double * padfTransform );
    virtual CPLErr      SetGeoTransform( double * );
    const char          *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
};

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSKRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class PCIDSKRasterBand : public GDALRasterBand
{
    friend class PCIDSKDataset;

  public:

                PCIDSKRasterBand( PCIDSKDataset *, int, GDALDataType );
                ~PCIDSKRasterBand();
};

/************************************************************************/
/*                           PCIDSKRasterBand()                         */
/************************************************************************/

PCIDSKRasterBand::PCIDSKRasterBand( PCIDSKDataset *poDS, int nBand,
                                    GDALDataType eType )
{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = eType;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           ~PCIDSKRasterBand()                        */
/************************************************************************/

PCIDSKRasterBand::~PCIDSKRasterBand()
{
}

/************************************************************************/
/*                           PCIDSKDataset()                            */
/************************************************************************/

PCIDSKDataset::PCIDSKDataset()
{
    pszFilename = NULL;
    fp = NULL;
    nBands = 0;
    poCreatTime = NULL;
    nGeoOffset = 0;
    pszProjection = CPLStrdup( "" );
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~BMPDataset()                             */
/************************************************************************/

PCIDSKDataset::~PCIDSKDataset()
{
    FlushCache();

    if ( pszProjection )
        CPLFree( pszProjection );
    if( fp != NULL )
        VSIFCloseL( fp );
    if( poCreatTime )
        delete poCreatTime;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSKDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(adfGeoTransform[0]) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSKDataset::SetGeoTransform( double * padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PCIDSKDataset::GetProjectionRef()
{
    if( pszProjection )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr PCIDSKDataset::SetProjection( const char *pszNewProjection )

{
    if ( pszProjection )
	CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void PCIDSKDataset::FlushCache()
{
    char        szTemp[64];

    GDALDataset::FlushCache();

/* -------------------------------------------------------------------- */
/*      Write out pixel size.                                           */
/* -------------------------------------------------------------------- */
    CPLPrintDouble( szTemp, "%16.9E", ABS(adfGeoTransform[1]) );
    CPLPrintDouble( szTemp + 16, "%16.9E", ABS(adfGeoTransform[5]) );

    VSIFSeekL( fp, 408, SEEK_SET );
    VSIFWriteL( (void *)szTemp, 1, 32, fp );

/* -------------------------------------------------------------------- */
/*      Write out Georeferencing segment.                               */
/* -------------------------------------------------------------------- */
    if ( nGeoOffset )
        WriteGeoSegment();
}

/************************************************************************/
/*                         WriteGeoSegment()                            */
/************************************************************************/

void PCIDSKDataset::WriteGeoSegment( )
{
    char            szTemp[3072];
    struct tm       oUpdateTime;
    time_t          nTime = VSITime(NULL);
    char            *pszP = pszProjection;
    OGRSpatialReference oSRS;
    int             i;

    VSILocalTime( &nTime, &oUpdateTime );

    CPLPrintStringFill( szTemp, "Master Georeferencing Segment for File", 64 );
    CPLPrintStringFill( szTemp + 64, "", 64 );
    // FIXME: should use poCreatTime
    CPLPrintTime( szTemp + 128, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    CPLPrintTime( szTemp + 144, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    CPLPrintStringFill( szTemp + 160, "", 64 );
    // Write the history line
    CPLPrintStringFill( szTemp + 384,
                        "GDAL: Master Georeferencing Segment for File", 64 );
    CPLPrintTime( szTemp + 448, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    // Fill other lines with spaces
    CPLPrintStringFill( szTemp + 464, "", 80 * 7 );

    // More history lines may be used if needed.
    // CPLPrintStringFill( szTemp + 464, "History2", 80 );
    // CPLPrintStringFill( szTemp + 544, "History3", 80 );
    // CPLPrintStringFill( szTemp + 624, "History4", 80 );
    // CPLPrintStringFill( szTemp + 704, "History5", 80 );
    // CPLPrintStringFill( szTemp + 784, "History6", 80 );
    // CPLPrintStringFill( szTemp + 864, "History7", 80 );
    // CPLPrintStringFill( szTemp + 944, "History8", 80 );

    VSIFSeekL( fp, nGeoOffset, SEEK_SET );
    VSIFWriteL( (void *)szTemp, 1, 1024, fp );

    CPLPrintStringFill( szTemp, "PROJECTION", 16 );
    CPLPrintStringFill( szTemp + 16, "PIXEL", 16 );

    if( pszProjection != NULL && !EQUAL( pszProjection, "" )
        && oSRS.importFromWkt( &pszP ) == OGRERR_NONE )
    {
        char      *pszProj = NULL;
        char      *pszUnits = NULL;
        double    *padfPrjParms = NULL;

        oSRS.exportToPCI( &pszProj, &pszUnits, &padfPrjParms );
        CPLPrintStringFill( szTemp + 32, pszProj, 16 );

        CPLPrintInt32( szTemp + 48, 3, 8 );
        CPLPrintInt32( szTemp + 56, 3, 8 );

        CPLPrintStringFill( szTemp + 64, pszUnits, 16 );

        for ( i = 0; i < 16; i++ )
        {
            CPLPrintDouble( szTemp + 80 + 26 * i,
                            "%26.18E", padfPrjParms[i] );
        }

        CPLPrintStringFill( szTemp + 522, "", 936 );

        if ( pszProj )
            CPLFree( pszProj );
        if ( pszUnits )
            CPLFree(pszUnits );
        if ( padfPrjParms )
            CPLFree( padfPrjParms );
    }
    else
    {
        CPLPrintStringFill( szTemp + 32, "PIXEL", 16 );
        CPLPrintInt32( szTemp + 48, 3, 8 );
        CPLPrintInt32( szTemp + 56, 3, 8 );
        CPLPrintStringFill( szTemp + 64, "METER", 16 );
        CPLPrintStringFill( szTemp + 80, "", 1378 );
    }

    /* TODO: USGS format */
    CPLPrintStringFill( szTemp + 1458, "", 1614 );

    for ( i = 0; i < 3; i++ )
    {
        CPLPrintDouble( szTemp + 1980 + 26 * i,
                        "%26.18E", adfGeoTransform[i] );
    }
    for ( i = 0; i < 3; i++ )
    {
        CPLPrintDouble( szTemp + 2526 + 26 * i,
                        "%26.18E", adfGeoTransform[i + 3] );
    }

    VSIFWriteL( (void *)szTemp, 1, 3072, fp );

    // Now make the segment active
    szTemp[0] = 'A';
    VSIFSeekL( fp, nGeoPtrOffset, SEEK_SET );
    VSIFWriteL( (void *)szTemp, 1, 1, fp );
}

/************************************************************************/
/*                         PCIDSKTypeToGDAL()                           */
/************************************************************************/

GDALDataType PCIDSKDataset::PCIDSKTypeToGDAL( const char *pszType )
{
    if ( EQUAL( pszType, "8U      ") )
        return GDT_Byte;
    if ( EQUAL( pszType, "16S     ") )
        return GDT_Int16;
    if ( EQUAL( pszType, "16U     ") )
        return GDT_UInt16;
    if ( EQUAL( pszType, "32R     ") )
        return GDT_Float32;

    return GDT_Unknown;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PCIDSKDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "PCIDSK  ", 8) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PCIDSKDataset   *poDS;

    poDS = new PCIDSKDataset();

    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );
    if ( !poDS->fp )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to re-open %s within PCIDSK driver.\n",
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* ==================================================================== */
/*   Read PCIDSK File Header.                                           */
/* ==================================================================== */
    char            szTemp[512];

/* -------------------------------------------------------------------- */
/*      Read File Identification.                                       */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 8, SEEK_SET );
    VSIFReadL( szTemp, 1, 8, poDS->fp );
    szTemp[8] = '\0';
    poDS->SetMetadataItem( "SOFTWARE",  szTemp );

    VSIFSeekL( poDS->fp, 48, SEEK_SET );
    VSIFReadL( szTemp, 1, 64, poDS->fp );
    szTemp[64] = '\0';
    poDS->SetMetadataItem( "FILE_ID",  szTemp );

    VSIFSeekL( poDS->fp, 112, SEEK_SET );
    VSIFReadL( szTemp, 1, 32, poDS->fp );
    szTemp[32] = '\0';
    poDS->SetMetadataItem( "GENERATING_FACILITY",  szTemp );

    VSIFSeekL( poDS->fp, 144, SEEK_SET );
    VSIFReadL( szTemp, 1, 64, poDS->fp );
    szTemp[64] = '\0';
    poDS->SetMetadataItem( "DESCRIPTION1",  szTemp );

    VSIFSeekL( poDS->fp, 208, SEEK_SET );
    VSIFReadL( szTemp, 1, 64, poDS->fp );
    szTemp[64] = '\0';
    poDS->SetMetadataItem( "DESCRIPTION2",  szTemp );

    VSIFSeekL( poDS->fp, 272, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    szTemp[16] = '\0';
    poDS->SetMetadataItem( "DATE_OF_CREATION",  szTemp );

    VSIFSeekL( poDS->fp, 288, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    szTemp[16] = '\0';
    poDS->SetMetadataItem( "DATE_OF_UPDATE",  szTemp );

/* -------------------------------------------------------------------- */
/*      Read Image Data.                                                */
/* -------------------------------------------------------------------- */
    vsi_l_offset    nImageStart;        // Start block of image data
    vsi_l_offset    nImgHdrsStart;      // Start block of image headers
    vsi_l_offset    nImageOffset;       // Offset to the first byte of the image
    int             nByteBands, nInt16Bands, nUInt16Bands, nFloat32Bands;

    VSIFSeekL( poDS->fp, 304, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    szTemp[16] = '\0';
    if ( !EQUAL( szTemp, "                " ) )
        nImageStart = atol( szTemp );       // XXX: should be atoll()
    else
        nImageStart = 0;
    nImageOffset = (nImageStart - 1) * 512;

    VSIFSeekL( poDS->fp, 336, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    szTemp[16] = '\0';
    nImgHdrsStart = atol( szTemp );     // XXX: should be atoll()

    VSIFSeekL( poDS->fp, 376, SEEK_SET );
    VSIFReadL( szTemp, 1, 24, poDS->fp );
    poDS->nBands = CPLScanLong( szTemp, 8 );
    poDS->nRasterXSize = CPLScanLong( szTemp + 8, 8 );
    poDS->nRasterYSize = CPLScanLong( szTemp + 16, 8 );

    VSIFSeekL( poDS->fp, 464, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    nByteBands = CPLScanLong( szTemp, 4 );
    nInt16Bands = CPLScanLong( szTemp + 4, 4 );
    nUInt16Bands = CPLScanLong( szTemp + 8, 4 );
    nFloat32Bands = CPLScanLong( szTemp + 12, 4 );

    // If these fields are blank, then it is assumed that all channels are 8-bit
    if ( nByteBands == 0 && nInt16Bands == 0
         && nUInt16Bands == 0 && nFloat32Bands == 0 )
        nByteBands = poDS->nBands;

/* ==================================================================== */
/*   Read Image Headers and create band information objects.            */
/* ==================================================================== */
    int             iBand;
    PCIDSKInterleaving eInterleaving;

/* -------------------------------------------------------------------- */
/*      Read type of interleaving and set up image parameters.          */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 360, SEEK_SET );
    VSIFReadL( szTemp, 1, 8, poDS->fp );
    szTemp[8] = '\0';
    if ( EQUALN( szTemp, "PIXEL", 5 ) )
        eInterleaving = PDI_PIXEL;
    else if ( EQUALN( szTemp, "BAND", 4 ) )
        eInterleaving = PDI_BAND;
    else if ( EQUALN( szTemp, "FILE", 4 ) )
        eInterleaving = PDI_FILE;
    else
    {
        CPLDebug( "PCIDSK",
                  "PCIDSK interleaving type %s is not supported by GDAL",
                  szTemp );
        delete poDS;
        return NULL;
    }

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        GDALDataType    eType;
        GDALRasterBand  *poBand;
        vsi_l_offset    nImgHdrOffset = (nImgHdrsStart - 1 + iBand * 2) * 512;
        vsi_l_offset    nPixelOffset = 0, nLineOffset = 0, nLineSize = 0;
        int             bNativeOrder;

        VSIFSeekL( poDS->fp, nImgHdrOffset + 160, SEEK_SET );
        VSIFReadL( szTemp, 1, 8, poDS->fp );
        szTemp[8] = '\0';
        eType = poDS->PCIDSKTypeToGDAL( szTemp );
        if ( eType == GDT_Unknown )
        {
            CPLDebug( "PCIDSK",
                      "PCIDSK data type %s is not supported by GDAL", szTemp );
            delete poDS;
            return NULL;
        }

        switch ( eInterleaving )
        {
            case PDI_PIXEL:
                nPixelOffset = nByteBands + 2 * (nInt16Bands + nUInt16Bands)
                               + 4 * nFloat32Bands;
                nLineSize = nPixelOffset * poDS->nRasterXSize;
                nLineOffset = ((int)((nLineSize + 511)/512)) * 512;
                break;
            case PDI_BAND:
                nPixelOffset = GDALGetDataTypeSize( eType ) / 8;
                nLineOffset = nPixelOffset * poDS->nRasterXSize;
                break;
            case PDI_FILE:
                // Read the filename
                VSIFSeekL( poDS->fp, nImgHdrOffset + 64, SEEK_SET );
                VSIFReadL( szTemp, 1, 64, poDS->fp );
                szTemp[64] = '\0';

                // Empty filename means we have data stored inside PCIDSK file
                if ( EQUAL( szTemp, "                                "
                                    "                                " ) )
                {
                    VSIFSeekL( poDS->fp, nImgHdrOffset + 168, SEEK_SET );
                    VSIFReadL( szTemp, 1, 16, poDS->fp );
                    szTemp[16] = '\0';
                    nImageOffset = atol( szTemp ); // XXX: should be atoll()

                    VSIFSeekL( poDS->fp, nImgHdrOffset + 184, SEEK_SET );
                    VSIFReadL( szTemp, 1, 16, poDS->fp );
                    nPixelOffset = CPLScanLong( szTemp, 8 );
                    nLineOffset = CPLScanLong( szTemp + 8, 8 );
                }
                else
                    iBand--;
                    poDS->nBands--;
                    continue;
                break;
            default: /* NOTREACHED */
                break;
        }

        VSIFSeekL( poDS->fp, nImgHdrOffset + 201, SEEK_SET );
        VSIFReadL( szTemp, 1, 1, poDS->fp );
        szTemp[1] = '\0';
#ifdef CPL_MSB
        bNativeOrder = ( szTemp[0] == 'S')?FALSE:TRUE;
#else
        bNativeOrder = ( szTemp[0] == 'S')?TRUE:FALSE;
#endif

        poBand = new RawRasterBand( poDS, iBand + 1, poDS->fp,
                                    nImageOffset, (int) nPixelOffset, 
                                    (int) nLineOffset, 
                                    eType, bNativeOrder, TRUE);
        poDS->SetBand( iBand + 1, poBand );

/* -------------------------------------------------------------------- */
/*      Read and assign few metadata parameters to each image band.     */
/* -------------------------------------------------------------------- */
        VSIFSeekL( poDS->fp, nImgHdrOffset, SEEK_SET );
        VSIFReadL( szTemp, 1, 64, poDS->fp );
        szTemp[64] = '\0';
        poBand->SetDescription( szTemp );

        VSIFSeekL( poDS->fp, nImgHdrOffset + 128, SEEK_SET );
        VSIFReadL( szTemp, 1, 16, poDS->fp );
        szTemp[16] = '\0';
        poBand->SetMetadataItem( "DATE_OF_CREATION",  szTemp );

        VSIFSeekL( poDS->fp, nImgHdrOffset + 144, SEEK_SET );
        VSIFReadL( szTemp, 1, 16, poDS->fp );
        szTemp[16] = '\0';
        poBand->SetMetadataItem( "DATE_OF_UPDATE",  szTemp );

        VSIFSeekL( poDS->fp, nImgHdrOffset + 202, SEEK_SET );
        VSIFReadL( szTemp, 1, 16, poDS->fp );
        szTemp[16] = '\0';
        if ( !EQUAL( szTemp, "                " ) )
            poBand->SetMetadataItem( "UNITS",  szTemp );

        int         i;
        for ( i = 0; i < 8; i++ )
        {
            VSIFSeekL( poDS->fp, nImgHdrOffset + 384 + i * 80, SEEK_SET );
            VSIFReadL( szTemp, 1, 80, poDS->fp );
            szTemp[80] = '\0';
            if ( !EQUAL( szTemp, "                                "
                                 "                                "
                                 "                " ) )
                poBand->SetMetadataItem(CPLSPrintf("HISTORY%d", i + 1), szTemp);
            
        }

        switch ( eInterleaving )
        {
            case PDI_PIXEL:
                nImageOffset += GDALGetDataTypeSize( eType ) / 8;
                break;
            case  PDI_BAND:
                nImageOffset += nLineOffset * poDS->nRasterYSize;
                break;
            default:
                break;
        }
    }
   
/* ==================================================================== */
/*   Read Segment Pointers.                                             */
/* ==================================================================== */
    vsi_l_offset    nSegPointersStart;  // Start block of Segment Pointers
    vsi_l_offset    nSegPointersOffset; // Offset in bytes to Pointers
    int             nSegBlocks;         // Number of blocks of Segment Pointers
    int             nSegments;          // Max number of segments in the file

    VSIFSeekL( poDS->fp, 440, SEEK_SET );
    VSIFReadL( szTemp, 1, 16, poDS->fp );
    szTemp[16] = '\0';
    nSegPointersStart = atol( szTemp ); // XXX: should be atoll()
    nSegPointersOffset = ( nSegPointersStart - 1 ) * 512;

    VSIFSeekL( poDS->fp, 456, SEEK_SET );
    VSIFReadL( szTemp, 1, 8, poDS->fp );
    nSegBlocks = CPLScanLong( szTemp, 8 );
    nSegments = ( nSegBlocks * 512 ) / 32;

/* -------------------------------------------------------------------- */
/*      Search for Georeferencing Segment.                              */
/* -------------------------------------------------------------------- */
    int             i;
    
    for ( i = 0; i < nSegments; i++ )
    {
        int     bActive;

        VSIFSeekL( poDS->fp, nSegPointersOffset + i * 32, SEEK_SET );
        VSIFReadL( szTemp, 1, 23, poDS->fp );
        szTemp[23] = '\0';

        if ( szTemp[0] == 'A' || szTemp[0] == 'L' )
            bActive = TRUE;
        else
            bActive = FALSE;

        switch ( CPLScanLong( szTemp + 1, 3 ) )
        {
            case 150:                   // GEO segment
            {
                vsi_l_offset    nGeoStart, nGeoDataOffset;
                int             j, nXCoeffs, nYCoeffs;
                OGRSpatialReference oSRS;

                poDS->nGeoPtrOffset = nSegPointersOffset + i * 32;
                nGeoStart = atol( szTemp + 12 ); // XXX: should be atoll()
                poDS->nGeoOffset = ( nGeoStart - 1 ) * 512;
                nGeoDataOffset = poDS->nGeoOffset + 1024;

                if ( bActive )
                {
                    VSIFSeekL( poDS->fp, nGeoDataOffset, SEEK_SET );
                    VSIFReadL( szTemp, 1, 16, poDS->fp );
                    szTemp[16] = '\0';
                    if ( EQUALN( szTemp, "POLYNOMIAL", 10 ) )
                    {
                        char        szProj[17];

                        // Read projection definition
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 32, SEEK_SET );
                        VSIFReadL( szProj, 1, 16, poDS->fp );
                        szProj[16] = '\0';
                        if ( EQUALN( szProj, "PIXEL", 5 )
                             || EQUALN( szProj, "METRE", 5 ) )
                            break;

                        // Read number of transform coefficients
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 48, SEEK_SET );
                        VSIFReadL( szTemp, 1, 16, poDS->fp );
                        nXCoeffs = CPLScanLong( szTemp, 8 );
                        if ( nXCoeffs > 3 )
                            nXCoeffs = 3;
                        nYCoeffs = CPLScanLong( szTemp + 8, 8 );
                        if ( nYCoeffs > 3 )
                            nYCoeffs = 3;

                        // Read geotransform coefficients
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 212, SEEK_SET );
                        VSIFReadL( szTemp, 1, nXCoeffs * 26, poDS->fp );
                        for ( j = 0; j < nXCoeffs; j++ )
                        {
                            poDS->adfGeoTransform[j] =
                                CPLScanDouble( szTemp + 26 * j, 26 );;
                        }
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 1642, SEEK_SET );
                        VSIFReadL( szTemp, 1, nYCoeffs * 26, poDS->fp );
                        for ( j = 0; j < nYCoeffs; j++ )
                        {
                            poDS->adfGeoTransform[j + 3] =
                                CPLScanDouble( szTemp + 26 * j, 26 );
                        }

                        oSRS.importFromPCI( szProj, NULL, NULL );
                        if ( poDS->pszProjection )
                            CPLFree( poDS->pszProjection );
                        oSRS.exportToWkt( &poDS->pszProjection );
                    }
                    else if ( EQUALN( szTemp, "PROJECTION", 10 ) )
                    {
                        char        szProj[17], szUnits[17];
                        double      adfProjParms[16];

                        // Read projection definition
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 32, SEEK_SET );
                        VSIFReadL( szProj, 1, 16, poDS->fp );
                        szProj[16] = '\0';
                        if ( EQUALN( szProj, "PIXEL", 5 )
                             || EQUALN( szProj, "METRE", 5 ) )
                            break;

                        // Read number of transform coefficients
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 48, SEEK_SET );
                        VSIFReadL( szTemp, 1, 16, poDS->fp );
                        nXCoeffs = CPLScanLong( szTemp, 8 );
                        if ( nXCoeffs > 3 )
                            nXCoeffs = 3;
                        nYCoeffs = CPLScanLong( szTemp + 8, 8 );
                        if ( nYCoeffs > 3 )
                            nYCoeffs = 3;

                        // Read grid units definition
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 64, SEEK_SET );
                        VSIFReadL( szUnits, 1, 16, poDS->fp );
                        szUnits[16] = '\0';

                        // Read 16 projection parameters
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 80, SEEK_SET );
                        VSIFReadL( szTemp, 1, 26 * 16, poDS->fp );
                        for ( j = 0; j < 16; j++ )
                        {
                            adfProjParms[j] =
                                CPLScanDouble( szTemp + 26 * j, 26 );
                        }

                        // Read geotransform coefficients
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 1980, SEEK_SET );
                        VSIFReadL( szTemp, 1, nXCoeffs * 26, poDS->fp );
                        for ( j = 0; j < nXCoeffs; j++ )
                        {
                            poDS->adfGeoTransform[j] =
                                CPLScanDouble( szTemp + 26 * j, 26 );;
                        }
                        VSIFSeekL( poDS->fp, nGeoDataOffset + 2526, SEEK_SET );
                        VSIFReadL( szTemp, 1, nYCoeffs * 26, poDS->fp );
                        for ( j = 0; j < nYCoeffs; j++ )
                        {
                            poDS->adfGeoTransform[j + 3] =
                                CPLScanDouble( szTemp + 26 * j, 26 );
                        }

                        oSRS.importFromPCI( szProj, szUnits, adfProjParms );
                        if ( poDS->pszProjection )
                            CPLFree( poDS->pszProjection );
                        oSRS.exportToWkt( &poDS->pszProjection );
                    }
                }
            }
            break;

            default:
            break;
        }
    }

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PCIDSKDataset::Create( const char * pszFilename,
                                    int nXSize, int nYSize, int nBands,
                                    GDALDataType eType, char **papszOptions )

{
    if ( eType != GDT_Byte
         && eType != GDT_Int16
         && eType != GDT_UInt16
         && eType != GDT_Float32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create PCIDSK dataset with an illegal data type (%s),\n"
              "only Byte, Int16, UInt16 and Float32 supported by the format.\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE        *fp = VSIFOpenL( pszFilename, "wb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create file %s.\n", pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get current time to fill appropriate fields.                    */
/* -------------------------------------------------------------------- */
    struct tm       oUpdateTime;
    time_t          nTime = VSITime(NULL);

    VSILocalTime( &nTime, &oUpdateTime );

/* ==================================================================== */
/*      Fill the PCIDSK File Header.                                    */
/* ==================================================================== */
    const char      *pszDesc;
    char            szTemp[1024];
    vsi_l_offset    nImageStart;        // Start block of image data
    vsi_l_offset    nSegPointersStart;  // Start block of Segment Pointers
    vsi_l_offset    nImageBlocks;       // Number of blocks of image data
    int             nImgHdrBlocks;      // Number of blocks of image header data
    const int       nSegBlocks = 64;    // Number of blocks of Segment Pointers
    const int       nGeoSegBlocks = 8;  // Number of blocks in GEO Segment

/* -------------------------------------------------------------------- */
/*      Calculate offsets.                                              */
/* -------------------------------------------------------------------- */
    nImgHdrBlocks = nBands * 2;
    nSegPointersStart = 2 + nImgHdrBlocks;
    nImageStart = nSegPointersStart + nSegBlocks;
    nImageBlocks = (vsi_l_offset)
        (nXSize * nYSize * nBands * GDALGetDataTypeSize(eType) / 8 + 512) / 512;
 
/* -------------------------------------------------------------------- */
/*      Fill the File Identification.                                   */
/* -------------------------------------------------------------------- */
    CPLPrintStringFill( szTemp, "PCIDSK  ", 8 );
    CPLPrintStringFill( szTemp + 8, "GDAL", 4 );
    CPLPrintStringFill( szTemp + 12, GDALVersionInfo( "VERSION_NUM" ), 4 );
    CPLPrintUIntBig( szTemp + 16,
                  nImageStart + nImageBlocks + nGeoSegBlocks - 1, 16 );
    CPLPrintStringFill( szTemp + 32, "", 16 );
    CPLPrintStringFill( szTemp + 48, CPLGetFilename(pszFilename), 64 );
    CPLPrintStringFill( szTemp + 112, "Created with GDAL", 32 );

    pszDesc = CSLFetchNameValue( papszOptions, "FILEDESC1" );
    if ( !pszDesc )
        pszDesc = "";
    CPLPrintStringFill( szTemp + 144, pszDesc, 64 );

    pszDesc = CSLFetchNameValue( papszOptions, "FILEDESC2" );
    if ( !pszDesc )
        pszDesc = "";
    CPLPrintStringFill( szTemp + 208, pszDesc, 64 );

    CPLPrintTime( szTemp + 272, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    CPLPrintTime( szTemp + 288, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );

/* -------------------------------------------------------------------- */
/*      Fill the Image Data and Segment Pointers.                       */
/* -------------------------------------------------------------------- */
    CPLPrintUIntBig( szTemp + 304, nImageStart, 16 );
    CPLPrintUIntBig( szTemp + 320, nImageBlocks, 16 );
    sprintf( szTemp + 336, "%16d", 2 );
    sprintf( szTemp + 352, "%8d", nImgHdrBlocks );
    CPLPrintStringFill( szTemp + 360, "BAND", 8 );
    CPLPrintStringFill( szTemp + 368, "", 8 );
    sprintf( szTemp + 376, "%8d", nBands );
    sprintf( szTemp + 384, "%8d", nXSize );
    sprintf( szTemp + 392, "%8d", nYSize );
    CPLPrintStringFill( szTemp + 400, "METRE", 8 );
    // Two following parameters will be filled in FlushCache()
    CPLPrintStringFill( szTemp + 408, "", 16 );    // X size of pixel
    CPLPrintStringFill( szTemp + 424, "", 16 );    // Y size of pixel

    CPLPrintUIntBig( szTemp + 440, nSegPointersStart, 16 );
    sprintf( szTemp + 456, "%8d", nSegBlocks );
    if ( eType == GDT_Byte )
        sprintf( szTemp + 464, "%4d", nBands );
    else
        CPLPrintStringFill( szTemp + 464, "", 4 );
    if ( eType == GDT_Int16 )
        sprintf( szTemp + 468, "%4d", nBands );
    else
        CPLPrintStringFill( szTemp + 468, "", 4 );
    if ( eType == GDT_UInt16 )
        sprintf( szTemp + 472, "%4d", nBands );
    else
        CPLPrintStringFill( szTemp + 472, "", 4 );
    if ( eType == GDT_Float32 )
        sprintf( szTemp + 476, "%4d", nBands );
    else
        CPLPrintStringFill( szTemp + 476, "", 4 );
    CPLPrintStringFill( szTemp + 480, "", 32 );
    
    VSIFSeekL( fp, 0, SEEK_SET );
    VSIFWriteL( (void *)szTemp, 1, 512, fp );

/* ==================================================================== */
/*      Fill the Image Headers.                                         */
/* ==================================================================== */
    int         i;

    for ( i = 0; i < nBands; i++ )
    {
        pszDesc =
            CSLFetchNameValue( papszOptions, CPLSPrintf("BANDDESC%d", i + 1) );

        if ( !pszDesc )
            pszDesc = CPLSPrintf( "Image band %d", i + 1 );

        CPLPrintStringFill( szTemp, pszDesc, 64 );
        CPLPrintStringFill( szTemp + 64, "", 64 );
        CPLPrintTime( szTemp + 128, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
        CPLPrintTime( szTemp + 144, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
        switch ( eType )
        {
            case GDT_Byte:
                CPLPrintStringFill( szTemp + 160, "8U", 8 );
                break;
            case GDT_Int16:
                CPLPrintStringFill( szTemp + 160, "16S", 8 );
                break;
            case GDT_UInt16:
                CPLPrintStringFill( szTemp + 160, "16U", 8 );
                break;
            case GDT_Float32:
                CPLPrintStringFill( szTemp + 160, "32R", 8 );
                break;
            default:
                break;
        }
        CPLPrintStringFill( szTemp + 168, "", 16 );
        CPLPrintStringFill( szTemp + 184, "", 8 );
        CPLPrintStringFill( szTemp + 192, "", 8 );
        CPLPrintStringFill( szTemp + 200, " ", 1 );
#ifdef CPL_MSB
        CPLPrintStringFill( szTemp + 201, "N", 1 );
#else
        if ( eType == GDT_Byte )
            CPLPrintStringFill( szTemp + 201, "N", 1 );
        else
            CPLPrintStringFill( szTemp + 201, "S", 1 );
#endif
        CPLPrintStringFill( szTemp + 202, "", 48 );
        CPLPrintStringFill( szTemp + 250, "", 32 );
        CPLPrintStringFill( szTemp + 282, "", 8 );
        CPLPrintStringFill( szTemp + 290, "", 94 );

        // Write the history line
        CPLPrintStringFill( szTemp + 384,
                            "GDAL: Image band created with GDAL", 64 );
        CPLPrintTime( szTemp + 448, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
        // Fill other lines with spaces
        CPLPrintStringFill( szTemp + 464, "", 80 * 7 );

        // More history lines may be used if needed.
        // CPLPrintStringFill( szTemp + 464, "HistoryLine2", 80 );
        // CPLPrintStringFill( szTemp + 544, "HistoryLine3", 80 );
        // CPLPrintStringFill( szTemp + 624, "HistoryLine4", 80 );
        // CPLPrintStringFill( szTemp + 704, "HistoryLine5", 80 );
        // CPLPrintStringFill( szTemp + 784, "HistoryLine6", 80 );
        // CPLPrintStringFill( szTemp + 864, "HistoryLine7", 80 );
        // CPLPrintStringFill( szTemp + 944, "HistoryLine8", 80 );

        VSIFWriteL( (void *)szTemp, 1, 1024, fp );
    }

/* ==================================================================== */
/*      Fill the Segment Pointers.                                      */
/* ==================================================================== */
    int         nSegments = ( nSegBlocks * 512 ) / 32;

/* -------------------------------------------------------------------- */
/*      Write out pointer to the Georeferencing segment.                */
/*      Segment will be disabled until data will be actualle written    */
/*      out in FlushCache().                                            */
/* -------------------------------------------------------------------- */
    CPLPrintStringFill( szTemp, " 150GEO", 7 );
    CPLPrintUIntBig( szTemp + 12, nImageStart + nImageBlocks, 11 );
    sprintf( szTemp + 23, "%9d", nGeoSegBlocks );
    VSIFWriteL( (void *)szTemp, 1, 32, fp );

/* -------------------------------------------------------------------- */
/*      Blank all other segment pointers                                */
/* -------------------------------------------------------------------- */
    CPLPrintStringFill( szTemp, "", 32 );
    for ( i = 1; i < nSegments; i++ )
        VSIFWriteL( (void *)szTemp, 1, 32, fp );

    VSIFCloseL( fp );
    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}



/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
PCIDSKDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                        int bStrict, char ** papszOptions, 
                        GDALProgressFunc pfnProgress, void * pProgressData )

{
    PCIDSKDataset	*poDS;
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int          iBand;

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    /* check that other bands match type- sets type */
    /* to unknown if they differ.                  */
    for( iBand = 1; iBand < poSrcDS->GetRasterCount(); iBand++ )
     {
         GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
         eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
     }

    poDS = (PCIDSKDataset *) Create( pszFilename, 
                                  poSrcDS->GetRasterXSize(), 
                                  poSrcDS->GetRasterYSize(), 
                                  poSrcDS->GetRasterCount(), 
                                  eType, papszOptions );

   /* Check that Create worked- return Null if it didn't */
    if (poDS == NULL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Copy the image data.                                            */
/* -------------------------------------------------------------------- */
    int         nXSize = poDS->GetRasterXSize();
    int         nYSize = poDS->GetRasterYSize();
    int  	nBlockXSize, nBlockYSize, nBlockTotal, nBlocksDone;

    poDS->GetRasterBand(1)->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nBlockTotal = ((nXSize + nBlockXSize - 1) / nBlockXSize)
        * ((nYSize + nBlockYSize - 1) / nBlockYSize)
        * poSrcDS->GetRasterCount();

    nBlocksDone = 0;
    for( iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDS->GetRasterBand( iBand+1 );
        int	       iYOffset, iXOffset;
        void           *pData;
        CPLErr  eErr;


        pData = CPLMalloc(nBlockXSize * nBlockYSize
                          * GDALGetDataTypeSize(eType) / 8);

        for( iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize )
        {
            for( iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize )
            {
                int	nTBXSize, nTBYSize;

                if( !pfnProgress( (nBlocksDone++) / (float) nBlockTotal,
                                  NULL, pProgressData ) )
                {
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated" );
                    delete poDS;

                    GDALDriver *poPCIDSKDriver = 
                        (GDALDriver *) GDALGetDriverByName( "PCIDSK" );
                    poPCIDSKDriver->Delete( pszFilename );
                    return NULL;
                }

                nTBXSize = MIN(nBlockXSize,nXSize-iXOffset);
                nTBYSize = MIN(nBlockYSize,nYSize-iYOffset);

                eErr = poSrcBand->RasterIO( GF_Read, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0 );
                if( eErr != CE_None )
                {
                    return NULL;
                }
            
                eErr = poDstBand->RasterIO( GF_Write, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0 );

                if( eErr != CE_None )
                {
                    return NULL;
                }
            }
        }

        CPLFree( pData );
    }

/* -------------------------------------------------------------------- */
/*      Copy georeferencing information, if enough is available.        */
/* -------------------------------------------------------------------- */

    double *tempGeoTransform=NULL; 

    tempGeoTransform = (double *) CPLMalloc(6*sizeof(double));

    if (( poSrcDS->GetGeoTransform( tempGeoTransform ) == CE_None)
        && (tempGeoTransform[0] != 0.0 || tempGeoTransform[1] != 1.0
        || tempGeoTransform[2] != 0.0 || tempGeoTransform[3] != 0.0
        || tempGeoTransform[4] != 0.0 || ABS(tempGeoTransform[5]) != 1.0 ))
    {
          poDS->SetProjection(poSrcDS->GetProjectionRef());
          poDS->SetGeoTransform(tempGeoTransform);
    }
    CPLFree(tempGeoTransform);
        
   
    poDS->FlushCache();

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, 
                  "User terminated" );
        delete poDS;

        GDALDriver *poPCIDSKDriver = 
            (GDALDriver *) GDALGetDriverByName( "PCIDSK" );
        poPCIDSKDriver->Delete( pszFilename );
        return NULL;
    }

    return poDS;
}
/************************************************************************/
/*                        GDALRegister_PCIDSK()                         */
/************************************************************************/

void GDALRegister_PCIDSK()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PCIDSK" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PCIDSK" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "PCIDSK Database File" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_pcidsk.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Int16 Float32" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='FILEDESC1' type='string' description='The first line of descriptive text'/>"
"   <Option name='FILEDESC2' type='string' description='The second line of descriptive text'/>"
"   <Option name='BANDDESCn' type='string' description='Text describing contents of the specified band'/>"
"</CreationOptionList>" ); 

        poDriver->pfnOpen = PCIDSKDataset::Open;
        poDriver->pfnCreate = PCIDSKDataset::Create;
        poDriver->pfnCreateCopy = PCIDSKDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

