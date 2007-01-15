/******************************************************************************
 * $Id$
 *
 * Project:  PCI .aux Driver
 * Purpose:  Implementation of PAuxDataset
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

#include "rawdataset.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_PAux(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				PAuxDataset				*/
/* ==================================================================== */
/************************************************************************/

class PAuxRasterBand;

class PAuxDataset : public RawDataset
{
    friend class PAuxRasterBand;

    FILE	*fpImage;	// image data file.

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    void        ScanForGCPs();
    char       *PCI2WKT( const char *pszGeosys, const char *pszProjParms );

    char       *pszProjection;

  public:
    		PAuxDataset();
    	        ~PAuxDataset();
    
    char	*pszAuxFilename;
    char	**papszAuxLines;
    int		bAuxUpdated;
    
    virtual const char *GetProjectionRef();
    virtual CPLErr GetGeoTransform( double * );
    virtual CPLErr SetGeoTransform( double * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/* ==================================================================== */
/*                           PAuxRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class PAuxRasterBand : public RawRasterBand
{
    GDALColorTable	*poCT;

  public:

                 PAuxRasterBand( GDALDataset *poDS, int nBand, FILE * fpRaw, 
                                 vsi_l_offset nImgOffset, int nPixelOffset,
                                 int nLineOffset,
                                 GDALDataType eDataType, int bNativeOrder );

                 ~PAuxRasterBand();

    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr SetNoDataValue( double );

    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();

    virtual void SetDescription( const char *pszNewDescription );
};

/************************************************************************/
/*                           PAuxRasterBand()                           */
/************************************************************************/

PAuxRasterBand::PAuxRasterBand( GDALDataset *poDS, int nBand,
                                FILE * fpRaw, vsi_l_offset nImgOffset,
                                int nPixelOffset, int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder )
        : RawRasterBand( poDS, nBand, fpRaw, 
                         nImgOffset, nPixelOffset, nLineOffset, 
                         eDataType, bNativeOrder, TRUE )

{
    PAuxDataset *poPDS = (PAuxDataset *) poDS;

    poCT = NULL;

/* -------------------------------------------------------------------- */
/*      Does this channel have a description?                           */
/* -------------------------------------------------------------------- */
    char	szTarget[128];

    sprintf( szTarget, "ChanDesc-%d", nBand );
    if( CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) != NULL )
        GDALRasterBand::SetDescription( 
            CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) );

/* -------------------------------------------------------------------- */
/*      See if we have colors.  Currently we must have color zero,      */
/*      but this shouldn't really be a limitation.                      */
/* -------------------------------------------------------------------- */
    sprintf( szTarget, "METADATA_IMG_%d_Class_%d_Color", nBand, 0 );
    if( CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) != NULL )
    {
        const char *pszLine;
        int         i;

        poCT = new GDALColorTable();

        for( i = 0; i < 256; i++ )
        {
            int	nRed, nGreen, nBlue;

            sprintf( szTarget, "METADATA_IMG_%d_Class_%d_Color", nBand, i );
            pszLine = CSLFetchNameValue( poPDS->papszAuxLines, szTarget );
            while( pszLine && *pszLine == ' ' )
                pszLine++;

            if( pszLine != NULL
                && EQUALN(pszLine, "(RGB:",5)
                && sscanf( pszLine+5, "%d %d %d", 
                           &nRed, &nGreen, &nBlue ) == 3 )
            {
                GDALColorEntry    oColor;
                
                oColor.c1 = (short) nRed;
                oColor.c2 = (short) nGreen;
                oColor.c3 = (short) nBlue;
                oColor.c4 = 255;

                poCT->SetColorEntry( i, &oColor );
            }
        }
    }
}

/************************************************************************/
/*                          ~PAuxRasterBand()                           */
/************************************************************************/

PAuxRasterBand::~PAuxRasterBand()

{
    if( poCT != NULL )
        delete poCT;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PAuxRasterBand::GetNoDataValue( int *pbSuccess )

{
    PAuxDataset *poPDS = (PAuxDataset *) poDS;
    char	szTarget[128];
    const char  *pszLine;

    sprintf( szTarget, "METADATA_IMG_%d_NO_DATA_VALUE", nBand );

    pszLine = CSLFetchNameValue( poPDS->papszAuxLines, szTarget );

    if( pbSuccess != NULL )
        *pbSuccess = (pszLine != NULL);

    if( pszLine == NULL )
        return -1e8;
    else
        return atof(pszLine);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PAuxRasterBand::SetNoDataValue( double dfNewValue )

{
    PAuxDataset *poPDS = (PAuxDataset *) poDS;
    char	szTarget[128];
    char	szValue[128];

    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, 
                  "Can't update readonly dataset." );
        return CE_Failure;
    }

    sprintf( szTarget, "METADATA_IMG_%d_NO_DATA_VALUE", nBand );
    sprintf( szValue, "%24.12f", dfNewValue );
    poPDS->papszAuxLines = 
        CSLSetNameValue( poPDS->papszAuxLines, szTarget, szValue );
    
    poPDS->bAuxUpdated = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           SetDescription()                           */
/*                                                                      */
/*      We override the set description so we can mark the auxfile      */
/*      info as changed.                                                */
/************************************************************************/

void PAuxRasterBand::SetDescription( const char *pszNewDescription )

{
    PAuxDataset *poPDS = (PAuxDataset *) poDS;

    if( GetAccess() == GA_Update )
    {
        char	szTarget[128];

        sprintf( szTarget, "ChanDesc-%d", nBand );
        poPDS->papszAuxLines = 
            CSLSetNameValue( poPDS->papszAuxLines, 
                             szTarget, pszNewDescription  );
    
        poPDS->bAuxUpdated = TRUE;
    }

    GDALRasterBand::SetDescription( pszNewDescription );
}


/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *PAuxRasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp PAuxRasterBand::GetColorInterpretation()

{
    if( poCT == NULL )
        return GCI_Undefined;
    else
        return GCI_PaletteIndex;
}

/************************************************************************/
/* ==================================================================== */
/*				PAuxDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            PAuxDataset()                             */
/************************************************************************/

PAuxDataset::PAuxDataset()
{
    papszAuxLines = NULL;
    fpImage = NULL;
    bAuxUpdated = FALSE;
    pszAuxFilename = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;

    pszProjection = NULL;
    pszGCPProjection = NULL;
}

/************************************************************************/
/*                            ~PAuxDataset()                            */
/************************************************************************/

PAuxDataset::~PAuxDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );

    if( bAuxUpdated )
    {
        CSLSetNameValueSeparator( papszAuxLines, ": " );
        CSLSave( papszAuxLines, pszAuxFilename );
    }

    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    CPLFree( pszAuxFilename );
    CSLDestroy( papszAuxLines );
}

/************************************************************************/
/*                              PCI2WKT()                               */
/*                                                                      */
/*      Convert PCI coordinate system to WKT.  For now this is very     */
/*      incomplete, but can be filled out in the future.                */
/************************************************************************/

char *PAuxDataset::PCI2WKT( const char *pszGeosys, 
                            const char *pszProjParms )

{
    OGRSpatialReference oSRS;

    while( *pszGeosys == ' ' )
        pszGeosys++;

/* -------------------------------------------------------------------- */
/*      Parse projection parameters array.                              */
/* -------------------------------------------------------------------- */
    double adfProjParms[16];
    
    memset( adfProjParms, 0, sizeof(adfProjParms) );

    if( pszProjParms != NULL )
    {
        char **papszTokens;
        int i;

        papszTokens = CSLTokenizeString( pszProjParms );
        
        for( i=0; papszTokens != NULL && papszTokens[i] != NULL && i < 16; i++)
            adfProjParms[i] = atof(papszTokens[i]);

        CSLDestroy( papszTokens );
    }
    
/* -------------------------------------------------------------------- */
/*      Convert to SRS.                                                 */
/* -------------------------------------------------------------------- */
    if( oSRS.importFromPCI( pszGeosys, NULL, adfProjParms ) == OGRERR_NONE )
    {
        char *pszResult = NULL;

        oSRS.exportToWkt( &pszResult );

        return pszResult;
    }
    else
        return NULL;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void PAuxDataset::ScanForGCPs()

{
#define MAX_GCP		256

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),MAX_GCP);

/* -------------------------------------------------------------------- */
/*      Get the GCP coordinate system.                                  */
/* -------------------------------------------------------------------- */
    const char	*pszMapUnits, *pszProjParms;

    pszMapUnits = CSLFetchNameValue( papszAuxLines, "GCP_1_MapUnits" );
    pszProjParms = CSLFetchNameValue( papszAuxLines, "GCP_1_ProjParms" );
    
    if( pszMapUnits != NULL )
        pszGCPProjection = PCI2WKT( pszMapUnits, pszProjParms );
    
/* -------------------------------------------------------------------- */
/*      Collect standalone GCPs.  They look like:                       */
/*                                                                      */
/*      GCP_1_n = row, col, x, y [,z [,"id"[, "desc"]]]			*/
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; nGCPCount < MAX_GCP; i++ )
    {
        char	szName[50];
        char    **papszTokens;

        sprintf( szName, "GCP_1_%d", i+1 );
        if( CSLFetchNameValue( papszAuxLines, szName ) == NULL )
            break;

        papszTokens = CSLTokenizeStringComplex( 
            CSLFetchNameValue( papszAuxLines, szName ), 
            " ", TRUE, FALSE );

        if( CSLCount(papszTokens) >= 4 )
        {
            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            pasGCPList[nGCPCount].dfGCPX = atof(papszTokens[2]);
            pasGCPList[nGCPCount].dfGCPY = atof(papszTokens[3]);
            pasGCPList[nGCPCount].dfGCPPixel = atof(papszTokens[0]);
            pasGCPList[nGCPCount].dfGCPLine = atof(papszTokens[1]);

            if( CSLCount(papszTokens) > 4 )
                pasGCPList[nGCPCount].dfGCPZ = atof(papszTokens[4]);

            CPLFree( pasGCPList[nGCPCount].pszId );
            if( CSLCount(papszTokens) > 5 )
            {
                pasGCPList[nGCPCount].pszId = papszTokens[5];
            }
            else
            {
                sprintf( szName, "GCP_%d", i+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szName );
            }

            if( CSLCount(papszTokens) > 6 )
            {
                CPLFree( pasGCPList[nGCPCount].pszInfo );
                pasGCPList[nGCPCount].pszInfo = papszTokens[6];
            }

            nGCPCount++;
        }
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int PAuxDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *PAuxDataset::GetGCPProjection()

{
    if( nGCPCount > 0 && pszGCPProjection != NULL )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *PAuxDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PAuxDataset::GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PAuxDataset::GetGeoTransform( double * padfGeoTransform )

{
    if( CSLFetchNameValue(papszAuxLines, "UpLeftX") != NULL 
        && CSLFetchNameValue(papszAuxLines, "UpLeftY") != NULL 
        && CSLFetchNameValue(papszAuxLines, "LoRightX") != NULL 
        && CSLFetchNameValue(papszAuxLines, "LoRightY") != NULL )
    {
        double	dfUpLeftX, dfUpLeftY, dfLoRightX, dfLoRightY;

        dfUpLeftX = atof(CSLFetchNameValue(papszAuxLines, "UpLeftX" ));
        dfUpLeftY = atof(CSLFetchNameValue(papszAuxLines, "UpLeftY" ));
        dfLoRightX = atof(CSLFetchNameValue(papszAuxLines, "LoRightX" ));
        dfLoRightY = atof(CSLFetchNameValue(papszAuxLines, "LoRightY" ));

        padfGeoTransform[0] = dfUpLeftX;
        padfGeoTransform[1] = (dfLoRightX - dfUpLeftX) / GetRasterXSize();
        padfGeoTransform[2] = 0.0;
        padfGeoTransform[3] = dfUpLeftY;
        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = (dfLoRightY - dfUpLeftY) / GetRasterYSize();

        return CE_None;
    }
    else
    {
        return CE_Failure;
    }
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PAuxDataset::SetGeoTransform( double * padfGeoTransform )

{
    char	szUpLeftX[128];
    char	szUpLeftY[128];
    char	szLoRightX[128];
    char	szLoRightY[128];

    if( ABS(padfGeoTransform[0]) < 181 
        && ABS(padfGeoTransform[1]) < 1 )
    {
        sprintf( szUpLeftX, "%.12f", padfGeoTransform[0] );
        sprintf( szUpLeftY, "%.12f", padfGeoTransform[3] );
        sprintf( szLoRightX, "%.12f", 
               padfGeoTransform[0] + padfGeoTransform[1] * GetRasterXSize() );
        sprintf( szLoRightY, "%.12f", 
               padfGeoTransform[3] + padfGeoTransform[5] * GetRasterYSize() );
    }
    else
    {
        sprintf( szUpLeftX, "%.3f", padfGeoTransform[0] );
        sprintf( szUpLeftY, "%.3f", padfGeoTransform[3] );
        sprintf( szLoRightX, "%.3f", 
               padfGeoTransform[0] + padfGeoTransform[1] * GetRasterXSize() );
        sprintf( szLoRightY, "%.3f", 
               padfGeoTransform[3] + padfGeoTransform[5] * GetRasterYSize() );
    }
        
    papszAuxLines = CSLSetNameValue( papszAuxLines, 
                                     "UpLeftX", szUpLeftX );
    papszAuxLines = CSLSetNameValue( papszAuxLines, 
                                     "UpLeftY", szUpLeftY );
    papszAuxLines = CSLSetNameValue( papszAuxLines, 
                                     "LoRightX", szLoRightX );
    papszAuxLines = CSLSetNameValue( papszAuxLines, 
                                     "LoRightY", szLoRightY );

    bAuxUpdated = TRUE;

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PAuxDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    char	*pszAuxFilename;
    char	**papszTokens;
    char	*pszTarget;
    
    if( poOpenInfo->nHeaderBytes < 1 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If this is an .aux file, fetch out and form the name of the     */
/*      file it references.                                             */
/* -------------------------------------------------------------------- */

    pszTarget = CPLStrdup( poOpenInfo->pszFilename );

    if( EQUAL(CPLGetExtension( poOpenInfo->pszFilename ),"aux")
        && EQUALN((const char *) poOpenInfo->pabyHeader,"AuxilaryTarget: ",16))
    {
        char	szAuxTarget[1024];
        char    *pszPath;
        const char *pszSrc = (const char *) poOpenInfo->pabyHeader+16;

        for( i = 0; 
             pszSrc[i] != 10 && pszSrc[i] != 13 && pszSrc[i] != '\0'
                 && i < (int) sizeof(szAuxTarget)-1;
             i++ )
        {
            szAuxTarget[i] = pszSrc[i];
        }
        szAuxTarget[i] = '\0';

        CPLFree( pszTarget );

        pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        pszTarget = CPLStrdup(CPLFormFilename(pszPath, szAuxTarget, NULL));
    }

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .aux           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    pszAuxFilename = CPLStrdup(CPLResetExtension(pszTarget,"aux"));

/* -------------------------------------------------------------------- */
/*      Do we have a .aux file?                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszAuxFilename, "r" );
    if( fp == NULL )
    {
        strcpy( pszAuxFilename + strlen(pszAuxFilename)-4, ".aux" );
        fp = VSIFOpen( pszAuxFilename, "r" );
    }

    if( fp == NULL )
    {
        CPLFree( pszTarget );
        CPLFree( pszAuxFilename );
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
        CPLFree( pszAuxFilename );
        CPLFree( pszTarget );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PAuxDataset 	*poDS;

    poDS = new PAuxDataset();

/* -------------------------------------------------------------------- */
/*      Load the .aux file into a string list suitable to be            */
/*      searched with CSLFetchNameValue().                              */
/* -------------------------------------------------------------------- */
    poDS->papszAuxLines = CSLLoad( pszAuxFilename );
    poDS->pszAuxFilename = pszAuxFilename;
    
/* -------------------------------------------------------------------- */
/*      Find the RawDefinition line to establish overall parameters.    */
/* -------------------------------------------------------------------- */
    pszLine = CSLFetchNameValue(poDS->papszAuxLines, "RawDefinition");

    // It seems PCI now writes out .aux files without RawDefinition in 
    // some cases.  See bug 947.
    if( pszLine == NULL )
    {
        delete poDS;
        return NULL;
    }

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
    poDS->eAccess = poOpenInfo->eAccess;

    CSLDestroy( papszTokens );
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->fpImage = VSIFOpenL( pszTarget, "rb+" );

        if( poDS->fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s is missing or read-only, check permissions.",
                      pszTarget );
            
            delete poDS;
            return NULL;
        }
    }
    else
    {
        poDS->fpImage = VSIFOpenL( pszTarget, "rb" );

        if( poDS->fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s is missing or unreadable.",
                      pszTarget );
            
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
        {
            // Skip the band with broken description
            poDS->nBands--;
            continue;
        }

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
            new PAuxRasterBand( poDS, i+1, poDS->fpImage,
                                CPLScanUIntBig(papszTokens[1],
                                               strlen(papszTokens[1])),
                                atoi(papszTokens[2]),
                                atoi(papszTokens[3]), eType, bNative ) );

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Get the projection.                                             */
/* -------------------------------------------------------------------- */
    const char	*pszMapUnits, *pszProjParms;

    pszMapUnits = CSLFetchNameValue( poDS->papszAuxLines, "MapUnits" );
    pszProjParms = CSLFetchNameValue( poDS->papszAuxLines, "ProjParms" );
    
    if( pszMapUnits != NULL )
        poDS->pszProjection = poDS->PCI2WKT( pszMapUnits, pszProjParms );
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszTarget );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( pszTarget );
    poDS->TryLoadXML();

    poDS->ScanForGCPs();

    CPLFree( pszTarget );

    poDS->bAuxUpdated = FALSE;

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
    VSIFWrite( (void *) "\0\0", 2, 1, fp );
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
    CPLFree( pszAuxFilename );
    
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
    vsi_l_offset    nImgOffset = 0;
    
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        const char  *pszTypeName;
        char        szImgOffset[64];
        int         nPixelOffset, nLineOffset, nChars;

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

        nChars = CPLPrintUIntBig( szImgOffset, nImgOffset, 63 );
        szImgOffset[nChars] = '\0';

        VSIFPrintf( fp, "ChanDefinition-%d: %s %s %d %d %s\n", iBand+1,
                    pszTypeName, strpbrk(szImgOffset, "-.0123456789"),
                    nPixelOffset, nLineOffset,
#ifdef CPL_LSB
                    "Swapped"
#else
                    "Unswapped"
#endif
                    );

        nImgOffset += (vsi_l_offset)nYSize * nLineOffset;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );

    return (GDALDataset *) GDALOpen( pszFilename, GA_Update );
}

/************************************************************************/
/*                             PAuxDelete()                             */
/************************************************************************/

CPLErr PAuxDelete( const char * pszBasename )

{
    FILE	*fp;
    const char *pszLine;

    fp = VSIFOpen( CPLResetExtension( pszBasename, "aux" ), "r" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s does not appear to be a PAux dataset, there is no .aux file.",
                  pszBasename );
        return CE_Failure;
    }

    pszLine = CPLReadLine( fp );
    VSIFClose( fp );
    
    if( pszLine == NULL || !EQUALN(pszLine,"AuxilaryTarget",14) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s does not appear to be a PAux dataset,\n"
                  "the .aux file does not start with AuxilaryTarget",
                  pszBasename );
        return CE_Failure;
    }

    if( VSIUnlink( pszBasename ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OS unlinking file %s.", pszBasename );
        return CE_Failure;
    }

    VSIUnlink( CPLResetExtension( pszBasename, "aux" ) );

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_PAux()                          */
/************************************************************************/

void GDALRegister_PAux()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PAux" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PAux" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "PCI .aux Labelled" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#PAux" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Float32" );

        poDriver->pfnOpen = PAuxDataset::Open;
        poDriver->pfnCreate = PAuxDataset::Create;
        poDriver->pfnDelete = PAuxDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

