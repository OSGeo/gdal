/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Driver
 * Purpose:  Implements GDAL interface to underlying library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "aigrid.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_AIGrid(void);
CPL_C_END

static const char*OSR_GDS( char **papszNV, const char * pszField, 
                           const char *pszDefaultValue );


/************************************************************************/
/* ==================================================================== */
/*				AIGDataset				*/
/* ==================================================================== */
/************************************************************************/

class AIGRasterBand;

class CPL_DLL AIGDataset : public GDALPamDataset
{
    friend class AIGRasterBand;
    
    AIGInfo_t	*psInfo;

    char	**papszPrj;
    char	*pszProjection;

    GDALColorTable *poCT;

    void        TranslateColorTable( const char * );

  public:
                AIGDataset();
                ~AIGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            AIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AIGRasterBand : public GDALPamRasterBand
{
    friend class AIGDataset;

  public:

                   AIGRasterBand( AIGDataset *, int );

    virtual CPLErr IReadBlock( int, int, void * );
    virtual double GetMinimum( int *pbSuccess );
    virtual double GetMaximum( int *pbSuccess );
    virtual double GetNoDataValue( int *pbSuccess );

    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
};

/************************************************************************/
/*                           AIGRasterBand()                            */
/************************************************************************/

AIGRasterBand::AIGRasterBand( AIGDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;

    nBlockXSize = poDS->psInfo->nBlockXSize;
    nBlockYSize = poDS->psInfo->nBlockYSize;

    if( poDS->psInfo->nCellType == AIG_CELLTYPE_INT
        && poDS->psInfo->dfMin >= 0.0 && poDS->psInfo->dfMax <= 254.0 )
    {
        eDataType = GDT_Byte;
    }
    else if( poDS->psInfo->nCellType == AIG_CELLTYPE_INT
        && poDS->psInfo->dfMin >= -32767 && poDS->psInfo->dfMax <= 32767 )
    {
        eDataType = GDT_Int16;
    }
    else if( poDS->psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        eDataType = GDT_Int32;
    }
    else
    {
        eDataType = GDT_Float32;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AIGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    AIGDataset	*poODS = (AIGDataset *) poDS;
    GInt32	*panGridRaster;
    int		i;

    if( poODS->psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        panGridRaster = (GInt32 *) CPLMalloc(4*nBlockXSize*nBlockYSize);
        if( AIGReadTile( poODS->psInfo, nBlockXOff, nBlockYOff, panGridRaster )
            != CE_None )
        {
            CPLFree( panGridRaster );
            return CE_Failure;
        }

        if( eDataType == GDT_Byte )
        {
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                if( panGridRaster[i] == ESRI_GRID_NO_DATA )
                    ((GByte *) pImage)[i] = 255;
                else
                    ((GByte *) pImage)[i] = (GByte) panGridRaster[i];
            }
        }
        else if( eDataType == GDT_Int16 )
        {
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
            {
                if( panGridRaster[i] == ESRI_GRID_NO_DATA )
                    ((GInt16 *) pImage)[i] = -32768;
                else
                    ((GInt16 *) pImage)[i] = (GInt16) panGridRaster[i];
            }
        }
        else
        {
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
                ((GInt32 *) pImage)[i] = panGridRaster[i];
        }
        
        CPLFree( panGridRaster );

        return CE_None;
    }
    else
    {
        return AIGReadFloatTile( poODS->psInfo, nBlockXOff, nBlockYOff,
                                 (float *) pImage );
    }
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double AIGRasterBand::GetMinimum( int *pbSuccess )

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return poODS->psInfo->dfMin;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double AIGRasterBand::GetMaximum( int *pbSuccess )

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    return poODS->psInfo->dfMax;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double AIGRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = TRUE;

    if( eDataType == GDT_Float32 )
        return ESRI_GRID_FLOAT_NO_DATA;
    else if( eDataType == GDT_Int16 )
        return -32768;
    else if( eDataType == GDT_Byte )
        return 255;
    else
        return ESRI_GRID_NO_DATA;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp AIGRasterBand::GetColorInterpretation()

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

    if( poODS->poCT != NULL )
        return GCI_PaletteIndex;
    else
        return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *AIGRasterBand::GetColorTable()

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

    return poODS->poCT;
}

/************************************************************************/
/* ==================================================================== */
/*                            AIGDataset                               */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            AIGDataset()                            */
/************************************************************************/

AIGDataset::AIGDataset()

{
    psInfo = NULL;
    papszPrj = NULL;
    pszProjection = CPLStrdup("");
    poCT = NULL;
}

/************************************************************************/
/*                           ~AIGDataset()                            */
/************************************************************************/

AIGDataset::~AIGDataset()

{
    FlushCache();
    CPLFree( pszProjection );
    CSLDestroy( papszPrj );
    if( psInfo != NULL )
        AIGClose( psInfo );

    if( poCT != NULL )
        delete poCT;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AIGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    AIGInfo_t	*psInfo;

/* -------------------------------------------------------------------- */
/*      If the pass name ends in .adf assume a file within the          */
/*      coverage has been selected, and strip that off the coverage     */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    CPLString osCoverName;

    osCoverName = poOpenInfo->pszFilename;
    if( osCoverName.size() > 4 
        && EQUAL(osCoverName.c_str()+osCoverName.size()-4,".adf") )
    {
        osCoverName = CPLGetDirname( poOpenInfo->pszFilename );
        if( osCoverName == "" )
            osCoverName = ".";
    }

/* -------------------------------------------------------------------- */
/*      Verify that a few of the "standard" files are available.        */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    CPLString osTestName;
    
    osTestName.Printf( "%s/hdr.adf", osCoverName.c_str() );
    if( VSIStatL( osTestName, &sStatBuf ) != 0 )

    {
        osTestName.Printf( "%s/HDR.ADF", osCoverName.c_str() );
        if( VSIStatL( osTestName, &sStatBuf ) != 0 )
            return NULL;
    }

    osTestName.Printf( "%s/w001001x.adf", osCoverName.c_str() );
    if( VSIStatL( osTestName, &sStatBuf ) != 0 )

    {
        osTestName.Printf( "%s/W001001X.ADF", osCoverName.c_str() );
        if( VSIStatL( osTestName, &sStatBuf ) != 0 )
            return NULL;
    }

    osTestName.Printf( "%s/w001001.adf", osCoverName.c_str() );
    if( VSIStatL( osTestName, &sStatBuf ) != 0 )

    {
        osTestName.Printf( "%s/W001001.ADF", osCoverName.c_str() );
        if( VSIStatL( osTestName, &sStatBuf ) != 0 )
            return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    psInfo = AIGOpen( osCoverName.c_str(), "r" );
    
    if( psInfo == NULL )
    {
        CPLErrorReset();
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AIGDataset 	*poDS;

    poDS = new AIGDataset();

    poDS->psInfo = psInfo;

/* -------------------------------------------------------------------- */
/*      Try to read a color table (.clr).  It seems it is legal to      */
/*      have more than one so we just use the first one found.          */
/* -------------------------------------------------------------------- */
    int  iFile;
    char **papszFiles = CPLReadDir( psInfo->pszCoverName );
    CPLString osClrFilename;
    CPLString osCleanPath = CPLCleanTrailingSlash( psInfo->pszCoverName );
  
    // first check for any .clr in coverage dir.
    for( iFile = 0; papszFiles != NULL && papszFiles[iFile] != NULL; iFile++ )
    {
        if( !EQUAL(CPLGetExtension(papszFiles[iFile]),"clr") && !EQUAL(CPLGetExtension(papszFiles[iFile]),"CLR"))
            continue;
      
        osClrFilename = CPLFormFilename( psInfo->pszCoverName,
                                         papszFiles[iFile], NULL );
        break;
    }
  
    CSLDestroy( papszFiles );
  
    // Look in parent if we don't find a .clr in the coverage dir.
    if( strlen(osClrFilename) == 0 )
    {
        osTestName.Printf( "%s/../%s.clr",
                           psInfo->pszCoverName,
                           CPLGetFilename( osCleanPath ) );
      
        if( VSIStatL( osTestName, &sStatBuf ) != 0 )

	{
            osTestName.Printf( "%s/../%s.CLR",
                               psInfo->pszCoverName,
                               CPLGetFilename( osCleanPath ) );
      
            if( !VSIStatL( osTestName, &sStatBuf ) )
                osClrFilename = osTestName;
        }
        else
            osClrFilename = osTestName;
    }
    
  
    if( strlen(osClrFilename) > 0 )
        poDS->TranslateColorTable( osClrFilename );
  
/* -------------------------------------------------------------------- */
/*      Establish raster info.                                          */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = psInfo->nPixels;
    poDS->nRasterYSize = psInfo->nLines;
    poDS->nBands = 1;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new AIGRasterBand( poDS, 1 ) );

/* -------------------------------------------------------------------- */
/*	Try to read projection file.					*/
/* -------------------------------------------------------------------- */
    const char	*pszPrjFilename;

    pszPrjFilename = CPLFormCIFilename( psInfo->pszCoverName, "prj", "adf" );
    if( VSIStatL( pszPrjFilename, &sStatBuf ) == 0 )
    {
        OGRSpatialReference	oSRS;

        poDS->papszPrj = CSLLoad( pszPrjFilename );

        if( oSRS.importFromESRI( poDS->papszPrj ) == OGRERR_NONE )
        {
            // If geographic values are in seconds, we must transform. 
            // Is there a code for minutes too? 
            if( oSRS.IsGeographic() 
                && EQUAL(OSR_GDS( poDS->papszPrj, "Units", ""), "DS") )
            {
                psInfo->dfLLX /= 3600.0;
                psInfo->dfURY /= 3600.0;
                psInfo->dfCellSizeX /= 3600.0;
                psInfo->dfCellSizeY /= 3600.0;
            }

            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
        }

    }

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( psInfo->pszCoverName );
    poDS->TryLoadXML();

    return( poDS );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AIGDataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = psInfo->dfLLX;
    padfTransform[1] = psInfo->dfCellSizeX;
    padfTransform[2] = 0;

    padfTransform[3] = psInfo->dfURY;
    padfTransform[4] = 0;
    padfTransform[5] = -psInfo->dfCellSizeY;
    
    return( CE_None );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *AIGDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                        TranslateColorTable()                         */
/************************************************************************/

void AIGDataset::TranslateColorTable( const char *pszClrFilename )

{
    int  iLine;
    char **papszClrLines;

    papszClrLines = CSLLoad( pszClrFilename );
    if( papszClrLines == NULL )
        return;

    poCT = new GDALColorTable();

    for( iLine = 0; papszClrLines[iLine] != NULL; iLine++ )
    {
        char **papszTokens = CSLTokenizeString( papszClrLines[iLine] );

        if( CSLCount(papszTokens) >= 4 && papszTokens[0][0] != '#' )
        {
            int nIndex;
            GDALColorEntry sEntry;

            nIndex = atoi(papszTokens[0]);
            sEntry.c1 = (short) atoi(papszTokens[1]);
            sEntry.c2 = (short) atoi(papszTokens[2]);
            sEntry.c3 = (short) atoi(papszTokens[3]);
            sEntry.c4 = 255;

            if( (nIndex < 0 || nIndex > 33000) 
                || (sEntry.c1 < 0 || sEntry.c1 > 255)
                || (sEntry.c2 < 0 || sEntry.c2 > 255)
                || (sEntry.c3 < 0 || sEntry.c3 > 255) )
            {
                CSLDestroy( papszTokens );
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Color table entry appears to be corrupt, skipping the rest. " );
                break;
            }

            poCT->SetColorEntry( nIndex, &sEntry );
        }

        CSLDestroy( papszTokens );
    }

    CSLDestroy( papszClrLines );
}

/************************************************************************/
/*                              OSR_GDS()                               */
/************************************************************************/

static const char*OSR_GDS( char **papszNV, const char * pszField, 
                           const char *pszDefaultValue )

{
    int         iLine;

    if( papszNV == NULL || papszNV[0] == NULL )
        return pszDefaultValue;

    for( iLine = 0; 
         papszNV[iLine] != NULL && 
             !EQUALN(papszNV[iLine],pszField,strlen(pszField));
         iLine++ ) {}

    if( papszNV[iLine] == NULL )
        return pszDefaultValue;
    else
    {
        static char     szResult[80];
        char    **papszTokens;
        
        papszTokens = CSLTokenizeString(papszNV[iLine]);

        if( CSLCount(papszTokens) > 1 )
            strncpy( szResult, papszTokens[1], sizeof(szResult));
        else
            strncpy( szResult, pszDefaultValue, sizeof(szResult));
        
        CSLDestroy( papszTokens );
        return szResult;
    }
}

/************************************************************************/
/*                          GDALRegister_AIG()                        */
/************************************************************************/

void GDALRegister_AIGrid()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "AIG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "AIG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Arc/Info Binary Grid" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#AIG" );
        
        poDriver->pfnOpen = AIGDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
