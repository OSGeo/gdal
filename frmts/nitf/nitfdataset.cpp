/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  GDALDataset/GDALRasterBand implementation on top of "nitflib".
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * Revision 1.20  2004/04/28 15:19:00  warmerda
 * added geocentric to geodetic conversion
 *
 * Revision 1.19  2004/04/16 15:26:04  warmerda
 * completed metadata support
 *
 * Revision 1.18  2004/04/15 20:53:15  warmerda
 * Added support for geocentric coordinates, and file level metadata
 *
 * Revision 1.17  2004/04/02 20:44:37  warmerda
 * preserve APBB (actual bits per pixel) field as metadata
 *
 * Revision 1.16  2004/03/17 21:16:58  warmerda
 * Added GCP support for the corner points if they dont produce a nice geotransform
 *
 * Revision 1.15  2004/02/09 05:18:07  warmerda
 * fixed up north/south MGRS support
 *
 * Revision 1.14  2004/02/09 05:04:41  warmerda
 * added ICORDS=U (MGRS) support
 *
 * Revision 1.13  2003/09/12 22:52:25  gwalter
 * Added recognition of header file in absence of other useable georeferencing
 * information.
 *
 * Revision 1.12  2003/09/11 19:51:55  warmerda
 * avoid type casting warnings
 *
 * Revision 1.11  2003/08/21 19:25:59  warmerda
 * added overview support
 *
 * Revision 1.10  2003/08/21 15:02:38  gwalter
 * Try to find a .nfw file if no other geotransform information is found.
 *
 * Revision 1.9  2003/06/23 18:32:06  warmerda
 * dont return projectionref if we dont have a geotransform
 *
 * Revision 1.8  2003/06/03 19:44:26  warmerda
 * added RPC coefficient support
 *
 * Revision 1.7  2003/03/24 15:10:54  warmerda
 * Don't crash out if no image segments found.
 *
 * Revision 1.6  2002/12/21 18:12:10  warmerda
 * added driver metadata
 *
 * Revision 1.5  2002/12/18 20:15:43  warmerda
 * support writing IGEOLO
 *
 * Revision 1.4  2002/12/18 06:35:15  warmerda
 * implement nodata support for mapped data
 *
 * Revision 1.3  2002/12/17 21:23:15  warmerda
 * implement LUT reading and writing
 *
 * Revision 1.2  2002/12/17 05:26:26  warmerda
 * implement basic write support
 *
 * Revision 1.1  2002/12/03 04:43:41  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "nitflib.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*				NITFDataset				*/
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand;

class NITFDataset : public GDALDataset
{
    friend class NITFRasterBand;

    NITFFile    *psFile;
    NITFImage   *psImage;

    int         bGotGeoTransform;
    double      adfGeoTransform[6];

    char        *pszProjection;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

  public:
                 NITFDataset();
                 ~NITFDataset();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            NITFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand : public GDALRasterBand
{
    friend class NITFDataset;

    NITFImage   *psImage;

    GDALColorTable *poColorTable;

  public:
                   NITFRasterBand( NITFDataset *, int );
                  ~NITFRasterBand();

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual CPLErr SetColorTable( GDALColorTable * ); 
    virtual double GetNoDataValue( int *pbSuccess = NULL );
};


/************************************************************************/
/*                           NITFRasterBand()                           */
/************************************************************************/

NITFRasterBand::NITFRasterBand( NITFDataset *poDS, int nBand )

{
    NITFBandInfo *psBandInfo = poDS->psImage->pasBandInfo + nBand - 1;

    this->poDS = poDS;
    this->nBand = nBand;

    this->eAccess = poDS->eAccess;
    this->psImage = poDS->psImage;

/* -------------------------------------------------------------------- */
/*      Translate data type(s).                                         */
/* -------------------------------------------------------------------- */
    if( psImage->nBitsPerSample <= 8 )
        eDataType = GDT_Byte;
    else if( psImage->nBitsPerSample == 16 
             && EQUAL(psImage->szPVType,"SI") )
        eDataType = GDT_Int16;
    else if( psImage->nBitsPerSample == 16 )
        eDataType = GDT_UInt16;
    else if( psImage->nBitsPerSample == 32 
             && EQUAL(psImage->szPVType,"SI") )
        eDataType = GDT_Int32;
    else if( psImage->nBitsPerSample == 32 
             && EQUAL(psImage->szPVType,"R") )
        eDataType = GDT_Float32;
    else if( psImage->nBitsPerSample == 32 )
        eDataType = GDT_UInt32;
    else if( psImage->nBitsPerSample == 64 
             && EQUAL(psImage->szPVType,"R") )
        eDataType = GDT_Float64;
    else
    {
        eDataType = GDT_Byte;
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Unsupported combination of PVTYPE(%s) and NBPP(%d).",
                  psImage->szPVType, psImage->nBitsPerSample );
    }

/* -------------------------------------------------------------------- */
/*      Work out block size. If the image is all one big block we       */
/*      handle via the scanline access API.                             */
/* -------------------------------------------------------------------- */
    if( psImage->nBlocksPerRow == 1 
        && psImage->nBlocksPerColumn == 1 
        && EQUAL(psImage->szIC,"NC") )
    {
        nBlockXSize = psImage->nBlockWidth;
        nBlockYSize = 1;
    }
    else
    {
        nBlockXSize = psImage->nBlockWidth;
        nBlockYSize = psImage->nBlockHeight;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a color table?                                       */
/* -------------------------------------------------------------------- */
    poColorTable = NULL;

    if( psBandInfo->nSignificantLUTEntries > 0 )
    {
        int  iColor;

        poColorTable = new GDALColorTable();

        for( iColor = 0; iColor < psBandInfo->nSignificantLUTEntries; iColor++)
        {
            GDALColorEntry sEntry;

            sEntry.c1 = psBandInfo->pabyLUT[  0 + iColor];
            sEntry.c2 = psBandInfo->pabyLUT[256 + iColor];
            sEntry.c3 = psBandInfo->pabyLUT[512 + iColor];
            sEntry.c4 = 255;

            poColorTable->SetColorEntry( iColor, &sEntry );
        }
    }
}

/************************************************************************/
/*                          ~NITFRasterBand()                           */
/************************************************************************/

NITFRasterBand::~NITFRasterBand()

{
    if( poColorTable != NULL )
        delete poColorTable;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr NITFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    int  nBlockResult;

/* -------------------------------------------------------------------- */
/*      Read the line/block                                             */
/* -------------------------------------------------------------------- */
    if( nBlockYSize == 1 )
    {
        nBlockResult = 
            NITFReadImageLine(psImage, nBlockYOff, nBand, pImage);
    }
    else
    {
        nBlockResult = 
            NITFReadImageBlock(psImage, nBlockXOff, nBlockYOff, nBand, pImage);
    }

    if( nBlockResult == BLKREAD_OK )
        return CE_None;
    else if( nBlockResult == BLKREAD_FAIL )
        return CE_Failure;
    else /* nBlockResult == BLKREAD_NULL */ 
    {
        if( psImage->bNoDataSet )
            memset( pImage, psImage->nNoDataValue, 
                    psImage->nWordSize*psImage->nBlockWidth*psImage->nBlockHeight);
        else
            memset( pImage, 0, 
                    psImage->nWordSize*psImage->nBlockWidth*psImage->nBlockHeight);

        return CE_None;
    }
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr NITFRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )
    
{
    int  nBlockResult;

/* -------------------------------------------------------------------- */
/*      Write the line/block                                            */
/* -------------------------------------------------------------------- */
    if( nBlockYSize == 1 )
    {
        nBlockResult = 
            NITFWriteImageLine(psImage, nBlockYOff, nBand, pImage);
    }
    else
    {
        nBlockResult = 
            NITFWriteImageBlock(psImage, nBlockXOff, nBlockYOff, nBand,pImage);
    }

    if( nBlockResult == BLKREAD_OK )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double NITFRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = psImage->bNoDataSet;

    if( psImage->bNoDataSet )
        return psImage->nNoDataValue;
    else
        return -1e10;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp NITFRasterBand::GetColorInterpretation()

{
    NITFBandInfo *psBandInfo = psImage->pasBandInfo + nBand - 1;

    if( poColorTable != NULL )
        return GCI_PaletteIndex;
    if( EQUAL(psBandInfo->szIREPBAND,"R") )
        return GCI_RedBand;
    if( EQUAL(psBandInfo->szIREPBAND,"G") )
        return GCI_GreenBand;
    if( EQUAL(psBandInfo->szIREPBAND,"B") )
        return GCI_BlueBand;
    if( EQUAL(psBandInfo->szIREPBAND,"M") )
        return GCI_GrayIndex;

    return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *NITFRasterBand::GetColorTable()

{
    return poColorTable;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr NITFRasterBand::SetColorTable( GDALColorTable *poNewCT )

{
    GByte abyNITFLUT[768];
    int   i;
    int   nCount = MIN(256,poNewCT->GetColorEntryCount());

    memset( abyNITFLUT, 0, 768 );
    for( i = 0; i < nCount; i++ )
    {
        GDALColorEntry sEntry;

        poNewCT->GetColorEntryAsRGB( i, &sEntry );
        abyNITFLUT[i    ] = (GByte) sEntry.c1;
        abyNITFLUT[i+256] = (GByte) sEntry.c2;
        abyNITFLUT[i+512] = (GByte) sEntry.c3;
    }

    if( NITFWriteLUT( psImage, nBand, nCount, abyNITFLUT ) )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                             NITFDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            NITFDataset()                             */
/************************************************************************/

NITFDataset::NITFDataset()

{
    psFile = NULL;
    psImage = NULL;
    bGotGeoTransform = FALSE;
    pszProjection = CPLStrdup("");

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = NULL;
}

/************************************************************************/
/*                            ~NITFDataset()                            */
/************************************************************************/

NITFDataset::~NITFDataset()

{
    FlushCache();

    if( psFile != NULL )
    {
        NITFClose( psFile );
        psFile = NULL;
    }
    CPLFree( pszProjection );

    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NITFDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 4 )
        return NULL;

    if( !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) 
        && !EQUALN((char *) poOpenInfo->pabyHeader,"NSIF",4) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file with library.                                     */
/* -------------------------------------------------------------------- */
    NITFFile *psFile;

    psFile = NITFOpen( poOpenInfo->pszFilename, 
                       poOpenInfo->eAccess == GA_Update );
    if( psFile == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Is there an image to operate on?                                */
/* -------------------------------------------------------------------- */
    int iSegment;
    NITFImage *psImage = NULL;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        if( EQUAL(psFile->pasSegmentInfo[iSegment].szSegmentType,"IM") )
        {
            psImage = NITFImageAccess( psFile, iSegment );
            if( psImage == NULL )
            {
                NITFClose( psFile );
                return NULL;
            }
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If no image segments found report this to the user.             */
/* -------------------------------------------------------------------- */
    if( psImage == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file %s appears to be an NITF file, but no image\n"
                  "blocks were found on it.  GDAL cannot utilize non-image\n"
                  "NITF files.", 
                  poOpenInfo->pszFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NITFDataset 	*poDS;

    poDS = new NITFDataset();

    poDS->psFile = psFile;
    poDS->psImage = psImage;
    poDS->eAccess = poOpenInfo->eAccess;

    poDS->nRasterXSize = psImage->nCols;
    poDS->nRasterYSize = psImage->nRows;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		iBand;

    for( iBand = 0; iBand < psImage->nBands; iBand++ )
        poDS->SetBand( iBand+1, new NITFRasterBand( poDS, iBand+1 ) );

/* -------------------------------------------------------------------- */
/*      Process the projection from the ICORDS.                         */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRSWork;

    if( psImage->chICORDS == 'G'  )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }
    else if( psImage->chICORDS == 'C' )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );

        /* convert latitudes from geocentric to geodetic form. */
        
        psImage->dfULY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfULY );
        psImage->dfLLY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfLLY );
        psImage->dfURY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfURY );
        psImage->dfLRY = 
            NITF_WGS84_Geocentric_Latitude_To_Geodetic_Latitude( 
                psImage->dfLRY );
    }
    else if( psImage->chICORDS == 'S' || psImage->chICORDS == 'N' )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;

        oSRSWork.SetUTM( psImage->nZone, psImage->chICORDS == 'N' );
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }
    else if( psImage->chICORDS == 'U' && psImage->nZone != 0 )
    {
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;

        oSRSWork.SetUTM( ABS(psImage->nZone), psImage->nZone > 0 );
        oSRSWork.SetWellKnownGeogCS( "WGS84" );
        oSRSWork.exportToWkt( &(poDS->pszProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Do we have IGEOLO data that can be treated as a geotransform?   */
/* -------------------------------------------------------------------- */
    if( psImage->dfULX == psImage->dfLLX 
        && psImage->dfURX == psImage->dfLRX
        && psImage->dfULY == psImage->dfURY
        && psImage->dfLLY == psImage->dfLRY
        && psImage->dfULX != psImage->dfLRX
        && psImage->dfULY != psImage->dfLRY )
    {
        poDS->bGotGeoTransform = TRUE;
        poDS->adfGeoTransform[0] = psImage->dfULX;
        poDS->adfGeoTransform[1] = 
            (psImage->dfLRX - psImage->dfULX) / poDS->nRasterXSize;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = psImage->dfULY;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 
            (psImage->dfLRY - psImage->dfULY) / poDS->nRasterYSize;
    }
/* -------------------------------------------------------------------- */
/*      Otherwise try looking for a .nfw file.                          */
/* -------------------------------------------------------------------- */
    else if( GDALReadWorldFile( poOpenInfo->pszFilename, "nfw", 
                                poDS->adfGeoTransform ) )
    {
        const char *pszHDR;
        FILE *fpHDR;
        char **papszLines;
        int isNorth;
        int zone;
        
        poDS->bGotGeoTransform = TRUE;

        /* If nfw found, try looking for a header with projection info */
        /* in space imaging style format                               */
        pszHDR = CPLResetExtension( poOpenInfo->pszFilename, "hdr" );
        
        fpHDR = VSIFOpen( pszHDR, "rt" );

#ifndef WIN32
        if( fpHDR == NULL )
        {
            pszHDR = CPLResetExtension( poOpenInfo->pszFilename, "HDR" );
            fpHDR = VSIFOpen( pszHDR, "rt" );
        }
#endif
    
        if( fpHDR != NULL )
        {
            VSIFClose( fpHDR );
            papszLines=CSLLoad(pszHDR);
            if (CSLCount(papszLines) == 16)
            {

                if (psImage->chICORDS == 'N')
                    isNorth=1;
                else if (psImage->chICORDS =='S')
                    isNorth=0;
                else
                {
                    if (psImage->dfLLY+psImage->dfLRY+psImage->dfULY+psImage->dfURY < 0)
                        isNorth=0;
                    else
                        isNorth=1;
                }
                if( (EQUALN(papszLines[7],
                            "Selected Projection: Universal Transverse Mercator",50)) &&
                    (EQUALN(papszLines[8],"Zone: ",6)) &&
                    (strlen(papszLines[8]) >= 7))
                {
                    CPLFree( poDS->pszProjection );
                    poDS->pszProjection = NULL;
                    zone=atoi(&(papszLines[8][6]));
                    oSRSWork.SetUTM( zone, isNorth );
                    oSRSWork.SetWellKnownGeogCS( "WGS84" );
                    oSRSWork.exportToWkt( &(poDS->pszProjection) );
                }
            }
            CSLDestroy(papszLines);
        }

    }
/* -------------------------------------------------------------------- */
/*      If we have IGEOLO that isn't north up, return it as GCPs.       */
/* -------------------------------------------------------------------- */
    else if( (psImage->dfULX != 0 || psImage->dfURX != 0 
              || psImage->dfLRX != 0 || psImage->dfLLX != 0)
             && psImage->chICORDS != 'N' )
    {
        poDS->nGCPCount = 4;
        poDS->pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),
                                                  poDS->nGCPCount);
        GDALInitGCPs( 4, poDS->pasGCPList );

        poDS->pasGCPList[0].dfGCPX = psImage->dfULX;
        poDS->pasGCPList[0].dfGCPY = psImage->dfULY;
        poDS->pasGCPList[0].dfGCPPixel = 0;
        poDS->pasGCPList[0].dfGCPLine = 0;
        CPLFree( poDS->pasGCPList[0].pszId );
        poDS->pasGCPList[0].pszId = CPLStrdup( "UpperLeft" );

        poDS->pasGCPList[1].dfGCPX = psImage->dfURX;
        poDS->pasGCPList[1].dfGCPY = psImage->dfURY;
        poDS->pasGCPList[1].dfGCPPixel = poDS->nRasterXSize;
        poDS->pasGCPList[1].dfGCPLine = 0;
        CPLFree( poDS->pasGCPList[1].pszId );
        poDS->pasGCPList[1].pszId = CPLStrdup( "UpperRight" );

        poDS->pasGCPList[2].dfGCPX = psImage->dfLLX;
        poDS->pasGCPList[2].dfGCPY = psImage->dfLLY;
        poDS->pasGCPList[2].dfGCPPixel = 0;
        poDS->pasGCPList[2].dfGCPLine = poDS->nRasterYSize;
        CPLFree( poDS->pasGCPList[2].pszId );
        poDS->pasGCPList[2].pszId = CPLStrdup( "LowerLeft" );

        poDS->pasGCPList[3].dfGCPX = psImage->dfLRX;
        poDS->pasGCPList[3].dfGCPY = psImage->dfLRY;
        poDS->pasGCPList[3].dfGCPPixel = poDS->nRasterXSize;
        poDS->pasGCPList[3].dfGCPLine = poDS->nRasterYSize;
        CPLFree( poDS->pasGCPList[3].pszId );
        poDS->pasGCPList[3].pszId = CPLStrdup( "LowerRight" );

        poDS->pszGCPProjection = CPLStrdup( poDS->pszProjection );
    }
                 
/* -------------------------------------------------------------------- */
/*      Do we have metadata.                                            */
/* -------------------------------------------------------------------- */
    char **papszMergedMD;

    papszMergedMD = CSLDuplicate( poDS->psFile->papszMetadata );
    papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                      CSLCount( papszMergedMD ),
                                      psImage->papszMetadata );

    if( psImage->pszComments != NULL && strlen(psImage->pszComments) != 0 )
        papszMergedMD = CSLSetNameValue( 
            papszMergedMD, "NITF_IMAGE_COMMENTS", psImage->pszComments );

    poDS->SetMetadata( papszMergedMD );
    CSLDestroy( papszMergedMD );
    
/* -------------------------------------------------------------------- */
/*      Do we have RPC info.                                            */
/* -------------------------------------------------------------------- */
    NITFRPC00BInfo sRPCInfo;

    if( NITFReadRPC00B( psImage, &sRPCInfo ) && sRPCInfo.SUCCESS )
    {
        char szValue[1280];
        int  i;

        sprintf( szValue, "%.16g", sRPCInfo.LINE_OFF );
        poDS->SetMetadataItem( "RPC_LINE_OFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LINE_SCALE );
        poDS->SetMetadataItem( "RPC_LINE_SCALE", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.SAMP_OFF );
        poDS->SetMetadataItem( "RPC_SAMP_OFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.SAMP_SCALE );
        poDS->SetMetadataItem( "RPC_SAMP_SCALE", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LONG_OFF );
        poDS->SetMetadataItem( "RPC_LONG_OFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LONG_SCALE );
        poDS->SetMetadataItem( "RPC_LONG_SCALE", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LAT_OFF );
        poDS->SetMetadataItem( "RPC_LAT_OFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LAT_SCALE );
        poDS->SetMetadataItem( "RPC_LAT_SCALE", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.HEIGHT_OFF );
        poDS->SetMetadataItem( "RPC_HEIGHT_OFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.HEIGHT_SCALE );
        poDS->SetMetadataItem( "RPC_HEIGHT_SCALE", szValue );

        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.LINE_NUM_COEFF[i] );
        poDS->SetMetadataItem( "RPC_LINE_NUM_COEFF", szValue );

        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.LINE_DEN_COEFF[i] );
        poDS->SetMetadataItem( "RPC_LINE_DEN_COEFF", szValue );
        
        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.SAMP_NUM_COEFF[i] );
        poDS->SetMetadataItem( "RPC_SAMP_NUM_COEFF", szValue );
        
        szValue[0] = '\0'; 
        for( i = 0; i < 20; i++ )
            sprintf( szValue+strlen(szValue), "%.16g ",  
                     sRPCInfo.SAMP_DEN_COEFF[i] );
        poDS->SetMetadataItem( "RPC_SAMP_DEN_COEFF", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LONG_OFF );
        poDS->SetMetadataItem( "RPC_MIN_LONG", szValue );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LONG_OFF + 2 * sRPCInfo.LONG_SCALE );
        poDS->SetMetadataItem( "RPC_MAX_LONG", szValue );

        sprintf( szValue, "%.16g", sRPCInfo.LAT_OFF );
        poDS->SetMetadataItem( "RPC_MIN_LAT", szValue );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LAT_OFF + 2 * sRPCInfo.LAT_SCALE );
        poDS->SetMetadataItem( "RPC_MAX_LAT", szValue );
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::GetGeoTransform( double *padfGeoTransform )

{
    if( bGotGeoTransform )
    {
        memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::SetGeoTransform( double *padfGeoTransform )

{
    double dfULX, dfULY, dfURX, dfURY, dfLRX, dfLRY, dfLLX, dfLLY;

    if( psImage->chICORDS != 'G' )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Writing non-geographic coordinates not currently supported by NITF drivre." );
        return CE_Failure;
    }


    dfULX = padfGeoTransform[0];
    dfULY = padfGeoTransform[3];
    dfURX = dfULX + padfGeoTransform[1] * nRasterXSize;
    dfURY = dfULY + padfGeoTransform[4] * nRasterXSize;
    dfLRX = dfULX + padfGeoTransform[1] * nRasterXSize
                  + padfGeoTransform[2] * nRasterYSize;
    dfLRY = dfULY + padfGeoTransform[4] * nRasterXSize
                  + padfGeoTransform[5] * nRasterYSize;
    dfLLX = dfULX + padfGeoTransform[2] * nRasterYSize;
    dfLLY = dfULY + padfGeoTransform[5] * nRasterYSize;

    if( fabs(dfULX) > 180 || fabs(dfURX) > 180 
        || fabs(dfLRX) > 180 || fabs(dfLLX) > 180 
        || fabs(dfULY) >  90 || fabs(dfURY) >  90
        || fabs(dfLRY) >  90 || fabs(dfLLY) >  90 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to write geographic bound outside of legal range." );
        return CE_Failure;
    }

    if( NITFWriteIGEOLO( psImage, psImage->chICORDS, 
                         dfULX, dfULY, dfURX, dfURY, 
                         dfLRX, dfLRY, dfLLX, dfLLY ) )
        return CE_Failure;
    else
        return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *NITFDataset::GetProjectionRef()

{
    if( bGotGeoTransform )
        return pszProjection;
    else
        return "";
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int NITFDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *NITFDataset::GetGCPProjection()

{
    if( nGCPCount > 0 && pszGCPProjection != NULL )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *NITFDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                         NITFDatasetCreate()                          */
/************************************************************************/

static GDALDataset *
NITFDatasetCreate( const char *pszFilename, int nXSize, int nYSize, int nBands,
                   GDALDataType eType, char **papszOptions )

{
    const char *pszPVType;

    switch( eType )
    {
      case GDT_Byte:
      case GDT_UInt16:
      case GDT_UInt32:
        pszPVType = "INT";
        break;

      case GDT_Int16:
      case GDT_Int32:
        pszPVType = "SI";
        break;

      case GDT_Float32:
      case GDT_Float64:
        pszPVType = "R";
        break;

      case GDT_CInt16:
      case GDT_CInt32:
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "NITF format does not support complex integer data." );
        return NULL;

      case GDT_CFloat32:
      case GDT_CFloat64:
        pszPVType = "C";
        break;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported raster pixel type (%d).", 
                  (int) eType );
        return NULL;
    }

    NITFCreate( pszFilename, nXSize, nYSize, nBands, 
                GDALGetDataTypeSize( eType ), pszPVType, 
                papszOptions );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                           NITFCreateCopy()                           */
/************************************************************************/

static GDALDataset *
NITFCreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                int bStrict, char **papszOptions, 
                GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALDataType eType;
    GDALRasterBand *poBand1 = poSrcDS->GetRasterBand(1);
    char  **papszFullOptions = CSLDuplicate( papszOptions );

    if( poBand1 == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get the data type.  Complex integers isn't supported by         */
/*      NITF, so map that to complex float if we aren't in strict       */
/*      mode.                                                           */
/* -------------------------------------------------------------------- */
    eType = poBand1->GetRasterDataType();
    if( !bStrict && (eType == GDT_CInt16 || eType == GDT_CInt32) )
        eType = GDT_CFloat32;

/* -------------------------------------------------------------------- */
/*      Set if we can set IREP.                                         */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszFullOptions,"IREP") == NULL )
    {
        if( poSrcDS->GetRasterCount() == 3 && eType == GDT_Byte )
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "RGB" );
        
        else if( poSrcDS->GetRasterCount() == 1 && eType == GDT_Byte
                 && poBand1->GetColorTable() != NULL )
        {
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "RGB/LUT" );
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "LUT_SIZE", 
                  CPLSPrintf("%d", 
                             poBand1->GetColorTable()->GetColorEntryCount()) );
        }
        else if( GDALDataTypeIsComplex(eType) )
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "NODISPLY" );
        
        else
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "IREP", "MONO" );
    }

/* -------------------------------------------------------------------- */
/*      Do we have lat/long georeferencing information?                 */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];
    int    bWriteGeoTransform = FALSE;

    if( EQUALN(poSrcDS->GetProjectionRef(),"GEOGCS",6)
        && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        papszFullOptions = 
            CSLSetNameValue( papszFullOptions, "ICORDS", "G" );
        bWriteGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output dataset.                                      */
/* -------------------------------------------------------------------- */
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterXSize();

    GDALDataset *poDstDS = NITFDatasetCreate( pszFilename, nXSize, nYSize,
                                              poSrcDS->GetRasterCount(),
                                              eType, papszFullOptions );
    CSLDestroy( papszFullOptions );

/* -------------------------------------------------------------------- */
/*      Set the georeferencing.                                         */
/* -------------------------------------------------------------------- */
    if( bWriteGeoTransform )
        poDstDS->SetGeoTransform( adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDstDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable or other metadata?              */
/* -------------------------------------------------------------------- */
        GDALColorTable *poCT;

        poCT = poSrcBand->GetColorTable();
        if( poCT != NULL )
            poDstBand->SetColorTable( poCT );

/* -------------------------------------------------------------------- */
/*      Copy image data.                                                */
/* -------------------------------------------------------------------- */
        void           *pData;
        CPLErr         eErr;

        pData = CPLMalloc(nXSize * GDALGetDataTypeSize(eType) / 8);

        for( int iLine = 0; iLine < nYSize; iLine++ )
        {
            eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );
            if( eErr != CE_None )
            {
                return NULL;
            }
            
            eErr = poDstBand->RasterIO( GF_Write, 0, iLine, nXSize, 1, 
                                        pData, nXSize, 1, eType, 0, 0 );

            if( eErr != CE_None )
            {
                return NULL;
            }

            if( !pfnProgress( (iBand + (iLine+1) / (double) nYSize)
                              / (double) poSrcDS->GetRasterCount(), 
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                delete poDstDS;
                return NULL;
            }
        }

        CPLFree( pData );
    }

    return poDstDS;
}

/************************************************************************/
/*                          GDALRegister_NITF()                         */
/************************************************************************/

void GDALRegister_NITF()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "NITF" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "NITF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "National Imagery Transmission Format" );
        
        poDriver->pfnOpen = NITFDataset::Open;
        poDriver->pfnCreate = NITFDatasetCreate;
        poDriver->pfnCreateCopy = NITFCreateCopy;

        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_nitf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ntf" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32 CFloat32 CFloat64" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
