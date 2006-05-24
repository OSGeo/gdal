/******************************************************************************
 * $Id$
 *
 * Project:  ESRI .hdr Driver
 * Purpose:  Implementation of EHdrDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.38  2006/05/24 22:26:20  fwarmerdam
 * added support for reading and writing NBITS=1-7 files
 *
 * Revision 1.37  2006/03/03 02:35:23  fwarmerdam
 * Be careful in constructor about zero band images.
 *
 * Revision 1.36  2006/02/08 17:10:52  fwarmerdam
 * Improved NODATA precision.
 *
 * Revision 1.35  2006/01/27 18:42:08  fwarmerdam
 * added the ability to save nodata and colortable information
 *
 * Revision 1.34  2006/01/20 13:33:24  fwarmerdam
 * Fallback to an assumption of nbits=8 if not specified.
 *
 * Revision 1.33  2006/01/17 13:16:20  fwarmerdam
 * as per Cees suggestions, correct handling of XLL/YLLCORNER
 * which is not at the center of the pixel.
 *
 * Revision 1.32  2006/01/17 12:28:27  fwarmerdam
 * Applied changes from Cees to default to signed, not unsigned and
 * for 32bit data to default to float, not integer.
 *
 * Revision 1.31  2006/01/13 00:18:12  fwarmerdam
 * Fixed error on failed open.
 *
 * Revision 1.30  2005/12/21 00:41:48  fwarmerdam
 * added support for reading/writing PIXELTYPE.
 * Added support for reading/writing writing geotransform and projection.
 *
 * Revision 1.29  2005/11/10 17:36:15  dron
 * _Always_ use 64-bit pointers when parsing header file.
 *
 * Revision 1.28  2005/11/10 17:24:48  dron
 * Use 64-bit pointers when parsing file header.
 *
 * Revision 1.27  2005/10/04 05:25:13  fwarmerdam
 * Added support for XLLCENTER/YLLCENTER.
 *
 * Revision 1.26  2005/05/05 13:55:41  fwarmerdam
 * PAM Enable
 *
 * Revision 1.25  2005/02/25 19:40:27  fwarmerdam
 * Support lower case byteorder values too.
 *
 * Revision 1.24  2004/07/10 12:19:01  dron
 * Read color table, when available (.clr file).
 *
 * Revision 1.23  2004/05/11 18:07:01  warmerda
 * Treat NBITS=32 and 64 as floating point cases.
 *
 * Revision 1.22  2004/02/12 10:15:33  dron
 * Fixed problem when computing band offset for band sequental files.
 *
 * Revision 1.21  2004/01/23 22:16:09  warmerda
 * fixed issue with static buffer overwriting in forming prj name
 *
 * Revision 1.20  2004/01/23 04:56:01  warmerda
 * support worldfiles if no corners in .hdr
 *
 * Revision 1.19  2004/01/20 16:22:56  dron
 * More clean usage of CPLFormCIFilename().
 *
 * Revision 1.18  2004/01/04 21:17:11  warmerda
 * fixed up to preserve path in CPLFormCIFilename calls
 */

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_EHdr(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				EHdrDataset				*/
/* ==================================================================== */
/************************************************************************/

class EHdrRasterBand;

class EHdrDataset : public RawDataset
{
    friend class EHdrRasterBand;

    FILE	*fpImage;	// image data file.

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    int         bHDRDirty;
    char      **papszHDR;

    CPLErr      RewriteHDR();
    void        ResetKeyValue( const char *pszKey, const char *pszValue );
    const char *GetKeyValue( const char *pszKey, const char *pszDefault = "" );
    void        RewriteColorTable( GDALColorTable * );

  public:
    		EHdrDataset();
    	        ~EHdrDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual CPLErr SetGeoTransform( double *padfTransform );
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset * poSrcDS, 
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
};

/************************************************************************/
/* ==================================================================== */
/*                          EHdrRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class EHdrRasterBand : public GDALPamRasterBand
{
    EHdrDataset   *poEDS;
    int            nBits;
    long           nStartBit;
    int            nPixelOffsetBits;
    int            nLineOffsetBits;

  public:

                   EHdrRasterBand( EHdrDataset *poDS );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );
};

/************************************************************************/
/*                           EHdrRasterBand()                           */
/************************************************************************/

EHdrRasterBand::EHdrRasterBand( EHdrDataset *poDS )

{
    poEDS = poDS;
    eDataType = GDT_Byte;

    nBits = atoi(poEDS->GetKeyValue("NBITS","1"));
    nStartBit = atoi(poDS->GetKeyValue("SKIPBYTES")) * 8;
    nPixelOffsetBits = nBits;
    nLineOffsetBits = atoi(poDS->GetKeyValue("TOTALROWBYTES")) * 8;
    if( nLineOffsetBits == 0 )
        nLineOffsetBits = nPixelOffsetBits * poEDS->GetRasterXSize();

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    SetMetadataItem( "NBITS", 
                     CPLString().Printf( "%ld", nBits ) );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr EHdrRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    vsi_l_offset   nLineStart;
    unsigned int   nLineBytes;
    int            iBitOffset;
    GByte         *pabyBuffer;

/* -------------------------------------------------------------------- */
/*      Establish desired position.                                     */
/* -------------------------------------------------------------------- */
    nLineBytes = (nPixelOffsetBits*nBlockXSize + 7)/8;
    nLineStart = (nStartBit + ((vsi_l_offset)nLineOffsetBits) * nBlockYOff) / 8;
    iBitOffset = 
        (nStartBit + ((vsi_l_offset)nLineOffsetBits) * nBlockYOff) % 8;

/* -------------------------------------------------------------------- */
/*      Read data into buffer.                                          */
/* -------------------------------------------------------------------- */
    pabyBuffer = (GByte *) CPLCalloc(nLineBytes,1);

    if( VSIFSeekL( poEDS->fpImage, nLineStart, SEEK_SET ) != 0 
        || VSIFReadL( pabyBuffer, 1, nLineBytes, poEDS->fpImage ) != nLineBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes at offset %d.\n%s",
                  nLineBytes, nLineStart, 
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Copy data, promoting to 8bit.                                   */
/* -------------------------------------------------------------------- */
    int iPixel = 0, iX;

    for( iX = 0; iX < nBlockXSize; iX++ )
    {
        int  nOutWord = 0, iBit;

        for( iBit = 0; iBit < nBits; iBit++ )
        {
            if( pabyBuffer[iBitOffset>>3]  & (0x80 >>(iBitOffset & 7)) )
                nOutWord |= (1 << (nBits - 1 - iBit));
            iBitOffset++;
        } 

        iBitOffset = iBitOffset + nPixelOffsetBits - nBits;
                
        ((GByte *) pImage)[iPixel++] = nOutWord;
    }

    CPLFree( pabyBuffer );

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr EHdrRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    vsi_l_offset   nLineStart;
    unsigned int   nLineBytes;
    int            iBitOffset;
    GByte         *pabyBuffer;

/* -------------------------------------------------------------------- */
/*      Establish desired position.                                     */
/* -------------------------------------------------------------------- */
    nLineBytes = (nPixelOffsetBits*nBlockXSize + 7)/8;
    nLineStart = (nStartBit + ((vsi_l_offset)nLineOffsetBits) * nBlockYOff) / 8;
    iBitOffset = 
        (nStartBit + ((vsi_l_offset)nLineOffsetBits) * nBlockYOff) % 8;

/* -------------------------------------------------------------------- */
/*      Read data into buffer.                                          */
/* -------------------------------------------------------------------- */
    pabyBuffer = (GByte *) CPLCalloc(nLineBytes,1);

    if( VSIFSeekL( poEDS->fpImage, nLineStart, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes at offset %d.\n%s",
                  nLineBytes, nLineStart, 
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

    VSIFReadL( pabyBuffer, 1, nLineBytes, poEDS->fpImage );

/* -------------------------------------------------------------------- */
/*      Copy data, promoting to 8bit.                                   */
/* -------------------------------------------------------------------- */
    int iPixel = 0, iX;

    for( iX = 0; iX < nBlockXSize; iX++ )
    {
        int iBit;
        int  nOutWord = ((GByte *) pImage)[iPixel++];

        for( iBit = 0; iBit < nBits; iBit++ )
        {
            if( nOutWord & (1 << (nBits - 1 - iBit)) )
                pabyBuffer[iBitOffset>>3] |= (0x80 >>(iBitOffset & 7));
            else
                pabyBuffer[iBitOffset>>3] &= ~((0x80 >>(iBitOffset & 7)));

            iBitOffset++;
        } 

        iBitOffset = iBitOffset + nPixelOffsetBits - nBits;
    }

/* -------------------------------------------------------------------- */
/*      Write the data back out.                                        */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( poEDS->fpImage, nLineStart, SEEK_SET ) != 0 
        || VSIFWriteL( pabyBuffer, 1, nLineBytes, poEDS->fpImage ) != nLineBytes )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %d bytes at offset %d.\n%s",
                  nLineBytes, nLineStart, 
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

    CPLFree( pabyBuffer );

    return CE_None;
}


/************************************************************************/
/* ==================================================================== */
/*				EHdrDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            EHdrDataset()                             */
/************************************************************************/

EHdrDataset::EHdrDataset()
{
    fpImage = NULL;
    pszProjection = CPLStrdup("");
    bGotTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    papszHDR = NULL;
    bHDRDirty = FALSE;
}

/************************************************************************/
/*                            ~EHdrDataset()                            */
/************************************************************************/

EHdrDataset::~EHdrDataset()

{
    FlushCache();

    if( nBands > 0 && GetAccess() == GA_Update )
    {
        int bNoDataSet;
        double dfNoData;
        RawRasterBand *poBand = (RawRasterBand *) GetRasterBand( 1 );

        dfNoData = poBand->GetNoDataValue(&bNoDataSet);
        if( bNoDataSet )
        {
            ResetKeyValue( "NODATA", 
                           CPLString().Printf( "%.8g", dfNoData ) );
        }

        if( poBand->GetColorTable() != NULL )
            RewriteColorTable( poBand->GetColorTable() );

        if( bHDRDirty )
            RewriteHDR();
    }

    if( fpImage != NULL )
        VSIFCloseL( fpImage );

    CPLFree( pszProjection );
    CSLDestroy( papszHDR );
}

/************************************************************************/
/*                            GetKeyValue()                             */
/************************************************************************/

const char *EHdrDataset::GetKeyValue( const char *pszKey, 
                                      const char *pszDefault )

{
    int i;

    for( i = 0; papszHDR[i] != NULL; i++ )
    {
        if( EQUALN(pszKey,papszHDR[i],strlen(pszKey))
            && isspace(papszHDR[i][strlen(pszKey)]) )
        {
            const char *pszValue = papszHDR[i] + strlen(pszKey);
            while( isspace(*pszValue) )
                pszValue++;
            
            return pszValue;
        }
    }

    return pszDefault;
}

/************************************************************************/
/*                           ResetKeyValue()                            */
/*                                                                      */
/*      Replace or add the keyword with the indicated value in the      */
/*      papszHDR list.                                                  */
/************************************************************************/

void EHdrDataset::ResetKeyValue( const char *pszKey, const char *pszValue )

{
    int i;
    char szNewLine[82];

    if( strlen(pszValue) > 65 )
    {
        CPLAssert( strlen(pszValue) <= 65 );
        return;
    }

    sprintf( szNewLine, "%-15s%s", pszKey, pszValue );

    for( i = CSLCount(papszHDR)-1; i >= 0; i-- )
    {
        if( EQUALN(papszHDR[i],szNewLine,strlen(pszKey)+1 ) )
        {
            if( strcmp(papszHDR[i],szNewLine) != 0 )
            {
                CPLFree( papszHDR[i] );
                papszHDR[i] = CPLStrdup( szNewLine );
                bHDRDirty = TRUE;
            }
            return;
        }
    }

    bHDRDirty = TRUE;
    papszHDR = CSLAddString( papszHDR, szNewLine );
}

/************************************************************************/
/*                         RewriteColorTable()                          */
/************************************************************************/

void EHdrDataset::RewriteColorTable( GDALColorTable *poTable )

{
    CPLString osCLRFilename = CPLResetExtension( GetDescription(), "clr" );
    FILE *fp;

    fp = VSIFOpenL( osCLRFilename, "wt" );
    if( fp != NULL )
    {
        int iColor;

        for( iColor = 0; iColor < poTable->GetColorEntryCount(); iColor++ )
        {
            CPLString oLine;
            GDALColorEntry sEntry;

            poTable->GetColorEntryAsRGB( iColor, &sEntry );

            // I wish we had a way to mark transparency.
            oLine.Printf( "%3d %3d %3d %3d\n",
                          iColor, sEntry.c1, sEntry.c2, sEntry.c3 );
            VSIFWriteL( (void *) oLine.c_str(), 1, strlen(oLine), fp );
        }
        VSIFCloseL( fp );
    }
    else
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create color file %s.", 
                  osCLRFilename.c_str() );
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *EHdrDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr EHdrDataset::SetProjection( const char *pszSRS )

{
/* -------------------------------------------------------------------- */
/*      Reset coordinate system on the dataset.                         */
/* -------------------------------------------------------------------- */
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszSRS );

    if( strlen(pszSRS) == 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Convert to ESRI WKT.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( pszSRS );
    char *pszESRI_SRS = NULL;

    oSRS.morphToESRI();
    oSRS.exportToWkt( &pszESRI_SRS );

/* -------------------------------------------------------------------- */
/*      Write to .prj file.                                             */
/* -------------------------------------------------------------------- */
    CPLString osPrjFilename = CPLResetExtension( GetDescription(), "prj" );
    FILE *fp;

    fp = VSIFOpen( osPrjFilename.c_str(), "wt" );
    if( fp != NULL )
    {
        VSIFWrite( pszESRI_SRS, 1, strlen(pszESRI_SRS), fp );
        VSIFWrite( (void *) "\n", 1, 1, fp );
        VSIFClose( fp );
    }

    CPLFree( pszESRI_SRS );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr EHdrDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }
    else
    {
        return GDALPamDataset::GetGeoTransform( padfTransform );
    }
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr EHdrDataset::SetGeoTransform( double *padfGeoTransform )

{
/* -------------------------------------------------------------------- */
/*      We only support non-rotated images with info in the .HDR file.  */
/* -------------------------------------------------------------------- */
    if( padfGeoTransform[2] != 0.0 
        || padfGeoTransform[4] != 0.0 )
    {
        return GDALPamDataset::SetGeoTransform( padfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Record new geotransform.                                        */
/* -------------------------------------------------------------------- */
    bGotTransform = TRUE;
    memcpy( adfGeoTransform, padfGeoTransform, sizeof(double) * 6 );

/* -------------------------------------------------------------------- */
/*      Strip out all old geotransform keywords from HDR records.       */
/* -------------------------------------------------------------------- */
    int i;
    for( i = CSLCount(papszHDR)-1; i >= 0; i-- )
    {
        if( EQUALN(papszHDR[i],"ul",2)
            || EQUALN(papszHDR[i]+1,"ll",2)
            || EQUALN(papszHDR[i],"cell",4)
            || EQUALN(papszHDR[i]+1,"dim",3) )
        {
            papszHDR = CSLRemoveStrings( papszHDR, i, 1, NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the transformation information.                             */
/* -------------------------------------------------------------------- */
    CPLString  oValue;

    oValue.Printf( "%.15g", adfGeoTransform[0] + adfGeoTransform[1] * 0.5 );
    ResetKeyValue( "ULXMAP", oValue );
                   
    oValue.Printf( "%.15g", adfGeoTransform[3] + adfGeoTransform[5] * 0.5 );
    ResetKeyValue( "ULYMAP", oValue );

    oValue.Printf( "%.15g", adfGeoTransform[1] );
    ResetKeyValue( "XDIM", oValue );

    oValue.Printf( "%.15g", fabs(adfGeoTransform[5]) );
    ResetKeyValue( "YDIM", oValue );

    return CE_None;
}

/************************************************************************/
/*                             RewriteHDR()                             */
/************************************************************************/

CPLErr EHdrDataset::RewriteHDR()

{
    CPLString osPath = CPLGetPath( GetDescription() );
    CPLString osName = CPLGetBasename( GetDescription() );
    CPLString osHDRFilename = CPLFormCIFilename( osPath, osName, "hdr" );

/* -------------------------------------------------------------------- */
/*      Write .hdr file.                                                */
/* -------------------------------------------------------------------- */
    FILE	*fp;
    int i;

    fp = VSIFOpen( osHDRFilename, "wt" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to rewrite .hdr file %s.", 
                  osHDRFilename.c_str() );
        return CE_Failure;
    }

    for( i = 0; papszHDR[i] != NULL; i++ )
    {
        VSIFWrite( papszHDR[i], 1, strlen(papszHDR[i]), fp );
        VSIFWrite( (void *) "\n", 1, 1, fp );
    }

    VSIFClose( fp );

    bHDRDirty = FALSE;

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *EHdrDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bSelectedHDR;
    const char	*pszHDRFilename;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .HDR           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup( CPLGetPath( poOpenInfo->pszFilename ) );
    char *pszName = CPLStrdup( CPLGetBasename( poOpenInfo->pszFilename ) );
    pszHDRFilename = CPLFormCIFilename( pszPath, pszName, "hdr" );

    bSelectedHDR = EQUAL( pszHDRFilename, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Do we have a .hdr file?                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszHDRFilename, "r" );
    
    if( fp == NULL )
    {
        CPLFree( pszName );
        CPLFree( pszPath );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is this file an ESRI header file?  Read a few lines of text     */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */
    const char *	pszLine;
    int			nRows = -1, nCols = -1, nBands = 1;
    int			nSkipBytes = 0;
    double		dfULXMap=0.5, dfULYMap = 0.5, dfYLLCorner = -123.456;
    int                 bCenter = TRUE;
    double		dfXDim = 1.0, dfYDim = 1.0, dfNoData = 0.0;
    int			nLineCount = 0, bNoDataSet = FALSE;
    GDALDataType	eDataType = GDT_Byte;
    int                 nBits = -1;
    char		chByteOrder = 'M';
    char                chPixelType = 'N'; // not defined
    char                szLayout[10] = "BIL";
    char              **papszHDR = NULL;

    while( (pszLine = CPLReadLine( fp )) )
    {
        char	**papszTokens;

        nLineCount++;

        if( nLineCount > 50 || strlen(pszLine) > 1000 )
            break;

        papszHDR = CSLAddString( papszHDR, pszLine );

        papszTokens = CSLTokenizeStringComplex( pszLine, " \t", TRUE, FALSE );
        if( CSLCount( papszTokens ) < 2 )
        {
            CSLDestroy( papszTokens );
            continue;
        }
        
        if( EQUAL(papszTokens[0],"ncols") )
        {
            nCols = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"nrows") )
        {
            nRows = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"skipbytes") )
        {
            nSkipBytes = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"ulxmap") 
                 || EQUAL(papszTokens[0],"xllcorner") 
                 || EQUAL(papszTokens[0],"xllcenter") )
        {
            dfULXMap = atof(papszTokens[1]);
            if( EQUAL(papszTokens[0],"xllcorner") )
                bCenter = FALSE;
        }
        else if( EQUAL(papszTokens[0],"ulymap") )
        {
            dfULYMap = atof(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"yllcorner") 
                 || EQUAL(papszTokens[0],"yllcenter") )
        {
            dfYLLCorner = atof(papszTokens[1]);
            if( EQUAL(papszTokens[0],"yllcorner") )
                bCenter = FALSE;
        }
        else if( EQUAL(papszTokens[0],"xdim") )
        {
            dfXDim = atof(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"ydim") )
        {
            dfYDim = atof(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"cellsize") )
        {
            dfXDim = dfYDim = atof(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"nbands") )
        {
            nBands = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"layout") )
        {
            strncpy( szLayout, papszTokens[1], sizeof(szLayout)-1 );
        }
        else if( EQUAL(papszTokens[0],"NODATA_value") 
                 || EQUAL(papszTokens[0],"NODATA") )
        {
            dfNoData = atof(papszTokens[1]);
            bNoDataSet = TRUE;
        }
        else if( EQUAL(papszTokens[0],"NBITS") )
        {
            nBits = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"PIXELTYPE") )
        {
            chPixelType = toupper(papszTokens[1][0]);
        }
        else if( EQUAL(papszTokens[0],"byteorder") )
        {
            chByteOrder = toupper(papszTokens[1][0]);
        }

        CSLDestroy( papszTokens );
    }
    
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 )
    {
        CSLDestroy( papszHDR );
        CPLFree( pszName );
        CPLFree( pszPath );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Has the user selected the .hdr file to open?                    */
/* -------------------------------------------------------------------- */
    if( bSelectedHDR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The selected file is an ESRI BIL header file, but to\n"
                  "open ESRI BIL datasets, the data file should be selected\n"
                  "instead of the .hdr file.  Please try again selecting\n"
                  "the data file (often with the extension .bil) corresponding\n"
                  "to the header file: %s\n", 
                  poOpenInfo->pszFilename );
        CSLDestroy( papszHDR );
        CPLFree( pszName );
        CPLFree( pszPath );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    EHdrDataset     *poDS;

    poDS = new EHdrDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->papszHDR = papszHDR;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %s with write permission.\n%s", 
                  pszName, VSIStrerror( errno ) );
        delete poDS;
        CPLFree( pszName );
        CPLFree( pszPath );
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Figure out the data type.                                       */
/* -------------------------------------------------------------------- */
    if( nBits == 16 )
    {
        if ( chPixelType == 'U' )
            eDataType = GDT_UInt16;
        else
            eDataType = GDT_Int16; // default
    }
    else if( nBits == 32 )
    {
        if( chPixelType == 'S' )
            eDataType = GDT_Int32;
        else if( chPixelType == 'U' )
            eDataType = GDT_UInt32;
        else if( chPixelType == 'F' )
            eDataType = GDT_Float32;
        else
            eDataType = GDT_Float32; // apparently common usage? 
    }
    else if( nBits == 8 || nBits == -1 )
        eDataType = GDT_Byte;

    else if( nBits < 8 && nBits >= 1 )
        eDataType = GDT_Byte;
    
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "EHdr driver does not support %d NBITS value.", 
                  nBits );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int             nItemSize = GDALGetDataTypeSize(eDataType)/8;
    int             nPixelOffset;
    vsi_l_offset    nLineOffset, nBandOffset;

    if( EQUAL(szLayout,"BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        nLineOffset = (vsi_l_offset)nPixelOffset * nCols;
        nBandOffset = (vsi_l_offset)nItemSize;
    }
    else if( EQUAL(szLayout,"BSQ") )
    {
        nPixelOffset = nItemSize;
        nLineOffset = (vsi_l_offset)nPixelOffset * nCols;
        nBandOffset = (vsi_l_offset)nLineOffset * nRows;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        nLineOffset = (vsi_l_offset)nItemSize * nBands * nCols;
        nBandOffset = (vsi_l_offset)nItemSize * nCols;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( i = 0; i < poDS->nBands; i++ )
    {
        GDALRasterBand	*poBand;

        if( nBits >= 8 )
        {
            
            poBand = 
                new RawRasterBand( poDS, i+1, poDS->fpImage,
                                   nSkipBytes + nBandOffset * i, 
                                   nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB                               
                                   chByteOrder == 'I' || chByteOrder == 'L',
#else
                                   chByteOrder == 'M',
#endif        
                                   TRUE );

            if( bNoDataSet )
                ((RawRasterBand *) poBand)->StoreNoDataValue( dfNoData );

        }
        else
        {
            poBand = new EHdrRasterBand( poDS );
        }
            
        poDS->SetBand( i+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file.                                          */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFilename = CPLFormCIFilename( pszPath, pszName, "prj" );

    fp = VSIFOpen( pszPrjFilename, "r" );
    if( fp != NULL )
    {
        char	**papszLines;
        OGRSpatialReference oSRS;

        VSIFClose( fp );
        
        papszLines = CSLLoad( pszPrjFilename );

        if( oSRS.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }

        CSLDestroy( papszLines );
    }

/* -------------------------------------------------------------------- */
/*      If we didn't get bounds in the .hdr, look for a worldfile.      */
/* -------------------------------------------------------------------- */
    if( dfYLLCorner != -123.456 )
    {
        if( bCenter )
            dfULYMap = dfYLLCorner + (nRows-1) * dfYDim;
        else
            dfULYMap = dfYLLCorner + nRows * dfYDim;
    }

    if( dfULYMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->bGotTransform = TRUE;

        if( bCenter )
        {
            poDS->adfGeoTransform[0] = dfULXMap - dfXDim * 0.5;
            poDS->adfGeoTransform[1] = dfXDim;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = dfULYMap + dfYDim * 0.5;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = - dfYDim;
        }
        else
        {
            poDS->adfGeoTransform[0] = dfULXMap;
            poDS->adfGeoTransform[1] = dfXDim;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = dfULYMap;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = - dfYDim;
        }
    }
    
    if( !poDS->bGotTransform )
        poDS->bGotTransform = 
            GDALReadWorldFile( poOpenInfo->pszFilename, "blw", 
                               poDS->adfGeoTransform );

    if( !poDS->bGotTransform )
        poDS->bGotTransform = 
            GDALReadWorldFile( poOpenInfo->pszFilename, "wld", 
                               poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Check for a color table.                                        */
/* -------------------------------------------------------------------- */
    const char  *pszCLRFilename = CPLFormCIFilename( pszPath, pszName, "clr" );

    fp = VSIFOpen( pszCLRFilename, "r" );
    if( fp != NULL )
    {
        GDALColorTable oColorTable;

        for(i = 0;;)
        {
            const char  *pszLine = CPLReadLine(fp);
            if ( !pszLine )
                break;

            char	**papszValues = CSLTokenizeString2(pszLine, "\t ",
                                                           CSLT_HONOURSTRINGS);
            GDALColorEntry oEntry;

            if ( CSLCount(papszValues) == 4 )
            {
                oEntry.c1 = atoi( papszValues[1] ); // Red
                oEntry.c2 = atoi( papszValues[2] ); // Green
                oEntry.c3 = atoi( papszValues[3] ); // Blue
                oEntry.c4 = 255;

                oColorTable.SetColorEntry( i++, &oEntry );
            }

            CSLDestroy( papszValues );
        }
    
        VSIFClose( fp );

        for( i = 1; i <= poDS->nBands; i++ )
        {
            GDALRasterBand *poBand = poDS->GetRasterBand( i );
            poBand->SetColorTable( &oColorTable );
            poBand->SetColorInterpretation( GCI_PaletteIndex );
        }
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
    
    CPLFree( pszName );
    CPLFree( pszPath );
    
    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *EHdrDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszParmList )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16
        && eType != GDT_Int16 && eType != GDT_Int32 && eType != GDT_UInt32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create ESRI .hdr labelled dataset with an illegal\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

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
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    VSIFWrite( (void *) "\0\0", 2, 1, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Create the hdr filename.                                        */
/* -------------------------------------------------------------------- */
    char *pszHdrFilename;

    pszHdrFilename = 
        CPLStrdup( CPLResetExtension( pszFilename, "hdr" ) );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszHdrFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszHdrFilename );
        CPLFree( pszHdrFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Decide how many bits the file should have.                      */
/* -------------------------------------------------------------------- */
    int nRowBytes;
    int nBits = GDALGetDataTypeSize(eType);

    if( CSLFetchNameValue( papszParmList, "NBITS" ) != NULL )
        nBits = atoi(CSLFetchNameValue( papszParmList, "NBITS" ));

    nRowBytes = (nBits * nXSize + 7) / 8;
    
/* -------------------------------------------------------------------- */
/*      Write out the raw definition for the dataset as a whole.        */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fp, "BYTEORDER      I\n" );
    VSIFPrintf( fp, "LAYOUT         BIL\n" );
    VSIFPrintf( fp, "NROWS          %d\n", nYSize );
    VSIFPrintf( fp, "NCOLS          %d\n", nXSize );
    VSIFPrintf( fp, "NBANDS         %d\n", nBands );
    VSIFPrintf( fp, "NBITS          %d\n", nBits );
    VSIFPrintf( fp, "BANDROWBYTES   %d\n", nRowBytes );
    VSIFPrintf( fp, "TOTALROWBYTES  %d\n", nRowBytes * nBands );
    
    if( eType == GDT_Float32 )
        VSIFPrintf( fp, "PIXELTYPE      FLOAT\n");
    else if( eType == GDT_Int16 || eType == GDT_Int32 )
        VSIFPrintf( fp, "PIXELTYPE      SIGNEDINT\n");
    else
        VSIFPrintf( fp, "PIXELTYPE      UNSIGNEDINT\n");

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );

    CPLFree( pszHdrFilename );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *EHdrDataset::CreateCopy( const char * pszFilename, 
                                      GDALDataset * poSrcDS, 
                                      int bStrict, char ** papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void * pProgressData )

{
    char **papszAdjustedOptions = CSLDuplicate( papszOptions );
    GDALDataset *poOutDS;

    if( poSrcDS->GetRasterBand(1)->GetMetadataItem( "NBITS" ) != NULL 
        && CSLFetchNameValue( papszOptions, "NBITS" ) == NULL )
    {
        papszAdjustedOptions = 
            CSLSetNameValue( papszAdjustedOptions, 
                             "NBITS", 
                             poSrcDS->GetRasterBand(1)->GetMetadataItem("NBITS") );
    }
    
    GDALDriver	*poDriver = (GDALDriver *) GDALGetDriverByName( "EHdr" );

    poOutDS = poDriver->DefaultCreateCopy( pszFilename, poSrcDS, bStrict,
                                           papszAdjustedOptions, 
                                           pfnProgress, pProgressData );
    CSLDestroy( papszAdjustedOptions );

    return poOutDS;
}
    
/************************************************************************/
/*                         GDALRegister_EHdr()                          */
/************************************************************************/

void GDALRegister_EHdr()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "EHdr" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "EHdr" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ESRI .hdr Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#EHdr" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='NBITS' type='int' description='Special pixel bits (1-7)'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = EHdrDataset::Open;
        poDriver->pfnCreate = EHdrDataset::Create;
        poDriver->pfnCreateCopy = EHdrDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
