/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis HKV labelled blob support
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
 * Revision 1.2  2000/03/13 14:34:42  warmerda
 * avoid const problem on write
 *
 * Revision 1.1  2000/03/07 21:33:42  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>

static GDALDriver	*poHKVDriver = NULL;

CPL_C_START
void	GDALRegister_HKV(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

class HKVDataset : public RawDataset
{
    FILE	*fpBlob;
    
  public:
    		HKVDataset();
    	        ~HKVDataset();
    
    char	**papszAttrib;
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            HKVDataset()                             */
/************************************************************************/

HKVDataset::HKVDataset()
{
    papszAttrib = NULL;
}

/************************************************************************/
/*                            ~HKVDataset()                            */
/************************************************************************/

HKVDataset::~HKVDataset()

{
    FlushCache();
    CSLDestroy( papszAttrib );
    if( fpBlob != NULL )
        VSIFClose( fpBlob );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    char        **papszAttrib;
    const char  *pszFilename, *pszValue;
    VSIStatBuf  sStat;
    
/* -------------------------------------------------------------------- */
/*      We assume the dataset is passed as a directory.  Check for      */
/*      an attrib and blob file as a minimum.                           */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bStatOK || !VSI_ISDIR(poOpenInfo->sStat.st_mode) )
        return NULL;
    
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "blob", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "attrib", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Load the attrib file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    papszAttrib = CSLLoad( pszFilename );
    if( papszAttrib == NULL )
        return NULL;

    for( i = 0; papszAttrib[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszAttrib[i];

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
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HKVDataset 	*poDS;

    poDS = new HKVDataset();

    poDS->poDriver = poHKVDriver;
    poDS->papszAttrib = papszAttrib;
    
/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    int bNative, bComplex;
    int nRawBands = 0;

    if( CSLFetchNameValue( papszAttrib, "extent.cols" ) == NULL 
        || CSLFetchNameValue( papszAttrib, "extent.rows" ) == NULL )
        return NULL;

    poDS->RasterInitialize( 
        atoi(CSLFetchNameValue(papszAttrib,"extent.cols")),
        atoi(CSLFetchNameValue(papszAttrib,"extent.rows")) );

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.order");
    if( pszValue == NULL )
        bNative = TRUE;
    else
    {
#ifdef CPL_MSB
        bNative = (strstr(pszValue,"*msbf") != NULL);
#else
        bNative = (strstr(pszValue,"*lsbf") != NULL);
#endif
    }

    pszValue = CSLFetchNameValue(papszAttrib,"channel.enumeration");
    if( pszValue != NULL )
        nRawBands = atoi(pszValue);
    else
        nRawBands = 1;

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.field");
    if( pszValue != NULL && strstr(pszValue,"*complex") != NULL )
        bComplex = TRUE;
    else
        bComplex = FALSE;

/* -------------------------------------------------------------------- */
/*      Figure out the datatype                                         */
/* -------------------------------------------------------------------- */
    const char * pszEncoding;
    int          nSize = 1;
    int          nPseudoBands;
    GDALDataType eType;

    pszEncoding = CSLFetchNameValue(papszAttrib,"pixel.encoding");
    if( pszEncoding == NULL )
        pszEncoding = "{ *unsigned }";

    if( CSLFetchNameValue(papszAttrib,"pixel.size") != NULL )
        nSize = atoi(CSLFetchNameValue(papszAttrib,"pixel.size"))/8;
        
    if( bComplex )
        nPseudoBands = 2;
    else 
        nPseudoBands = 1;

    if( nSize == 1 )
        eType = GDT_Byte;
    else if( nSize == 2 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt16;
    else if( nSize == 2 )
        eType = GDT_Int16;
    else if( nSize == 4 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt32;
    else if( nSize == 4 && strstr(pszEncoding,"*two") != NULL )
        eType = GDT_Int32;
    else if( nSize == 4 )
        eType = GDT_Float32;
    else if( nSize == 8 )
        eType = GDT_Float64;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported pixel data type in %s.\n"
                  "pixel.size=%d pixel.encoding=%s\n", 
                  poOpenInfo->pszFilename, nSize, pszEncoding );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the blob file.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "blob", NULL );
    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fpBlob = VSIFOpen( pszFilename, "rb" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for read access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }
    else
    {
        poDS->fpBlob = VSIFOpen( pszFilename, "rb+" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for update access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Define the bands.                                               */
/* -------------------------------------------------------------------- */
    int    nPixelOffset, nLineOffset, nOffset;

    nPixelOffset = nRawBands * nSize;
    nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    nOffset = 0;

    for( int iRawBand=0; iRawBand < nRawBands; iRawBand++ )
    {
        poDS->SetBand( poDS->GetRasterCount()+1, 
            new RawRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                               nOffset, nPixelOffset, nLineOffset, 
                               eType, bNative ) );
        nOffset += GDALGetDataTypeSize( eType ) / 8;
        
        if( bComplex )
        {
            poDS->SetBand( poDS->GetRasterCount()+1, 
               new RawRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                                  nOffset, nPixelOffset, nLineOffset, 
                                  eType, bNative ) );
            nOffset += GDALGetDataTypeSize( eType ) / 8;
        }
    }

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HKVDataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 
        && eType != GDT_UInt16 && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create HKV file with currently unsupported\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish the name of the directory we will be creating the     */
/*      new HKV directory in.  Verify that this is a directory.         */
/* -------------------------------------------------------------------- */
    char	*pszBaseDir;
    VSIStatBuf  sStat;

    if( strlen(CPLGetPath(pszFilenameIn)) == 0 )
        pszBaseDir = CPLStrdup(".");
    else
        pszBaseDir = CPLStrdup(CPLGetPath(pszFilenameIn));

    if( VSIStat( pszBaseDir, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create HKV dataset under %s,\n"
                  "but this is not a valid directory.\n", 
                  pszBaseDir);
        CPLFree( pszBaseDir );
        return NULL;
    }

    if( VSIMkdir( pszFilenameIn, 0755 ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create directory %s.\n", 
                  pszFilenameIn );
        return NULL;
    }

    CPLFree( pszBaseDir );

/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    const char *pszFilename;

    pszFilename = CPLFormFilename( pszFilenameIn, "attrib", NULL );

    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }
    
    fprintf( fp, "channel.enumeration = %d\n", nBands );
    fprintf( fp, "channel.interleave = { *pixel tile sequential }\n" );
    fprintf( fp, "extent.cols = %d\n", nXSize );
    fprintf( fp, "extent.rows = %d\n", nYSize );
    
    switch( eType )
    {
      case GDT_Byte:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_UInt16:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_Int16:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned *twos-complement ieee-754 }\n" );
        break;

      case GDT_Float32:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned twos-complement *ieee-754 }\n" );
        break;

      default:
        CPLAssert( FALSE );
    }
    
    fprintf( fp, "pixel.size = %d\n", GDALGetDataTypeSize(eType) );
    fprintf( fp, "pixel.field = { *real complex }\n" );
#ifdef CPL_MSB     
    fprintf( fp, "pixel.order = { lsbf *msbf }\n";
#else
    fprintf( fp, "pixel.order = { *lsbf msbf }\n" );
#endif
    VSIFClose( fp );
   
/* -------------------------------------------------------------------- */
/*      Create the blob file.                                           */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename( pszFilenameIn, "blob", NULL );
    fp = VSIFOpen( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }
    
    VSIFWrite( (void*)"", 1, 1, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilenameIn, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_HKV()                          */
/************************************************************************/

void GDALRegister_HKV()

{
    GDALDriver	*poDriver;

    if( poHKVDriver == NULL )
    {
        poHKVDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "HKV";
        poDriver->pszLongName = "Atlantis HKV Raster";
        
        poDriver->pfnOpen = HKVDataset::Open;
         poDriver->pfnCreate = HKVDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
