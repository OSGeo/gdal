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
 * Revision 1.6  2003/10/08 07:58:53  dron
 * Few fixes in WriteGeoSegment().
 *
 * Revision 1.5  2003/10/05 15:31:38  dron
 * Added support for external raw files.
 *
 * Revision 1.4  2003/09/28 14:15:27  dron
 * Implemented GCP segment reading, header parsing improved.
 *
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

const int       nSegBlocks = 64;    // Number of blocks of Segment Pointers
const int       nGeoSegBlocks = 8;  // Number of blocks in GEO Segment

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

    char                *pszCreatTime;  // Date/time of the database creation

    vsi_l_offset        nGeoPtrOffset;  // Offset in bytes to the pointer
                                        // to GEO segment
    vsi_l_offset        nGeoOffset;     // Offset in bytes to the GEO segment
    vsi_l_offset        nGcpPtrOffset;  // Offset in bytes to the pointer
                                        // to GCP segment
    vsi_l_offset        nGcpOffset;     // Offset in bytes to the GCP segment

    int                 bGeoSegmentDirty;

    GDAL_GCP            *pasGCPList;
    long                 nGCPCount;

    double              adfGeoTransform[6];
    char                *pszProjection;
    char                *pszGCPProjection;

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
    virtual int         GetGCPCount();
    virtual const       char *GetGCPProjection();
    virtual const       GDAL_GCP *GetGCPs();
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
    pszCreatTime = NULL;
    nGeoOffset = 0;
    bGeoSegmentDirty = FALSE;
    pszProjection = CPLStrdup( "" );
    pszGCPProjection = CPLStrdup( "" );
    nGCPCount = 0;
    pasGCPList = NULL;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~PCIDSKDataset()                          */
/************************************************************************/

PCIDSKDataset::~PCIDSKDataset()
{
    FlushCache();

    if ( pszProjection )
        CPLFree( pszProjection );
    if ( pszGCPProjection )
        CPLFree( pszGCPProjection );
    if( fp != NULL )
        VSIFCloseL( fp );
    if( pszCreatTime )
        CPLFree( pszCreatTime );
    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
        {
            if ( pasGCPList[i].pszId )
                CPLFree( pasGCPList[i].pszId );
            if ( pasGCPList[i].pszInfo )
                CPLFree( pasGCPList[i].pszInfo );
        }

        CPLFree( pasGCPList );
    }
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
    bGeoSegmentDirty = TRUE;

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
    bGeoSegmentDirty = TRUE;

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int PCIDSKDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *PCIDSKDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *PCIDSKDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void PCIDSKDataset::FlushCache()
{
    GDALDataset::FlushCache();

    if( GetAccess() == GA_Update )
    {
        char        szTemp[64];

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
        if ( nGeoOffset && bGeoSegmentDirty )
        {
            WriteGeoSegment();
            bGeoSegmentDirty = FALSE;
        }
    }
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

#ifdef DEBUG
    CPLDebug( "PCIDSK", "Writing out georeferencing segment." );
#endif
    
    VSILocalTime( &nTime, &oUpdateTime );

    CPLPrintStringFill( szTemp, "Master Georeferencing Segment for File", 64 );
    CPLPrintStringFill( szTemp + 64, "", 64 );
    if ( pszCreatTime )
        CPLPrintStringFill( szTemp + 128, pszCreatTime, 16 );
    else
        CPLPrintTime( szTemp + 128, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    CPLPrintTime( szTemp + 144, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    CPLPrintStringFill( szTemp + 160, "", 224 );
    // Write the history line
    CPLPrintStringFill( szTemp + 384,
                        "GDAL: Master Georeferencing Segment for File", 64 );
    CPLPrintTime( szTemp + 448, 16, "%H:%M %d-%b-%y ", &oUpdateTime, "C" );
    // Fill other lines with spaces
    CPLPrintStringFill( szTemp + 464, "", 80 * 7 );

    // More history lines may be used, if needed.
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
    if ( EQUALN( pszType, "8U", 2 ) )
        return GDT_Byte;
    if ( EQUALN( pszType, "16S", 3 ) )
        return GDT_Int16;
    if ( EQUALN( pszType, "16U", 3 ) )
        return GDT_UInt16;
    if ( EQUALN( pszType, "32R", 3 ) )
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
        delete poDS;
        return NULL;
    }
    poDS->eAccess = poOpenInfo->eAccess;

/* ==================================================================== */
/*   Read PCIDSK File Header.                                           */
/* ==================================================================== */
    char            szTemp[1024];
    char            *pszString;

/* -------------------------------------------------------------------- */
/*      Read File Identification.                                       */
/* -------------------------------------------------------------------- */
    VSIFSeekL( poDS->fp, 0, SEEK_SET );
    VSIFReadL( szTemp, 1, 512, poDS->fp );

    pszString = CPLScanString( szTemp + 8, 8, TRUE, TRUE );
    poDS->SetMetadataItem( "SOFTWARE", pszString );
    CPLFree( pszString );

    pszString = CPLScanString( szTemp + 48, 64, TRUE, TRUE );
    poDS->SetMetadataItem( "FILE_ID", pszString );
    CPLFree( pszString );

    pszString = CPLScanString( szTemp + 112, 32, TRUE, TRUE );
    poDS->SetMetadataItem( "GENERATING_FACILITY", pszString );
    CPLFree( pszString );

    pszString = CPLScanString( szTemp + 144, 64, TRUE, TRUE );
    poDS->SetMetadataItem( "DESCRIPTION1", pszString );
    CPLFree( pszString );

    pszString = CPLScanString( szTemp + 208, 64, TRUE, TRUE );
    poDS->SetMetadataItem( "DESCRIPTION2", pszString );
    CPLFree( pszString );

    pszString = CPLScanString( szTemp + 272, 16, TRUE, TRUE );
    poDS->SetMetadataItem( "DATE_OF_CREATION", pszString );
    poDS->pszCreatTime = pszString;

    pszString = CPLScanString( szTemp + 288, 16, TRUE, TRUE );
    poDS->SetMetadataItem( "DATE_OF_UPDATE", pszString );
    CPLFree( pszString );

/* -------------------------------------------------------------------- */
/*      Read Image Data.                                                */
/* -------------------------------------------------------------------- */
    vsi_l_offset    nImageStart;        // Start block of image data
    vsi_l_offset    nImgHdrsStart;      // Start block of image headers
    vsi_l_offset    nImageOffset;       // Offset to the first byte of the image
    int             nByteBands, nInt16Bands, nUInt16Bands, nFloat32Bands;

    pszString = CPLScanString( szTemp + 304, 16, TRUE, FALSE );
    if ( !EQUAL( pszString, "" ) )
        nImageStart = atol( pszString );// XXX: should be atoll()
    else
        nImageStart = 1;
    CPLFree( pszString );
    nImageOffset = (nImageStart - 1) * 512;

    pszString = CPLScanString( szTemp + 336, 16, TRUE, FALSE );
    nImgHdrsStart = atol( pszString );  // XXX: should be atoll()
    CPLFree( pszString );

    poDS->nBands = CPLScanLong( szTemp + 376, 8 );
    poDS->nRasterXSize = CPLScanLong( szTemp + 384, 8 );
    poDS->nRasterYSize = CPLScanLong( szTemp + 392, 8 );

    nByteBands = CPLScanLong( szTemp + 464, 4 );
    nInt16Bands = CPLScanLong( szTemp + 468, 4 );
    nUInt16Bands = CPLScanLong( szTemp + 472, 4 );
    nFloat32Bands = CPLScanLong( szTemp + 476, 4 );

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
    pszString = CPLScanString( szTemp + 360, 8, TRUE, FALSE );
    if ( EQUALN( pszString, "PIXEL", 5 ) )
        eInterleaving = PDI_PIXEL;
    else if ( EQUALN( pszString, "BAND", 4 ) )
        eInterleaving = PDI_BAND;
    else if ( EQUALN( pszString, "FILE", 4 ) )
        eInterleaving = PDI_FILE;
    else
    {
        CPLDebug( "PCIDSK",
                  "PCIDSK interleaving type %s is not supported by GDAL",
                  pszString );
        delete poDS;
        return NULL;
    }
    CPLFree( pszString );

    for( iBand = 0; iBand < poDS->nBands; iBand++ )
    {
        GDALDataType    eType;
        GDALRasterBand  *poBand;
        vsi_l_offset    nImgHdrOffset = (nImgHdrsStart - 1 + iBand * 2) * 512;
        vsi_l_offset    nPixelOffset = 0, nLineOffset = 0, nLineSize = 0;
        int             bNativeOrder;
        int             i;
        FILE            *fp = poDS->fp;

        VSIFSeekL( poDS->fp, nImgHdrOffset, SEEK_SET );
        VSIFReadL( szTemp, 1, 1024, poDS->fp );

        pszString = CPLScanString( szTemp + 160, 8, TRUE, FALSE );
        eType = poDS->PCIDSKTypeToGDAL( pszString );
        if ( eType == GDT_Unknown )
        {
            CPLDebug( "PCIDSK",
                      "PCIDSK data type %s is not supported by GDAL",
                      pszString );
            delete poDS;
            return NULL;
        }
        CPLFree( pszString );

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
                {
                    char    *pszFilename;

                    // Read the filename
                    pszFilename = CPLScanString( szTemp + 64, 64, TRUE, FALSE );

                    // Non-empty filename means we have data stored in
                    // external raw file
                    if ( !EQUAL(pszFilename, "") )
                    {
                        CPLDebug( "PCIDSK", "pszFilename=%s", pszFilename );
                        if( poOpenInfo->eAccess == GA_ReadOnly )
                            fp = VSIFOpenL( pszFilename, "rb" );
                        else
                            fp = VSIFOpenL( pszFilename, "r+b" );

                        if ( !fp )
                        {
                            CPLDebug( "PCIDSK",
                                      "Cannot open external raw file %s",
                                      pszFilename );
                            iBand--;
                            poDS->nBands--;
                            CPLFree( pszFilename );
                            continue;
                        }
                    }

                    pszString = CPLScanString( szTemp + 168, 16, TRUE, FALSE );
                    nImageOffset = atol( pszString ); // XXX: should be atoll()
                    CPLFree( pszString );

                    nPixelOffset = CPLScanLong( szTemp + 184, 8 );
                    nLineOffset = CPLScanLong( szTemp +  + 192, 8 );

                    CPLFree( pszFilename );
                }
                break;
            default: /* NOTREACHED */
                break;
        }

#ifdef CPL_MSB
        bNativeOrder = ( szTemp[201] == 'S')?FALSE:TRUE;
#else
        bNativeOrder = ( szTemp[201] == 'S')?TRUE:FALSE;
#endif

#ifdef DEBUG
    #if defined(WIN32) && defined(_MSC_VER)
        CPLDebug( "PCIDSK",
                  "Band %d: nImageOffset=%I64d, nPixelOffset=%I64d, "
                  "nLineOffset=%I64d, nLineSize=%I64d",
                  iBand + 1, nImageOffset, nPixelOffset,
                  nLineOffset, nLineSize );
    #elif HAVE_LONG_LONG
        CPLDebug( "PCIDSK",
                  "Band %d: nImageOffset=%Ld, nPixelOffset=%Ld, "
                  "nLineOffset=%Ld, nLineSize=%Ld",
                  iBand + 1, nImageOffset, nPixelOffset,
                  nLineOffset, nLineSize );
    #else
        CPLDebug( "PCIDSK",
                  "Band %d: nImageOffset=%ld, nPixelOffset=%ld, "
                  "nLineOffset=%ld, nLineSize=%ld",
                  iBand + 1, nImageOffset, nPixelOffset,
                  nLineOffset, nLineSize );
    #endif
#endif

        poBand = new RawRasterBand( poDS, iBand + 1, fp,
                                    nImageOffset, (int) nPixelOffset, 
                                    (int) nLineOffset, 
                                    eType, bNativeOrder, TRUE);
        poDS->SetBand( iBand + 1, poBand );

/* -------------------------------------------------------------------- */
/*      Read and assign few metadata parameters to each image band.     */
/* -------------------------------------------------------------------- */
        pszString = CPLScanString( szTemp, 64, TRUE, TRUE );
        poBand->SetDescription( pszString );
        CPLFree( pszString );

        pszString = CPLScanString( szTemp + 128, 16, TRUE, TRUE );
        poBand->SetMetadataItem( "DATE_OF_CREATION", pszString );
        CPLFree( pszString );

        pszString = CPLScanString( szTemp + 144, 16, TRUE, TRUE );
        poBand->SetMetadataItem( "DATE_OF_UPDATE",  pszString );
        CPLFree( pszString );

        pszString = CPLScanString( szTemp + 202, 16, TRUE, TRUE );
        if ( !EQUAL( szTemp, "" ) )
            poBand->SetMetadataItem( "UNITS",  pszString );
        CPLFree( pszString );

        for ( i = 0; i < 8; i++ )
        {
            pszString = CPLScanString( szTemp + 384 + i * 80, 80, TRUE, TRUE );
            if ( !EQUAL( pszString, "" ) )
                poBand->SetMetadataItem( CPLSPrintf("HISTORY%d", i + 1),
                                         pszString );
            CPLFree( pszString );
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
/*      Read segments.                                                  */
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
                                CPLScanDouble( szTemp + 26 * j, 26 );
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

            case 214:                       // GCP segment
            {
                vsi_l_offset    nGcpStart, nGcpDataOffset;
                int             j;
                OGRSpatialReference oSRS;

                poDS->nGcpPtrOffset = nSegPointersOffset + i * 32;
                nGcpStart = atol( szTemp + 12 ); // XXX: should be atoll()
                poDS->nGcpOffset = ( nGcpStart - 1 ) * 512;
                nGcpDataOffset = poDS->nGcpOffset + 1024;

                if ( bActive && !poDS->nGCPCount )  // XXX: We will read the
                                                    // first GCP segment only
                {
                    VSIFSeekL( poDS->fp, nGcpDataOffset, SEEK_SET );
                    VSIFReadL( szTemp, 1, 80, poDS->fp );
                    poDS->nGCPCount = CPLScanLong( szTemp, 16 );
                    if ( poDS->nGCPCount > 0 )
                    {
                        double      dfUnitConv = 1.0;
                        char        szProj[17];

                        memcpy( szProj, szTemp + 32, 16 );
                        szProj[16] = '\0';
                        oSRS.importFromPCI( szProj, NULL, NULL );
                        if ( poDS->pszGCPProjection )
                                CPLFree( poDS->pszGCPProjection );
                        oSRS.exportToWkt( &poDS->pszGCPProjection );
                        poDS->pasGCPList = (GDAL_GCP *)
                            CPLCalloc( poDS->nGCPCount, sizeof(GDAL_GCP) );
                        GDALInitGCPs( poDS->nGCPCount, poDS->pasGCPList );
                        if ( EQUALN( szTemp + 64, "FEET     ", 9 ) )
                            dfUnitConv = atof(SRS_UL_FOOT_CONV);
                        for ( j = 0; j < poDS->nGCPCount; j++ )
                        {
                            VSIFSeekL( poDS->fp, nGcpDataOffset + j * 128 + 512,
                                       SEEK_SET );
                            VSIFReadL( szTemp, 1, 128, poDS->fp );
                            poDS->pasGCPList[j].dfGCPPixel =
                                CPLScanDouble( szTemp + 6, 18 );
                            poDS->pasGCPList[j].dfGCPLine = 
                                CPLScanDouble( szTemp + 24, 18 );
                            poDS->pasGCPList[j].dfGCPX = 
                                CPLScanDouble( szTemp + 60, 18 );
                            poDS->pasGCPList[j].dfGCPY = 
                                CPLScanDouble( szTemp + 78, 18 );
                            poDS->pasGCPList[j].dfGCPZ =
                                CPLScanDouble( szTemp + 96, 18 ) / dfUnitConv;
                        }
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
    // Two following parameters will be filled with real values in FlushCache()
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
/*      Segment will be disabled until data will be actually written    */
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

