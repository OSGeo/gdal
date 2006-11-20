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
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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
 * Revision 1.64  2006/11/20 15:08:10  fwarmerdam
 * Added support for ICORDS='D'.
 *
 * Revision 1.63  2006/11/20 14:55:25  fwarmerdam
 * Add ability to scan forward for start of jpeg datastream.  The file
 * jpeg_nsif.nsf seems to have extra "stuff" before the data stream.
 *
 * Revision 1.62  2006/10/24 02:20:23  fwarmerdam
 * Fixed NITF_IMAG metadata.
 *
 * Revision 1.61  2006/10/24 02:18:06  fwarmerdam
 * added image attachment metadata
 *
 * Revision 1.60  2006/10/23 18:33:59  fwarmerdam
 * Added warning about odd numbers of bits.
 *
 * Revision 1.59  2006/06/08 02:56:04  fwarmerdam
 * added logic to find Q level and use for jpeg compression
 *
 * Revision 1.58  2006/06/01 03:25:41  fwarmerdam
 * Added support for multi-blocked jpeg compressed files.
 *
 * Revision 1.57  2006/05/31 18:40:25  fwarmerdam
 * Fixed problem with jpeg in later IM segments.
 *
 * Revision 1.56  2006/05/31 18:30:04  fwarmerdam
 * Fixed initialization of poJPEGDataset.
 *
 * Revision 1.55  2006/05/31 01:16:30  fwarmerdam
 * added very preliminary C3/JPEG read support
 *
 * Revision 1.54  2006/05/30 18:40:36  fwarmerdam
 * Allow nitf files without image data to be opened.
 *
 * Revision 1.53  2006/05/29 19:42:29  fwarmerdam
 * Return SLOC_ROW and SLOC_COL instead of X/Y the wrong way around.
 *
 * Revision 1.52  2006/05/29 19:33:15  fwarmerdam
 * added CGM read support via metadata
 *
 * Revision 1.51  2006/01/05 23:12:03  fwarmerdam
 * Added subdataset support to access multiple images in one file.
 *
 * Revision 1.50  2005/12/21 01:06:29  fwarmerdam
 * Added IMAGE_STRUCTURE COMPRESSION support.
 *
 * Revision 1.49  2005/10/03 17:39:45  fwarmerdam
 * fixed memory leak of GCPs as per bug 920
 *
 * Revision 1.48  2005/09/19 21:55:24  gwalter
 * Avoid creating gcps when geotransform already found.
 *
 * Revision 1.47  2005/09/14 12:54:40  dron
 * Avoid possible using of uninitialized value.
 *
 * Revision 1.46  2005/09/14 01:12:52  fwarmerdam
 * Removed complex types from creatable formats, apparently not
 * currently supported.
 *
 * Revision 1.45  2005/07/28 20:00:57  fwarmerdam
 * upgrade to support 2-4GB files, use large file api
 *
 * Revision 1.44  2005/05/23 06:57:36  fwarmerdam
 * fix flushing to go through pam
 *
 * Revision 1.43  2005/05/05 15:54:49  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.42  2005/04/18 17:58:12  fwarmerdam
 * fixed up ICORDS=' ' case to mean no geotransform
 *
 * Revision 1.41  2005/04/18 15:45:17  gwalter
 * Check for Space Imaging style .hdr + .nfw, and
 * default to these rather than in-file geocoding
 * information if both files are present and have
 * recognizable information.
 *
 * Revision 1.40  2005/04/18 13:40:22  fwarmerdam
 * Remove the default geotransform if we don't have a valid geotransform.
 *
 * Revision 1.39  2005/03/21 16:25:41  fwarmerdam
 * When writing IGEOLO adjust for center of corner pixels instead of
 * outer image corners.  Also support generating geotransform from
 * rotated IGEOLO values.
 *
 * Revision 1.38  2005/03/17 19:34:31  fwarmerdam
 * Added IC and IMODE metadata.
 *
 * Revision 1.37  2005/03/01 14:28:06  fwarmerdam
 * Added support for overriding JPEG2000 profile.
 *
 * Revision 1.36  2005/02/25 17:01:18  fwarmerdam
 * forward AdviseRead and IRasterIO on dataset to j2kdataset if needed
 *
 * Revision 1.35  2005/02/24 15:11:23  fwarmerdam
 * fixed improper j2kdataset cleanup in ~NITFDataset
 *
 * Revision 1.34  2005/02/22 08:20:58  fwarmerdam
 * ensure we can capture color interp on jpeg2000 streams
 *
 * Revision 1.33  2005/02/18 19:29:16  fwarmerdam
 * added SetColorIntepretation support: bug 752
 *
 * Revision 1.32  2005/02/10 04:49:31  fwarmerdam
 * added color interpretation setting on J2K bands
 *
 * Revision 1.31  2005/02/08 05:46:33  fwarmerdam
 * Fixed up ~NITFDataset() image length/COMRAT patching.
 *
 * Revision 1.30  2005/02/08 05:20:16  fwarmerdam
 * preliminary implementation of JPEG2000 support via Create()
 *
 * Revision 1.29  2005/01/28 03:47:26  fwarmerdam
 * Fixed return value of SetGeoTransform().
 *
 * Revision 1.28  2005/01/15 07:47:20  fwarmerdam
 * enable NPJE profile
 */

#include "gdal_pam.h"
#include "nitflib.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void NITFPatchImageLength( const char *pszFilename,
                                  long nImageOffset, 
                                  GIntBig nPixelCount );

static GDALDataset *poWritableJ2KDataset = NULL;
static CPLErr NITFSetColorInterpretation( NITFImage *psImage, 
                                          int nBand,
                                          GDALColorInterp eInterp );

/************************************************************************/
/* ==================================================================== */
/*				NITFDataset				*/
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand;

class NITFDataset : public GDALPamDataset
{
    friend class NITFRasterBand;

    NITFFile    *psFile;
    NITFImage   *psImage;

    GDALDataset *poJ2KDataset;
    int         bJP2Writing;

    GDALDataset *poJPEGDataset;

    int         bGotGeoTransform;
    double      adfGeoTransform[6];

    char        *pszProjection;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    char       **papszCGMMetadata;

    void         InitializeCGMMetadata();


    int         *panJPEGBlockOffset;
    GByte       *pabyJPEGBlock;
    int          nQLevel;

    int          ScanJPEGQLevel( GUInt32 *pnDataStart );
    CPLErr       ScanJPEGBlocks( void );
    CPLErr       ReadJPEGBlock( int, int );

    CPLString    osNITFFilename;

  public:
                 NITFDataset();
                 ~NITFDataset();

    virtual CPLErr AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                               int nBufXSize, int nBufYSize, 
                               GDALDataType eDT, 
                               int nBandCount, int *panBandList,
                               char **papszOptions );

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *, int, int, int );

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual void   FlushCache();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *
    NITFCreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                    int bStrict, char **papszOptions, 
                    GDALProgressFunc pfnProgress, void * pProgressData );

};

/************************************************************************/
/* ==================================================================== */
/*                            NITFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class NITFRasterBand : public GDALPamRasterBand
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
    virtual CPLErr SetColorInterpretation( GDALColorInterp );
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
    NITFDataset *poGDS = (NITFDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Special case for JPEG blocks.                                   */
/* -------------------------------------------------------------------- */
    if( EQUAL(psImage->szIC,"C3") )
    {
        CPLErr eErr = poGDS->ReadJPEGBlock( nBlockXOff, nBlockYOff );
        int nBlockBandSize = psImage->nBlockWidth*psImage->nBlockHeight;

        if( eErr != CE_None )
            return eErr;

        memcpy( pImage, 
                poGDS->pabyJPEGBlock + (nBand - 1) * nBlockBandSize, 
                nBlockBandSize );

        return eErr;
    }

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
    if( EQUAL(psBandInfo->szIREPBAND,"Y") )
        return GCI_YCbCr_YBand;
    if( EQUAL(psBandInfo->szIREPBAND,"Cb") )
        return GCI_YCbCr_CbBand;
    if( EQUAL(psBandInfo->szIREPBAND,"Cr") )
        return GCI_YCbCr_CrBand;

    return GCI_Undefined;
}

/************************************************************************/
/*                     NITFSetColorInterpretation()                     */
/************************************************************************/

static CPLErr NITFSetColorInterpretation( NITFImage *psImage, 
                                          int nBand,
                                          GDALColorInterp eInterp )

{
    NITFBandInfo *psBandInfo = psImage->pasBandInfo + nBand - 1;
    const char *pszREP = NULL;
    GUInt32 nOffset;

    if( eInterp == GCI_RedBand )
        pszREP = "R";
    else if( eInterp == GCI_GreenBand )
        pszREP = "G";
    else if( eInterp == GCI_BlueBand )
        pszREP = "B";
    else if( eInterp == GCI_GrayIndex )
        pszREP = "M";
    else if( eInterp == GCI_YCbCr_YBand )
        pszREP = "Y";
    else if( eInterp == GCI_YCbCr_CbBand )
        pszREP = "Cb";
    else if( eInterp == GCI_YCbCr_CrBand )
        pszREP = "Cr";
    else if( eInterp == GCI_Undefined )
        return CE_None;

    if( pszREP == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Requested color interpretation (%s) not supported in NITF.",
                  GDALGetColorInterpretationName( eInterp ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Where does this go in the file?                                 */
/* -------------------------------------------------------------------- */
    strcpy( psBandInfo->szIREPBAND, pszREP );
    nOffset = NITFIHFieldOffset( psImage, "IREPBAND" );

    if( nOffset != 0 )
        nOffset += (nBand - 1) * 13;
    
/* -------------------------------------------------------------------- */
/*      write it (space padded).                                        */
/* -------------------------------------------------------------------- */
    char szPadded[4];
    strcpy( szPadded, pszREP );
    strcat( szPadded, " " );
    
    if( nOffset != 0 )
    {
        if( VSIFSeekL( psImage->psFile->fp, nOffset, SEEK_SET ) != 0 
            || VSIFWriteL( (void *) szPadded, 1, 2, psImage->psFile->fp ) != 2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "IO failure writing new IREPBAND value to NITF file." );
            return CE_Failure;
        }
    }
    
    return CE_None;
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr NITFRasterBand::SetColorInterpretation( GDALColorInterp eInterp )

{
    return NITFSetColorInterpretation( psImage, nBand, eInterp );
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
    poJ2KDataset = NULL;
    bJP2Writing = FALSE;
    poJPEGDataset = NULL;

    panJPEGBlockOffset = NULL;
    pabyJPEGBlock = NULL;
    nQLevel = 0;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    papszCGMMetadata = NULL;
}

/************************************************************************/
/*                            ~NITFDataset()                            */
/************************************************************************/

NITFDataset::~NITFDataset()

{
    FlushCache();

/* -------------------------------------------------------------------- */
/*      If we have been writing to a JPEG2000 file, check if the        */
/*      color interpretations were set.  If so, apply the settings      */
/*      to the NITF file.                                               */
/* -------------------------------------------------------------------- */
    if( poJ2KDataset != NULL && bJP2Writing )
    {
        int i;

        for( i = 0; i < nBands && papoBands != NULL; i++ )
        {
            if( papoBands[i]->GetColorInterpretation() != GCI_Undefined )
                NITFSetColorInterpretation( psImage, i+1, 
                                papoBands[i]->GetColorInterpretation() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close the underlying NITF file.                                 */
/* -------------------------------------------------------------------- */
    int nImageStart = -1;
    if( psFile != NULL )
    {
        nImageStart = psFile->pasSegmentInfo[0].nSegmentStart;

        NITFClose( psFile );
        psFile = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Free datastructures.                                            */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );

    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

/* -------------------------------------------------------------------- */
/*      If we have a jpeg2000 output file, make sure it gets closed     */
/*      and flushed out.                                                */
/* -------------------------------------------------------------------- */
    if( poJ2KDataset != NULL )
    {
        int i;

        GDALClose( (GDALDatasetH) poJ2KDataset );
        
        // the bands are really jpeg2000 bands ... remove them 
        // from the NITF list so they won't get destroyed twice.
        for( i = 0; i < nBands && papoBands != NULL; i++ )
            papoBands[i] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Update file length, and COMRAT for JPEG2000 files we are        */
/*      writing to.                                                     */
/* -------------------------------------------------------------------- */
    if( bJP2Writing )
    {
        GIntBig nPixelCount = nRasterXSize * ((GIntBig) nRasterYSize) * 
            nBands;

        NITFPatchImageLength( GetDescription(), nImageStart, nPixelCount );
    }

/* -------------------------------------------------------------------- */
/*      If we have a jpeg output file, make sure it gets closed         */
/*      and flushed out.                                                */
/* -------------------------------------------------------------------- */
    if( poJPEGDataset != NULL )
    {
        int i;

        GDALClose( (GDALDatasetH) poJPEGDataset );
        
        // the bands are really jpeg bands ... remove them 
        // from the NITF list so they won't get destroyed twice.
        for( i = 0; i < nBands && papoBands != NULL; i++ )
            papoBands[i] = NULL;
    }

    CSLDestroy( papszCGMMetadata );
    CPLFree( panJPEGBlockOffset );
    CPLFree( pabyJPEGBlock );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void NITFDataset::FlushCache()

{
    if( poJ2KDataset != NULL && bJP2Writing)
        poJ2KDataset->FlushCache();

    GDALPamDataset::FlushCache();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *NITFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int nIMIndex = -1;
    const char *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Select a specific subdataset.                                   */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszFilename, "NITF_IM:",8) )
    {
        pszFilename += 8;
        nIMIndex = atoi(pszFilename);
        
        while( *pszFilename != '\0' && *pszFilename != ':' )
            pszFilename++;

        if( *pszFilename == ':' )
            pszFilename++;
    }

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    else
    {
        if( poOpenInfo->nHeaderBytes < 4 )
            return NULL;
        
        if( !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) 
            && !EQUALN((char *) poOpenInfo->pabyHeader,"NSIF",4)
            && !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) )
            return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      Open the file with library.                                     */
/* -------------------------------------------------------------------- */
    NITFFile *psFile;

    psFile = NITFOpen( pszFilename, poOpenInfo->eAccess == GA_Update );
    if( psFile == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Is there an image to operate on?                                */
/* -------------------------------------------------------------------- */
    int iSegment, nThisIM = 0;
    NITFImage *psImage = NULL;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        if( EQUAL(psFile->pasSegmentInfo[iSegment].szSegmentType,"IM") 
            && (nThisIM++ == nIMIndex || nIMIndex == -1) )
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
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "The file %s appears to be an NITF file, but no image\n"
                  "blocks were found on it.", 
                  poOpenInfo->pszFilename );
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    NITFDataset 	*poDS;

    poDS = new NITFDataset();

    poDS->psFile = psFile;
    poDS->psImage = psImage;
    poDS->eAccess = poOpenInfo->eAccess;
    poDS->osNITFFilename = pszFilename;

    if( psImage )
    {
        poDS->nRasterXSize = psImage->nCols;
        poDS->nRasterYSize = psImage->nRows;
    }
    else
    {
        poDS->nRasterXSize = 1;
        poDS->nRasterYSize = 1;
    }

/* -------------------------------------------------------------------- */
/*      If the image is JPEG2000 (C8) compressed, we will need to       */
/*      open the image data as a JPEG2000 dataset.                      */
/* -------------------------------------------------------------------- */
    int nUsableBands = 0;
    int		iBand;

    if( psImage )
        nUsableBands = psImage->nBands;

    if( psImage != NULL && EQUAL(psImage->szIC,"C8") )
    {
        char *pszDSName = CPLStrdup( 
            CPLSPrintf( "J2K_SUBFILE:%d,%d,%s", 
                        psFile->pasSegmentInfo[iSegment].nSegmentStart,
                        psFile->pasSegmentInfo[iSegment].nSegmentSize,
                        pszFilename ) );

        if( poWritableJ2KDataset != NULL )
        {
            poDS->poJ2KDataset = poWritableJ2KDataset; 
            poDS->bJP2Writing = TRUE;
            poWritableJ2KDataset = NULL;
        }
        else
        {
            poDS->poJ2KDataset = (GDALDataset *) 
                GDALOpen( pszDSName, GA_ReadOnly );
            CPLFree( pszDSName );
        }

        if( poDS->poJ2KDataset == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to open JPEG2000 image within NITF file.\n"
                      "Is the JP2KAK driver available?" );
            delete poDS;
            return NULL;
        }

        if( poDS->poJ2KDataset->GetRasterCount() < nUsableBands )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "JPEG2000 data stream has less useful bands than expected, likely\n"
                      "because some channels have differing resolutions." );
            
            nUsableBands = poDS->poJ2KDataset->GetRasterCount();
        }

        // Force NITF derived color space info 
        for( iBand = 0; iBand < nUsableBands; iBand++ )
        {
            NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;
            GDALRasterBand *poBand=poDS->poJ2KDataset->GetRasterBand(iBand+1);
            
            if( EQUAL(psBandInfo->szIREPBAND,"R") )
                poBand->SetColorInterpretation( GCI_RedBand );
            if( EQUAL(psBandInfo->szIREPBAND,"G") )
                poBand->SetColorInterpretation( GCI_GreenBand );
            if( EQUAL(psBandInfo->szIREPBAND,"B") )
                poBand->SetColorInterpretation( GCI_BlueBand );
            if( EQUAL(psBandInfo->szIREPBAND,"M") )
                poBand->SetColorInterpretation( GCI_GrayIndex );
            if( EQUAL(psBandInfo->szIREPBAND,"Y") )
                poBand->SetColorInterpretation( GCI_YCbCr_YBand );
            if( EQUAL(psBandInfo->szIREPBAND,"Cb") )
                poBand->SetColorInterpretation( GCI_YCbCr_CbBand );
            if( EQUAL(psBandInfo->szIREPBAND,"Cr") )
                poBand->SetColorInterpretation( GCI_YCbCr_CrBand );
        }
    }

/* -------------------------------------------------------------------- */
/*      If the image is JPEG (C3) compressed, we will need to open      */
/*      the image data as a JPEG dataset.                               */
/* -------------------------------------------------------------------- */
    else if( psImage != NULL
             && EQUAL(psImage->szIC,"C3") 
             && psImage->nBlocksPerRow == 1
             && psImage->nBlocksPerColumn == 1 )
    {
        GUInt32 nJPEGStart = psFile->pasSegmentInfo[iSegment].nSegmentStart;

        poDS->nQLevel = poDS->ScanJPEGQLevel( &nJPEGStart );

        char *pszDSName = CPLStrdup( 
            CPLSPrintf( "JPEG_SUBFILE:Q%d,%d,%d,%s", 
                        poDS->nQLevel,
                        nJPEGStart,
                        psFile->pasSegmentInfo[iSegment].nSegmentSize
                        - (nJPEGStart - psFile->pasSegmentInfo[iSegment].nSegmentStart),
                        pszFilename ) );

        CPLDebug( "GDAL", 
                  "NITFDataset::Open() as IC=C3 (JPEG compressed)\n");

        poDS->poJPEGDataset = (GDALDataset *) GDALOpen( pszDSName, GA_ReadOnly );
        CPLFree( pszDSName );

        if( poDS->poJPEGDataset == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to open JPEG image within NITF file.\n"
                      "Is the JPEG driver available?" );
            delete poDS;
            return NULL;
        }

        if( poDS->poJPEGDataset->GetRasterCount() < nUsableBands )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "JPEG data stream has less useful bands than expected, likely\n"
                      "because some channels have differing resolutions." );
            
            nUsableBands = poDS->poJPEGDataset->GetRasterCount();
        }

        // Force NITF derived color space info 
        for( iBand = 0; iBand < nUsableBands; iBand++ )
        {
            NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;
            GDALRasterBand *poBand=poDS->poJPEGDataset->GetRasterBand(iBand+1);
            
            if( EQUAL(psBandInfo->szIREPBAND,"R") )
                poBand->SetColorInterpretation( GCI_RedBand );
            if( EQUAL(psBandInfo->szIREPBAND,"G") )
                poBand->SetColorInterpretation( GCI_GreenBand );
            if( EQUAL(psBandInfo->szIREPBAND,"B") )
                poBand->SetColorInterpretation( GCI_BlueBand );
            if( EQUAL(psBandInfo->szIREPBAND,"M") )
                poBand->SetColorInterpretation( GCI_GrayIndex );
            if( EQUAL(psBandInfo->szIREPBAND,"Y") )
                poBand->SetColorInterpretation( GCI_YCbCr_YBand );
            if( EQUAL(psBandInfo->szIREPBAND,"Cb") )
                poBand->SetColorInterpretation( GCI_YCbCr_CbBand );
            if( EQUAL(psBandInfo->szIREPBAND,"Cr") )
                poBand->SetColorInterpretation( GCI_YCbCr_CrBand );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < nUsableBands; iBand++ )
    {
        if( poDS->poJ2KDataset != NULL )
            poDS->SetBand( iBand+1, 
                           poDS->poJ2KDataset->GetRasterBand(iBand+1) );
        else if( poDS->poJPEGDataset != NULL )
            poDS->SetBand( iBand+1, 
                           poDS->poJPEGDataset->GetRasterBand(iBand+1) );
        else
            poDS->SetBand( iBand+1, new NITFRasterBand( poDS, iBand+1 ) );
    }

/* -------------------------------------------------------------------- */
/*      Report problems with odd bit sizes.                             */
/* -------------------------------------------------------------------- */
    if( psImage != NULL 
        && (psImage->nBitsPerSample < 8 || psImage->nBitsPerSample % 8 != 0) )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Image with %d bits per sample will not be interpreted properly.", 
                  psImage->nBitsPerSample );
    }

/* -------------------------------------------------------------------- */
/*      Process the projection from the ICORDS.                         */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRSWork;

    if( psImage == NULL )
    {
        /* nothing */
    }
    else if( psImage->chICORDS == 'G'  || psImage->chICORDS == 'D' )
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
/*      Try looking for a .nfw file.                                    */
/* -------------------------------------------------------------------- */
    if( psImage
        && GDALReadWorldFile( pszFilename, "nfw", 
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
        pszHDR = CPLResetExtension( pszFilename, "hdr" );
        
        fpHDR = VSIFOpen( pszHDR, "rt" );

#ifndef WIN32
        if( fpHDR == NULL )
        {
            pszHDR = CPLResetExtension( pszFilename, "HDR" );
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
                else
                {
                    /* Couldn't find associated projection info.
                       Go back to original file for geotransform.
                    */
                    poDS->bGotGeoTransform = FALSE;
                }
            }
            else
                poDS->bGotGeoTransform = FALSE;
            CSLDestroy(papszLines);
        }
        else
            poDS->bGotGeoTransform = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have IGEOLO data that can be treated as a                 */
/*      geotransform?  Our approach should support images in an         */
/*      affine rotated frame of reference.                              */
/* -------------------------------------------------------------------- */
    int nGCPCount = 0;
    GDAL_GCP    *psGCPs = NULL;

    if( psImage && !poDS->bGotGeoTransform && psImage->chICORDS != ' ' )
    {
        nGCPCount = 4;

        psGCPs = (GDAL_GCP *) CPLMalloc(sizeof(GDAL_GCP) * nGCPCount);
        GDALInitGCPs( nGCPCount, psGCPs );

        psGCPs[0].dfGCPPixel	= 0.0;
        psGCPs[0].dfGCPLine		= 0.0;
        psGCPs[0].dfGCPX		= psImage->dfULX;
        psGCPs[0].dfGCPY		= psImage->dfULY;

        psGCPs[1].dfGCPPixel = poDS->nRasterXSize;
        psGCPs[1].dfGCPLine = 0.0;
        psGCPs[1].dfGCPX		= psImage->dfURX;
        psGCPs[1].dfGCPY		= psImage->dfURY;

        psGCPs[2].dfGCPPixel = poDS->nRasterXSize;
        psGCPs[2].dfGCPLine = poDS->nRasterYSize;
        psGCPs[2].dfGCPX		= psImage->dfLRX;
        psGCPs[2].dfGCPY		= psImage->dfLRY;

        psGCPs[3].dfGCPPixel = 0.0;
        psGCPs[3].dfGCPLine = poDS->nRasterYSize;
        psGCPs[3].dfGCPX		= psImage->dfLLX;
        psGCPs[3].dfGCPY		= psImage->dfLLY;
    }

/* -------------------------------------------------------------------- */
/*      Convert the GCPs into a geotransform definition, if possible.   */
/* -------------------------------------------------------------------- */
    if( !psImage )
    {
        /* nothing */
    }
    else if( poDS->bGotGeoTransform == FALSE 
             && nGCPCount > 0 
             && GDALGCPsToGeoTransform( nGCPCount, psGCPs, 
                                        poDS->adfGeoTransform, TRUE ) )
    {	
        poDS->bGotGeoTransform = TRUE;
    } 

/* -------------------------------------------------------------------- */
/*      If we have IGEOLO that isn't north up, return it as GCPs.       */
/* -------------------------------------------------------------------- */
    else if( (psImage->dfULX != 0 || psImage->dfURX != 0 
              || psImage->dfLRX != 0 || psImage->dfLLX != 0)
             && psImage->chICORDS != ' ' && 
             ( poDS->bGotGeoTransform == FALSE ) )
    {
        CPLDebug( "GDAL", 
                  "NITFDataset::Open() wasn't able to derive a first order\n"
                  "geotransform.  It will be returned as GCPs.");

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

    // This cleans up the original copy of the GCPs used to test if 
    // this IGEOLO could be used for a geotransform.
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, psGCPs );
        CPLFree( psGCPs );
    }

/* -------------------------------------------------------------------- */
/*      Do we have metadata.                                            */
/* -------------------------------------------------------------------- */
    char **papszMergedMD;
    char **papszUSE00A_MD;

    // File and Image level metadata.
    papszMergedMD = CSLDuplicate( poDS->psFile->papszMetadata );

    if( psImage )
    {
        papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                          CSLCount( papszMergedMD ),
                                          psImage->papszMetadata );

        // Comments.
        if( psImage->pszComments != NULL && strlen(psImage->pszComments) != 0 )
            papszMergedMD = CSLSetNameValue( 
                papszMergedMD, "NITF_IMAGE_COMMENTS", psImage->pszComments );
        
        // Compression code. 
        papszMergedMD = CSLSetNameValue( papszMergedMD, "NITF_IC", 
                                         psImage->szIC );
        
        // IMODE
        char szIMODE[2];
        szIMODE[0] = psImage->chIMODE;
        szIMODE[1] = '\0';
        papszMergedMD = CSLSetNameValue( papszMergedMD, "NITF_IMODE", szIMODE );

        // ILOC/Attachment info
        if( psImage->nILOCRow != 0 )
        {
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IDLVL", 
                                 CPLString().Printf("%d",psImage->nIDLVL) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IALVL", 
                                 CPLString().Printf("%d",psImage->nIALVL) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_ILOC_ROW", 
                                 CPLString().Printf("%d",psImage->nILOCRow) );
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_ILOC_COLUMN", 
                                CPLString().Printf("%d",psImage->nILOCColumn));
            papszMergedMD = 
                CSLSetNameValue( papszMergedMD, "NITF_IMAG", 
                                 psImage->szIMAG );
        }

        // USE00A 
        papszUSE00A_MD = NITFReadUSE00A( psImage );
        if( papszUSE00A_MD != NULL )
        {
            papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                              CSLCount( papszUSE00A_MD ),
                                              papszUSE00A_MD );
            CSLDestroy( papszUSE00A_MD );
        }
        
        papszUSE00A_MD = NITFReadSTDIDC( psImage );
        if( papszUSE00A_MD != NULL )
        {
            papszMergedMD = CSLInsertStrings( papszMergedMD, 
                                              CSLCount( papszUSE00A_MD ),
                                              papszUSE00A_MD );
            CSLDestroy( papszUSE00A_MD );
        }
    }
        
    poDS->SetMetadata( papszMergedMD );
    CSLDestroy( papszMergedMD );

/* -------------------------------------------------------------------- */
/*      Image structure metadata.                                       */
/* -------------------------------------------------------------------- */
    if( psImage == NULL )
        /* do nothing */;
    else if( psImage->szIC[1] == '1' )
        poDS->SetMetadataItem( "COMPRESSION", "BILEVEL", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '2' )
        poDS->SetMetadataItem( "COMPRESSION", "???", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '3' )
        poDS->SetMetadataItem( "COMPRESSION", "JPEG", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '4' )
        poDS->SetMetadataItem( "COMPRESSION", "VECTOR QUANTIZATION", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '5' )
        poDS->SetMetadataItem( "COMPRESSION", "LOSSLESS JPEG", 
                               "IMAGE_STRUCTURE" );
    else if( psImage->szIC[1] == '8' )
        poDS->SetMetadataItem( "COMPRESSION", "JPEG2000", 
                               "IMAGE_STRUCTURE" );
    
/* -------------------------------------------------------------------- */
/*      Do we have RPC info.                                            */
/* -------------------------------------------------------------------- */
    NITFRPC00BInfo sRPCInfo;

    if( psImage
        && NITFReadRPC00B( psImage, &sRPCInfo ) && sRPCInfo.SUCCESS )
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

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LONG_OFF - ( sRPCInfo.LONG_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "RPC_MIN_LONG", szValue );

        sprintf( szValue, "%.16g",
                 sRPCInfo.LONG_OFF + ( sRPCInfo.LONG_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "RPC_MAX_LONG", szValue );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LAT_OFF - ( sRPCInfo.LAT_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "RPC_MIN_LAT", szValue );

        sprintf( szValue, "%.16g", 
                 sRPCInfo.LAT_OFF + ( sRPCInfo.LAT_SCALE / 2.0 ) );
        poDS->SetMetadataItem( "RPC_MAX_LAT", szValue );
    }

/* -------------------------------------------------------------------- */
/*      Do we have Chip info?                                            */
/* -------------------------------------------------------------------- */
    NITFICHIPBInfo sChipInfo;

    if( psImage
        && NITFReadICHIPB( psImage, &sChipInfo ) && sChipInfo.XFRM_FLAG == 0 )
    {
        char szValue[1280];

        sprintf( szValue, "%.16g", sChipInfo.SCALE_FACTOR );
        poDS->SetMetadataItem( "ICHIP_SCALE_FACTOR", szValue );

        sprintf( szValue, "%d", sChipInfo.ANAMORPH_CORR );
        poDS->SetMetadataItem( "ICHIP_ANAMORPH_CORR", szValue );

        sprintf( szValue, "%d", sChipInfo.SCANBLK_NUM );
        poDS->SetMetadataItem( "ICHIP_SCANBLK_NUM", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_11 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_11 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_12 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_12 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_21 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_21 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_ROW_22 );
        poDS->SetMetadataItem( "ICHIP_OP_ROW_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.OP_COL_22 );
        poDS->SetMetadataItem( "ICHIP_OP_COL_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_11 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_11 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_11", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_12 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_12 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_12", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_21 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_21 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_21", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_ROW_22 );
        poDS->SetMetadataItem( "ICHIP_FI_ROW_22", szValue );

        sprintf( szValue, "%.16g", sChipInfo.FI_COL_22 );
        poDS->SetMetadataItem( "ICHIP_FI_COL_22", szValue );

        sprintf( szValue, "%d", sChipInfo.FI_ROW );
        poDS->SetMetadataItem( "ICHIP_FI_ROW", szValue );

        sprintf( szValue, "%d", sChipInfo.FI_COL );
        poDS->SetMetadataItem( "ICHIP_FI_COL", szValue );

    }

/* -------------------------------------------------------------------- */
/*      If there are multiple image segments, and we are the zeroth,    */
/*      then setup the subdataset metadata.                             */
/* -------------------------------------------------------------------- */
    if( nIMIndex == -1 )
    {
        char **papszSubdatasets = NULL;
        int nIMCounter = 0;

        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            if( EQUAL(psFile->pasSegmentInfo[iSegment].szSegmentType,"IM") )
            {
                CPLString oName;
                CPLString oValue;

                oName.Printf( "SUBDATASET_%d_NAME", nIMCounter+1 );
                oValue.Printf( "NITF_IM:%d:%s", nIMCounter, pszFilename );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                    oName, oValue );

                oName.Printf( "SUBDATASET_%d_DESC", nIMCounter+1 );
                oValue.Printf( "Image %d of %s", nIMCounter+1, pszFilename );
                papszSubdatasets = CSLSetNameValue( papszSubdatasets, 
                                                    oName, oValue );

                nIMCounter++;
            }
        }

        if( CSLCount(papszSubdatasets) > 2 )
            poDS->GDALMajorObject::SetMetadata( papszSubdatasets, "SUBDATASETS" );
        
        CSLDestroy( papszSubdatasets );
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr NITFDataset::AdviseRead( int nXOff, int nYOff, int nXSize, int nYSize,
                                int nBufXSize, int nBufYSize, 
                                GDALDataType eDT, 
                                int nBandCount, int *panBandList,
                                char **papszOptions )
    
{
    if( poJ2KDataset == NULL )
        return GDALDataset::AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                        nBufXSize, nBufYSize, eDT, 
                                        nBandCount, panBandList, 
                                        papszOptions);
    else if( poJPEGDataset != NULL )
        return poJPEGDataset->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                          nBufXSize, nBufYSize, eDT, 
                                          nBandCount, panBandList, 
                                          papszOptions);
    else
        return poJ2KDataset->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
                                         nBufXSize, nBufYSize, eDT, 
                                         nBandCount, panBandList, 
                                         papszOptions);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr NITFDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType, 
                               int nBandCount, int *panBandMap,
                               int nPixelSpace, int nLineSpace, int nBandSpace)
    
{
    if( poJ2KDataset != NULL )
        return poJ2KDataset->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nBandCount, panBandMap, 
                                       nPixelSpace, nLineSpace, nBandSpace );
    else if( poJPEGDataset != NULL )
        return poJPEGDataset->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        pData, nBufXSize, nBufYSize, eBufType,
                                        nBandCount, panBandMap, 
                                        nPixelSpace, nLineSpace, nBandSpace );
    else 
        return GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nBandCount, panBandMap, 
                                       nPixelSpace, nLineSpace, nBandSpace );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::GetGeoTransform( double *padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );

    if( bGotGeoTransform )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NITFDataset::SetGeoTransform( double *padfGeoTransform )

{
    double dfIGEOLOULX, dfIGEOLOULY, dfIGEOLOURX, dfIGEOLOURY, 
           dfIGEOLOLRX, dfIGEOLOLRY, dfIGEOLOLLX, dfIGEOLOLLY;


    dfIGEOLOULX = padfGeoTransform[0] + 0.5 * padfGeoTransform[1] 
                                      + 0.5 * padfGeoTransform[2];
    dfIGEOLOULY = padfGeoTransform[3] + 0.5 * padfGeoTransform[4] 
                                      + 0.5 * padfGeoTransform[5];
    dfIGEOLOURX = dfIGEOLOULX + padfGeoTransform[1] * (nRasterXSize - 1);
    dfIGEOLOURY = dfIGEOLOULY + padfGeoTransform[4] * (nRasterXSize - 1);
    dfIGEOLOLRX = dfIGEOLOULX + padfGeoTransform[1] * (nRasterXSize - 1)
                              + padfGeoTransform[2] * (nRasterYSize - 1);
    dfIGEOLOLRY = dfIGEOLOULY + padfGeoTransform[4] * (nRasterXSize - 1)
                              + padfGeoTransform[5] * (nRasterYSize - 1);
    dfIGEOLOLLX = dfIGEOLOULX + padfGeoTransform[2] * (nRasterYSize - 1);
    dfIGEOLOLLY = dfIGEOLOULY + padfGeoTransform[5] * (nRasterYSize - 1);

    if( NITFWriteIGEOLO( psImage, psImage->chICORDS, 
                         psImage->nZone, 
                         dfIGEOLOULX, dfIGEOLOULY, dfIGEOLOURX, dfIGEOLOURY, 
                         dfIGEOLOLRX, dfIGEOLOLRY, dfIGEOLOLLX, dfIGEOLOLLY ) )
        return CE_None;
    else
        return CE_Failure;
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
/*                       InitializeCGMMetadata()                        */
/************************************************************************/

void NITFDataset::InitializeCGMMetadata()

{
    int iSegment;
    int iCGM = 0;

    if( papszCGMMetadata != NULL )
        return;

    papszCGMMetadata = 
        CSLSetNameValue( papszCGMMetadata, "SEGMENT_COUNT", "0" );

/* ==================================================================== */
/*      Process all graphics segments.                                  */
/* ==================================================================== */
    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegment = psFile->pasSegmentInfo + iSegment;

        if( !EQUAL(psSegment->szSegmentType,"GR") 
            && !EQUAL(psSegment->szSegmentType,"SY") )
            continue;

/* -------------------------------------------------------------------- */
/*      Load the graphic subheader.                                     */
/* -------------------------------------------------------------------- */
        char achSubheader[298];
        int  nSTYPEOffset;

        if( VSIFSeekL( psFile->fp, psSegment->nSegmentHeaderStart, 
                       SEEK_SET ) != 0 
            || VSIFReadL( achSubheader, 1, sizeof(achSubheader), 
                          psFile->fp ) < 258 )
        {
            CPLError( CE_Warning, CPLE_FileIO, 
                      "Failed to read graphic subheader at %d.", 
                      psSegment->nSegmentHeaderStart );
            return;
        }

        // NITF 2.0. (also works for NITF 2.1)
        nSTYPEOffset = 200;
        if( EQUALN(achSubheader+193,"999998",6) )
            nSTYPEOffset += 40;

        // We don't want bitmaps or anything other than cgm for now.
        if( achSubheader[nSTYPEOffset] != 'C' )
            continue;

        char szSLOC_R[6], szSLOC_C[6];

        strncpy( szSLOC_R, achSubheader + nSTYPEOffset + 20, 5 );
        strncpy( szSLOC_C, achSubheader + nSTYPEOffset + 20 + 5, 5 );
        
        szSLOC_R[5] = '\0';
        szSLOC_C[5] = '\0';

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SLOC_ROW", iCGM), 
                             szSLOC_R );
        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_SLOC_COL", iCGM), 
                             szSLOC_C );

/* -------------------------------------------------------------------- */
/*      Load the raw CGM data itself.                                   */
/* -------------------------------------------------------------------- */
        char *pabyCGMData, *pszEscapedCGMData;

        pabyCGMData = (char *) CPLCalloc(1,psSegment->nSegmentSize);
        if( VSIFSeekL( psFile->fp, psSegment->nSegmentStart, 
                       SEEK_SET ) != 0 
            || VSIFReadL( pabyCGMData, 1, psSegment->nSegmentSize, 
                          psFile->fp ) != psSegment->nSegmentSize )
        {
            CPLError( CE_Warning, CPLE_FileIO, 
                      "Failed to read %d bytes of graphic data at %d.", 
                      psSegment->nSegmentSize,
                      psSegment->nSegmentStart );
            return;
        }

        pszEscapedCGMData = CPLEscapeString( pabyCGMData, 
                                             psSegment->nSegmentSize, 
                                             CPLES_BackslashQuotable );

        papszCGMMetadata = 
            CSLSetNameValue( papszCGMMetadata, 
                             CPLString().Printf("SEGMENT_%d_DATA", iCGM), 
                             pszEscapedCGMData );
        CPLFree( pszEscapedCGMData );
        CPLFree( pabyCGMData );

        iCGM++;
    }

/* -------------------------------------------------------------------- */
/*      Record the CGM segment count.                                   */
/* -------------------------------------------------------------------- */
    papszCGMMetadata = 
        CSLSetNameValue( papszCGMMetadata, 
                         "SEGMENT_COUNT", 
                         CPLString().Printf( "%d", iCGM ) );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **NITFDataset::GetMetadata( const char * pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain,"CGM") )
        return GDALPamDataset::GetMetadata( pszDomain );

    InitializeCGMMetadata();

    return papszCGMMetadata;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *NITFDataset::GetMetadataItem(const char * pszName,
                                         const char * pszDomain )

{
    if( pszDomain == NULL || !EQUAL(pszDomain,"CGM") )
        return GDALPamDataset::GetMetadataItem( pszName, pszDomain );

    InitializeCGMMetadata();

    return CSLFetchNameValue( papszCGMMetadata, pszName );
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
/*                           ScanJPEGQLevel()                           */
/*                                                                      */
/*      Search the NITF APP header in the jpeg data stream to find      */
/*      out what predefined Q level tables should be used (or -1 if     */
/*      they are inline).                                               */
/************************************************************************/

int NITFDataset::ScanJPEGQLevel( GUInt32 *pnDataStart )

{
    GByte abyHeader[100];

    if( VSIFSeekL( psFile->fp, *pnDataStart,
                   SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Seek error to jpeg data stream." );
        return 0;
    }
        
    if( VSIFReadL( abyHeader, 1, sizeof(abyHeader), psFile->fp ) 
        < sizeof(abyHeader) )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Read error to jpeg data stream." );
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Scan ahead for jpeg magic code.  In some files (eg. NSIF)       */
/*      there seems to be some extra junk before the image data stream. */
/* -------------------------------------------------------------------- */
    GUInt32 nOffset = 0;
    while( nOffset < sizeof(abyHeader) - 23 
           && (abyHeader[nOffset+0] != 0xff
               || abyHeader[nOffset+1] != 0xd8
               || abyHeader[nOffset+2] != 0xff) )
        nOffset++;

    if( nOffset >= sizeof(abyHeader) - 23 )
        return 0;

    *pnDataStart += nOffset;

    if( nOffset > 0 )
        CPLDebug( "NITF", 
                  "JPEG data stream at offset %d from start of data segement, NSIF?", 
                  nOffset );

/* -------------------------------------------------------------------- */
/*      Do we have an NITF app tag?  If so, pull out the Q level.       */
/* -------------------------------------------------------------------- */
    if( !EQUAL((char *)abyHeader+nOffset+6,"NITF") )
        return 0;

    return abyHeader[22+nOffset];
}

/************************************************************************/
/*                           ScanJPEGBlocks()                           */
/************************************************************************/

CPLErr NITFDataset::ScanJPEGBlocks()

{
    int iBlock;
    GUInt32 nJPEGStart = 
        psFile->pasSegmentInfo[psImage->iSegment].nSegmentStart;

    nQLevel = ScanJPEGQLevel( &nJPEGStart );

/* -------------------------------------------------------------------- */
/*      Allocate offset array                                           */
/* -------------------------------------------------------------------- */
    panJPEGBlockOffset = (int *) 
        CPLCalloc(sizeof(int),
                  psImage->nBlocksPerRow*psImage->nBlocksPerColumn+1);
    panJPEGBlockOffset[0] = nJPEGStart;

    for( iBlock = psImage->nBlocksPerRow * psImage->nBlocksPerColumn - 1;
         iBlock > 0; iBlock-- )
        panJPEGBlockOffset[iBlock] = -1;
    
/* -------------------------------------------------------------------- */
/*      Scan through the whole image data stream identifying all        */
/*      block boundaries.  Each block starts with 0xFFD8 (SOI).         */
/*      They also end with 0xFFD9, but we don't currently look for      */
/*      that.                                                           */
/* -------------------------------------------------------------------- */
    int iNextBlock = 1;
    int iSegOffset = 2;
    int iSegSize = psFile->pasSegmentInfo[psImage->iSegment].nSegmentSize
        - (nJPEGStart - psFile->pasSegmentInfo[psImage->iSegment].nSegmentStart);
    GByte abyBlock[512];

    while( iSegOffset < iSegSize-1 )
    {
        int nReadSize = MIN((int)sizeof(abyBlock),iSegSize - iSegOffset);
        int i;

        if( VSIFSeekL( psFile->fp, panJPEGBlockOffset[0] + iSegOffset, 
                       SEEK_SET ) != 0 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Seek error to jpeg data stream." );
            return CE_Failure;
        }
        
        if( VSIFReadL( abyBlock, 1, nReadSize, psFile->fp ) < (size_t)nReadSize)
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Read error to jpeg data stream." );
            return CE_Failure;
        }
        
        for( i = 0; i < nReadSize-1; i++ )
        {
            if( abyBlock[i] == 0xff && abyBlock[i+1] == 0xd8 )
            {
                CPLAssert( iNextBlock 
                          <= psImage->nBlocksPerRow*psImage->nBlocksPerColumn );

                panJPEGBlockOffset[iNextBlock++] 
                    = panJPEGBlockOffset[0] + iSegOffset + i; 
            }
        }

        iSegOffset += nReadSize - 1;
    }

    return CE_None;
}

/************************************************************************/
/*                           ReadJPEGBlock()                            */
/************************************************************************/

CPLErr NITFDataset::ReadJPEGBlock( int iBlockX, int iBlockY )

{
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      If this is our first request, do a scan for block boundaries.   */
/* -------------------------------------------------------------------- */
    if( panJPEGBlockOffset == NULL )
    {
        eErr = ScanJPEGBlocks();
        if( eErr != CE_None )
            return eErr;
    }
    
/* -------------------------------------------------------------------- */
/*    Allocate image data block (where the uncompressed image will go)  */
/* -------------------------------------------------------------------- */
    if( pabyJPEGBlock == NULL )
    {
        // this is really 8bit only for now. 
        pabyJPEGBlock = (GByte *) 
            CPLCalloc(psImage->nBands,
                      psImage->nBlockWidth * psImage->nBlockHeight);
    }

/* -------------------------------------------------------------------- */
/*      Read JPEG Chunk.                                                */
/* -------------------------------------------------------------------- */
    CPLString osFilename;
    int iBlock = iBlockX + iBlockY * psImage->nBlocksPerRow;
    GDALDataset *poDS;
    int anBands[3] = { 1, 2, 3 };

    osFilename.Printf( "JPEG_SUBFILE:Q%d,%d,%d,%s", 
                       nQLevel,
                       panJPEGBlockOffset[iBlock], 0, 
                       osNITFFilename.c_str() );

    poDS = (GDALDataset *) GDALOpen( osFilename, GA_ReadOnly );
    if( poDS == NULL )
        return CE_Failure;

    if( poDS->GetRasterXSize() != psImage->nBlockWidth
        || poDS->GetRasterYSize() != psImage->nBlockHeight )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "JPEG block %d not same size as NITF blocksize.", 
                  iBlock );
        return CE_Failure;
    }
                       
    eErr = poDS->RasterIO( GF_Read, 
                           0, 0, 
                           psImage->nBlockWidth, psImage->nBlockHeight,
                           pabyJPEGBlock, 
                           psImage->nBlockWidth, psImage->nBlockHeight,
                           GDT_Byte, psImage->nBands, anBands, 0, 0, 0 );

    delete poDS;

    return eErr;
}

/************************************************************************/
/*                         GDALToNITFDataType()                         */
/************************************************************************/

static const char *GDALToNITFDataType( GDALDataType eType )

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

    return pszPVType;
}

/************************************************************************/
/*                           NITFJP2Options()                           */
/*                                                                      */
/*      Prepare JP2-in-NITF creation options based in part of the       */
/*      NITF creation options.                                          */
/************************************************************************/

static char **NITFJP2Options( char **papszOptions )

{
    int i;
    static char *apszOptions[] = { 
        "PROFILE=NPJE", 
        "CODESTREAM_ONLY=TRUE", 
        NULL,
        NULL };
    
    apszOptions[2] = NULL;
    for( i = 0; papszOptions != NULL && papszOptions[i] != NULL; i++ )
    {
        if( EQUALN(papszOptions[i],"PROFILE=",8) )
            apszOptions[0] = papszOptions[i];
        if( EQUALN(papszOptions[i],"TARGET=",7) )
            apszOptions[2] = papszOptions[i];
    }

    return apszOptions;
}

/************************************************************************/
/*                         NITFDatasetCreate()                          */
/************************************************************************/

static GDALDataset *
NITFDatasetCreate( const char *pszFilename, int nXSize, int nYSize, int nBands,
                   GDALDataType eType, char **papszOptions )

{
    const char *pszPVType = GDALToNITFDataType( eType );
    const char *pszIC = CSLFetchNameValue( papszOptions, "IC" );

    if( pszPVType == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We disallow any IC value except NC when creating this way.      */
/* -------------------------------------------------------------------- */
    GDALDriver *poJ2KDriver = NULL;

    if( pszIC != NULL && EQUAL(pszIC,"C8") )
    {
        int bHasCreate = FALSE;

        poJ2KDriver = GetGDALDriverManager()->GetDriverByName( "JP2ECW" );
        if( poJ2KDriver != NULL )
            bHasCreate = poJ2KDriver->GetMetadataItem( GDAL_DCAP_CREATE, 
                                                       NULL ) != NULL;
        if( !bHasCreate )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create JPEG2000 encoded NITF files.  The\n"
                      "JP2ECW driver is unavailable, or missing Create support." );
            return NULL;
        }
    }

    else if( pszIC != NULL && !EQUAL(pszIC,"NC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported compression (IC=%s) used in direct\n"
                  "NITF File creation", 
                  pszIC );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the file.                                                */
/* -------------------------------------------------------------------- */

    if( !NITFCreate( pszFilename, nXSize, nYSize, nBands, 
                     GDALGetDataTypeSize( eType ), pszPVType, 
                     papszOptions ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Various special hacks related to JPEG2000 encoded files.        */
/* -------------------------------------------------------------------- */
    if( poJ2KDriver )
    {
        NITFFile *psFile = NITFOpen( pszFilename, TRUE );
        int nImageOffset = psFile->pasSegmentInfo[0].nSegmentStart;

        char *pszDSName = CPLStrdup( 
            CPLSPrintf( "J2K_SUBFILE:%d,%d,%s", nImageOffset, -1,
                        pszFilename ) );

        NITFClose( psFile );

        poWritableJ2KDataset = 
            poJ2KDriver->Create( pszDSName, nXSize, nYSize, nBands, eType, 
                                 NITFJP2Options( papszOptions ) );
        CPLFree( pszDSName );

        if( poWritableJ2KDataset == NULL )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the dataset in update mode.                                */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                           NITFCreateCopy()                           */
/************************************************************************/

GDALDataset *
NITFDataset::NITFCreateCopy( 
    const char *pszFilename, GDALDataset *poSrcDS,
    int bStrict, char **papszOptions, 
    GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALDataType eType;
    GDALRasterBand *poBand1 = poSrcDS->GetRasterBand(1);
    char  **papszFullOptions = CSLDuplicate( papszOptions );
    int   bJPEG2000 = FALSE;
    NITFDataset *poDstDS = NULL;
    GDALDriver *poJ2KDriver = NULL;

    if( poBand1 == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We disallow any IC value except NC when creating this way.      */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszOptions, "IC" ) != NULL )
    {
        if( EQUAL(CSLFetchNameValue( papszOptions, "IC" ),"NC") )
            /* ok */;
        else if( EQUAL(CSLFetchNameValue( papszOptions, "IC" ),"C8") )
        {
            poJ2KDriver = 
                GetGDALDriverManager()->GetDriverByName( "JP2ECW" );
            if( poJ2KDriver == NULL )
            {
                CPLError( 
                    CE_Failure, CPLE_AppDefined, 
                    "Unable to write JPEG2000 compressed NITF file.\n"
                    "No 'subfile' JPEG2000 write supporting drivers are\n"
                    "configured." );
                return NULL;
            }
            bJPEG2000 = TRUE;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Only IC=NC (uncompressed) and IC=C8 (JPEG2000) allowed\n"
                      "with NITF CreateCopy method." );
            return NULL;
        }
    }

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
    int    bNorth, nZone = 0;
    OGRSpatialReference oSRS;
    char *pszWKT = (char *) poSrcDS->GetProjectionRef();

    if( pszWKT != NULL )
        oSRS.importFromWkt( &pszWKT );

    // For now we write all geographic coordinate systems whether WGS84 or not.
    // But really NITF is always WGS84 and we ought to reflect that. 
    if( oSRS.IsGeographic() && oSRS.GetPrimeMeridian() == 0.0 
        && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        papszFullOptions = 
            CSLSetNameValue( papszFullOptions, "ICORDS", "G" );
        bWriteGeoTransform = TRUE;
    }

    else if( oSRS.GetUTMZone( &bNorth ) > 0 
        && poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        if( bNorth )
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "ICORDS", "N" );
        else
            papszFullOptions = 
                CSLSetNameValue( papszFullOptions, "ICORDS", "S" );

        nZone = oSRS.GetUTMZone( NULL );
        bWriteGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();
    const char *pszPVType = GDALToNITFDataType( eType );

    if( pszPVType == NULL )
        return NULL;

    NITFCreate( pszFilename, nXSize, nYSize, poSrcDS->GetRasterCount(),
                GDALGetDataTypeSize( eType ), pszPVType, 
                papszFullOptions );

    CSLDestroy( papszFullOptions );

/* ==================================================================== */
/*      Loop copying bands to an uncompressed file.                     */
/* ==================================================================== */
    if( !bJPEG2000 )
    {
        poDstDS = (NITFDataset *) GDALOpen( pszFilename, GA_Update );
        if( poDstDS == NULL )
            return NULL;
        
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
    }

/* -------------------------------------------------------------------- */
/*      JPEG2000 case.  We need to write the data through a J2K         */
/*      driver in pixel interleaved form.                               */
/* -------------------------------------------------------------------- */
    else if( bJPEG2000 )
    {
        NITFFile *psFile = NITFOpen( pszFilename, TRUE );
        GDALDataset *poJ2KDataset = NULL;
        int nImageOffset = psFile->pasSegmentInfo[0].nSegmentStart;

        char *pszDSName = CPLStrdup( 
            CPLSPrintf( "J2K_SUBFILE:%d,%d,%s", nImageOffset, -1,
                        pszFilename ) );

        NITFClose( psFile );

        poJ2KDataset = 
            poJ2KDriver->CreateCopy( pszDSName, poSrcDS, FALSE,
                                     NITFJP2Options(papszOptions),
                                     pfnProgress, pProgressData );
        CPLFree( pszDSName );
        if( poJ2KDataset == NULL )
            return NULL;

        delete poJ2KDataset;

        // Now we need to figure out the actual length of the file
        // and correct the image segment size information.
        GIntBig nPixelCount = nXSize * ((GIntBig) nYSize) * 
            poSrcDS->GetRasterCount();

        NITFPatchImageLength( pszFilename, nImageOffset, nPixelCount );

        poDstDS = (NITFDataset *) GDALOpen( pszFilename, GA_Update );

        if( poDstDS == NULL )
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Set the georeferencing.                                         */
/* -------------------------------------------------------------------- */
    if( bWriteGeoTransform )
    {
        poDstDS->psImage->nZone = nZone;
        poDstDS->SetGeoTransform( adfGeoTransform );
    }

    poDstDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDstDS;
}

/************************************************************************/
/*                        NITFPatchImageLength()                        */
/*                                                                      */
/*      Fixup various stuff we don't know till we have written the      */
/*      imagery.  In particular the file length, image data length      */
/*      and the compression ratio achieved.                             */
/************************************************************************/

static void NITFPatchImageLength( const char *pszFilename,
                                  long nImageOffset,
                                  GIntBig nPixelCount )

{
    FILE *fpVSIL = VSIFOpenL( pszFilename, "r+b" );
    if( fpVSIL == NULL )
        return;
    
    VSIFSeekL( fpVSIL, 0, SEEK_END );
    GIntBig nFileLen = VSIFTellL( fpVSIL );

/* -------------------------------------------------------------------- */
/*      Update total file length.                                       */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fpVSIL, 342, SEEK_SET );
    VSIFWriteL( (void *) CPLSPrintf("%012d",nFileLen), 
                1, 12, fpVSIL );
    
/* -------------------------------------------------------------------- */
/*      Update the image data length.                                   */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fpVSIL, 369, SEEK_SET );
    VSIFWriteL( (void *) CPLSPrintf("%010d",nFileLen-nImageOffset), 
                1, 10, fpVSIL );

/* -------------------------------------------------------------------- */
/*      Update COMRAT, the compression rate variable.  It is a bit      */
/*      hard to know right here whether we have an IGEOLO segment,      */
/*      so the COMRAT will either be at offset 778 or 838.              */
/* -------------------------------------------------------------------- */
    char szIC[2];
    VSIFSeekL( fpVSIL, 779-2, SEEK_SET );
    VSIFReadL( szIC, 2, 1, fpVSIL );
    if( szIC[0] != 'C' && szIC[1] != '8' )
    {
        VSIFSeekL( fpVSIL, 839-2, SEEK_SET );
        VSIFReadL( szIC, 2, 1, fpVSIL );
    }
    
    if( szIC[0] != 'C' && szIC[1] != '8' )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Unable to locate COMRAT to update in NITF header." );
    }
    else
    {
        char szCOMRAT[5];

        double dfRate = (nFileLen-nImageOffset) * 8 / (double) nPixelCount;
        dfRate = MAX(0.01,MIN(99.99,dfRate));
        
        // We emit in wxyz format with an implicit decimal place
        // between wx and yz as per spec for lossy compression. 
        // We really should have a special case for lossless compression.
        sprintf( szCOMRAT, "%04d", (int) (dfRate * 100));
        VSIFWriteL( szCOMRAT, 4, 1, fpVSIL );
    }
    
    VSIFCloseL( fpVSIL );
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
        poDriver->pfnCreateCopy = NITFDataset::NITFCreateCopy;

        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_nitf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ntf" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte UInt16 Int16 UInt32 Int32 Float32" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
