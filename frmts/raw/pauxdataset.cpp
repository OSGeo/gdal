/******************************************************************************
 * $Id$
 *
 * Project:  PCI .aux Driver
 * Purpose:  Implementation of PAuxDataset
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
 * Revision 1.4  2000/02/28 16:32:20  warmerda
 * use SetBand method
 *
 * Revision 1.3  2000/01/06 14:39:30  warmerda
 * Improved error reporting.
 *
 * Revision 1.2  1999/08/13 03:27:14  warmerda
 * fixed byte order handling
 *
 * Revision 1.1  1999/08/13 02:36:14  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"

static GDALDriver	*poPAuxDriver = NULL;

CPL_C_START
void	GDALRegister_PAux(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				PAuxDataset				*/
/* ==================================================================== */
/************************************************************************/

class PAuxDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    
  public:
    		PAuxDataset();
    	        ~PAuxDataset();
    
    char	**papszAuxLines;
    
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            PAuxDataset()                             */
/************************************************************************/

PAuxDataset::PAuxDataset()
{
    papszAuxLines = NULL;
    fpImage = NULL;
}

/************************************************************************/
/*                            ~PAuxDataset()                            */
/************************************************************************/

PAuxDataset::~PAuxDataset()

{
    FlushCache();
    CSLDestroy( papszAuxLines );
    if( fpImage != NULL )
        VSIFClose( fpImage );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PAuxDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    char	*pszHDRFilename;
    char	**papszTokens;
    
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the binary file.		*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 1 && poOpenInfo->fp != NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .aux           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(poOpenInfo->pszFilename)+5);
    strcpy( pszHDRFilename, poOpenInfo->pszFilename );;

    for( i = strlen(pszHDRFilename)-1; i > 0; i-- )
    {
        if( pszHDRFilename[i] == '.' )
        {
            pszHDRFilename[i] = '\0';
            break;
        }
    }

    strcat( pszHDRFilename, ".aux" );

/* -------------------------------------------------------------------- */
/*      Do we have a .aux file?                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszHDRFilename, "r" );
    if( fp == NULL )
    {
        strcpy( pszHDRFilename + strlen(pszHDRFilename)-4, ".aux" );
        fp = VSIFOpen( pszHDRFilename, "r" );
    }

    if( fp == NULL )
    {
        CPLFree( pszHDRFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is this file a PCI .aux file?  Check the first line for the	*/
/*	telltale AuxilaryTarget keyword.				*/
/*									*/
/*	At this point we should be verifying that it refers to our	*/
/*	binary file, but that is a pretty involved test.		*/
/* -------------------------------------------------------------------- */
    const char *	pszLine;

    pszLine = CPLReadLine( fp );

    VSIFClose( fp );

    if( pszLine == NULL || !EQUALN(pszLine,"AuxilaryTarget",14) )
    {
        CPLFree( pszHDRFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PAuxDataset 	*poDS;

    poDS = new PAuxDataset();

    poDS->poDriver = poPAuxDriver;

/* -------------------------------------------------------------------- */
/*      Load the .aux file into a string list suitable to be            */
/*      searched with CSLFetchNameValue().                              */
/* -------------------------------------------------------------------- */
    poDS->papszAuxLines = CSLLoad( pszHDRFilename );
    CPLFree( pszHDRFilename );
    
/* -------------------------------------------------------------------- */
/*      Find the RawDefinition line to establish overall parameters.    */
/* -------------------------------------------------------------------- */
    pszLine = CSLFetchNameValue(poDS->papszAuxLines, "RawDefinition");
    papszTokens = CSLTokenizeString(pszLine);

    if( CSLCount(papszTokens) < 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "RawDefinition missing or corrupt in %s.",
                  poOpenInfo->pszFilename );

        return NULL;
    }

    poDS->nRasterXSize = atoi(papszTokens[0]);
    poDS->nRasterYSize = atoi(papszTokens[1]);
    poDS->nBands = atoi(papszTokens[2]);

    CSLDestroy( papszTokens );
    
/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo      */
/*      if read access is requested, otherwise close this handle,       */
/*      and open with write access.                                     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    if( poOpenInfo->eAccess == GA_Update )
    {
        VSIFClose( poDS->fpImage );
        poDS->fpImage = VSIFOpen( poOpenInfo->pszFilename, "rb+" );

        if( poDS->fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s is read-only, check permissions.",
                      poOpenInfo->pszFilename );

            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect raw definitions of each channel and create              */
/*      corresponding bands.                                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < poDS->nBands; i++ )
    {
        char	szDefnName[32];
        GDALDataType eType;
        int	bNative = TRUE;

        sprintf( szDefnName, "ChanDefinition-%d", i+1 );

        pszLine = CSLFetchNameValue(poDS->papszAuxLines, szDefnName);
        papszTokens = CSLTokenizeString(pszLine);
        if( CSLCount(papszTokens) < 4 )
            continue;

        if( EQUAL(papszTokens[0],"16U") )
            eType = GDT_UInt16;
        else if( EQUAL(papszTokens[0],"16S") )
            eType = GDT_Int16;
        else if( EQUAL(papszTokens[0],"32R") )
            eType = GDT_Float32;
        else
            eType = GDT_Byte;

        if( CSLCount(papszTokens) > 4 )
        {
#ifdef CPL_LSB
            bNative = EQUAL(papszTokens[4],"Swapped");
#else
            bNative = EQUAL(papszTokens[4],"Unswapped");
#endif
        }
        
        poDS->SetBand( i+1, 
            new RawRasterBand( poDS, i+1, poDS->fpImage,
                               atoi(papszTokens[1]),
                               atoi(papszTokens[2]),
                               atoi(papszTokens[3]), eType, bNative ) );

        CSLDestroy( papszTokens );
    }

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PAuxDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char ** /* papszParmList */ )

{
    char	*pszAuxFilename;

/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if( eType != GDT_Byte && eType != GDT_Float32 && eType != GDT_UInt16
        && eType != GDT_Int16 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create PCI .Aux labelled dataset with an illegal\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszFilename, "w" );

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
    VSIFWrite( "\0\0", 2, 1, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Create the aux filename.                                        */
/* -------------------------------------------------------------------- */
    pszAuxFilename = (char *) CPLMalloc(strlen(pszFilename)+5);
    strcpy( pszAuxFilename, pszFilename );;

    for( int i = strlen(pszAuxFilename)-1; i > 0; i-- )
    {
        if( pszAuxFilename[i] == '.' )
        {
            pszAuxFilename[i] = '\0';
            break;
        }
    }

    strcat( pszAuxFilename, ".aux" );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszAuxFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszAuxFilename );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      We need to write out the original filename but without any      */
/*      path components in the AuxilaryTarget line.  Do so now.         */
/* -------------------------------------------------------------------- */
    int		iStart;

    iStart = strlen(pszFilename)-1;
    while( iStart > 0 && pszFilename[iStart-1] != '/'
           && pszFilename[iStart-1] != '\\' )
        iStart--;

    VSIFPrintf( fp, "AuxilaryTarget: %s\n", pszFilename + iStart );

/* -------------------------------------------------------------------- */
/*      Write out the raw definition for the dataset as a whole.        */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fp, "RawDefinition: %d %d %d\n",
                nXSize, nYSize, nBands );

/* -------------------------------------------------------------------- */
/*      Write out a definition for each band.  We always write band     */
/*      sequential files for now as these are pretty efficiently        */
/*      handled by GDAL.                                                */
/* -------------------------------------------------------------------- */
    int		nImgOffset = 0;
    
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        const char * pszTypeName;
        int	     nPixelOffset;
        int	     nLineOffset;

        nPixelOffset = GDALGetDataTypeSize(eType)/8;
        nLineOffset = nXSize * nPixelOffset;

        if( eType == GDT_Float32 )
            pszTypeName = "32R";
        else if( eType == GDT_Int16 )
            pszTypeName = "16S";
        else if( eType == GDT_UInt16 )
            pszTypeName = "16U";
        else
            pszTypeName = "8U";

        VSIFPrintf( fp, "ChanDefinition-%d: %s %d %d %d %s\n",
                    iBand+1, pszTypeName,
                    nImgOffset, nPixelOffset, nLineOffset,
#ifdef CPL_LSB
                    "Swapped"
#else
                    "Unswapped"
#endif
                    );

        nImgOffset += nYSize * nLineOffset;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_PAux()                          */
/************************************************************************/

void GDALRegister_PAux()

{
    GDALDriver	*poDriver;

    if( poPAuxDriver == NULL )
    {
        poPAuxDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "PAux";
        poDriver->pszLongName = "PCI .aux Labelled";
        
        poDriver->pfnOpen = PAuxDataset::Open;
        poDriver->pfnCreate = PAuxDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

