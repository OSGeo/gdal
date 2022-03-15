/******************************************************************************
 *
 * Project:  ERMapper .ers Driver
 * Purpose:  Implementation of .ers driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ershdrnode.h"
#include "gdal_frmts.h"
#include "gdal_proxy.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

#include <limits>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              ERSDataset                              */
/* ==================================================================== */
/************************************************************************/

class ERSRasterBand;

class ERSDataset final: public RawDataset
{
    friend class ERSRasterBand;

    VSILFILE    *fpImage;  // Image data file.
    GDALDataset *poDepFile;

    int         bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    CPLString   osRawFilename;

    int         bHDRDirty;
    ERSHdrNode *poHeader;

    const char *Find( const char *, const char * );

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

    void          ReadGCPs();

    int         bHasNoDataValue;
    double      dfNoDataValue;

    CPLString      osProj, osProjForced;
    CPLString      osDatum, osDatumForced;
    CPLString      osUnits, osUnitsForced;
    void           WriteProjectionInfo(const char* pszProj,
                                       const char* pszDatum,
                                       const char* pszUnits);

    CPLStringList oERSMetadataList;

  protected:
    int CloseDependentDatasets() override;

  public:
    ERSDataset();
    ~ERSDataset() override;

    void FlushCache(bool bAtClosing) override;
    CPLErr GetGeoTransform( double * padfTransform ) override;
    CPLErr SetGeoTransform( double *padfTransform ) override;
    const char *_GetProjectionRef(void) override;
    CPLErr _SetProjection( const char * ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }
    char **GetFileList(void) override;

    int GetGCPCount() override;
    const char *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    const GDAL_GCP *GetGCPs() override;
    CPLErr _SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                    const char *pszGCPProjection ) override;
    using RawDataset::SetGCPs;
    CPLErr SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                    const OGRSpatialReference* poSRS ) override {
        return OldSetGCPsFromNew(nGCPCountIn, pasGCPListIn, poSRS);
    }

    char **GetMetadataDomainList() override;
    const char *GetMetadataItem( const char * pszName,
                                 const char * pszDomain = "" ) override;
    char **GetMetadata( const char * pszDomain = "" ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParamList );
};

/************************************************************************/
/*                            ERSDataset()                             */
/************************************************************************/

ERSDataset::ERSDataset() :
    fpImage(nullptr),
    poDepFile(nullptr),
    bGotTransform(FALSE),
    pszProjection(CPLStrdup("")),
    bHDRDirty(FALSE),
    poHeader(nullptr),
    nGCPCount(0),
    pasGCPList(nullptr),
    pszGCPProjection(CPLStrdup("")),
    bHasNoDataValue(FALSE),
    dfNoDataValue(0.0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ERSDataset()                            */
/************************************************************************/

ERSDataset::~ERSDataset()

{
    ERSDataset::FlushCache(true);

    if( fpImage != nullptr )
    {
        VSIFCloseL( fpImage );
    }

    ERSDataset::CloseDependentDatasets();

    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    if( poHeader != nullptr )
        delete poHeader;
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

int ERSDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = RawDataset::CloseDependentDatasets();

    if( poDepFile != nullptr )
    {
        bHasDroppedRef = TRUE;

        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            delete papoBands[iBand];
            papoBands[iBand] = nullptr;
        }
        nBands = 0;

        GDALClose( (GDALDatasetH) poDepFile );
        poDepFile = nullptr;
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void ERSDataset::FlushCache(bool bAtClosing)

{
    if( bHDRDirty )
    {
        VSILFILE * fpERS = VSIFOpenL( GetDescription(), "w" );
        if( fpERS == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to rewrite %s header.",
                      GetDescription() );
        }
        else
        {
            VSIFPrintfL( fpERS, "DatasetHeader Begin\n" );
            poHeader->WriteSelf( fpERS, 1 );
            VSIFPrintfL( fpERS, "DatasetHeader End\n" );
            VSIFCloseL( fpERS );
        }
    }

    RawDataset::FlushCache(bAtClosing);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **ERSDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "ERS", nullptr);
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *ERSDataset::GetMetadataItem( const char * pszName,
                                         const char * pszDomain )
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "ERS") && pszName != nullptr)
    {
        if (EQUAL(pszName, "PROJ"))
            return osProj.size() ? osProj.c_str() : nullptr;
        if (EQUAL(pszName, "DATUM"))
            return osDatum.size() ? osDatum.c_str() : nullptr;
        if (EQUAL(pszName, "UNITS"))
            return osUnits.size() ? osUnits.c_str() : nullptr;
    }
    return GDALPamDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **ERSDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain, "ERS") )
    {
        oERSMetadataList.Clear();
        if (!osProj.empty() )
            oERSMetadataList.AddString(CPLSPrintf("%s=%s", "PROJ", osProj.c_str()));
        if (!osDatum.empty() )
            oERSMetadataList.AddString(CPLSPrintf("%s=%s", "DATUM", osDatum.c_str()));
        if (!osUnits.empty() )
            oERSMetadataList.AddString(CPLSPrintf("%s=%s", "UNITS", osUnits.c_str()));
        return oERSMetadataList.List();
    }

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int ERSDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *ERSDataset::_GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *ERSDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr ERSDataset::_SetGCPs( int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                            const char *pszGCPProjectionIn )

{
/* -------------------------------------------------------------------- */
/*      Clean old gcps.                                                 */
/* -------------------------------------------------------------------- */
    CPLFree( pszGCPProjection );
    pszGCPProjection = nullptr;

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );

        pasGCPList = nullptr;
        nGCPCount = 0;
    }

/* -------------------------------------------------------------------- */
/*      Copy new ones.                                                  */
/* -------------------------------------------------------------------- */
    nGCPCount = nGCPCountIn;
    pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPListIn );
    pszGCPProjection = CPLStrdup( pszGCPProjectionIn );

/* -------------------------------------------------------------------- */
/*      Setup the header contents corresponding to these GCPs.          */
/* -------------------------------------------------------------------- */
    bHDRDirty = TRUE;

    poHeader->Set( "RasterInfo.WarpControl.WarpType", "Polynomial" );
    if( nGCPCount > 6 )
        poHeader->Set( "RasterInfo.WarpControl.WarpOrder", "2" );
    else
        poHeader->Set( "RasterInfo.WarpControl.WarpOrder", "1" );
    poHeader->Set( "RasterInfo.WarpControl.WarpSampling", "Nearest" );

/* -------------------------------------------------------------------- */
/*      Translate the projection.                                       */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS( pszGCPProjection );
    char szERSProj[32], szERSDatum[32], szERSUnits[32];

    oSRS.exportToERM( szERSProj, szERSDatum, szERSUnits );

    /* Write the above computed values, unless they have been overridden by */
    /* the creation options PROJ, DATUM or UNITS */

    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Datum",
                   CPLString().Printf( "\"%s\"",
                        (osDatum.size()) ? osDatum.c_str() : szERSDatum ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Projection",
                   CPLString().Printf( "\"%s\"",
                        (osProj.size()) ? osProj.c_str() : szERSProj ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.CoordinateType",
                   CPLString().Printf( "EN" ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Units",
                   CPLString().Printf( "\"%s\"",
                        (osUnits.size()) ? osUnits.c_str() : szERSUnits ) );
    poHeader->Set( "RasterInfo.WarpControl.CoordinateSpace.Rotation",
                   "0:0:0.0" );

/* -------------------------------------------------------------------- */
/*      Translate the GCPs.                                             */
/* -------------------------------------------------------------------- */
    CPLString osControlPoints = "{\n";

    for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        CPLString osLine;

        CPLString osId = pasGCPList[iGCP].pszId;
        if( osId.empty() )
            osId.Printf( "%d", iGCP + 1 );

        osLine.Printf( "\t\t\t\t\"%s\"\tYes\tYes\t%.6f\t%.6f\t%.15g\t%.15g\t%.15g\n",
                       osId.c_str(),
                       pasGCPList[iGCP].dfGCPPixel,
                       pasGCPList[iGCP].dfGCPLine,
                       pasGCPList[iGCP].dfGCPX,
                       pasGCPList[iGCP].dfGCPY,
                       pasGCPList[iGCP].dfGCPZ );
        osControlPoints += osLine;
    }
    osControlPoints += "\t\t}";

    poHeader->Set( "RasterInfo.WarpControl.ControlPoints", osControlPoints );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ERSDataset::_GetProjectionRef()

{
    // try xml first
    const char* pszPrj = GDALPamDataset::_GetProjectionRef();
    if(pszPrj && strlen(pszPrj) > 0)
        return pszPrj;

    return pszProjection;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr ERSDataset::_SetProjection( const char *pszSRS )

{
    if( pszProjection && EQUAL(pszSRS,pszProjection) )
        return CE_None;

    if( pszSRS == nullptr )
        pszSRS = "";

    CPLFree( pszProjection );
    pszProjection = CPLStrdup(pszSRS);

    OGRSpatialReference oSRS( pszSRS );
    char szERSProj[32], szERSDatum[32], szERSUnits[32];

    oSRS.exportToERM( szERSProj, szERSDatum, szERSUnits );

    /* Write the above computed values, unless they have been overridden by */
    /* the creation options PROJ, DATUM or UNITS */
    if( !osProjForced.empty() )
        osProj = osProjForced;
    else
        osProj = szERSProj;
    if( !osDatumForced.empty() )
        osDatum = osDatumForced;
    else
        osDatum = szERSDatum;
    if( !osUnitsForced.empty() )
        osUnits = osUnitsForced;
    else
        osUnits = szERSUnits;

    WriteProjectionInfo( osProj, osDatum, osUnits );

    return CE_None;
}

/************************************************************************/
/*                         WriteProjectionInfo()                        */
/************************************************************************/

void ERSDataset::WriteProjectionInfo(const char* pszProj,
                                     const char* pszDatum,
                                     const char* pszUnits)
{
    bHDRDirty = TRUE;
    poHeader->Set( "CoordinateSpace.Datum",
                   CPLString().Printf( "\"%s\"", pszDatum ) );
    poHeader->Set( "CoordinateSpace.Projection",
                   CPLString().Printf( "\"%s\"", pszProj ) );
    poHeader->Set( "CoordinateSpace.CoordinateType",
                   CPLString().Printf( "EN" ) );
    poHeader->Set( "CoordinateSpace.Units",
                   CPLString().Printf( "\"%s\"", pszUnits ) );
    poHeader->Set( "CoordinateSpace.Rotation",
                   "0:0:0.0" );

/* -------------------------------------------------------------------- */
/*      It seems that CoordinateSpace needs to come before              */
/*      RasterInfo.  Try moving it up manually.                         */
/* -------------------------------------------------------------------- */
    int iRasterInfo = -1;
    int iCoordSpace = -1;

    for( int i = 0; i < poHeader->nItemCount; i++ )
    {
        if( EQUAL(poHeader->papszItemName[i],"RasterInfo") )
            iRasterInfo = i;

        if( EQUAL(poHeader->papszItemName[i],"CoordinateSpace") )
        {
            iCoordSpace = i;
            break;
        }
    }

    if( iCoordSpace > iRasterInfo && iRasterInfo != -1 )
    {
        for( int i = iCoordSpace; i > 0 && i != iRasterInfo; i-- )
        {

            ERSHdrNode *poTemp = poHeader->papoItemChild[i];
            poHeader->papoItemChild[i] = poHeader->papoItemChild[i-1];
            poHeader->papoItemChild[i-1] = poTemp;

            char *pszTemp = poHeader->papszItemName[i];
            poHeader->papszItemName[i] = poHeader->papszItemName[i-1];
            poHeader->papszItemName[i-1] = pszTemp;

            pszTemp = poHeader->papszItemValue[i];
            poHeader->papszItemValue[i] = poHeader->papszItemValue[i-1];
            poHeader->papszItemValue[i-1] = pszTemp;
        }
    }
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

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr ERSDataset::SetGeoTransform( double *padfTransform )

{
    if( memcmp( padfTransform, adfGeoTransform, sizeof(double)*6 ) == 0 )
        return CE_None;

    if( adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Rotated and skewed geotransforms not currently supported for ERS driver." );
        return CE_Failure;
    }

    bGotTransform = TRUE;
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    bHDRDirty = TRUE;

    poHeader->Set( "RasterInfo.CellInfo.Xdimension",
                   CPLString().Printf( "%.15g", fabs(adfGeoTransform[1]) ) );
    poHeader->Set( "RasterInfo.CellInfo.Ydimension",
                   CPLString().Printf( "%.15g", fabs(adfGeoTransform[5]) ) );
    poHeader->Set( "RasterInfo.RegistrationCoord.Eastings",
                   CPLString().Printf( "%.15g", adfGeoTransform[0] ) );
    poHeader->Set( "RasterInfo.RegistrationCoord.Northings",
                   CPLString().Printf( "%.15g", adfGeoTransform[3] ) );

    if( CPLAtof(poHeader->Find("RasterInfo.RegistrationCellX", "0")) != 0.0 ||
        CPLAtof(poHeader->Find("RasterInfo.RegistrationCellY", "0")) != 0.0 )
    {
        // Reset RegistrationCellX/Y to 0 if the header gets rewritten (#5493)
        poHeader->Set("RasterInfo.RegistrationCellX", "0");
        poHeader->Set("RasterInfo.RegistrationCellY", "0");
    }

    return CE_None;
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
        CSLDestroy(papszTokens);
        return CPLAtof( pszDMS );
    }

    double dfResult = fabs(CPLAtof(papszTokens[0]))
        + CPLAtof(papszTokens[1]) / 60.0
        + CPLAtof(papszTokens[2]) / 3600.0;

    if( CPLAtof(papszTokens[0]) < 0 )
        dfResult *= -1;

    CSLDestroy( papszTokens );
    return dfResult;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ERSDataset::GetFileList()

{
    static thread_local int nRecLevel = 0;
    if( nRecLevel > 0 )
        return nullptr;

    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    // Add raw data file if we have one.
    if( !osRawFilename.empty() )
        papszFileList = CSLAddString( papszFileList, osRawFilename );

    // If we have a dependent file, merge its list of files in.
    if( poDepFile )
    {
        nRecLevel ++;
        char **papszDepFiles = poDepFile->GetFileList();
        nRecLevel --;
        papszFileList =
            CSLInsertStrings( papszFileList, -1, papszDepFiles );
        CSLDestroy( papszDepFiles );
    }

    return papszFileList;
}

/************************************************************************/
/*                              ReadGCPs()                              */
/*                                                                      */
/*      Read the GCPs from the header.                                  */
/************************************************************************/

void ERSDataset::ReadGCPs()

{
    const char *pszCP =
        poHeader->Find( "RasterInfo.WarpControl.ControlPoints", nullptr );

    if( pszCP == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Parse the control points.  They will look something like:       */
/*                                                                      */
/*   "1035" Yes No 2344.650885 3546.419458 483270.73 3620906.21 3.105   */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeStringComplex( pszCP, "{ \t}", TRUE,FALSE);
    int nItemCount = CSLCount(papszTokens);

/* -------------------------------------------------------------------- */
/*      Work out if we have elevation values or not.                    */
/* -------------------------------------------------------------------- */
    int nItemsPerLine;

    if( nItemCount == 7 )
        nItemsPerLine = 7;
    else if( nItemCount == 8 )
        nItemsPerLine = 8;
    else if( nItemCount < 14 )
    {
        CPLDebug("ERS", "Invalid item count for ControlPoints");
        CSLDestroy( papszTokens );
        return;
    }
    else if( EQUAL(papszTokens[8],"Yes") || EQUAL(papszTokens[8],"No") )
        nItemsPerLine = 7;
    else if( EQUAL(papszTokens[9],"Yes") || EQUAL(papszTokens[9],"No") )
        nItemsPerLine = 8;
    else
    {
        CPLDebug("ERS", "Invalid format for ControlPoints");
        CSLDestroy( papszTokens );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Setup GCPs.                                                     */
/* -------------------------------------------------------------------- */
    CPLAssert( nGCPCount == 0 );

    nGCPCount = nItemCount / nItemsPerLine;
    pasGCPList = (GDAL_GCP *) CPLCalloc(nGCPCount,sizeof(GDAL_GCP));
    GDALInitGCPs( nGCPCount, pasGCPList );

    for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
    {
        GDAL_GCP *psGCP = pasGCPList + iGCP;

        CPLFree( psGCP->pszId );
        psGCP->pszId = CPLStrdup(papszTokens[iGCP*nItemsPerLine+0]);
        psGCP->dfGCPPixel = CPLAtof(papszTokens[iGCP*nItemsPerLine+3]);
        psGCP->dfGCPLine  = CPLAtof(papszTokens[iGCP*nItemsPerLine+4]);
        psGCP->dfGCPX     = CPLAtof(papszTokens[iGCP*nItemsPerLine+5]);
        psGCP->dfGCPY     = CPLAtof(papszTokens[iGCP*nItemsPerLine+6]);
        if( nItemsPerLine == 8 )
            psGCP->dfGCPZ = CPLAtof(papszTokens[iGCP*nItemsPerLine+7]);
    }

    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Parse the GCP projection.                                       */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    osProj = poHeader->Find( "RasterInfo.WarpControl.CoordinateSpace.Projection", "" );
    osDatum = poHeader->Find( "RasterInfo.WarpControl.CoordinateSpace.Datum", "" );
    osUnits = poHeader->Find( "RasterInfo.WarpControl.CoordinateSpace.Units", "" );

    oSRS.importFromERM( !osProj.empty() ? osProj.c_str() : "RAW",
                        !osDatum.empty() ? osDatum.c_str() : "WGS84",
                        !osUnits.empty() ? osUnits.c_str() : "METERS" );

    CPLFree( pszGCPProjection );
    oSRS.exportToWkt( &pszGCPProjection );
}

/************************************************************************/
/* ==================================================================== */
/*                             ERSRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ERSRasterBand final: public RawRasterBand
{
  public:
    ERSRasterBand( GDALDataset *poDS, int nBand, VSILFILE * fpRaw,
                   vsi_l_offset nImgOffset, int nPixelOffset,
                   int nLineOffset,
                   GDALDataType eDataType, int bNativeOrder );

    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    CPLErr SetNoDataValue( double ) override;
};

/************************************************************************/
/*                           ERSRasterBand()                            */
/************************************************************************/

ERSRasterBand::ERSRasterBand( GDALDataset *poDSIn, int nBandIn, VSILFILE * fpRawIn,
                              vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                              int nLineOffsetIn,
                              GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn,
                  nLineOffsetIn, eDataTypeIn, bNativeOrderIn,
                  RawRasterBand::OwnFP::NO)
{}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ERSRasterBand::GetNoDataValue( int *pbSuccess )
{
    ERSDataset* poGDS = cpl::down_cast<ERSDataset*>(poDS);
    if (poGDS->bHasNoDataValue)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return poGDS->dfNoDataValue;
    }

    return RawRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ERSRasterBand::SetNoDataValue( double dfNoDataValue )
{
    ERSDataset* poGDS = cpl::down_cast<ERSDataset*>(poDS);
    if (!poGDS->bHasNoDataValue || poGDS->dfNoDataValue != dfNoDataValue)
    {
        poGDS->bHasNoDataValue = TRUE;
        poGDS->dfNoDataValue = dfNoDataValue;

        poGDS->bHDRDirty = TRUE;
        poGDS->poHeader->Set( "RasterInfo.NullCellValue",
                    CPLString().Printf( "%.16g", dfNoDataValue) );
    }
    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ERSDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user selects the .ers file.                       */
/* -------------------------------------------------------------------- */
    CPLString osHeader((const char *)poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes);

    if( osHeader.ifind( "Algorithm Begin" ) != std::string::npos )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s appears to be an algorithm ERS file, which is not currently supported.",
                  poOpenInfo->pszFilename );
        return FALSE;
    }

    if( osHeader.ifind( "DatasetHeader " ) != std::string::npos )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                         ERSProxyRasterBand                           */
/************************************************************************/

namespace {

static int& GetRecLevel()
{
    static thread_local int nRecLevel = 0;
    return nRecLevel;
}

class ERSProxyRasterBand final : public GDALProxyRasterBand
{
public:
    explicit ERSProxyRasterBand(GDALRasterBand* poUnderlyingBand):
        m_poUnderlyingBand(poUnderlyingBand)
    {
        poUnderlyingBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        eDataType = poUnderlyingBand->GetRasterDataType();
    }

    int GetOverviewCount() override;

protected:
    GDALRasterBand* RefUnderlyingRasterBand() const override { return m_poUnderlyingBand; }

private:
    GDALRasterBand* m_poUnderlyingBand;
};

int ERSProxyRasterBand::GetOverviewCount()
{
    int& nRecLevel = GetRecLevel();
    nRecLevel++;
    int nRet = GDALProxyRasterBand::GetOverviewCount();
    nRecLevel--;
    return nRet;
}


} // namespace

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ERSDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( GetRecLevel() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt at recursively opening ERS dataset");
        return nullptr;
    }

    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Ingest the file as a tree of header nodes.                      */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poHeader = new ERSHdrNode();

    if( !poHeader->ParseHeader( poOpenInfo->fpL ) )
    {
        delete poHeader;
        VSIFCloseL( poOpenInfo->fpL );
        poOpenInfo->fpL = nullptr;
        return nullptr;
    }

    VSIFCloseL( poOpenInfo->fpL );
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Do we have the minimum required information from this header?   */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.NrOfLines" ) == nullptr
        || poHeader->Find( "RasterInfo.NrOfCellsPerLine" ) == nullptr
        || poHeader->Find( "RasterInfo.NrOfBands" ) == nullptr )
    {
        if( poHeader->FindNode( "Algorithm" ) != nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "%s appears to be an algorithm ERS file, which is not currently supported.",
                      poOpenInfo->pszFilename );
        }
        delete poHeader;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ERSDataset *poDS = new ERSDataset();
    poDS->poHeader = poHeader;
    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    int nBands = atoi(poHeader->Find( "RasterInfo.NrOfBands" ));
    poDS->nRasterXSize = atoi(poHeader->Find( "RasterInfo.NrOfCellsPerLine" ));
    poDS->nRasterYSize = atoi(poHeader->Find( "RasterInfo.NrOfLines" ));

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*     Get the HeaderOffset if it exists in the header                  */
/* -------------------------------------------------------------------- */
    GIntBig nHeaderOffset = 0;
    if( poHeader->Find( "HeaderOffset" ) != nullptr )
    {
        nHeaderOffset = atoi(poHeader->Find( "HeaderOffset" ));
    }

/* -------------------------------------------------------------------- */
/*      Establish the data type.                                        */
/* -------------------------------------------------------------------- */
    CPLString osCellType = poHeader->Find( "RasterInfo.CellType",
                                           "Unsigned8BitInteger" );
    GDALDataType eType;
    if( EQUAL(osCellType,"Unsigned8BitInteger") )
        eType = GDT_Byte;
    else if( EQUAL(osCellType,"Signed8BitInteger") )
        eType = GDT_Byte;
    else if( EQUAL(osCellType,"Unsigned16BitInteger") )
        eType = GDT_UInt16;
    else if( EQUAL(osCellType,"Signed16BitInteger") )
        eType = GDT_Int16;
    else if( EQUAL(osCellType,"Unsigned32BitInteger") )
        eType = GDT_UInt32;
    else if( EQUAL(osCellType,"Signed32BitInteger") )
        eType = GDT_Int32;
    else if( EQUAL(osCellType,"IEEE4ByteReal") )
        eType = GDT_Float32;
    else if( EQUAL(osCellType,"IEEE8ByteReal") )
        eType = GDT_Float64;
    else
    {
        CPLDebug( "ERS", "Unknown CellType '%s'", osCellType.c_str() );
        eType = GDT_Byte;
    }

/* -------------------------------------------------------------------- */
/*      Pick up the word order.                                         */
/* -------------------------------------------------------------------- */
    const int bNative =
#ifdef CPL_LSB
    EQUAL(poHeader->Find( "ByteOrder", "LSBFirst" ), "LSBFirst")
#else
    EQUAL(poHeader->Find( "ByteOrder", "MSBFirst" ), "MSBFirst")
#endif
    ;
/* -------------------------------------------------------------------- */
/*      Figure out the name of the target file.                         */
/* -------------------------------------------------------------------- */
    CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    CPLString osDataFile = poHeader->Find( "DataFile", "" );

    if( osDataFile.length() == 0 ) // just strip off extension.
    {
        osDataFile = CPLGetFilename( poOpenInfo->pszFilename );
        osDataFile = osDataFile.substr( 0, osDataFile.find_last_of('.') );
    }

    CPLString osDataFilePath = CPLFormFilename( osPath, osDataFile, nullptr );

/* -------------------------------------------------------------------- */
/*      DataSetType = Translated files are links to things like ecw     */
/*      files.                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(poHeader->Find("DataSetType",""),"Translated") )
    {
        int& nRecLevel = GetRecLevel();
        nRecLevel++;
        poDS->poDepFile = GDALDataset::FromHandle(
            GDALOpen( osDataFilePath, poOpenInfo->eAccess ));
        nRecLevel--;

        if( poDS->poDepFile != nullptr
            && poDS->poDepFile->GetRasterXSize() == poDS->GetRasterXSize()
            && poDS->poDepFile->GetRasterYSize() == poDS->GetRasterYSize()
            && poDS->poDepFile->GetRasterCount() >= nBands )
        {
            for( int iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand( iBand+1,
                    new ERSProxyRasterBand(
                            poDS->poDepFile->GetRasterBand( iBand+1 )) );
            }
        }
        else
        {
            delete poDS->poDepFile;
            poDS->poDepFile = nullptr;
        }
    }

/* ==================================================================== */
/*      While ERStorage indicates a raw file.                           */
/* ==================================================================== */
    else if( EQUAL(poHeader->Find("DataSetType",""),"ERStorage") )
    {
        // Open data file.
        if( poOpenInfo->eAccess == GA_Update )
            poDS->fpImage = VSIFOpenL( osDataFilePath, "r+" );
        else
            poDS->fpImage = VSIFOpenL( osDataFilePath, "r" );

        poDS->osRawFilename = osDataFilePath;

        if( poDS->fpImage != nullptr && nBands > 0 )
        {
            int iWordSize = GDALGetDataTypeSizeBytes(eType);

            const auto knIntMax = std::numeric_limits<int>::max();
            if( nBands > knIntMax / iWordSize ||
                poDS->nRasterXSize > knIntMax / (nBands * iWordSize) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "int overflow");
                delete poDS;
                return nullptr;
            }

            if( !RAWDatasetCheckMemoryUsage(poDS->nRasterXSize,
                                            poDS->nRasterYSize,
                                            nBands,
                                            iWordSize,
                                            iWordSize,
                                            iWordSize * nBands * poDS->nRasterXSize,
                                            nHeaderOffset,
                                            iWordSize * poDS->nRasterXSize,
                                            poDS->fpImage) )
            {
                delete poDS;
                return nullptr;
            }

            for( int iBand = 0; iBand < nBands; iBand++ )
            {
                // Assume pixel interleaved.
                poDS->SetBand(
                    iBand+1,
                    new ERSRasterBand( poDS, iBand+1, poDS->fpImage,
                                       nHeaderOffset
                                       + iWordSize * iBand * poDS->nRasterXSize,
                                       iWordSize,
                                       iWordSize * nBands * poDS->nRasterXSize,
                                       eType, bNative ));
                if( EQUAL(osCellType,"Signed8BitInteger") )
                    poDS->GetRasterBand(iBand+1)->
                        SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE",
                                         "IMAGE_STRUCTURE" );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we have an error!                                     */
/* -------------------------------------------------------------------- */
    if( poDS->nBands == 0 )
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Look for band descriptions.                                     */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poRI = poHeader->FindNode( "RasterInfo" );

    for( int iChild = 0, iBand = 0;
         poRI != nullptr && iChild < poRI->nItemCount && iBand < poDS->nBands;
         iChild++ )
    {
        if( poRI->papoItemChild[iChild] != nullptr
            && EQUAL(poRI->papszItemName[iChild],"BandId") )
        {
            const char *pszValue =
                poRI->papoItemChild[iChild]->Find( "Value", nullptr );

            iBand++;
            if( pszValue )
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                poDS->GetRasterBand( iBand )->SetDescription( pszValue );
                CPLPopErrorHandler();
            }

            pszValue = poRI->papoItemChild[iChild]->Find( "Units", nullptr );
            if ( pszValue )
            {
                CPLPushErrorHandler( CPLQuietErrorHandler );
                poDS->GetRasterBand( iBand )->SetUnitType( pszValue );
                CPLPopErrorHandler();
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Look for projection.                                            */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    poDS->osProj = poHeader->Find( "CoordinateSpace.Projection", "" );
    poDS->osDatum = poHeader->Find( "CoordinateSpace.Datum", "" );
    poDS->osUnits = poHeader->Find( "CoordinateSpace.Units", "" );

    oSRS.importFromERM( !poDS->osProj.empty() ? poDS->osProj.c_str() : "RAW",
                        !poDS->osDatum.empty() ? poDS->osDatum.c_str() : "WGS84",
                        !poDS->osUnits.empty() ? poDS->osUnits.c_str() : "METERS" );

    CPLFree( poDS->pszProjection );
    oSRS.exportToWkt( &(poDS->pszProjection) );

/* -------------------------------------------------------------------- */
/*      Look for the geotransform.                                      */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", nullptr ) )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = CPLAtof(
            poHeader->Find( "RasterInfo.RegistrationCoord.Eastings", "" ));
        poDS->adfGeoTransform[1] = CPLAtof(
            poHeader->Find( "RasterInfo.CellInfo.Xdimension", "1.0" ));
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = CPLAtof(
            poHeader->Find( "RasterInfo.RegistrationCoord.Northings", "" ));
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = -CPLAtof(
            poHeader->Find( "RasterInfo.CellInfo.Ydimension", "1.0" ));
    }
    else if( poHeader->Find( "RasterInfo.RegistrationCoord.Latitude", nullptr )
             && poHeader->Find( "RasterInfo.CellInfo.Xdimension", nullptr ) )
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
/*      Adjust if we have a registration cell.                          */
/* -------------------------------------------------------------------- */

    /* http://geospatial.intergraph.com/Libraries/Tech_Docs/ERDAS_ER_Mapper_Customization_Guide.sflb.ashx */
    /* Page 27 : */
    /* RegistrationCellX and RegistrationCellY : The image X and Y
       coordinates of the cell which corresponds to the Registration
       Coordinate. Note that the RegistrationCellX and
       RegistrationCellY can be fractional values. If
       RegistrationCellX and RegistrationCellY are not specified,
       they are assumed to be (0,0), which is the top left corner of the
       image.
       */
    double dfCellX = CPLAtof(poHeader->Find("RasterInfo.RegistrationCellX", "0"));
    double dfCellY = CPLAtof(poHeader->Find("RasterInfo.RegistrationCellY", "0"));

    if( poDS->bGotTransform )
    {
        poDS->adfGeoTransform[0] -=
            dfCellX * poDS->adfGeoTransform[1]
            + dfCellY * poDS->adfGeoTransform[2];
        poDS->adfGeoTransform[3] -=
            dfCellX * poDS->adfGeoTransform[4]
            + dfCellY * poDS->adfGeoTransform[5];
    }

/* -------------------------------------------------------------------- */
/*      Check for null values.                                          */
/* -------------------------------------------------------------------- */
    if( poHeader->Find( "RasterInfo.NullCellValue", nullptr ) )
    {
        poDS->bHasNoDataValue = TRUE;
        poDS->dfNoDataValue = CPLAtofM(poHeader->Find( "RasterInfo.NullCellValue" ));

        if (poDS->poDepFile != nullptr)
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );

            for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
                poDS->GetRasterBand(iBand)->SetNoDataValue(poDS->dfNoDataValue);

            CPLPopErrorHandler();
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have an "All" region?                                     */
/* -------------------------------------------------------------------- */
    ERSHdrNode *poAll = nullptr;

    for( int iChild = 0;
         poRI != nullptr && iChild < poRI->nItemCount;
         iChild++ )
    {
        if( poRI->papoItemChild[iChild] != nullptr
            && EQUAL(poRI->papszItemName[iChild],"RegionInfo") )
        {
            if( EQUAL(poRI->papoItemChild[iChild]->Find("RegionName",""),
                      "All") )
                poAll = poRI->papoItemChild[iChild];
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have statistics?                                          */
/* -------------------------------------------------------------------- */
    if( poAll && poAll->FindNode( "Stats" ) )
    {
        CPLPushErrorHandler( CPLQuietErrorHandler );

        for( int iBand = 1; iBand <= poDS->nBands; iBand++ )
        {
            const char *pszValue =
                poAll->FindElem( "Stats.MinimumValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MINIMUM", pszValue );

            pszValue = poAll->FindElem( "Stats.MaximumValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MAXIMUM", pszValue );

            pszValue = poAll->FindElem( "Stats.MeanValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MEAN", pszValue );

            pszValue = poAll->FindElem( "Stats.MedianValue", iBand-1 );

            if( pszValue )
                poDS->GetRasterBand(iBand)->SetMetadataItem(
                    "STATISTICS_MEDIAN", pszValue );
        }

        CPLPopErrorHandler();
    }

/* -------------------------------------------------------------------- */
/*      Do we have GCPs.                                                */
/* -------------------------------------------------------------------- */
    if( poHeader->FindNode( "RasterInfo.WarpControl" ) )
        poDS->ReadGCPs();

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    // if no SR in xml, try aux
    const char* pszPrj = poDS->GDALPamDataset::_GetProjectionRef();
    if( !pszPrj || strlen(pszPrj) == 0 )
    {
        // try aux
        GDALDataset* poAuxDS = GDALFindAssociatedAuxFile( poOpenInfo->pszFilename, GA_ReadOnly, poDS );
        if( poAuxDS )
        {
            pszPrj = poAuxDS->GetProjectionRef();
            if( pszPrj && strlen(pszPrj) > 0 )
            {
                CPLFree( poDS->pszProjection );
                poDS->pszProjection = CPLStrdup(pszPrj);
            }

            GDALClose( poAuxDS );
        }
    }
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *ERSDataset::Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify settings.                                                */
/* -------------------------------------------------------------------- */
    if (nBands <= 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "ERS driver does not support %d bands.\n", nBands);
        return nullptr;
    }

    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_UInt16
        && eType != GDT_Int32 && eType != GDT_UInt32
        && eType != GDT_Float32 && eType != GDT_Float64 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "The ERS driver does not supporting creating files of types %s.",
                  GDALGetDataTypeName( eType ) );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Work out the name we want to use for the .ers and binary        */
/*      data files.                                                     */
/* -------------------------------------------------------------------- */
    CPLString osBinFile, osErsFile;

    if( EQUAL(CPLGetExtension( pszFilename ), "ers") )
    {
        osErsFile = pszFilename;
        osBinFile = osErsFile.substr(0,osErsFile.length()-4);
    }
    else
    {
        osBinFile = pszFilename;
        osErsFile = osBinFile + ".ers";
    }

/* -------------------------------------------------------------------- */
/*      Work out some values we will write.                             */
/* -------------------------------------------------------------------- */
    const char *pszCellType = "Unsigned8BitInteger";

    if( eType == GDT_Byte )
        pszCellType = "Unsigned8BitInteger";
    else if( eType == GDT_Int16 )
        pszCellType = "Signed16BitInteger";
    else if( eType == GDT_UInt16 )
        pszCellType = "Unsigned16BitInteger";
    else if( eType == GDT_Int32 )
        pszCellType = "Signed32BitInteger";
    else if( eType == GDT_UInt32 )
        pszCellType = "Unsigned32BitInteger";
    else if( eType == GDT_Float32 )
        pszCellType = "IEEE4ByteReal";
    else if( eType == GDT_Float64 )
        pszCellType = "IEEE8ByteReal";
    else
    {
        CPLAssert( false );
    }

/* -------------------------------------------------------------------- */
/*      Handling for signed eight bit data.                             */
/* -------------------------------------------------------------------- */
    const char *pszPixelType = CSLFetchNameValue( papszOptions, "PIXELTYPE" );
    if( pszPixelType
        && EQUAL(pszPixelType,"SIGNEDBYTE")
        && eType == GDT_Byte )
        pszCellType = "Signed8BitInteger";

/* -------------------------------------------------------------------- */
/*      Write binary file.                                              */
/* -------------------------------------------------------------------- */
    VSILFILE *fpBin = VSIFOpenL( osBinFile, "w" );

    if( fpBin == nullptr )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to create %s:\n%s",
                  osBinFile.c_str(), VSIStrerror( errno ) );
        return nullptr;
    }

    GUIntBig nSize = nXSize * (GUIntBig) nYSize
        * nBands * (GDALGetDataTypeSize(eType) / 8);
    GByte byZero = 0;
    if( VSIFSeekL( fpBin, nSize-1, SEEK_SET ) != 0
        || VSIFWriteL( &byZero, 1, 1, fpBin ) != 1 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %s:\n%s",
                  osBinFile.c_str(), VSIStrerror( errno ) );
        VSIFCloseL( fpBin );
        return nullptr;
    }
    VSIFCloseL( fpBin );

/* -------------------------------------------------------------------- */
/*      Try writing header file.                                        */
/* -------------------------------------------------------------------- */
    VSILFILE *fpERS = VSIFOpenL( osErsFile, "w" );

    if( fpERS == nullptr )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to create %s:\n%s",
                  osErsFile.c_str(), VSIStrerror( errno ) );
        return nullptr;
    }

    VSIFPrintfL( fpERS, "DatasetHeader Begin\n" );
    VSIFPrintfL( fpERS, "\tVersion\t\t = \"6.0\"\n" );
    VSIFPrintfL( fpERS, "\tName\t\t= \"%s\"\n", CPLGetFilename(osErsFile) );

// Last updated requires timezone info which we don't necessarily get
// get from VSICTime() so perhaps it is better to omit this.
//    VSIFPrintfL( fpERS, "\tLastUpdated\t= %s",
//                 VSICTime( VSITime( NULL ) ) );

    VSIFPrintfL( fpERS, "\tDataSetType\t= ERStorage\n" );
    VSIFPrintfL( fpERS, "\tDataType\t= Raster\n" );
    VSIFPrintfL( fpERS, "\tByteOrder\t= LSBFirst\n" );
    VSIFPrintfL( fpERS, "\tRasterInfo Begin\n" );
    VSIFPrintfL( fpERS, "\t\tCellType\t= %s\n", pszCellType );
    VSIFPrintfL( fpERS, "\t\tNrOfLines\t= %d\n", nYSize );
    VSIFPrintfL( fpERS, "\t\tNrOfCellsPerLine\t= %d\n", nXSize );
    VSIFPrintfL( fpERS, "\t\tNrOfBands\t= %d\n", nBands );
    VSIFPrintfL( fpERS, "\tRasterInfo End\n" );
    if( VSIFPrintfL( fpERS, "DatasetHeader End\n" ) < 17 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %s:\n%s",
                  osErsFile.c_str(), VSIStrerror( errno ) );
        return nullptr;
    }

    VSIFCloseL( fpERS );

/* -------------------------------------------------------------------- */
/*      Reopen.                                                         */
/* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo( osErsFile, GA_Update );
    ERSDataset* poDS = (ERSDataset*) Open( &oOpenInfo );
    if (poDS == nullptr)
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Fetch DATUM, PROJ and UNITS creation option                     */
/* -------------------------------------------------------------------- */
    const char *pszDatum = CSLFetchNameValue( papszOptions, "DATUM" );
    if( pszDatum )
    {
        poDS->osDatumForced = pszDatum;
        poDS->osDatum = pszDatum;
    }
    const char *pszProj = CSLFetchNameValue( papszOptions, "PROJ" );
    if( pszProj )
    {
        poDS->osProjForced = pszProj;
        poDS->osProj = pszProj;
    }
    const char *pszUnits = CSLFetchNameValue( papszOptions, "UNITS" );
    if( pszUnits )
    {
        poDS->osUnitsForced = pszUnits;
        poDS->osUnits = pszUnits;
    }

    if (pszDatum || pszProj || pszUnits)
    {
        poDS->WriteProjectionInfo(pszProj ? pszProj : "RAW",
                                  pszDatum ? pszDatum : "RAW",
                                  pszUnits ? pszUnits : "METERS");
    }

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_ERS()                           */
/************************************************************************/

void GDALRegister_ERS()

{
    if( GDALGetDriverByName( "ERS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ERS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ERMapper .ers Labelled" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/ers.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "ers" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 "
                               "Float32 Float64" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='PIXELTYPE' type='string' description='By setting this to SIGNEDBYTE, a new Byte file can be forced to be written as signed byte'/>"
"   <Option name='PROJ' type='string' description='ERS Projection Name'/>"
"   <Option name='DATUM' type='string' description='ERS Datum Name' />"
"   <Option name='UNITS' type='string-select' description='ERS Projection Units'>"
"       <Value>METERS</Value>"
"       <Value>FEET</Value>"
"   </Option>"
"</CreationOptionList>" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = ERSDataset::Open;
    poDriver->pfnIdentify = ERSDataset::Identify;
    poDriver->pfnCreate = ERSDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
