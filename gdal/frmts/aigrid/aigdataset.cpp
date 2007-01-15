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
 *****************************************************************************
 *
 * $Log$
 * Revision 1.29  2006/12/09 06:25:10  dreamil
 * Fixed indentation
 *
 * Revision 1.28  2006/12/08 20:53:24  dreamil
 * Added support for all uppercase name of coverage
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1358
 *
 * Revision 1.27  2006/11/13 18:46:23  fwarmerdam
 * Added support for .clr file in directory above the coverage.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1311
 *
 * Revision 1.26  2005/11/24 20:25:32  fwarmerdam
 * Test for hdf.adf and w001001x.adf in ::Open() method, so we don't
 * have to suppress error messages for AIGOpen().  Added error checking
 * in the AIGReadBlockIndex() to recognise corrupted files.
 *
 * Revision 1.25  2005/09/16 20:38:19  fwarmerdam
 * added overview support
 *
 * Revision 1.24  2005/05/05 15:52:48  fwarmerdam
 * PAM Enabled
 *
 * Revision 1.23  2005/02/21 03:04:18  fwarmerdam
 * fixed for files in arcseconds (bug 775)
 *
 * Revision 1.22  2004/02/02 15:34:05  warmerda
 * Use CPLFormCIFilename() to get PRJ.ADF as well as prj.adf.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=480
 *
 * Revision 1.21  2003/07/08 15:35:22  warmerda
 * avoid warnings
 *
 * Revision 1.20  2003/04/09 13:45:12  warmerda
 * added limited colortable support
 *
 * Revision 1.19  2003/03/03 15:27:54  warmerda
 * The ULX and ULY values are edges, not pixel centers.  Fixed geotransform.
 *
 * Revision 1.18  2003/02/13 17:22:37  warmerda
 * updated email
 *
 * Revision 1.17  2002/11/05 03:29:29  warmerda
 * added GInt16 handling
 *
 * Revision 1.16  2002/11/05 03:19:29  warmerda
 * remap nodata to 255, and only use byte type if max < 255
 *
 * Revision 1.15  2002/09/04 06:50:36  warmerda
 * avoid static driver pointers
 *
 * Revision 1.14  2002/07/10 17:43:20  warmerda
 * Fixed error supression.
 *
 * Revision 1.13  2002/06/12 21:12:24  warmerda
 * update to metadata based driver info
 *
 * Revision 1.12  2002/02/21 15:38:32  warmerda
 * fixed nodata value for floats
 *
 * Revision 1.11  2001/11/11 23:50:59  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.10  2001/09/11 13:46:25  warmerda
 * return nodata value
 *
 * Revision 1.9  2001/07/18 04:51:56  warmerda
 * added CPL_CVSID
 *
 * Revision 1.8  2001/03/13 19:39:24  warmerda
 * changed short name to AIG, added help link
 *
 * Revision 1.7  2000/11/09 06:22:51  warmerda
 * fixed geotransform, added limited projection support
 *
 * Revision 1.6  2000/04/20 14:05:16  warmerda
 * added support for provided min/max
 *
 * Revision 1.5  2000/02/28 16:32:19  warmerda
 * use SetBand method
 *
 * Revision 1.4  1999/08/13 03:27:50  warmerda
 * added support for GDT_Int32 and GDT_Float32 access
 *
 * Revision 1.3  1999/07/23 14:28:26  warmerda
 * supress errors in AIGOpen() since we don't know if it's really AIG yet
 *
 * Revision 1.2  1999/06/26 21:00:38  warmerda
 * Relax checking that filename is a directory ... AIGOpen() now handles.
 *
 * Revision 1.1  1999/02/04 22:15:44  warmerda
 * New
 *
 */

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
