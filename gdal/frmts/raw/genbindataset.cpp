/******************************************************************************
 * $Id: ehdrdataset.cpp 12350 2007-10-08 17:41:32Z rouault $
 *
 * Project:  GDAL
 * Purpose:  Generic Binary format driver (.hdr but not ESRI .hdr!)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ehdrdataset.cpp 12350 2007-10-08 17:41:32Z rouault $");

CPL_C_START
void	GDALRegister_GenBin(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				EHdrDataset				*/
/* ==================================================================== */
/************************************************************************/

class GenBinDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    int         bHDRDirty;
    char      **papszHDR;

  public:
    GenBinDataset();
    ~GenBinDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);
    virtual CPLErr SetProjection( const char * );
    
    virtual char **GetFileList();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*				GenBinDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GenBinDataset()                             */
/************************************************************************/

GenBinDataset::GenBinDataset()
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
/*                            ~GenBinDataset()                            */
/************************************************************************/

GenBinDataset::~GenBinDataset()

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

const char *GenBinDataset::GetProjectionRef()

{
    if (pszProjection && strlen(pszProjection) > 0)
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GenBinDataset::SetProjection( const char *pszSRS )

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

CPLErr GenBinDataset::GetGeoTransform( double * padfTransform )

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
/*                            GetFileList()                             */
/************************************************************************/

char **GenBinDataset::GetFileList()

{
    CPLString osPath = CPLGetPath( GetDescription() );
    CPLString osName = CPLGetBasename( GetDescription() );
    char **papszFileList = NULL;

    // Main data file, etc. 
    papszFileList = GDALPamDataset::GetFileList();

    // Header file.
    CPLString osFilename = CPLFormCIFilename( osPath, osName, "hdr" );
    papszFileList = CSLAddString( papszFileList, osFilename );

    return papszFileList;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GenBinDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bSelectedHDR;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 2 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .HDR           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    CPLString osName = CPLGetBasename( poOpenInfo->pszFilename );

    int iFile = CSLFindString(poOpenInfo->papszSiblingFiles, 
                              CPLFormFilename( NULL, osName, "hdr" ) );
    if( iFile < 0 ) // return if there is no corresponding .hdr file
        return NULL;

    CPLString osHDRFilename = 
        CPLFormFilename( osPath, poOpenInfo->papszSiblingFiles[iFile], NULL );

    bSelectedHDR = EQUAL( osHDRFilename, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Do we have a .hdr file?                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpenL( osHDRFilename, "r" );
    
    if( fp == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read a chunk to skim for expected keywords.                     */
/* -------------------------------------------------------------------- */
    char achHeader[1000];
    
    VSIFReadL( achHeader, 1, sizeof(achHeader), fp );
    achHeader[999] = '\0';

    if( strstr( achHeader, "BANDS:" ) == NULL 
        || strstr( achHeader, "ROWS:" ) == NULL 
        || strstr( achHeader, "COLS:" ) == NULL )
    {
        VSIFCloseL( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Has the user selected the .hdr file to open?                    */
/* -------------------------------------------------------------------- */
    if( bSelectedHDR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The selected file is an Generic Binary header file, but to\n"
                  "open Generic Binary datasets, the data file should be selected\n"
                  "instead of the .hdr file.  Please try again selecting\n"
                  "the raw data file corresponding to the header file: %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the .hdr file.                                             */
/* -------------------------------------------------------------------- */
    char **papszHdr = NULL;
    const char *pszLine = CPLReadLineL( fp );

    while( pszLine != NULL )
    {
        if( EQUAL(pszLine,"PROJECTION_PARAMETERS:") )
        {
            CPLString osPP = pszLine;

            pszLine = CPLReadLineL(fp);
            while( pszLine != NULL 
                   && (*pszLine == '\t' || *pszLine == ' ') )
            {
                osPP += pszLine;
                pszLine = CPLReadLine(fp);
            }
        }
        else
        {
            papszHdr = CSLAddString( papszHdr, pszLine );
            pszLine = CPLReadLineL( fp );
        }
    }

    VSIFCloseL( fp );

    if( CSLFetchNameValue( papszHdr, "COLS" ) == NULL
        || CSLFetchNameValue( papszHdr, "ROWS" ) == NULL
        || CSLFetchNameValue( papszHdr, "BANDS" ) == NULL )
    {
        CSLDestroy( papszHdr );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GenBinDataset     *poDS;

    poDS = new GenBinDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int nBands = atoi(CSLFetchNameValue( papszHdr, "BANDS" ));

    poDS->nRasterXSize = atoi(CSLFetchNameValue( papszHdr, "COLS" ));
    poDS->nRasterYSize = atoi(CSLFetchNameValue( papszHdr, "ROWS" ));
    poDS->papszHDR = papszHdr;

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
                  osName.c_str(), VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Figure out the data type.                                       */
/* -------------------------------------------------------------------- */
    const char *pszDataType = CSLFetchNameValue( papszHdr, "DATATYPE" );
    GDALDataType eDataType;

    if( pszDataType == NULL )
        eDataType = GDT_Byte;
    else if( EQUAL(pszDataType,"U16") )
        eDataType = GDT_UInt16;
    else if( EQUAL(pszDataType,"S16") )
        eDataType = GDT_Int16;
    else if( EQUAL(pszDataType,"F32") )
        eDataType = GDT_Float32;
    else if( EQUAL(pszDataType,"F64") )
        eDataType = GDT_Float64;
    else if( EQUAL(pszDataType,"U8") )
        eDataType = GDT_Byte;
    else
    {
        eDataType = GDT_Byte;
        CPLError( CE_Warning, CPLE_AppDefined,
                  "DATATYPE=%s not recognised, assuming Byte.",
                  pszDataType );
    }

/* -------------------------------------------------------------------- */
/*      Do we need byte swapping?                                       */
/* -------------------------------------------------------------------- */
    const char *pszBYTE_ORDER = CSLFetchNameValue(papszHdr,"BYHTE_ORDER");
    int bNative = TRUE;
    
    if( pszBYTE_ORDER != NULL )
    {
#ifdef CPL_LSB
        bNative = EQUAL(pszBYTE_ORDER,"INTEL");
#else
        bNative = !EQUAL(pszBYTE_ORDER,"INTEL");
#endif        
    }

/* -------------------------------------------------------------------- */
/*	Work out interleaving info.					*/
/* -------------------------------------------------------------------- */
    int nItemSize = GDALGetDataTypeSize(eDataType)/8;
    const char *pszInterleaving = CSLFetchNameValue(papszHdr,"INTERLEAVING");
    int             nPixelOffset;
    vsi_l_offset    nLineOffset, nBandOffset;

    if( pszInterleaving == NULL )
        pszInterleaving = "BIL";
    
    if( EQUAL(pszInterleaving,"BSQ") || EQUAL(pszInterleaving,"NA") )
    {
        nPixelOffset = nItemSize;
        nLineOffset = nItemSize * poDS->nRasterXSize;
        nBandOffset = nLineOffset * poDS->nRasterYSize;
    }
    else if( EQUAL(pszInterleaving,"BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        nLineOffset = nPixelOffset * poDS->nRasterXSize;
        nBandOffset = nItemSize;
    }
    else
    {
        if( !EQUAL(pszInterleaving,"BIL") )
            CPLError( CE_Warning, CPLE_AppDefined,
                      "INTERLEAVING:%s not recognised, assume BIL.",
                      pszInterleaving );

        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nBands * poDS->nRasterXSize;
        nBandOffset = nItemSize * poDS->nRasterXSize;
    }

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->PamInitialize();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( 
            i+1, new RawRasterBand( poDS, i+1, poDS->fpImage,
                                    nBandOffset * i, nPixelOffset, nLineOffset,
                                    eDataType, bNative ) );
    }

/* -------------------------------------------------------------------- */
/*      Get geotransform.                                               */
/* -------------------------------------------------------------------- */
#ifdef notdef
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
            GDALReadWorldFile( poOpenInfo->pszFilename, 0, 
                               poDS->adfGeoTransform );

    if( !poDS->bGotTransform )
        poDS->bGotTransform = 
            GDALReadWorldFile( poOpenInfo->pszFilename, "wld", 
                               poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Check for a .prj file.                                          */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFilename = CPLFormCIFilename( osPath, osName, "prj" );

    fp = VSIFOpen( pszPrjFilename, "r" );
    if( fp != NULL )
    {
        char	**papszLines;
        OGRSpatialReference oSRS;

        VSIFClose( fp );
        
        papszLines = CSLLoad( pszPrjFilename );

        if( oSRS.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            // If geographic values are in seconds, we must transform. 
            // Is there a code for minutes too? 
            if( oSRS.IsGeographic() 
                && EQUAL(OSR_GDS( papszLines, "Units", ""), "DS") )
            {
                poDS->adfGeoTransform[0] /= 3600.0;
                poDS->adfGeoTransform[1] /= 3600.0;
                poDS->adfGeoTransform[2] /= 3600.0;
                poDS->adfGeoTransform[3] /= 3600.0;
                poDS->adfGeoTransform[4] /= 3600.0;
                poDS->adfGeoTransform[5] /= 3600.0;
            }

            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }

        CSLDestroy( papszLines );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();
    
    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_GenBin()                          */
/************************************************************************/

void GDALRegister_GenBin()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GenBin" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GenBin" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ESRI .hdr Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#GenBin" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='NBITS' type='int' description='Special pixel bits (1-7)'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = GenBinDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
