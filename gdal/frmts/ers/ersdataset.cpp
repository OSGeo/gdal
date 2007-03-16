/******************************************************************************
 * $Id: ehdrdataset.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  ERMapper .ers Driver
 * Purpose:  Implementation of .ers driver.
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
#include "ershdrnode.h"

CPL_CVSID("$Id: ehdrdataset.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/* ==================================================================== */
/*				ERSDataset				*/
/* ==================================================================== */
/************************************************************************/

class ERSRasterBand;

class ERSDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.
    GDALDataset *poDepFile;

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    int         bHDRDirty;
    ERSHdrNode *poHeader;

    const char *Find( const char *, const char * );

  public:
    		ERSDataset();
	       ~ERSDataset();
    
    virtual CPLErr GetGeoTransform( double * padfTransform );
//    virtual CPLErr SetGeoTransform( double *padfTransform );
    virtual const char *GetProjectionRef(void);
//    virtual CPLErr SetProjection( const char * );
    
    static GDALDataset *Open( GDALOpenInfo * );
#ifdef notdef
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset * poSrcDS, 
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );
#endif
};

/************************************************************************/
/* ==================================================================== */
/*				ERSDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ERSDataset()                             */
/************************************************************************/

ERSDataset::ERSDataset()
{
    fpImage = NULL;
    poDepFile = NULL;
    pszProjection = CPLStrdup("");
    bGotTransform = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    poHeader = NULL;
    bHDRDirty = FALSE;
}

/************************************************************************/
/*                            ~ERSDataset()                            */
/************************************************************************/

ERSDataset::~ERSDataset()

{
    FlushCache();
    
    if( fpImage != NULL )
    {
        VSIFCloseL( fpImage );
    }

    if( poDepFile != NULL )
    {
        int iBand;

        for( iBand = 0; iBand < nBands; iBand++ )
            papoBands[iBand] = NULL;

        GDALClose( (GDALDatasetH) poDepFile );
    }

    CPLFree( pszProjection );

    if( poHeader != NULL )
        delete poHeader;
}
/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ERSDataset::GetProjectionRef()

{
    if (pszProjection && strlen(pszProjection) > 0)
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}
/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ERSDataset::GetGeoTransform( double * padfTransform )

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
/*                             ERSDMS2Dec()                             */
/*                                                                      */
/*      Convert ERS DMS format to decimal degrees.   Input is like      */
/*      "-180:00:00".                                                   */
/************************************************************************/

static double ERSDMS2Dec( const char *pszDMS )

{
    char **papszTokens = CSLTokenizeStringComplex( pszDMS, ":", FALSE, FALSE );

    if( CSLCount(papszTokens) != 3 )
    {
        return CPLAtof( pszDMS );
    }
    else
    {
        double dfResult = fabs(CPLAtof(papszTokens[0]))
            + CPLAtof(papszTokens[1]) / 60.0
            + CPLAtof(papszTokens[2]) / 3600.0;

        if( CPLAtof(papszTokens[0]) < 0 )
            dfResult *= -1;

        CSLDestroy( papszTokens );
        return dfResult;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ERSDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user selects the .ers file.                       */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 15 
        || !EQUALN((const char *) poOpenInfo->pabyHeader,"DatasetHeader ",14) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the .ers file, and read the first line.                    */
/* -------------------------------------------------------------------- */
    FILE *fpERS = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    
    if( fpERS == NULL )
        return NULL;

    CPLReadLineL( fpERS );

/* -------------------------------------------------------------------- */
/*      Now ingest the rest of the file as a tree of header nodes.      */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poHeader = new ERSHdrNode();

    if( !poHeader->ParseChildren( fpERS ) )
    {
        delete poHeader;
        return NULL;
    }

    VSIFCloseL( fpERS );

/* -------------------------------------------------------------------- */
/*      Do we have the minimum required information from this header?   */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.NrOfLines" ) == NULL 
        || poHeader->Find( "RasterInfo.NrOfCellsPerLine" ) == NULL 
        || poHeader->Find( "RasterInfo.NrOfBands" ) == NULL )
    {
        delete poHeader;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ERSDataset     *poDS;

    poDS = new ERSDataset();
    poDS->poHeader = poHeader;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int nBands = atoi(poHeader->Find( "RasterInfo.NrOfBands" ));
    poDS->nRasterXSize = atoi(poHeader->Find( "RasterInfo.NrOfCellsPerLine" ));
    poDS->nRasterYSize = atoi(poHeader->Find( "RasterInfo.NrOfLines" ));

/* -------------------------------------------------------------------- */
/*      Establish the data type.                                        */
/* -------------------------------------------------------------------- */
    GDALDataType eType;
    CPLString osCellType = poHeader->Find( "RasterInfo.CellType", 
                                           "Unsigned8BitInteger" );
    if( EQUAL(osCellType,"Unsigned8BitInteger") )
        eType = GDT_Byte;
    else if( EQUAL(osCellType,"Unsigned16BitInteger") )
        eType = GDT_UInt16;
    else if( EQUAL(osCellType,"Signed16BitInteger") )
        eType = GDT_Int16;
    else if( EQUAL(osCellType,"IEEE4ByteReal") )
        eType = GDT_Float32;
    else
    {
        CPLDebug( "ERS", "Unknown CellType '%s'", osCellType.c_str() );
        eType = GDT_Byte;
    }

/* -------------------------------------------------------------------- */
/*      Pick up the word order.                                         */
/* -------------------------------------------------------------------- */
    int bNative;

#ifdef CPL_LSB
    bNative = EQUAL(poHeader->Find( "RasterInfo.ByteOrder", "LSBFirst" ),
                    "LSBFirst");
#else
    bNative = EQUAL(poHeader->Find( "RasterInfo.ByteOrder", "MSBFirst" ),
                    "MSBFirst");
#endif

/* -------------------------------------------------------------------- */
/*      DataSetType = Translated files are links to things like ecw     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(poHeader->Find("DataSetType",""),"Translated") )
    {
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        CPLString osDataFile = poHeader->Find( "DataFile", "" );
        CPLString osDataFilePath = CPLFormFilename( osPath, osDataFile, NULL );

        if( osDataFile.length() == 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "DataFile field not found in ers header." );
        }
        else
        {
            poDS->poDepFile = (GDALDataset *) 
                GDALOpenShared( osDataFile, poOpenInfo->eAccess );
        }

        if( poDS->poDepFile != NULL 
            && poDS->poDepFile->GetRasterCount() >= nBands )
        {
            int iBand;

            for( iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand( iBand+1, 
                               poDS->poDepFile->GetRasterBand( iBand+1 ) );
            }
        }
    }

/* ==================================================================== */
/*      While ERStorage indicates a raw file.                           */
/* ==================================================================== */
    else if( EQUAL(poHeader->Find("DataSetType",""),"ERStorage") )
    {
        int i;

        // Just strip .ers off for data file.
        char *pszDataFilename = CPLStrdup(poOpenInfo->pszFilename);
        for( i = strlen(pszDataFilename)-1; i > 0; i-- )
        {
            if( pszDataFilename[i] == '.' )
            {
                pszDataFilename[i] = '\0';
                break;
            }
        }

        // Open data file.
        if( poOpenInfo->eAccess == GA_Update )
            poDS->fpImage = VSIFOpenL( pszDataFilename, "r+" );
        else
            poDS->fpImage = VSIFOpenL( pszDataFilename, "r" );

        CPLFree( pszDataFilename );

        if( poDS->fpImage != NULL )
        {
            int iWordSize = GDALGetDataTypeSize(eType) / 8;
            int iBand;

            for( iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand( 
                    iBand+1, 
                    new RawRasterBand( poDS, iBand+1, poDS->fpImage,
                                       iWordSize * iBand * poDS->nRasterXSize,
                                       iWordSize,
                                       iWordSize * nBands * poDS->nRasterXSize,
                                       eType, bNative, TRUE ));
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we have an error!                                     */
/* -------------------------------------------------------------------- */
    if( poDS->nBands == 0 )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Look for band descriptions.                                     */
/* -------------------------------------------------------------------- */
    int iChild, iBand = 0;
    ERSHdrNode *poRI = poHeader->FindNode( "RasterInfo" );

    for( iChild = 0; 
         poRI != NULL && iChild < poRI->nItemCount && iBand < poDS->nBands; 
         iChild++ )
    {
        if( poRI->papoItemChild[iChild] != NULL
            && EQUAL(poRI->papszItemName[iChild],"BandId") )
        {
            const char *pszValue = 
                poRI->papoItemChild[iChild]->Find( "Value", NULL );

            iBand++;
            if( pszValue )
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                poDS->GetRasterBand( iBand )->SetDescription( pszValue );
                CPLPopErrorHandler();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Look for projection.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    CPLString osProjection = poHeader->Find( "CoordinateSpace.Projection", 
                                             "RAW" );
    CPLString osDatum = poHeader->Find( "CoordinateSpace.Datum", "WGS84" );
    CPLString osUnits = poHeader->Find( "CoordinateSpace.Units", "METERS" );

    oSRS.importFromERM( osProjection, osDatum, osUnits );

    CPLFree( poDS->pszProjection );
    oSRS.exportToWkt( &(poDS->pszProjection) );

/* -------------------------------------------------------------------- */
/*      Look for the geotransform.                                      */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", NULL )
        && poHeader->Find( "RasterInfo.CellInfo.Xdimension", NULL ) )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = CPLAtof( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", "" ));
        poDS->adfGeoTransform[1] = CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Xdimension", "" ));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtof( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Northings", "" ));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Ydimension", "" ));
    }
    else if( poHeader->Find( "RasterInfo.RegistrationCoord.Latitude", NULL )
        && poHeader->Find( "RasterInfo.CellInfo.Xdimension", NULL ) )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = ERSDMS2Dec( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Longitude", "" ));
        poDS->adfGeoTransform[1] = CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Xdimension", "" ));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = ERSDMS2Dec( 
            poHeader->Find( "RasterInfo.RegistrationCoord.Latitude", "" ));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -CPLAtof( 
            poHeader->Find( "RasterInfo.CellInfo.Ydimension", "" ));
    }

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
/*                         GDALRegister_ERS()                           */
/************************************************************************/

void GDALRegister_ERS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ERS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ERS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "ERMapper .ers Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_ers.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Float32" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"</CreationOptionList>" );

        poDriver->pfnOpen = ERSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
