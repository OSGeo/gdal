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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "gdal_rat.h"
#include "aigrid.h"
#include "avc.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_AIGrid(void);
CPL_C_END

static CPLString OSR_GDS( char **papszNV, const char * pszField, 
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
    int         bHasReadRat;

    void        TranslateColorTable( const char * );

    void        ReadRAT();
    GDALRasterAttributeTable *poRAT;

  public:
                AIGDataset();
                ~AIGDataset();

    static GDALDataset *Open( GDALOpenInfo * );

    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual char **GetFileList(void);
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
    virtual GDALRasterAttributeTable *GetDefaultRAT();
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
        panGridRaster = (GInt32 *) VSIMalloc3(4,nBlockXSize,nBlockYSize);
        if( panGridRaster == NULL ||
            AIGReadTile( poODS->psInfo, nBlockXOff, nBlockYOff, panGridRaster )
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
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *AIGRasterBand::GetDefaultRAT()

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Read info raster attribute table, if present.                   */
/* -------------------------------------------------------------------- */
    if (!poODS->bHasReadRat)
    {
        poODS->ReadRAT();
        poODS->bHasReadRat = TRUE;
    }

    if( poODS->poRAT )
        return poODS->poRAT;
    else
        return GDALPamRasterBand::GetDefaultRAT();
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
        return GDALPamRasterBand::GetColorInterpretation();
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *AIGRasterBand::GetColorTable()

{
    AIGDataset	*poODS = (AIGDataset *) poDS;

    if( poODS->poCT != NULL )
        return poODS->poCT;
    else
        return GDALPamRasterBand::GetColorTable();
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
    poRAT = NULL;
    bHasReadRat = FALSE;
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

    if( poRAT != NULL )
        delete poRAT;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **AIGDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    // Add in all files in the cover directory.
    char **papszCoverFiles = VSIReadDir( GetDescription() );
    int i;

    for( i = 0; papszCoverFiles != NULL && papszCoverFiles[i] != NULL; i++ )
    {
        if( EQUAL(papszCoverFiles[i],".")
            || EQUAL(papszCoverFiles[i],"..") )
            continue;

        papszFileList = 
            CSLAddString( papszFileList, 
                          CPLFormFilename( GetDescription(), 
                                           papszCoverFiles[i], 
                                           NULL ) );
    }
    CSLDestroy(papszCoverFiles);
    
    return papszFileList;
}

/************************************************************************/
/*                              ReadRAT()                               */
/************************************************************************/

void AIGDataset::ReadRAT()

{
#ifndef OGR_ENABLED
#else
/* -------------------------------------------------------------------- */
/*      Check if we have an associated info directory.  If not          */
/*      return quietly.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osInfoPath, osTableName;
    VSIStatBufL sStatBuf;

    osInfoPath = psInfo->pszCoverName;
    osInfoPath += "/../info";
    
    if( VSIStatL( osInfoPath, &sStatBuf ) != 0 )
    {
        CPLDebug( "AIG", "No associated info directory at: %s, skip RAT.",
                  osInfoPath.c_str() );
        return;
    }
    
    osInfoPath += "/";

/* -------------------------------------------------------------------- */
/*      Attempt to open the VAT table associated with this coverage.    */
/* -------------------------------------------------------------------- */
    osTableName = CPLGetFilename(psInfo->pszCoverName);
    osTableName += ".VAT";

    AVCBinFile *psFile = 
        AVCBinReadOpen( osInfoPath, osTableName,
                        AVCCoverTypeUnknown, AVCFileTABLE, NULL );

    CPLErrorReset();
    if( psFile == NULL )
        return;

    AVCTableDef *psTableDef = psFile->hdr.psTableDef;

/* -------------------------------------------------------------------- */
/*      Setup columns in corresponding RAT.                             */
/* -------------------------------------------------------------------- */
    int iField;

    poRAT = new GDALDefaultRasterAttributeTable();

    for( iField = 0; iField < psTableDef->numFields; iField++ )
    {
        AVCFieldInfo *psFDef = psTableDef->pasFieldDef + iField;
        GDALRATFieldUsage eFUsage = GFU_Generic;
        GDALRATFieldType eFType = GFT_String;

        CPLString osFName = psFDef->szName;
        osFName.Trim();

        if( EQUAL(osFName,"VALUE") )
            eFUsage = GFU_MinMax;
        else if( EQUAL(osFName,"COUNT") )
            eFUsage = GFU_PixelCount;
        
        if( psFDef->nType1 * 10 == AVC_FT_BININT )
            eFType = GFT_Integer;
        else if( psFDef->nType1 * 10 == AVC_FT_BINFLOAT )
            eFType = GFT_Real;

        poRAT->CreateColumn( osFName, eFType, eFUsage );
    }

/* -------------------------------------------------------------------- */
/*      Process all records into RAT.                                   */
/* -------------------------------------------------------------------- */
    AVCField *pasFields;
    int iRecord = 0;

    while( (pasFields = AVCBinReadNextTableRec(psFile)) != NULL )
    {
        iRecord++;

        for( iField = 0; iField < psTableDef->numFields; iField++ )
        {
            switch( psTableDef->pasFieldDef[iField].nType1 * 10 )
            {
              case AVC_FT_DATE:
              case AVC_FT_FIXINT:
              case AVC_FT_CHAR:
              case AVC_FT_FIXNUM:
              {
                  // XXX - I bet mloskot would like to see const_cast + static_cast :-)
                  const char* pszTmp = (const char*)(pasFields[iField].pszStr);
                  CPLString osStrValue( pszTmp );
                  poRAT->SetValue( iRecord-1, iField, osStrValue.Trim() );
              }
              break;

              case AVC_FT_BININT:
                if( psTableDef->pasFieldDef[iField].nSize == 4 )
                    poRAT->SetValue( iRecord-1, iField, 
                                     pasFields[iField].nInt32 );
                else
                    poRAT->SetValue( iRecord-1, iField, 
                                     pasFields[iField].nInt16 );
                break;

              case AVC_FT_BINFLOAT:
                if( psTableDef->pasFieldDef[iField].nSize == 4 )
                    poRAT->SetValue( iRecord-1, iField, 
                                     pasFields[iField].fFloat );
                else
                    poRAT->SetValue( iRecord-1, iField, 
                                     pasFields[iField].dDouble );
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

    AVCBinReadClose( psFile );

    /* Workaround against #2447 and #3031, to avoid binding languages */
    /* not being able to open the dataset */
    CPLErrorReset();

#endif /* OGR_ENABLED */
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
/*      Otherwise verify we were already given a directory.             */
/* -------------------------------------------------------------------- */
    else if( !poOpenInfo->bIsDirectory )
    {
        return NULL;
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

/* -------------------------------------------------------------------- */
/*      Confirm we have at least one raster data file.  These can be    */
/*      sparse so we don't require particular ones to exists but if     */
/*      there are none this is likely not a grid.                       */
/* -------------------------------------------------------------------- */
    char **papszFileList = VSIReadDir( osCoverName );
    int iFile;
    int bGotOne = FALSE;

    if (papszFileList == NULL)
    {
        /* Useful when reading from /vsicurl/ on servers that don't */
        /* return a file list */
        /* such as /vsicurl/http://eros.usgs.gov/archive/nslrsda/GeoTowns/NLCD/89110458 */
        do
        {
            osTestName.Printf( "%s/W001001.ADF", osCoverName.c_str() );
            if( VSIStatL( osTestName, &sStatBuf ) == 0 )
            {
                bGotOne = TRUE;
                break;
            }

            osTestName.Printf( "%s/w001001.adf", osCoverName.c_str() );
            if( VSIStatL( osTestName, &sStatBuf ) == 0 )
            {
                bGotOne = TRUE;
                break;
            }
        } while(0);
    }

    for( iFile = 0; 
         papszFileList != NULL && papszFileList[iFile] != NULL && !bGotOne;
         iFile++ )
    {
        if( strlen(papszFileList[iFile]) != 11 )
            continue;

        // looking for something like w001001.adf or z001013.adf
        if( papszFileList[iFile][0] != 'w'
            && papszFileList[iFile][0] != 'W'
            && papszFileList[iFile][0] != 'z'
            && papszFileList[iFile][0] != 'Z' )
            continue;

        if( strncmp(papszFileList[iFile] + 1, "0010", 4) != 0 )
            continue;

        if( !EQUAL(papszFileList[iFile] + 7, ".adf") )
            continue;

        bGotOne = TRUE;
    }
    CSLDestroy( papszFileList );

    if( !bGotOne )
        return NULL;
    
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
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        AIGClose(psInfo);
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The AIG driver does not support update access to existing"
                  " datasets.\n" );
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
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( psInfo->pszCoverName );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, psInfo->pszCoverName );

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

static CPLString OSR_GDS( char **papszNV, const char * pszField, 
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
        CPLString osResult;
        char    **papszTokens;
        
        papszTokens = CSLTokenizeString(papszNV[iLine]);

        if( CSLCount(papszTokens) > 1 )
            osResult = papszTokens[1];
        else
            osResult = pszDefaultValue;
        
        CSLDestroy( papszTokens );
        return osResult;
    }
}

/************************************************************************/
/*                             AIGRename()                              */
/*                                                                      */
/*      Custom renamer for AIG dataset.                                 */
/************************************************************************/

static CPLErr AIGRename( const char *pszNewName, const char *pszOldName )

{
/* -------------------------------------------------------------------- */
/*      Make sure we are talking about paths to the coverage            */
/*      directory.                                                      */
/* -------------------------------------------------------------------- */
    CPLString osOldPath, osNewPath;

    if( strlen(CPLGetExtension(pszNewName)) > 0 )
        osNewPath = CPLGetPath(pszNewName);
    else
        osNewPath = pszNewName;

    if( strlen(CPLGetExtension(pszOldName)) > 0 )
        osOldPath = CPLGetPath(pszOldName);
    else
        osOldPath = pszOldName;

/* -------------------------------------------------------------------- */
/*      Get file list.                                                  */
/* -------------------------------------------------------------------- */

    GDALDatasetH hDS = GDALOpen( osOldPath, GA_ReadOnly );
    if( hDS == NULL )
        return CE_Failure;

    char **papszFileList = GDALGetFileList( hDS );
    GDALClose( hDS );

    if( papszFileList == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Work out the corresponding new names.                           */
/* -------------------------------------------------------------------- */
    char **papszNewFileList = NULL;
    int i;
    
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        CPLString osNewFilename;

        if( !EQUALN(papszFileList[i],osOldPath,strlen(osOldPath)) )
        {
            CPLAssert( FALSE );
            return CE_Failure;
        }

        osNewFilename = osNewPath + (papszFileList[i] + strlen(osOldPath));
        
        papszNewFileList = CSLAddString( papszNewFileList, osNewFilename );
    }

/* -------------------------------------------------------------------- */
/*      Try renaming the directory.                                     */
/* -------------------------------------------------------------------- */
    if( VSIRename( osNewPath, osOldPath ) != 0 )
    {
        if( VSIMkdir( osNewPath, 0777 ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create directory %s:\n%s",
                      osNewPath.c_str(),
                      VSIStrerror(errno) );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy/rename any remaining files.                                */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( VSIStatL( papszFileList[i], &sStatBuf ) == 0 
            && VSI_ISREG( sStatBuf.st_mode ) )
        {
            if( CPLMoveFile( papszNewFileList[i], papszFileList[i] ) != 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to move %s to %s:\n%s",
                          papszFileList[i],
                          papszNewFileList[i], 
                          VSIStrerror(errno) );
                return CE_Failure;
            }
        }
    }

    if( VSIStatL( osOldPath, &sStatBuf ) == 0 )
        CPLUnlinkTree( osOldPath );

    return CE_None;
}

/************************************************************************/
/*                             AIGDelete()                              */
/*                                                                      */
/*      Custom dataset deleter for AIG dataset.                         */
/************************************************************************/

static CPLErr AIGDelete( const char *pszDatasetname )

{
/* -------------------------------------------------------------------- */
/*      Get file list.                                                  */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpen( pszDatasetname, GA_ReadOnly );
    if( hDS == NULL )
        return CE_Failure;

    char **papszFileList = GDALGetFileList( hDS );
    GDALClose( hDS );

    if( papszFileList == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Delete all regular files.                                       */
/* -------------------------------------------------------------------- */
    int i;
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        VSIStatBufL sStatBuf;
        if( VSIStatL( papszFileList[i], &sStatBuf ) == 0 
            && VSI_ISREG( sStatBuf.st_mode ) )
        {
            if( VSIUnlink( papszFileList[i] ) != 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to delete '%s':\n%s", 
                          papszFileList[i], VSIStrerror( errno ) );
                return CE_Failure;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Delete directories.                                             */
/* -------------------------------------------------------------------- */
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        VSIStatBufL sStatBuf;
        if( VSIStatL( papszFileList[i], &sStatBuf ) == 0 
            && VSI_ISDIR( sStatBuf.st_mode ) )
        {
            if( CPLUnlinkTree( papszFileList[i] ) != 0 )
                return CE_Failure;
        }
    }

    return CE_None;
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
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        
        poDriver->pfnOpen = AIGDataset::Open;

        poDriver->pfnRename = AIGRename;
        poDriver->pfnDelete = AIGDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
