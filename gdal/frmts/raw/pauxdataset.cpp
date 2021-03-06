/******************************************************************************
 *
 * Project:  PCI .aux Driver
 * Purpose:  Implementation of PAuxDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <cmath>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              PAuxDataset                             */
/* ==================================================================== */
/************************************************************************/

class PAuxRasterBand;

class PAuxDataset final: public RawDataset
{
    friend class PAuxRasterBand;

    VSILFILE    *fpImage;  // Image data file.

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    void        ScanForGCPs();
    char       *PCI2WKT( const char *pszGeosys, const char *pszProjParams );

    char       *pszProjection;

    CPL_DISALLOW_COPY_ASSIGN(PAuxDataset)

  public:
    PAuxDataset();
    ~PAuxDataset() override;

    // TODO(schwehr): Why are these public?
    char        *pszAuxFilename;
    char        **papszAuxLines;
    int         bAuxUpdated;

    const char *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr GetGeoTransform( double * ) override;
    CPLErr SetGeoTransform( double * ) override;

    int    GetGCPCount() override;
    const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    const GDAL_GCP *GetGCPs() override;

    char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParamList );
};

/************************************************************************/
/* ==================================================================== */
/*                           PAuxRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class PAuxRasterBand final: public RawRasterBand
{
    CPL_DISALLOW_COPY_ASSIGN(PAuxRasterBand)

  public:

    PAuxRasterBand( GDALDataset *poDS, int nBand, VSILFILE * fpRaw,
                    vsi_l_offset nImgOffset, int nPixelOffset,
                    int nLineOffset,
                    GDALDataType eDataType, int bNativeOrder );

    ~PAuxRasterBand() override;

    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    CPLErr SetNoDataValue( double ) override;

    GDALColorTable *GetColorTable() override;
    GDALColorInterp GetColorInterpretation() override;

    void SetDescription( const char *pszNewDescription ) override;
};

/************************************************************************/
/*                           PAuxRasterBand()                           */
/************************************************************************/

PAuxRasterBand::PAuxRasterBand( GDALDataset *poDSIn, int nBandIn,
                                VSILFILE * fpRawIn, vsi_l_offset nImgOffsetIn,
                                int nPixelOffsetIn, int nLineOffsetIn,
                                GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand( poDSIn, nBandIn, fpRawIn,
                   nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn,
                   eDataTypeIn, bNativeOrderIn, RawRasterBand::OwnFP::NO )
{
    PAuxDataset *poPDS = reinterpret_cast<PAuxDataset *>( poDS );

/* -------------------------------------------------------------------- */
/*      Does this channel have a description?                           */
/* -------------------------------------------------------------------- */
    char szTarget[128] = { '\0' };

    snprintf( szTarget, sizeof(szTarget), "ChanDesc-%d", nBand );
    if( CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) != nullptr )
        GDALRasterBand::SetDescription(
            CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) );

/* -------------------------------------------------------------------- */
/*      See if we have colors.  Currently we must have color zero,      */
/*      but this should not really be a limitation.                     */
/* -------------------------------------------------------------------- */
    snprintf( szTarget, sizeof(szTarget),
              "METADATA_IMG_%d_Class_%d_Color", nBand, 0 );
    if( CSLFetchNameValue( poPDS->papszAuxLines, szTarget ) != nullptr )
    {
        poCT = new GDALColorTable();

        for( int i = 0; i < 256; i++ )
        {
            snprintf( szTarget, sizeof(szTarget),
                      "METADATA_IMG_%d_Class_%d_Color", nBand, i );
            const char *pszLine
                = CSLFetchNameValue( poPDS->papszAuxLines, szTarget );
            while( pszLine && *pszLine == ' ' )
                pszLine++;

            int nRed = 0;
            int nGreen = 0;
            int nBlue = 0;
            // TODO(schwehr): Replace sscanf with something safe.
            if( pszLine != nullptr
                && STARTS_WITH_CI(pszLine, "(RGB:")
                && sscanf( pszLine+5, "%d %d %d",
                           &nRed, &nGreen, &nBlue ) == 3 )
            {
                GDALColorEntry oColor = {
                    static_cast<short>(nRed),
                    static_cast<short>(nGreen),
                    static_cast<short>(nBlue),
                    255
                };

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
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double PAuxRasterBand::GetNoDataValue( int *pbSuccess )

{
    char szTarget[128] = { '\0' };
    snprintf( szTarget, sizeof(szTarget),
              "METADATA_IMG_%d_NO_DATA_VALUE", nBand );

    PAuxDataset *poPDS = reinterpret_cast<PAuxDataset *>( poDS );
    const char  *pszLine = CSLFetchNameValue( poPDS->papszAuxLines, szTarget );

    if( pbSuccess != nullptr )
        *pbSuccess = (pszLine != nullptr);

    if( pszLine == nullptr )
        return -1.0e8;

    return CPLAtof(pszLine);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr PAuxRasterBand::SetNoDataValue( double dfNewValue )

{
    if( GetAccess() == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Can't update readonly dataset." );
        return CE_Failure;
    }

    char szTarget[128] = { '\0' };
    char szValue[128] = { '\0' };
    snprintf( szTarget, sizeof(szTarget),
              "METADATA_IMG_%d_NO_DATA_VALUE", nBand );
    CPLsnprintf( szValue, sizeof(szValue),
                 "%24.12f", dfNewValue );

    PAuxDataset *poPDS = reinterpret_cast<PAuxDataset *>( poDS );
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
    if( GetAccess() == GA_Update )
    {
        char szTarget[128] = { '\0' };
        snprintf( szTarget, sizeof(szTarget), "ChanDesc-%d", nBand );

        PAuxDataset *poPDS = reinterpret_cast<PAuxDataset *>( poDS );
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
    if( poCT == nullptr )
        return GCI_Undefined;

    return GCI_PaletteIndex;
}

/************************************************************************/
/* ==================================================================== */
/*                              PAuxDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            PAuxDataset()                             */
/************************************************************************/

PAuxDataset::PAuxDataset() :
    fpImage(nullptr),
    nGCPCount(0),
    pasGCPList(nullptr),
    pszGCPProjection(nullptr),
    pszProjection(nullptr),
    pszAuxFilename(nullptr),
    papszAuxLines(nullptr),
    bAuxUpdated(FALSE)
{}

/************************************************************************/
/*                            ~PAuxDataset()                            */
/************************************************************************/

PAuxDataset::~PAuxDataset()

{
    FlushCache();
    if( fpImage != nullptr && VSIFCloseL( fpImage ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "I/O error" );
    }

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
/*                            GetFileList()                             */
/************************************************************************/

char **PAuxDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();
    papszFileList = CSLAddString( papszFileList, pszAuxFilename );
    return papszFileList;
}

/************************************************************************/
/*                              PCI2WKT()                               */
/*                                                                      */
/*      Convert PCI coordinate system to WKT.  For now this is very     */
/*      incomplete, but can be filled out in the future.                */
/************************************************************************/

char *PAuxDataset::PCI2WKT( const char *pszGeosys,
                            const char *pszProjParams )

{
    while( *pszGeosys == ' ' )
        pszGeosys++;

/* -------------------------------------------------------------------- */
/*      Parse projection parameters array.                              */
/* -------------------------------------------------------------------- */
    double adfProjParams[16] = { 0.0 };

    if( pszProjParams != nullptr )
    {
        char **papszTokens = CSLTokenizeString( pszProjParams );

        for( int i = 0;
             i < 16 && papszTokens != nullptr && papszTokens[i] != nullptr;
             i++ )
            adfProjParams[i] = CPLAtof(papszTokens[i]);

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Convert to SRS.                                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    if( oSRS.importFromPCI( pszGeosys, nullptr, adfProjParams ) == OGRERR_NONE )
    {
        char *pszResult = nullptr;

        oSRS.exportToWkt( &pszResult );

        return pszResult;
    }

    return nullptr;
}

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void PAuxDataset::ScanForGCPs()

{
    const int MAX_GCP = 256;

    nGCPCount = 0;
    CPLAssert( pasGCPList == nullptr );
    pasGCPList = reinterpret_cast<GDAL_GCP *>(
        CPLCalloc( sizeof(GDAL_GCP), MAX_GCP ) );

/* -------------------------------------------------------------------- */
/*      Get the GCP coordinate system.                                  */
/* -------------------------------------------------------------------- */
    const char *pszMapUnits =
        CSLFetchNameValue( papszAuxLines, "GCP_1_MapUnits" );
    const char *pszProjParams =
        CSLFetchNameValue( papszAuxLines, "GCP_1_ProjParms" );

    if( pszMapUnits != nullptr )
        pszGCPProjection = PCI2WKT( pszMapUnits, pszProjParams );

/* -------------------------------------------------------------------- */
/*      Collect standalone GCPs.  They look like:                       */
/*                                                                      */
/*      GCP_1_n = row, col, x, y [,z [,"id"[, "desc"]]]                 */
/* -------------------------------------------------------------------- */
    for( int i = 0; nGCPCount < MAX_GCP; i++ )
    {
        char szName[50] = { '\0' };
        snprintf( szName, sizeof(szName), "GCP_1_%d", i+1 );
        if( CSLFetchNameValue( papszAuxLines, szName ) == nullptr )
            break;

        char **papszTokens = CSLTokenizeStringComplex(
            CSLFetchNameValue( papszAuxLines, szName ),
            " ", TRUE, FALSE );

        if( CSLCount(papszTokens) >= 4 )
        {
            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            pasGCPList[nGCPCount].dfGCPX = CPLAtof(papszTokens[2]);
            pasGCPList[nGCPCount].dfGCPY = CPLAtof(papszTokens[3]);
            pasGCPList[nGCPCount].dfGCPPixel = CPLAtof(papszTokens[0]);
            pasGCPList[nGCPCount].dfGCPLine = CPLAtof(papszTokens[1]);

            if( CSLCount(papszTokens) > 4 )
                pasGCPList[nGCPCount].dfGCPZ = CPLAtof(papszTokens[4]);

            CPLFree( pasGCPList[nGCPCount].pszId );
            if( CSLCount(papszTokens) > 5 )
            {
                pasGCPList[nGCPCount].pszId = CPLStrdup(papszTokens[5]);
            }
            else
            {
                snprintf( szName, sizeof(szName), "GCP_%d", i+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szName );
            }

            if( CSLCount(papszTokens) > 6 )
            {
                CPLFree( pasGCPList[nGCPCount].pszInfo );
                pasGCPList[nGCPCount].pszInfo = CPLStrdup(papszTokens[6]);
            }

            nGCPCount++;
        }

        CSLDestroy(papszTokens);
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

const char *PAuxDataset::_GetGCPProjection()

{
    if( nGCPCount > 0 && pszGCPProjection != nullptr )
        return pszGCPProjection;

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

const char *PAuxDataset::_GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;

    return GDALPamDataset::_GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PAuxDataset::GetGeoTransform( double * padfGeoTransform )

{
    if( CSLFetchNameValue(papszAuxLines, "UpLeftX") == nullptr
        || CSLFetchNameValue(papszAuxLines, "UpLeftY") == nullptr
        || CSLFetchNameValue(papszAuxLines, "LoRightX") == nullptr
        || CSLFetchNameValue(papszAuxLines, "LoRightY") == nullptr )
    {
        padfGeoTransform[0] = 0.0;
        padfGeoTransform[1] = 1.0;
        padfGeoTransform[2] = 0.0;
        padfGeoTransform[3] = 0.0;
        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = 1.0;

        return CE_Failure;
    }

    const double dfUpLeftX =
        CPLAtof(CSLFetchNameValue(papszAuxLines, "UpLeftX" ));
    const double dfUpLeftY =
        CPLAtof(CSLFetchNameValue(papszAuxLines, "UpLeftY" ));
    const double dfLoRightX =
        CPLAtof(CSLFetchNameValue(papszAuxLines, "LoRightX" ));
    const double dfLoRightY =
        CPLAtof(CSLFetchNameValue(papszAuxLines, "LoRightY" ));

    padfGeoTransform[0] = dfUpLeftX;
    padfGeoTransform[1] = (dfLoRightX - dfUpLeftX) / GetRasterXSize();
    padfGeoTransform[2] = 0.0;
    padfGeoTransform[3] = dfUpLeftY;
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[5] = (dfLoRightY - dfUpLeftY) / GetRasterYSize();

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PAuxDataset::SetGeoTransform( double * padfGeoTransform )

{
    char szUpLeftX[128] = { '\0' };
    char szUpLeftY[128] = { '\0' };
    char szLoRightX[128] = { '\0' };
    char szLoRightY[128] = { '\0' };

    if( std::abs(padfGeoTransform[0]) < 181
        && std::abs(padfGeoTransform[1]) < 1 )
    {
        CPLsnprintf( szUpLeftX, sizeof(szUpLeftX), "%.12f",
                     padfGeoTransform[0] );
        CPLsnprintf( szUpLeftY, sizeof(szUpLeftY), "%.12f",
                     padfGeoTransform[3] );
        CPLsnprintf( szLoRightX, sizeof(szLoRightX), "%.12f",
                     padfGeoTransform[0] +
                     padfGeoTransform[1] * GetRasterXSize() );
        CPLsnprintf( szLoRightY, sizeof(szLoRightY), "%.12f",
                     padfGeoTransform[3] +
                     padfGeoTransform[5] * GetRasterYSize() );
    }
    else
    {
        CPLsnprintf( szUpLeftX, sizeof(szUpLeftX), "%.3f",
                     padfGeoTransform[0] );
        CPLsnprintf( szUpLeftY, sizeof(szUpLeftY), "%.3f",
                     padfGeoTransform[3] );
        CPLsnprintf( szLoRightX, sizeof(szLoRightX), "%.3f",
                     padfGeoTransform[0] +
                     padfGeoTransform[1] * GetRasterXSize() );
        CPLsnprintf( szLoRightY, sizeof(szLoRightY), "%.3f",
                     padfGeoTransform[3] +
                     padfGeoTransform[5] * GetRasterYSize() );
    }

    papszAuxLines = CSLSetNameValue( papszAuxLines, "UpLeftX", szUpLeftX );
    papszAuxLines = CSLSetNameValue( papszAuxLines, "UpLeftY", szUpLeftY );
    papszAuxLines = CSLSetNameValue( papszAuxLines, "LoRightX", szLoRightX );
    papszAuxLines = CSLSetNameValue( papszAuxLines, "LoRightY", szLoRightY );

    bAuxUpdated = TRUE;

    return CE_None;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PAuxDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 1 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If this is an .aux file, fetch out and form the name of the     */
/*      file it references.                                             */
/* -------------------------------------------------------------------- */

    CPLString osTarget = poOpenInfo->pszFilename;

    if( EQUAL(CPLGetExtension( poOpenInfo->pszFilename ),"aux")
        && STARTS_WITH_CI(reinterpret_cast<char *>( poOpenInfo->pabyHeader ),
                          "AuxilaryTarget: ") )
    {
        const char *pszSrc = reinterpret_cast<const char *>(
            poOpenInfo->pabyHeader+16 );

        char szAuxTarget[1024] = { '\0' };
        for( int i = 0;
             i < static_cast<int>( sizeof(szAuxTarget) ) - 1 &&
             pszSrc[i] != 10 && pszSrc[i] != 13 && pszSrc[i] != '\0';
             i++ )
        {
            szAuxTarget[i] = pszSrc[i];
        }
        szAuxTarget[sizeof(szAuxTarget) - 1] = '\0';

        const std::string osPath(CPLGetPath(poOpenInfo->pszFilename));
        osTarget = CPLFormFilename(osPath.c_str(), szAuxTarget, nullptr);
    }

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .aux           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    CPLString osAuxFilename = CPLResetExtension(osTarget,"aux");

/* -------------------------------------------------------------------- */
/*      Do we have a .aux file?                                         */
/* -------------------------------------------------------------------- */
    char** papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if( papszSiblingFiles != nullptr
        && CSLFindString( papszSiblingFiles,
                          CPLGetFilename(osAuxFilename) ) == -1 )
    {
        return nullptr;
    }

    VSILFILE *fp = VSIFOpenL( osAuxFilename, "r" );
    if( fp == nullptr )
    {
        osAuxFilename = CPLResetExtension(osTarget,"AUX");
        fp = VSIFOpenL( osAuxFilename, "r" );
    }

    if( fp == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Is this file a PCI .aux file?  Check the first line for the     */
/*      telltale AuxilaryTarget keyword.                                */
/*                                                                      */
/*      At this point we should be verifying that it refers to our      */
/*      binary file, but that is a pretty involved test.                */
/* -------------------------------------------------------------------- */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    const char *pszLine = CPLReadLine2L( fp, 1024, nullptr );
    CPLPopErrorHandler();

    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    if( pszLine == nullptr
        || (!STARTS_WITH_CI(pszLine, "AuxilaryTarget")
            && !STARTS_WITH_CI(pszLine, "AuxiliaryTarget")) )
    {
        CPLErrorReset();
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    PAuxDataset *poDS = new PAuxDataset();

/* -------------------------------------------------------------------- */
/*      Load the .aux file into a string list suitable to be            */
/*      searched with CSLFetchNameValue().                              */
/* -------------------------------------------------------------------- */
    poDS->papszAuxLines = CSLLoad2( osAuxFilename, 1024, 1024, nullptr );
    poDS->pszAuxFilename = CPLStrdup(osAuxFilename);

/* -------------------------------------------------------------------- */
/*      Find the RawDefinition line to establish overall parameters.    */
/* -------------------------------------------------------------------- */
    pszLine = CSLFetchNameValue(poDS->papszAuxLines, "RawDefinition");

    // It seems PCI now writes out .aux files without RawDefinition in
    // some cases.  See bug 947.
    if( pszLine == nullptr )
    {
        delete poDS;
        return nullptr;
    }

    char **papszTokens = CSLTokenizeString(pszLine);

    if( CSLCount(papszTokens) < 3 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "RawDefinition missing or corrupt in %s.",
                  poOpenInfo->pszFilename );
        delete poDS;
        CSLDestroy( papszTokens );
        return nullptr;
    }

    poDS->nRasterXSize = atoi(papszTokens[0]);
    poDS->nRasterYSize = atoi(papszTokens[1]);
    poDS->nBands = atoi(papszTokens[2]);
    poDS->eAccess = poOpenInfo->eAccess;

    CSLDestroy( papszTokens );

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(poDS->nBands, FALSE))
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        poDS->fpImage = VSIFOpenL( osTarget, "rb+" );

        if( poDS->fpImage == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s is missing or read-only, check permissions.",
                      osTarget.c_str() );
            delete poDS;
            return nullptr;
        }
    }
    else
    {
        poDS->fpImage = VSIFOpenL( osTarget, "rb" );

        if( poDS->fpImage == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "File %s is missing or unreadable.",
                      osTarget.c_str() );
            delete poDS;
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect raw definitions of each channel and create              */
/*      corresponding bands.                                            */
/* -------------------------------------------------------------------- */
    int iBand = 0;
    for( int i = 0; i < poDS->nBands; i++ )
    {
        char szDefnName[32] = { '\0' };
        snprintf( szDefnName, sizeof(szDefnName), "ChanDefinition-%d", i+1 );

        pszLine = CSLFetchNameValue(poDS->papszAuxLines, szDefnName);
        if (pszLine == nullptr)
        {
            continue;
        }

        papszTokens = CSLTokenizeString(pszLine);
        if( CSLCount(papszTokens) < 4 )
        {
            // Skip the band with broken description
            CSLDestroy( papszTokens );
            continue;
        }

        GDALDataType eType = GDT_Unknown;
        if( EQUAL(papszTokens[0],"16U") )
            eType = GDT_UInt16;
        else if( EQUAL(papszTokens[0],"16S") )
            eType = GDT_Int16;
        else if( EQUAL(papszTokens[0],"32R") )
            eType = GDT_Float32;
        else
            eType = GDT_Byte;

        bool bNative = true;
        if( CSLCount(papszTokens) > 4 )
        {
#ifdef CPL_LSB
            bNative = EQUAL(papszTokens[4], "Swapped");
#else
            bNative = EQUAL(papszTokens[4], "Unswapped");
#endif
        }

        const vsi_l_offset nBandOffset =
            CPLScanUIntBig( papszTokens[1],
                            static_cast<int>(strlen(papszTokens[1])) );
        const int nPixelOffset = atoi(papszTokens[2]);
        const int nLineOffset = atoi(papszTokens[3]);

        if (nPixelOffset <= 0 || nLineOffset <= 0)
        {
            // Skip the band with broken offsets.
            CSLDestroy( papszTokens );
            continue;
        }

        poDS->SetBand( iBand+1,
            new PAuxRasterBand( poDS, iBand+1, poDS->fpImage,
                                nBandOffset,
                                nPixelOffset,
                                nLineOffset, eType, bNative ) );
        iBand++;

        CSLDestroy( papszTokens );
    }

    poDS->nBands = iBand;

/* -------------------------------------------------------------------- */
/*      Get the projection.                                             */
/* -------------------------------------------------------------------- */
    const char *pszMapUnits =
        CSLFetchNameValue( poDS->papszAuxLines, "MapUnits" );
    const char *pszProjParams =
        CSLFetchNameValue( poDS->papszAuxLines, "ProjParams" );

    if( pszMapUnits != nullptr )
        poDS->pszProjection = poDS->PCI2WKT( pszMapUnits, pszProjParams );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( osTarget );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, osTarget );

    poDS->ScanForGCPs();
    poDS->bAuxUpdated = FALSE;

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *PAuxDataset::Create( const char * pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char **papszOptions )

{
    const char *pszInterleave = CSLFetchNameValue( papszOptions, "INTERLEAVE" );
    if( pszInterleave == nullptr )
        pszInterleave = "BAND";

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

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Sum the sizes of the band pixel types.                          */
/* -------------------------------------------------------------------- */
    int nPixelSizeSum = 0;

    for( int iBand = 0; iBand < nBands; iBand++ )
        nPixelSizeSum += GDALGetDataTypeSizeBytes(eType);

/* -------------------------------------------------------------------- */
/*      Try to create the file.                                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( pszFilename, "w" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszFilename );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Just write out a couple of bytes to establish the binary        */
/*      file, and then close it.                                        */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFWriteL( "\0\0", 2, 1, fp ));
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Create the aux filename.                                        */
/* -------------------------------------------------------------------- */
    char *pszAuxFilename = static_cast<char *>(
        CPLMalloc( strlen( pszFilename ) + 5 ) );
    strcpy( pszAuxFilename, pszFilename );

    for( int i = static_cast<int>(strlen(pszAuxFilename))-1; i > 0; i-- )
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
    fp = VSIFOpenL( pszAuxFilename, "wt" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file `%s' failed.\n",
                  pszAuxFilename );
        return nullptr;
    }
    CPLFree( pszAuxFilename );

/* -------------------------------------------------------------------- */
/*      We need to write out the original filename but without any      */
/*      path components in the AuxilaryTarget line.  Do so now.         */
/* -------------------------------------------------------------------- */
    int iStart = static_cast<int>(strlen(pszFilename))-1;
    while( iStart > 0 && pszFilename[iStart-1] != '/'
           && pszFilename[iStart-1] != '\\' )
        iStart--;

    CPL_IGNORE_RET_VAL(VSIFPrintfL( fp, "AuxilaryTarget: %s\n", pszFilename + iStart ));

/* -------------------------------------------------------------------- */
/*      Write out the raw definition for the dataset as a whole.        */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFPrintfL( fp, "RawDefinition: %d %d %d\n",
                nXSize, nYSize, nBands ));

/* -------------------------------------------------------------------- */
/*      Write out a definition for each band.  We always write band     */
/*      sequential files for now as these are pretty efficiently        */
/*      handled by GDAL.                                                */
/* -------------------------------------------------------------------- */
    vsi_l_offset nImgOffset = 0;

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        int nPixelOffset = 0;
        int nLineOffset = 0;
        vsi_l_offset nNextImgOffset = 0;

/* -------------------------------------------------------------------- */
/*      Establish our file layout based on supplied interleaving.       */
/* -------------------------------------------------------------------- */
        if( EQUAL(pszInterleave,"LINE") )
        {
            nPixelOffset = GDALGetDataTypeSizeBytes(eType);
            nLineOffset = nXSize * nPixelSizeSum;
            nNextImgOffset = nImgOffset + nPixelOffset * nXSize;
        }
        else if( EQUAL(pszInterleave,"PIXEL") )
        {
            nPixelOffset = nPixelSizeSum;
            nLineOffset = nXSize * nPixelOffset;
            nNextImgOffset = nImgOffset + GDALGetDataTypeSizeBytes(eType);
        }
        else /* default to band */
        {
            nPixelOffset = GDALGetDataTypeSize(eType)/8;
            nLineOffset = nXSize * nPixelOffset;
            nNextImgOffset =
                nImgOffset + nYSize * static_cast<vsi_l_offset>( nLineOffset );
        }

/* -------------------------------------------------------------------- */
/*      Write out line indicating layout.                               */
/* -------------------------------------------------------------------- */
        const char *pszTypeName = nullptr;
        if( eType == GDT_Float32 )
            pszTypeName = "32R";
        else if( eType == GDT_Int16 )
            pszTypeName = "16S";
        else if( eType == GDT_UInt16 )
            pszTypeName = "16U";
        else
            pszTypeName = "8U";

        CPL_IGNORE_RET_VAL(
            VSIFPrintfL( fp, "ChanDefinition-%d: %s " CPL_FRMT_GIB " %d %d %s\n",
                         iBand+1,
                         pszTypeName, static_cast<GIntBig>( nImgOffset ),
                         nPixelOffset, nLineOffset,
#ifdef CPL_LSB
                         "Swapped"
#else
                         "Unswapped"
#endif
                         ) );

        nImgOffset = nNextImgOffset;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    return static_cast<GDALDataset *>(
        GDALOpen( pszFilename, GA_Update ) );
}

/************************************************************************/
/*                             PAuxDelete()                             */
/************************************************************************/

static CPLErr PAuxDelete( const char * pszBasename )

{
    VSILFILE *fp = VSIFOpenL( CPLResetExtension( pszBasename, "aux" ), "r" );
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a PAux dataset: "
                  "there is no .aux file.",
                  pszBasename );
        return CE_Failure;
    }

    const char *pszLine = CPLReadLineL( fp );
    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

    if( pszLine == nullptr || !STARTS_WITH_CI(pszLine, "AuxilaryTarget") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s does not appear to be a PAux dataset:"
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
    if( GDALGetDriverByName( "PAux" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PAux" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "PCI .aux Labelled" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/paux.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Float32" );
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='INTERLEAVE' type='string-select' default='BAND'>"
        "       <Value>BAND</Value>"
        "       <Value>LINE</Value>"
        "       <Value>PIXEL</Value>"
        "   </Option>"
        "</CreationOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = PAuxDataset::Open;
    poDriver->pfnCreate = PAuxDataset::Create;
    poDriver->pfnDelete = PAuxDelete;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
