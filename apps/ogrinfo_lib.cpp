/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "ogr_feature.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

#include <set>

struct GDALVectorInfoOptions
{
    std::string osFilename{};
    std::string osWHERE{};
    CPLStringList aosLayers{};
    std::unique_ptr<OGRGeometry> poSpatialFilter;
    bool bAllLayers = false;
    std::string osSQLStatement{};
    std::string osDialect{};
    std::string osGeomField{};
    CPLStringList aosExtraMDDomains{};
    bool bListMDD = false;
    bool bShowMetadata = true;
    bool bFeatureCount = true;
    bool bExtent = true;
    bool bGeomType = true;
    bool bDatasetGetNextFeature = false;
    bool bVerbose = true;
    bool bSuperQuiet = false;
    bool bSummaryOnly = false;
    GIntBig nFetchFID = OGRNullFID;
    std::string osWKTFormat = "WKT2";
    std::string osFieldDomain{};
    CPLStringList aosOptions{};
    bool bStdoutOutput = false; // only set by ogrinfo_bin
    int nRepeatCount = 1;
};

/************************************************************************/
/*                     GDALVectorInfoOptionsFree()                      */
/************************************************************************/

/**
 * Frees the GDALVectorInfoOptions struct.
 *
 * @param psOptions the options struct for GDALVectorInfo().
 *
 * @since GDAL 3.7
 */

void GDALVectorInfoOptionsFree( GDALVectorInfoOptions *psOptions )
{
    delete psOptions;
}

/************************************************************************/
/*                            Concat()                                  */
/************************************************************************/

static void Concat( CPLString& osRet, bool bStdoutOutput,
                    const char* pszFormat, ... ) CPL_PRINT_FUNC_FORMAT (3, 4);

static void Concat( CPLString& osRet, bool bStdoutOutput,
                    const char* pszFormat, ... )
{
    va_list args;
    va_start( args, pszFormat );

    if( bStdoutOutput )
    {
        vfprintf(stdout, pszFormat, args );
    }
    else
    {
        try
        {
            CPLString osTarget;
            osTarget.vPrintf( pszFormat, args );

            osRet += osTarget;
        }
        catch( const std::bad_alloc& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        }
    }

    va_end( args );
}

/************************************************************************/
/*                        ReportFieldDomain()                           */
/************************************************************************/

static void ReportFieldDomain(CPLString& osRet,
                              const GDALVectorInfoOptions *psOptions,
                              const OGRFieldDomain* poDomain)
{
    Concat(osRet, psOptions->bStdoutOutput, "Domain %s:\n", poDomain->GetName().c_str());
    const std::string& osDesc = poDomain->GetDescription();
    if( !osDesc.empty() )
    {
        Concat(osRet, psOptions->bStdoutOutput, "  Description: %s\n", osDesc.c_str());
    }
    switch( poDomain->GetDomainType() )
    {
        case OFDT_CODED: Concat(osRet, psOptions->bStdoutOutput, "  Type: coded\n"); break;
        case OFDT_RANGE: Concat(osRet, psOptions->bStdoutOutput, "  Type: range\n"); break;
        case OFDT_GLOB:  Concat(osRet, psOptions->bStdoutOutput, "  Type: glob\n"); break;
    }
    const char* pszFieldType = (poDomain->GetFieldSubType() != OFSTNone)
        ? CPLSPrintf(
              "%s(%s)",
              OGRFieldDefn::GetFieldTypeName(poDomain->GetFieldType()),
              OGRFieldDefn::GetFieldSubTypeName(poDomain->GetFieldSubType()))
        : OGRFieldDefn::GetFieldTypeName(poDomain->GetFieldType());
    Concat(osRet, psOptions->bStdoutOutput, "  Field type: %s\n", pszFieldType);
    switch( poDomain->GetSplitPolicy() )
    {
        case OFDSP_DEFAULT_VALUE:  Concat(osRet, psOptions->bStdoutOutput, "  Split policy: default value\n"); break;
        case OFDSP_DUPLICATE:      Concat(osRet, psOptions->bStdoutOutput, "  Split policy: duplicate\n"); break;
        case OFDSP_GEOMETRY_RATIO: Concat(osRet, psOptions->bStdoutOutput, "  Split policy: geometry ratio\n"); break;
    }
    switch( poDomain->GetMergePolicy() )
    {
        case OFDMP_DEFAULT_VALUE:     Concat(osRet, psOptions->bStdoutOutput, "  Merge policy: default value\n"); break;
        case OFDMP_SUM:               Concat(osRet, psOptions->bStdoutOutput, "  Merge policy: sum\n"); break;
        case OFDMP_GEOMETRY_WEIGHTED: Concat(osRet, psOptions->bStdoutOutput, "  Merge policy: geometry weighted\n"); break;
    }
    switch( poDomain->GetDomainType() )
    {
        case OFDT_CODED:
        {
            const auto poCodedFieldDomain =
                cpl::down_cast<const OGRCodedFieldDomain*>(poDomain);
            const OGRCodedValue* enumeration = poCodedFieldDomain->GetEnumeration();
            Concat(osRet, psOptions->bStdoutOutput, "  Coded values:\n");
            for( int i = 0; enumeration[i].pszCode != nullptr; ++i )
            {
                if( enumeration[i].pszValue )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "    %s: %s\n",
                           enumeration[i].pszCode,
                           enumeration[i].pszValue);
                }
                else
                {
                    Concat(osRet, psOptions->bStdoutOutput, "    %s\n", enumeration[i].pszCode);
                }
            }
            break;
        }

        case OFDT_RANGE:
        {
            const auto poRangeFieldDomain =
                cpl::down_cast<const OGRRangeFieldDomain*>(poDomain);
            bool bMinIsIncluded = false;
            const OGRField& sMin = poRangeFieldDomain->GetMin(bMinIsIncluded);
            bool bMaxIsIncluded = false;
            const OGRField& sMax = poRangeFieldDomain->GetMax(bMaxIsIncluded);
            if( poDomain->GetFieldType() == OFTInteger )
            {
                if( !OGR_RawField_IsUnset(&sMin) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Minimum value: %d%s\n",
                           sMin.Integer,
                           bMinIsIncluded ? "" : " (excluded)");
                }
                if( !OGR_RawField_IsUnset(&sMax) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Maximum value: %d%s\n",
                           sMax.Integer,
                           bMaxIsIncluded ? "" : " (excluded)");
                }
            }
            else if( poDomain->GetFieldType() == OFTInteger64 )
            {
                if( !OGR_RawField_IsUnset(&sMin) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Minimum value: " CPL_FRMT_GIB "%s\n",
                           sMin.Integer64,
                           bMinIsIncluded ? "" : " (excluded)");
                }
                if( !OGR_RawField_IsUnset(&sMax) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Maximum value: " CPL_FRMT_GIB "%s\n",
                           sMax.Integer64,
                           bMaxIsIncluded ? "" : " (excluded)");
                }
            }
            else if( poDomain->GetFieldType() == OFTReal )
            {
                if( !OGR_RawField_IsUnset(&sMin) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Minimum value: %g%s\n",
                           sMin.Real,
                           bMinIsIncluded ? "" : " (excluded)");
                }
                if( !OGR_RawField_IsUnset(&sMax) )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "  Maximum value: %g%s\n",
                           sMax.Real,
                           bMaxIsIncluded ? "" : " (excluded)");
                }
            }
            break;
        }

        case OFDT_GLOB:
        {
            const auto poGlobFieldDomain =
                cpl::down_cast<const OGRGlobFieldDomain*>(poDomain);
            Concat(osRet, psOptions->bStdoutOutput, "  Glob: %s\n", poGlobFieldDomain->GetGlob().c_str());
            break;
        }
    }
}

/************************************************************************/
/*                     GDALVectorInfoPrintMetadata()                    */
/************************************************************************/

static void GDALVectorInfoPrintMetadata( CPLString& osRet,
                                         const GDALVectorInfoOptions *psOptions,
                                         GDALMajorObjectH hObject,
                                         const char *pszDomain,
                                         const char *pszDisplayedname,
                                         const char *pszIndent )
{
    bool bIsxml = false;

    if( pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "xml:") )
        bIsxml = true;

    char **papszMetadata = GDALGetMetadata(hObject, pszDomain);
    if( CSLCount(papszMetadata) > 0 )
    {
        Concat(osRet, psOptions->bStdoutOutput, "%s%s:\n", pszIndent, pszDisplayedname);
        for( int i = 0; papszMetadata[i] != nullptr; i++ )
        {
            if( bIsxml )
                Concat(osRet, psOptions->bStdoutOutput, "%s%s\n", pszIndent, papszMetadata[i]);
            else
                Concat(osRet, psOptions->bStdoutOutput, "%s  %s\n", pszIndent, papszMetadata[i]);
        }
    }
}

/************************************************************************/
/*                    GDALVectorInfoReportMetadata()                    */
/************************************************************************/

static void GDALVectorInfoReportMetadata( CPLString& osRet,
                                          const GDALVectorInfoOptions *psOptions,
                                          GDALMajorObject* poMajorObject,
                                          bool bListMDD,
                                          bool bShowMetadata,
                                          CSLConstList papszExtraMDDomains )
{
    const char* pszIndent = "";
    auto hObject = GDALMajorObject::ToHandle(poMajorObject);

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if( bListMDD )
    {
        char** papszMDDList = GDALGetMetadataDomainList(hObject);
        char** papszIter = papszMDDList;

        if( papszMDDList != nullptr )
            Concat(osRet, psOptions->bStdoutOutput, "%sMetadata domains:\n", pszIndent);
        while( papszIter != nullptr && *papszIter != nullptr )
        {
            if( EQUAL(*papszIter, "") )
                Concat(osRet, psOptions->bStdoutOutput, "%s  (default)\n", pszIndent);
            else
                Concat(osRet, psOptions->bStdoutOutput, "%s  %s\n", pszIndent, *papszIter);
            papszIter ++;
        }
        CSLDestroy(papszMDDList);
    }

    if( !bShowMetadata )
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALVectorInfoPrintMetadata(osRet, psOptions,
                                hObject, nullptr, "Metadata", pszIndent);

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if( papszExtraMDDomains != nullptr )
    {
        char **papszExtraMDDomainsExpanded = nullptr;

        if( EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == nullptr )
        {
            char** papszMDDList = GDALGetMetadataDomainList(hObject);
            char** papszIter = papszMDDList;

            while( papszIter != nullptr && *papszIter != nullptr )
            {
                if( !EQUAL(*papszIter, "") && !EQUAL(*papszIter, "SUBDATASETS") )
                {
                    papszExtraMDDomainsExpanded = CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter ++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for( int iMDD = 0;
             papszExtraMDDomainsExpanded != nullptr &&
               papszExtraMDDomainsExpanded[iMDD] != nullptr;
             iMDD++ )
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)",
                     papszExtraMDDomainsExpanded[iMDD]);
            GDALVectorInfoPrintMetadata(osRet, psOptions,
                                        hObject, papszExtraMDDomainsExpanded[iMDD],
                                        pszDisplayedname, pszIndent);
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }
    GDALVectorInfoPrintMetadata(osRet, psOptions,
                                hObject, "SUBDATASETS", "Subdatasets", pszIndent);
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer( CPLString& osRet,
                           const GDALVectorInfoOptions *psOptions,
                           OGRLayer * poLayer,
                           bool bForceSummary,
                           bool bTakeIntoAccountWHERE,
                           bool bTakeIntoAccountSpatialFilter,
                           bool bTakeIntoAccountGeomField )
{
    OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
    if( bTakeIntoAccountWHERE && !psOptions->osWHERE.empty() )
    {
        if( poLayer->SetAttributeFilter(psOptions->osWHERE.c_str()) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "SetAttributeFilter(%s) failed.", psOptions->osWHERE.c_str());
            return;
        }
    }

    if( bTakeIntoAccountSpatialFilter && psOptions->poSpatialFilter != nullptr )
    {
        if( bTakeIntoAccountGeomField && !psOptions->osGeomField.empty() )
        {
            const int iGeomField = poDefn->GetGeomFieldIndex(psOptions->osGeomField.c_str());
            if( iGeomField >= 0 )
                poLayer->SetSpatialFilter(iGeomField, psOptions->poSpatialFilter.get());
            else
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot find geometry field %s.",
                         psOptions->osGeomField.c_str());
        }
        else
        {
            poLayer->SetSpatialFilter(psOptions->poSpatialFilter.get());
        }
    }

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
    if( !psOptions->bSuperQuiet )
    {
        Concat(osRet, psOptions->bStdoutOutput, "\n");
        Concat(osRet, psOptions->bStdoutOutput, "Layer name: %s\n", poLayer->GetName());
    }

    GDALVectorInfoReportMetadata(osRet,
                                 psOptions,
                                 poLayer,
                                 psOptions->bListMDD,
                                 psOptions->bShowMetadata,
                                 psOptions->aosExtraMDDomains.List());

    if( psOptions->bVerbose )
    {
        const int nGeomFieldCount =
            psOptions->bGeomType ? poLayer->GetLayerDefn()->GetGeomFieldCount() : 0;
        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                Concat(osRet, psOptions->bStdoutOutput, "Geometry (%s): %s\n", poGFldDefn->GetNameRef(),
                       OGRGeometryTypeToName(poGFldDefn->GetType()));
            }
        }
        else if( psOptions->bGeomType )
        {
            Concat(osRet, psOptions->bStdoutOutput, "Geometry: %s\n",
                   OGRGeometryTypeToName(poLayer->GetGeomType()));
        }

        if( psOptions->bFeatureCount )
            Concat(osRet, psOptions->bStdoutOutput, "Feature Count: " CPL_FRMT_GIB "\n",
                   poLayer->GetFeatureCount());

        OGREnvelope oExt;
        if( psOptions->bExtent && nGeomFieldCount > 1 )
        {
            for( int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                if( poLayer->GetExtent(iGeom, &oExt, TRUE) == OGRERR_NONE )
                {
                    OGRGeomFieldDefn* poGFldDefn =
                        poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                    Concat(osRet, psOptions->bStdoutOutput,
                           "Extent (%s): (%f, %f) - (%f, %f)\n",
                           poGFldDefn->GetNameRef(),
                           oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
                }
            }
        }
        else if( psOptions->bExtent && poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE )
        {
            Concat(osRet, psOptions->bStdoutOutput,
                   "Extent: (%f, %f) - (%f, %f)\n",
                   oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
        }

        const auto displayExtraInfoSRS = [&osRet, &psOptions](const OGRSpatialReference* poSRS) {

            const double dfCoordinateEpoch = poSRS->GetCoordinateEpoch();
            if( dfCoordinateEpoch > 0 )
            {
                std::string osCoordinateEpoch = CPLSPrintf("%f", dfCoordinateEpoch);
                if( osCoordinateEpoch.find('.') != std::string::npos )
                {
                    while( osCoordinateEpoch.back() == '0' )
                        osCoordinateEpoch.resize(osCoordinateEpoch.size()-1);
                }
                Concat(osRet, psOptions->bStdoutOutput, "Coordinate epoch: %s\n", osCoordinateEpoch.c_str());
            }

            const auto mapping = poSRS->GetDataAxisToSRSAxisMapping();
            Concat(osRet, psOptions->bStdoutOutput, "Data axis to CRS axis mapping: ");
            for( size_t i = 0; i < mapping.size(); i++ )
            {
                if( i > 0 )
                {
                    Concat(osRet, psOptions->bStdoutOutput, ",");
                }
                Concat(osRet, psOptions->bStdoutOutput, "%d", mapping[i]);
            }
            Concat(osRet, psOptions->bStdoutOutput, "\n");
        };

        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                OGRSpatialReference* poSRS = poGFldDefn->GetSpatialRef();
                char *pszWKT = nullptr;
                if( poSRS == nullptr )
                {
                    pszWKT = CPLStrdup("(unknown)");
                }
                else
                {
                    CPLString osWKTFormat("FORMAT=");
                    osWKTFormat += psOptions->osWKTFormat;
                    const char* const apszWKTOptions[] =
                        { osWKTFormat.c_str(), "MULTILINE=YES", nullptr };
                    poSRS->exportToWkt(&pszWKT, apszWKTOptions);
                }

                Concat(osRet, psOptions->bStdoutOutput, "SRS WKT (%s):\n%s\n",
                       poGFldDefn->GetNameRef(), pszWKT);
                CPLFree(pszWKT);
                if( poSRS )
                {
                    displayExtraInfoSRS(poSRS);
                }
            }
        }
        else
        {
            char *pszWKT = nullptr;
            auto poSRS = poLayer->GetSpatialRef();
            if( poSRS == nullptr )
            {
                pszWKT = CPLStrdup("(unknown)");
            }
            else
            {
                CPLString osWKTFormat("FORMAT=");
                osWKTFormat += psOptions->osWKTFormat;
                const char* const apszWKTOptions[] =
                    { osWKTFormat.c_str(), "MULTILINE=YES", nullptr };
                poSRS->exportToWkt(&pszWKT, apszWKTOptions);
            }

            Concat(osRet, psOptions->bStdoutOutput, "Layer SRS WKT:\n%s\n", pszWKT);
            CPLFree(pszWKT);
            if( poSRS )
            {
                displayExtraInfoSRS(poSRS);
            }
        }

        if( strlen(poLayer->GetFIDColumn()) > 0 )
            Concat(osRet, psOptions->bStdoutOutput, "FID Column = %s\n",
                   poLayer->GetFIDColumn());

        for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
        {
            OGRGeomFieldDefn* poGFldDefn =
                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
            if( nGeomFieldCount == 1 &&
                EQUAL(poGFldDefn->GetNameRef(), "") &&
                poGFldDefn->IsNullable() )
                break;
            Concat(osRet, psOptions->bStdoutOutput, "Geometry Column ");
            if( nGeomFieldCount > 1 )
                Concat(osRet, psOptions->bStdoutOutput, "%d ", iGeom + 1);
            if( !poGFldDefn->IsNullable() )
                Concat(osRet, psOptions->bStdoutOutput, "NOT NULL ");
            Concat(osRet, psOptions->bStdoutOutput, "= %s\n", poGFldDefn->GetNameRef());
        }

        for( int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
        {
            OGRFieldDefn *poField = poDefn->GetFieldDefn(iAttr);
            const char* pszType = (poField->GetSubType() != OFSTNone)
                ? CPLSPrintf(
                      "%s(%s)",
                      OGRFieldDefn::GetFieldTypeName(poField->GetType()),
                      OGRFieldDefn::GetFieldSubTypeName(poField->GetSubType()))
                : OGRFieldDefn::GetFieldTypeName(poField->GetType());
            Concat(osRet, psOptions->bStdoutOutput, "%s: %s (%d.%d)",
                   poField->GetNameRef(),
                   pszType,
                   poField->GetWidth(),
                   poField->GetPrecision());
            if( poField->IsUnique() )
                Concat(osRet, psOptions->bStdoutOutput, " UNIQUE");
            if( !poField->IsNullable() )
                Concat(osRet, psOptions->bStdoutOutput, " NOT NULL");
            if( poField->GetDefault() != nullptr )
                Concat(osRet, psOptions->bStdoutOutput, " DEFAULT %s", poField->GetDefault());
            const char* pszAlias = poField->GetAlternativeNameRef();
            if( pszAlias != nullptr && pszAlias[0])
                Concat(osRet, psOptions->bStdoutOutput, ", alternative name=\"%s\"", pszAlias);
            const std::string& osDomain = poField->GetDomainName();
            if( !osDomain.empty() )
                Concat(osRet, psOptions->bStdoutOutput, ", domain name=%s", osDomain.c_str());
            Concat(osRet, psOptions->bStdoutOutput, "\n");
        }
    }

/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */

    if( psOptions->nFetchFID == OGRNullFID && !bForceSummary && !psOptions->bSummaryOnly )
    {
        if( !psOptions->bSuperQuiet )
        {
            for( auto& poFeature: poLayer )
            {
                Concat(osRet, psOptions->bStdoutOutput,
                       "%s",
                       poFeature->DumpReadableAsString(psOptions->aosOptions.List()).c_str());
            }
        }
    }
    else if( psOptions->nFetchFID != OGRNullFID )
    {
        OGRFeature *poFeature = poLayer->GetFeature(psOptions->nFetchFID);
        if( poFeature == nullptr )
        {
            Concat(osRet, psOptions->bStdoutOutput, "Unable to locate feature id " CPL_FRMT_GIB
                   " on this layer.\n",
                   psOptions->nFetchFID);
        }
        else
        {
            Concat(osRet, psOptions->bStdoutOutput,
                   "%s",
                   poFeature->DumpReadableAsString(psOptions->aosOptions.List()).c_str());
            OGRFeature::DestroyFeature(poFeature);
        }
    }
}

/************************************************************************/
/*                           PrintLayerSummary()                        */
/************************************************************************/

static void PrintLayerSummary(CPLString& osRet,
                              const GDALVectorInfoOptions *psOptions,
                              OGRLayer* poLayer,
                              bool bIsPrivate)
{
    Concat(osRet, psOptions->bStdoutOutput, "%s", poLayer->GetName());

    const char* pszTitle = poLayer->GetMetadataItem("TITLE");
    if( pszTitle )
    {
        Concat(osRet, psOptions->bStdoutOutput, " (title: %s)", pszTitle);
    }

    const int nGeomFieldCount =
        psOptions->bGeomType ? poLayer->GetLayerDefn()->GetGeomFieldCount() : 0;
    if( nGeomFieldCount > 1 )
    {
        Concat(osRet, psOptions->bStdoutOutput, " (");
        for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom++ )
        {
            if( iGeom > 0 )
                Concat(osRet, psOptions->bStdoutOutput, ", ");
            OGRGeomFieldDefn* poGFldDefn =
                poLayer->GetLayerDefn()->
                    GetGeomFieldDefn(iGeom);
            Concat(osRet, psOptions->bStdoutOutput,
                "%s",
                OGRGeometryTypeToName(
                    poGFldDefn->GetType()));
        }
        Concat(osRet, psOptions->bStdoutOutput, ")");
    }
    else if( psOptions->bGeomType && poLayer->GetGeomType() != wkbUnknown )
        Concat(osRet, psOptions->bStdoutOutput, " (%s)",
               OGRGeometryTypeToName(
                   poLayer->GetGeomType()));

    if( bIsPrivate )
    {
        Concat(osRet, psOptions->bStdoutOutput, " [private]");
    }

    Concat(osRet, psOptions->bStdoutOutput, "\n");
}

/************************************************************************/
/*                       ReportHiearchicalLayers()                      */
/************************************************************************/

static void ReportHiearchicalLayers(CPLString& osRet,
                                    const GDALVectorInfoOptions *psOptions,
                                    const GDALGroup* group,
                                    const std::string& indent,
                                    bool bGeomType)
{
    const auto aosVectorLayerNames = group->GetVectorLayerNames();
    for( const auto& osVectorLayerName: aosVectorLayerNames )
    {
        OGRLayer* poLayer = group->OpenVectorLayer(osVectorLayerName);
        if( poLayer )
        {
            Concat(osRet, psOptions->bStdoutOutput, "%sLayer: ", indent.c_str());
            PrintLayerSummary(osRet, psOptions, poLayer,/* bIsPrivate=*/ false);
        }
    }

    const std::string subIndent(indent + "  ");
    auto aosSubGroupNames = group->GetGroupNames();
    for( const auto& osSubGroupName: aosSubGroupNames )
    {
        auto poSubGroup = group->OpenGroup(osSubGroupName);
        if( poSubGroup )
        {
            Concat(osRet, psOptions->bStdoutOutput, "Group %s", indent.c_str());
            Concat(osRet, psOptions->bStdoutOutput, "%s:\n", osSubGroupName.c_str());
            ReportHiearchicalLayers(osRet, psOptions,
                                    poSubGroup.get(), subIndent, bGeomType);
        }
    }
}

/************************************************************************/
/*                           GDALVectorInfo()                           */
/************************************************************************/

/**
 * Lists various information about a GDAL supported vector dataset.
 *
 * This is the equivalent of the <a href="/programs/ogrinfo.html">ogrinfo</a> utility.
 *
 * GDALVectorInfoOptions* must be allocated and freed with GDALVectorInfoOptionsNew()
 * and GDALVectorInfoOptionsFree() respectively.
 *
 * @param hDataset the dataset handle.
 * @param psOptions the options structure returned by GDALVectorInfoOptionsNew() or NULL.
 * @return string corresponding to the information about the raster dataset (must be freed with CPLFree()), or NULL in case of error.
 *
 * @since GDAL 3.7
 */
char *GDALVectorInfo( GDALDatasetH hDataset, const GDALVectorInfoOptions *psOptions )
{
    auto poDS = GDALDataset::FromHandle(hDataset);

    GDALDriver *poDriver = poDS->GetDriver();

    CPLString osRet;
    const std::string osFilename(
            !psOptions->osFilename.empty() ? psOptions->osFilename :
            std::string(poDS->GetDescription()) );

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( psOptions->bVerbose )
    {
        Concat(osRet, psOptions->bStdoutOutput,
               "INFO: Open of `%s'\n"
               "      using driver `%s' successful.\n",
               osFilename.c_str(), poDriver ? poDriver->GetDescription() : "(null)");
    }

    if( psOptions->bVerbose && !EQUAL(osFilename.c_str(),poDS->GetDescription()) )
    {
        Concat(osRet, psOptions->bStdoutOutput,
               "INFO: Internal data source name `%s'\n"
               "      different from user name `%s'.\n",
               poDS->GetDescription(), osFilename.c_str());
    }

    GDALVectorInfoReportMetadata( osRet, psOptions,
                                  poDS,
                                  psOptions->bListMDD,
                                  psOptions->bShowMetadata,
                                  psOptions->aosExtraMDDomains.List() );

    if( !psOptions->osFieldDomain.empty() )
    {
        auto poDomain = poDS->GetFieldDomain(psOptions->osFieldDomain);
        if( poDomain == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Domain %s cannot be found.",
                     psOptions->osFieldDomain.c_str());
            return nullptr;
        }
        Concat(osRet, psOptions->bStdoutOutput,"\n");
        ReportFieldDomain(osRet, psOptions, poDomain);
        Concat(osRet, psOptions->bStdoutOutput,"\n");
    }

    int nRepeatCount = psOptions->nRepeatCount;
    if( psOptions->bDatasetGetNextFeature )
    {
        nRepeatCount = 0;  // skip layer reporting.

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
        if( !psOptions->osWHERE.empty() || psOptions->poSpatialFilter != nullptr )
        {
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d.",
                             iLayer);
                    return nullptr;
                }

                if( !psOptions->osWHERE.empty() )
                {
                    if( poLayer->SetAttributeFilter(psOptions->osWHERE.c_str()) != OGRERR_NONE )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "SetAttributeFilter(%s) failed on layer %s.",
                                 psOptions->osWHERE.c_str(), poLayer->GetName());
                    }
                }

                if( psOptions->poSpatialFilter != nullptr )
                {
                    if( !psOptions->osGeomField.empty() )
                    {
                        OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();
                        const int iGeomField =
                            poDefn->GetGeomFieldIndex(psOptions->osGeomField.c_str());
                        if( iGeomField >= 0 )
                            poLayer->SetSpatialFilter(iGeomField,
                                                      psOptions->poSpatialFilter.get());
                        else
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Cannot find geometry field %s.",
                                     psOptions->osGeomField.c_str());
                    }
                    else
                    {
                        poLayer->SetSpatialFilter(psOptions->poSpatialFilter.get());
                    }
                }
            }
        }

        std::set<OGRLayer*> oSetLayers;
        while( true )
        {
            OGRLayer* poLayer = nullptr;
            OGRFeature* poFeature = poDS->GetNextFeature(&poLayer, nullptr,
                                                         nullptr, nullptr);
            if( poFeature == nullptr )
                break;
            if( psOptions->aosLayers.empty() || poLayer == nullptr ||
                CSLFindString(psOptions->aosLayers.List(), poLayer->GetName()) >= 0 )
            {
                if( psOptions->bVerbose && poLayer != nullptr &&
                    oSetLayers.find(poLayer) == oSetLayers.end() )
                {
                    oSetLayers.insert(poLayer);
                    ReportOnLayer(osRet, psOptions,
                                  poLayer,
                                  /*bForceSummary = */ true,
                                  /*bTakeIntoAccountWHERE = */ false,
                                  /*bTakeIntoAccountSpatialFilter = */ false,
                                  /*bTakeIntoAccountGeomField = */ false);
                }
                if( !psOptions->bSuperQuiet && !psOptions->bSummaryOnly )
                    poFeature->DumpReadable(nullptr,
                        const_cast<char**>(psOptions->aosOptions.List()));
            }
            OGRFeature::DestroyFeature(poFeature);
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    else if( !psOptions->osSQLStatement.empty() )
    {
        nRepeatCount = 0;  // skip layer reporting.

        if( !psOptions->aosLayers.empty() )
            Concat(osRet, psOptions->bStdoutOutput, "layer names ignored in combination with -sql.\n");

        OGRLayer *poResultSet =
            poDS->ExecuteSQL(
                psOptions->osSQLStatement.c_str(),
                psOptions->osGeomField.empty() ? psOptions->poSpatialFilter.get() : nullptr,
                psOptions->osDialect.empty() ? nullptr : psOptions->osDialect.c_str());

        if( poResultSet != nullptr )
        {
            if( !psOptions->osWHERE.empty() )
            {
                if( poResultSet->SetAttributeFilter(psOptions->osWHERE.c_str()) != OGRERR_NONE )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "SetAttributeFilter(%s) failed.\n",
                             psOptions->osWHERE.c_str());
                    return nullptr;
                }
            }

            if( !psOptions->osGeomField.empty() )
                ReportOnLayer(osRet, psOptions,
                              poResultSet,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ false,
                              /*bTakeIntoAccountSpatialFilter = */ true,
                              /*bTakeIntoAccountGeomField = */ true);
            else
                ReportOnLayer(osRet, psOptions,
                              poResultSet,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ false,
                              /*bTakeIntoAccountSpatialFilter = */ false,
                              /*bTakeIntoAccountGeomField = */ false);

            poDS->ReleaseResultSet(poResultSet);
        }
    }

    // coverity[tainted_data]
    auto papszLayers = psOptions->aosLayers.List();
    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        if( papszLayers == nullptr || papszLayers[0] == nullptr )
        {
            const int nLayerCount = poDS->GetLayerCount();
            if( iRepeat == 0 )
                CPLDebug("OGR", "GetLayerCount() = %d\n",
                         nLayerCount);

            bool bDone = false;
            auto poRootGroup = poDS->GetRootGroup();
            if( !psOptions->bAllLayers && poRootGroup &&
                (!poRootGroup->GetGroupNames().empty() ||
                 !poRootGroup->GetVectorLayerNames().empty()) )
            {
                ReportHiearchicalLayers(osRet,
                                        psOptions,
                                        poRootGroup.get(),
                                        std::string(),
                                        psOptions->bGeomType);
                bDone = true;
            }

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
            for( int iLayer = 0; !bDone && iLayer < nLayerCount; iLayer++ )
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch advertised layer %d.",
                             iLayer);
                    return nullptr;
                }

                if( !psOptions->bAllLayers )
                {
                    Concat(osRet, psOptions->bStdoutOutput, "%d: ", iLayer + 1);
                    PrintLayerSummary(osRet, psOptions, poLayer,
                                      poDS->IsLayerPrivate(iLayer));
                }
                else
                {
                    if( iRepeat != 0 )
                        poLayer->ResetReading();

                    ReportOnLayer(osRet, psOptions, poLayer,
                                  /*bForceSummary = */ false,
                                  /*bTakeIntoAccountWHERE = */ true,
                                  /*bTakeIntoAccountSpatialFilter = */ true,
                                  /*bTakeIntoAccountGeomField = */ true);
                }
            }
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */

            for( CSLConstList papszIter = papszLayers;
                 *papszIter != nullptr;
                 ++papszIter )
            {
                OGRLayer *poLayer = poDS->GetLayerByName(*papszIter);

                if( poLayer == nullptr )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Couldn't fetch requested layer %s.",
                             *papszIter);
                    return nullptr;
                }

                if( iRepeat != 0 )
                    poLayer->ResetReading();

                ReportOnLayer(osRet, psOptions, poLayer,
                              /*bForceSummary = */ false,
                              /*bTakeIntoAccountWHERE = */ true,
                              /*bTakeIntoAccountSpatialFilter = */ true,
                              /*bTakeIntoAccountGeomField = */ true);
            }
        }
    }

    return VSI_STRDUP_VERBOSE(osRet);
}

/************************************************************************/
/*                             RemoveBOM()                              */
/************************************************************************/

/* Remove potential UTF-8 BOM from data (must be NUL terminated) */
static void RemoveBOM(GByte* pabyData)
{
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
    {
        memmove(pabyData, pabyData + 3,
                strlen(reinterpret_cast<char *>(pabyData) + 3) + 1);
    }
}

/************************************************************************/
/*                        RemoveSQLComments()                           */
/************************************************************************/

static std::string RemoveSQLComments(const std::string& osInput)
{
    char** papszLines = CSLTokenizeStringComplex(osInput.c_str(), "\r\n", FALSE, FALSE);
    std::string osSQL;
    for( char** papszIter = papszLines; papszIter && *papszIter; ++papszIter )
    {
        const char* pszLine = *papszIter;
        char chQuote = 0;
        int i = 0;
        for(; pszLine[i] != '\0'; ++i )
        {
            if( chQuote )
            {
                if( pszLine[i] == chQuote )
                {
                    if( pszLine[i+1] == chQuote )
                    {
                        i++;
                    }
                    else
                    {
                        chQuote = 0;
                    }
                }
            }
            else if( pszLine[i] == '\'' || pszLine[i] == '"' )
            {
                chQuote = pszLine[i];
            }
            else if( pszLine[i] == '-' && pszLine[i+1] == '-' )
            {
                break;
            }
        }
        if( i > 0 )
        {
            osSQL.append(pszLine, i);
        }
        osSQL += ' ';
    }
    CSLDestroy(papszLines);
    return osSQL;
}

/************************************************************************/
/*                      GDALVectorInfoOptionsNew()                      */
/************************************************************************/

/**
 * Allocates a GDALVectorInfoOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/ogrinfo.html">ogrinfo</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (ogrinfo_bin.cpp use case) must be allocated with
 *                           GDALVectorInfoOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options, subdataset number...
 * @return pointer to the allocated GDALVectorInfoOptions struct. Must be freed with GDALVectorInfoOptionsFree().
 *
 * @since GDAL 3.7
 */

GDALVectorInfoOptions *GDALVectorInfoOptionsNew(char** papszArgv, GDALVectorInfoOptionsForBinary* psOptionsForBinary)
{
    auto psOptions = cpl::make_unique<GDALVectorInfoOptions>();
    bool bGotFilename = false;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int iArg = 0; papszArgv != nullptr && papszArgv[iArg] != nullptr; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "-ro") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bReadOnly = true;
        }
        else if( EQUAL(papszArgv[iArg], "-update") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bUpdate = true;
        }
        else if( EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            psOptions->bVerbose = false;
            if( psOptionsForBinary )
                psOptionsForBinary->bVerbose = false;
        }
        else if( EQUAL(papszArgv[iArg], "-qq") )
        {
            /* Undocumented: mainly only useful for AFL testing */
            psOptions->bVerbose = false;
            if( psOptionsForBinary )
                psOptionsForBinary->bVerbose = false;
            psOptions->bSuperQuiet = true;
        }
        else if( EQUAL(papszArgv[iArg], "-fid") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->nFetchFID = CPLAtoGIntBig(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-spat") &&
                 papszArgv[iArg+1] != nullptr &&
                 papszArgv[iArg+2] != nullptr &&
                 papszArgv[iArg+3] != nullptr &&
                 papszArgv[iArg+4] != nullptr )
        {
            OGRLinearRing oRing;
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+3]),
                           CPLAtof(papszArgv[iArg+4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+3]),
                           CPLAtof(papszArgv[iArg+2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+2]));

            auto poPolygon = cpl::make_unique<OGRPolygon>();
            poPolygon->addRing(&oRing);
            psOptions->poSpatialFilter.reset(poPolygon.release());
            iArg += 4;
        }
        else if( EQUAL(papszArgv[iArg], "-geomfield") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->osGeomField = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-where") &&
                 papszArgv[iArg+1] != nullptr )
        {
            iArg++;
            GByte* pabyRet = nullptr;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet,
                              nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->osWHERE = reinterpret_cast<char *>(pabyRet);
                VSIFree(pabyRet);
            }
            else
            {
                psOptions->osWHERE = papszArgv[iArg];
            }
        }
        else if( EQUAL(papszArgv[iArg], "-sql") &&
                 papszArgv[iArg+1] != nullptr )
        {
            iArg++;
            GByte* pabyRet = nullptr;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet,
                              nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                psOptions->osSQLStatement = RemoveSQLComments(
                                                reinterpret_cast<char *>(pabyRet));
                VSIFree(pabyRet);
            }
            else
            {
                psOptions->osSQLStatement = papszArgv[iArg];
            }
        }
        else if( EQUAL(papszArgv[iArg], "-dialect") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->osDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-rc") &&
                 papszArgv[iArg+1] != nullptr )
        {
            // Only for fuzzing purposes!
            psOptions->nRepeatCount = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-al") )
        {
            psOptions->bAllLayers = true;
        }
        else if( EQUAL(papszArgv[iArg], "-so") ||
                 EQUAL(papszArgv[iArg], "-summary")  )
        {
            psOptions->bSummaryOnly = true;
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-fields=") )
        {
            psOptions->aosOptions.SetNameValue(
                "DISPLAY_FIELDS", papszArgv[iArg] + strlen("-fields="));
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-geom=") )
        {
            psOptions->aosOptions.SetNameValue(
                "DISPLAY_GEOMETRY", papszArgv[iArg] + strlen("-geom="));
        }
        else if( EQUAL(papszArgv[iArg], "-oo") &&
                 papszArgv[iArg+1] != nullptr )
        {
            ++iArg;
            if( psOptionsForBinary )
                psOptionsForBinary->aosOpenOptions.AddString(papszArgv[iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-nomd") )
        {
            psOptions->bShowMetadata = false;
        }
        else if( EQUAL(papszArgv[iArg], "-listmdd") )
        {
           psOptions-> bListMDD = true;
        }
        else if( EQUAL(papszArgv[iArg], "-mdd") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->aosExtraMDDomains.AddString(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-nocount") )
        {
            psOptions->bFeatureCount = false;
        }
        else if( EQUAL(papszArgv[iArg], "-noextent") )
        {
            psOptions->bExtent = false;
        }
        else if( EQUAL(papszArgv[iArg], "-nogeomtype") )
        {
            psOptions->bGeomType = false;
        }
        else if( EQUAL(papszArgv[iArg], "-rl"))
        {
            psOptions->bDatasetGetNextFeature = true;
        }
        else if( EQUAL(papszArgv[iArg], "-wkt_format") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->osWKTFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-fielddomain") &&
                 papszArgv[iArg+1] != nullptr )
        {
            psOptions->osFieldDomain = papszArgv[++iArg];
        }

        else if( EQUAL(papszArgv[iArg], "-if") && papszArgv[iArg+1] != nullptr )
        {
            iArg++;
            if( psOptionsForBinary )
            {
                if( GDALGetDriverByName(papszArgv[iArg]) == nullptr )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s is not a recognized driver", papszArgv[iArg]);
                }
                psOptionsForBinary->aosAllowInputDrivers.AddString(papszArgv[iArg]);
            }
        }
        /* Not documented: used by gdalinfo_bin.cpp only */
        else if( EQUAL(papszArgv[iArg], "-stdout") )
            psOptions->bStdoutOutput = true;
        else if( papszArgv[iArg][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[iArg]);
            return nullptr;
        }
        else if( !bGotFilename )
        {
            bGotFilename = true;
            psOptions->osFilename = papszArgv[iArg];
            if( psOptionsForBinary )
                psOptionsForBinary->osFilename = psOptions->osFilename;
        }
        else
        {
            psOptions->aosLayers.AddString(papszArgv[iArg]);
            psOptions->bAllLayers = false;
        }
    }

    if( psOptionsForBinary )
        psOptionsForBinary->osSQLStatement = psOptions->osSQLStatement;

    if( !psOptions->osDialect.empty() && !psOptions->osWHERE.empty() &&
        psOptions->osSQLStatement.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "-dialect is ignored with -where. Use -sql instead");
    }

    if( psOptions->bDatasetGetNextFeature && !psOptions->osSQLStatement.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "-rl is incompatible with -sql");
        return nullptr;
    }

    return psOptions.release();
}
