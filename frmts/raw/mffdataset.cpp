/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis MFF Support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.6  2000/05/15 14:18:27  warmerda
 * added COMPLEX_INTERPRETATION metadata
 *
 * Revision 1.5  2000/04/21 21:59:26  warmerda
 * added overview support
 *
 * Revision 1.4  2000/03/24 20:00:46  warmerda
 * Don't require IMAGE_FILE_FORMAT.
 *
 * Revision 1.3  2000/03/13 14:34:42  warmerda
 * avoid const problem on write
 *
 * Revision 1.2  2000/03/06 21:51:32  warmerda
 * fixed docs
 *
 * Revision 1.1  2000/03/06 19:23:20  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>

static GDALDriver	*poMFFDriver = NULL;

CPL_C_START
void	GDALRegister_MFF(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				MFFDataset				*/
/* ==================================================================== */
/************************************************************************/

class MFFDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
  public:
    		MFFDataset();
    	        ~MFFDataset();
    
    char	**papszHdrLines;
    
    FILE        **pafpBandFiles;
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            MFFDataset()                             */
/************************************************************************/

MFFDataset::MFFDataset()
{
    papszHdrLines = NULL;
    pafpBandFiles = NULL;
}

/************************************************************************/
/*                            ~MFFDataset()                            */
/************************************************************************/

MFFDataset::~MFFDataset()

{
    FlushCache();
    CSLDestroy( papszHdrLines );
    if( pafpBandFiles != NULL )
    {
        for( int i = 0; i < GetRasterCount(); i++ )
        {
            if( pafpBandFiles[i] != NULL )
                VSIFClose( pafpBandFiles[i] );
        }
        CPLFree( pafpBandFiles );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MFFDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNative = TRUE;
    char        **papszHdrLines;
    
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header file.              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 17 || poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"hdr") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Load the .hdr file, and compress white space out around the     */
/*      equal sign.                                                     */
/* -------------------------------------------------------------------- */
    papszHdrLines = CSLLoad( poOpenInfo->pszFilename );
    if( papszHdrLines == NULL )
        return NULL;

    for( i = 0; papszHdrLines[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszHdrLines[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Verify it is an MFF file.                                       */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszHdrLines, "IMAGE_FILE_FORMAT" ) != NULL
        && !EQUAL(CSLFetchNameValue(papszHdrLines,"IMAGE_FILE_FORMAT"),"MFF") )
    {
        CSLDestroy( papszHdrLines );
        return NULL;
    }

    if( CSLFetchNameValue( papszHdrLines, "IMAGE_LINES" ) == NULL 
        || CSLFetchNameValue(papszHdrLines,"LINE_SAMPLES") == NULL )
    {
        CSLDestroy( papszHdrLines );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    MFFDataset 	*poDS;

    poDS = new MFFDataset();

    poDS->poDriver = poMFFDriver;
    poDS->papszHdrLines = papszHdrLines;
    
/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszHdrLines, "IMAGE_LINES" ) == NULL 
        || CSLFetchNameValue( papszHdrLines, "LINE_SAMPLES" ) == NULL )
        return NULL;

    poDS->RasterInitialize( 
        atoi(CSLFetchNameValue(papszHdrLines,"LINE_SAMPLES")),
        atoi(CSLFetchNameValue(papszHdrLines,"IMAGE_LINES")) );

    if( CSLFetchNameValue( papszHdrLines, "BYTEORDER" ) != NULL )
    {
#ifdef CPL_MSB
        bNative = EQUAL(CSLFetchNameValue(papszHdrLines,"BYTE_ORDER"),"MSB");
#else
        bNative = EQUAL(CSLFetchNameValue(papszHdrLines,"BYTE_ORDER"),"LSB");
#endif
    }

/* -------------------------------------------------------------------- */
/*      Read the directory to find matching band files.                 */
/* -------------------------------------------------------------------- */
    char       **papszDirFiles;
    char       *pszTargetBase, *pszTargetPath;
    int        nRawBand;

    pszTargetPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    pszTargetBase = CPLStrdup(CPLGetBasename( poOpenInfo->pszFilename ));
    papszDirFiles = CPLReadDir( CPLGetPath( poOpenInfo->pszFilename ) );
    if( papszDirFiles == NULL )
        return NULL;

    for( nRawBand = 0; TRUE; nRawBand++ )
    {
        const char  *pszExtension;
        int          nBand;
        int         bComplex = FALSE;
        GDALDataType eDataType;

        /* Find the next raw band file. */
        for( i = 0; papszDirFiles[i] != NULL; i++ )
        {
            if( !EQUAL(CPLGetBasename(papszDirFiles[i]),pszTargetBase) )
                continue;

            pszExtension = CPLGetExtension(papszDirFiles[i]);
            if( isdigit(pszExtension[1])
                && atoi(pszExtension+1) == nRawBand 
                && strchr("bBcCiIjJrRxX",pszExtension[0]) != NULL )
                break;
        }

        if( papszDirFiles[i] == NULL  )
            break;

        /* open the file for required level of access */
        FILE     *fpRaw;
        const char *pszRawFilename = CPLFormFilename(pszTargetPath, 
                                                     papszDirFiles[i], NULL );

        if( poOpenInfo->eAccess == GA_Update )
            fpRaw = VSIFOpen( pszRawFilename, "rb+" );
        else
            fpRaw = VSIFOpen( pszRawFilename, "rb" );
        
        if( fpRaw == NULL )
        {
            CPLError( CE_Warning, CPLE_OpenFailed, 
                      "Unable to open %s ... skipping.\n", 
                      pszRawFilename );
            continue;
        }

        pszExtension = CPLGetExtension(papszDirFiles[i]);
        if( EQUALN(pszExtension,"b",1) )
        {
            eDataType = GDT_Byte;
        }
        else if( EQUALN(pszExtension,"c",1) )
        {
            eDataType = GDT_Byte;
            bComplex = TRUE;
        }
        else if( EQUALN(pszExtension,"i",1) )
        {
            eDataType = GDT_UInt16;
        }
        else if( EQUALN(pszExtension,"j",1) )
        {
            eDataType = GDT_Int16;
            bComplex = TRUE;
        }
        else if( EQUALN(pszExtension,"r",1) )
        {
            eDataType = GDT_Float32;
        }
        else if( EQUALN(pszExtension,"x",1) )
        {
            eDataType = GDT_Float32;
            bComplex = TRUE;
        }
        else
            continue;

        nBand = poDS->GetRasterCount() + 1;

        int   nPixelOffset;

        if( bComplex )
            nPixelOffset = (GDALGetDataTypeSize(eDataType)/8) * 2;
        else
            nPixelOffset = GDALGetDataTypeSize(eDataType)/8;
            
        poDS->SetBand( nBand, 
            new RawRasterBand( poDS, nBand, fpRaw,
                               0, nPixelOffset, 
                               nPixelOffset * poDS->GetRasterXSize(),
                               eDataType, bNative ) );

        if( bComplex )
        {
            poDS->GetRasterBand(poDS->nBands)->
                SetMetadataItem( "COMPLEX_INTERPRETATION", "REAL" );

            poDS->SetBand( nBand+1, 
               new RawRasterBand( poDS, nBand+1, fpRaw,
                                  nPixelOffset/2, nPixelOffset, 
                                  nPixelOffset * poDS->GetRasterXSize(),
                                  eDataType, bNative ) );

            poDS->GetRasterBand(poDS->nBands)->
                SetMetadataItem( "COMPLEX_INTERPRETATION", "IMAGINARY" );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree(pszTargetPath);
    CPLFree(pszTargetBase);
    CSLDestroy(papszDirFiles);

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *MFFDataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create MFF file with currently unsupported\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish the base filename (path+filename, less extension).    */
/* -------------------------------------------------------------------- */
    char	*pszBaseFilename;
    int         i;

    pszBaseFilename = (char *) CPLMalloc(strlen(pszFilenameIn)+5);
    strcpy( pszBaseFilename, pszFilenameIn );
    
    for( i = strlen(pszBaseFilename)-1; i > 0; i-- )
    {
        if( pszBaseFilename[i] == '.' )
        {
            pszBaseFilename[i] = '\0';
            break;
        }

        if( pszBaseFilename[i] == '/' || pszBaseFilename[i] == '\\' )
            break;
    }
    
/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    const char *pszFilename;

    pszFilename = CPLFormFilename( NULL, pszBaseFilename, "hdr" );

    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }

    fprintf( fp, "IMAGE_FILE_FORMAT = MFF\n" );
    fprintf( fp, "FILE_TYPE = IMAGE\n" );
    fprintf( fp, "IMAGE_LINES = %d\n", nYSize );
    fprintf( fp, "LINE_SAMPLES = %d\n", nXSize );
#ifdef CPL_MSB     
    fprintf( fp, "BYTE_ORDER = MSB\n" );
#else
    fprintf( fp, "BYTE_ORDER = LSB\n" );
#endif
    fprintf( fp, "END\n" );
    VSIFClose( fp );
   
/* -------------------------------------------------------------------- */
/*      Create the data files, but don't bother writing any data to them.*/
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        char       szExtension[4];

        if( eType == GDT_Byte )
            sprintf( szExtension, "b%02d", iBand );
        else if( eType == GDT_UInt16 )
            sprintf( szExtension, "i%02d", iBand );
        else if( eType == GDT_Float32 )
            sprintf( szExtension, "r%02d", iBand );

        pszFilename = CPLFormFilename( NULL, pszBaseFilename, szExtension );
        fp = VSIFOpen( pszFilename, "wb" );
        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Couldn't create %s.\n", pszFilename );
            return NULL;
        }

        VSIFWrite( (void *) "", 1, 1, fp );
        VSIFClose( fp );
    }

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS;

    strcat( pszBaseFilename, ".hdr" );
    poDS = (GDALDataset *) GDALOpen( pszBaseFilename, GA_Update );
    CPLFree( pszBaseFilename );
    
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_MFF()                          */
/************************************************************************/

void GDALRegister_MFF()

{
    GDALDriver	*poDriver;

    if( poMFFDriver == NULL )
    {
        poMFFDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "MFF";
        poDriver->pszLongName = "Atlantis MFF Raster";
        
        poDriver->pfnOpen = MFFDataset::Open;
        poDriver->pfnCreate = MFFDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

