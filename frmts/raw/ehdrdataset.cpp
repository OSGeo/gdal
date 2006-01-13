/******************************************************************************
 * $Id$
 *
 * Project:  ESRI .hdr Driver
 * Purpose:  Implementation of EHdrDataset
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
 *
 * Revision 1.17  2003/09/26 13:49:42  warmerda
 * fixed multi band support, implement rudimentary write support
 *
 * Revision 1.16  2003/08/25 13:33:19  dron
 * Use CPLFormCIFilename() to case insensitive search for .HDR and .PRJ files.
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

class EHdrDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;
    char      **papszHDR;

    CPLErr      RewriteHDR();

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
};

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
}

/************************************************************************/
/*                            ~EHdrDataset()                            */
/************************************************************************/

EHdrDataset::~EHdrDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
    CPLFree( pszProjection );
    CSLDestroy( papszHDR );
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
/*      Strip out old geotransform keywords from HDR records.           */
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
    CPLString  oLine;

    oLine.Printf( "ULXMAP         %.15g", 
                  adfGeoTransform[0] + adfGeoTransform[1] * 0.5 );
    papszHDR = CSLAddString( papszHDR, oLine );

    oLine.Printf( "ULYMAP         %.15g", 
                  adfGeoTransform[3] + adfGeoTransform[5] * 0.5 );
    papszHDR = CSLAddString( papszHDR, oLine );

    oLine.Printf( "XDIM           %.15g", adfGeoTransform[1] );
    papszHDR = CSLAddString( papszHDR, oLine );

    oLine.Printf( "YDIM           %.15g", fabs(adfGeoTransform[5]) );
    papszHDR = CSLAddString( papszHDR, oLine );

    return RewriteHDR();
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
    double		dfXDim = 1.0, dfYDim = 1.0, dfNoData = 0.0;
    int			nLineCount = 0, bNoDataSet = FALSE;
    GDALDataType	eDataType = GDT_Byte;
    int                 nBits;
    char		chByteOrder = 'M';
    char                chPixelType = 'U';
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
        }
        else if( EQUAL(papszTokens[0],"ulymap") )
        {
            dfULYMap = atof(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"yllcorner") 
                 || EQUAL(papszTokens[0],"yllcenter") )
        {
            dfYLLCorner = atof(papszTokens[1]);
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
        if( chPixelType == 'S' )
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }
    else if( nBits == 32 )
    {
        if( chPixelType == 'S' )
            eDataType = GDT_Int32;
        else if( chPixelType == 'F' )
            eDataType = GDT_Float32;
        else
            eDataType = GDT_UInt32;
    }
    else if( nBits == 8 )
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
        RawRasterBand	*poBand;

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
            poBand->StoreNoDataValue( dfNoData );

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
        dfULYMap = dfYLLCorner + (nRows-1) * dfYDim;

    if( dfULYMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = dfULXMap - dfXDim * 0.5;
        poDS->adfGeoTransform[1] = dfXDim;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfULYMap + dfYDim * 0.5;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = - dfYDim;
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
                                  char ** /* papszParmList */ )

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
/*      Write out the raw definition for the dataset as a whole.        */
/* -------------------------------------------------------------------- */
    int nItemSize = GDALGetDataTypeSize(eType) / 8;

    VSIFPrintf( fp, "BYTEORDER      I\n" );
    VSIFPrintf( fp, "LAYOUT         BIL\n" );
    VSIFPrintf( fp, "NROWS          %d\n", nYSize );
    VSIFPrintf( fp, "NCOLS          %d\n", nXSize );
    VSIFPrintf( fp, "NBANDS         %d\n", nBands );
    VSIFPrintf( fp, "NBITS          %d\n", GDALGetDataTypeSize(eType) );
    VSIFPrintf( fp, "BANDROWBYTES   %d\n", nItemSize * nXSize );
    VSIFPrintf( fp, "TOTALROWBYTES  %d\n", nItemSize * nXSize * nBands );
    
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

        poDriver->pfnOpen = EHdrDataset::Open;
        poDriver->pfnCreate = EHdrDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}


//  LocalWords:  oLine
