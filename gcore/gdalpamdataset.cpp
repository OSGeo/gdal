/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamDataset, a dataset base class that
 *           knows how to persist auxiliary metadata into a support XML file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_pam.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           GDALPamDataset()                           */
/************************************************************************/

/**
 * \class GDALPamDataset "gdal_pam.h"
 *
 * A subclass of GDALDataset which introduces the ability to save and
 * restore auxiliary information (coordinate system, gcps, metadata,
 * etc) not supported by a file format via an "auxiliary metadata" file
 * with the .aux.xml extension.
 *
 * <h3>Enabling PAM</h3>
 *
 * PAM support can be enabled (resp. disabled) in GDAL by setting the
 * GDAL_PAM_ENABLED configuration option (via CPLSetConfigOption(), or the
 * environment) to the value of YES (resp. NO). Note: The default value is
 * build dependent and defaults to YES in Windows and Unix builds. Warning:
 * For GDAL < 3.5, setting this option to OFF may have unwanted side-effects on drivers
 * that rely on PAM functionality.
 *
 * <h3>PAM Proxy Files</h3>
 *
 * In order to be able to record auxiliary information about files on
 * read-only media such as CDROMs or in directories where the user does not
 * have write permissions, it is possible to enable the "PAM Proxy Database".
 * When enabled the .aux.xml files are kept in a different directory, writable
 * by the user. Overviews will also be stored in the PAM proxy directory.
 *
 * To enable this, set the GDAL_PAM_PROXY_DIR configuration option to be
 * the name of the directory where the proxies should be kept. The configuration
 * option must be set *before* the first access to PAM, because its value is
 * cached for later access.
 *
 * <h3>Adding PAM to Drivers</h3>
 *
 * Drivers for physical file formats that wish to support persistent auxiliary
 * metadata in addition to that for the format itself should derive their
 * dataset class from GDALPamDataset instead of directly from GDALDataset.
 * The raster band classes should also be derived from GDALPamRasterBand.
 *
 * They should also call something like this near the end of the Open()
 * method:
 *
 * \code
 *      poDS->SetDescription( poOpenInfo->pszFilename );
 *      poDS->TryLoadXML();
 * \endcode
 *
 * The SetDescription() is necessary so that the dataset will have a valid
 * filename set as the description before TryLoadXML() is called.  TryLoadXML()
 * will look for an .aux.xml file with the same basename as the dataset and
 * in the same directory.  If found the contents will be loaded and kept
 * track of in the GDALPamDataset and GDALPamRasterBand objects.  When a
 * call like GetProjectionRef() is not implemented by the format specific
 * class, it will fall through to the PAM implementation which will return
 * information if it was in the .aux.xml file.
 *
 * Drivers should also try to call the GDALPamDataset/GDALPamRasterBand
 * methods as a fallback if their implementation does not find information.
 * This allows using the .aux.xml for variations that can't be stored in
 * the format.  For instance, the GeoTIFF driver GetProjectionRef() looks
 * like this:
 *
 * \code
 *      if( EQUAL(pszProjection,"") )
 *          return GDALPamDataset::GetProjectionRef();
 *      else
 *          return( pszProjection );
 * \endcode
 *
 * So if the geotiff header is missing, the .aux.xml file will be
 * consulted.
 *
 * Similarly, if SetProjection() were called with a coordinate system
 * not supported by GeoTIFF, the SetProjection() method should pass it on
 * to the GDALPamDataset::SetProjection() method after issuing a warning
 * that the information can't be represented within the file itself.
 *
 * Drivers for subdataset based formats will also need to declare the
 * name of the physical file they are related to, and the name of their
 * subdataset before calling TryLoadXML().
 *
 * \code
 *      poDS->SetDescription( poOpenInfo->pszFilename );
 *      poDS->SetPhysicalFilename( poDS->pszFilename );
 *      poDS->SetSubdatasetName( osSubdatasetName );
 *
 *      poDS->TryLoadXML();
 * \endcode
 */
class GDALPamDataset;

GDALPamDataset::GDALPamDataset()
{
    SetMOFlags( GetMOFlags() | GMO_PAM_CLASS );
}

/************************************************************************/
/*                          ~GDALPamDataset()                           */
/************************************************************************/

GDALPamDataset::~GDALPamDataset()

{
    if( bSuppressOnClose )
    {
        if( psPam && psPam->pszPamFilename != nullptr )
            VSIUnlink(psPam->pszPamFilename);
    }
    else if( nPamFlags & GPF_DIRTY )
    {
        CPLDebug( "GDALPamDataset", "In destructor with dirty metadata." );
        GDALPamDataset::TrySaveXML();
    }

    PamClear();
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void GDALPamDataset::FlushCache(bool bAtClosing)

{
    GDALDataset::FlushCache(bAtClosing);
    if( nPamFlags & GPF_DIRTY )
        TrySaveXML();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLXMLNode *GDALPamDataset::SerializeToXML( const char *pszUnused )

{
    if( psPam == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Setup root node and attributes.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = CPLCreateXMLNode( nullptr, CXT_Element, "PAMDataset" );

/* -------------------------------------------------------------------- */
/*      SRS                                                             */
/* -------------------------------------------------------------------- */
    if( psPam->poSRS && !psPam->poSRS->IsEmpty() )
    {
        char* pszWKT = nullptr;
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            if( psPam->poSRS->exportToWkt(&pszWKT) != OGRERR_NONE )
            {
                CPLFree(pszWKT);
                pszWKT = nullptr;
                const char* const apszOptions[] = { "FORMAT=WKT2", nullptr };
                psPam->poSRS->exportToWkt(&pszWKT, apszOptions);
            }
        }
        CPLXMLNode* psSRSNode = CPLCreateXMLElementAndValue( psDSTree, "SRS", pszWKT );
        CPLFree(pszWKT);
        const auto& mapping = psPam->poSRS->GetDataAxisToSRSAxisMapping();
        CPLString osMapping;
        for( size_t i = 0; i < mapping.size(); ++i )
        {
            if( !osMapping.empty() )
                osMapping += ",";
            osMapping += CPLSPrintf("%d", mapping[i]);
        }
        CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                   osMapping.c_str());

        const double dfCoordinateEpoch = psPam->poSRS->GetCoordinateEpoch();
        if( dfCoordinateEpoch > 0 )
        {
            std::string osCoordinateEpoch = CPLSPrintf("%f", dfCoordinateEpoch);
            if( osCoordinateEpoch.find('.') != std::string::npos )
            {
                while( osCoordinateEpoch.back() == '0' )
                    osCoordinateEpoch.resize(osCoordinateEpoch.size()-1);
            }
            CPLAddXMLAttributeAndValue(psSRSNode, "coordinateEpoch",
                                       osCoordinateEpoch.c_str());
        }
    }

/* -------------------------------------------------------------------- */
/*      GeoTransform.                                                   */
/* -------------------------------------------------------------------- */
    if( psPam->bHaveGeoTransform )
    {
        CPLString oFmt;
        oFmt.Printf( "%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                     psPam->adfGeoTransform[0],
                     psPam->adfGeoTransform[1],
                     psPam->adfGeoTransform[2],
                     psPam->adfGeoTransform[3],
                     psPam->adfGeoTransform[4],
                     psPam->adfGeoTransform[5] );
        CPLSetXMLValue( psDSTree, "GeoTransform", oFmt );
    }

/* -------------------------------------------------------------------- */
/*      Metadata.                                                       */
/* -------------------------------------------------------------------- */
    if( psPam->bHasMetadata )
    {
        CPLXMLNode *psMD = oMDMD.Serialize();
        if( psMD != nullptr )
        {
            CPLAddXMLChild( psDSTree, psMD );
        }
    }

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( psPam->nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psDSTree,
                                   psPam->pasGCPList,
                                   psPam->nGCPCount,
                                   psPam->poGCP_SRS );
    }

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */

    // Find last child
    CPLXMLNode* psLastChild = psDSTree->psChild;
    for( ; psLastChild != nullptr && psLastChild->psNext;
                                    psLastChild = psLastChild->psNext )
    {
    }

    for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        GDALRasterBand * const poBand = GetRasterBand(iBand+1);

        if( poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        CPLXMLNode * const psBandTree =
            cpl::down_cast<GDALPamRasterBand *>(poBand)->SerializeToXML( pszUnused );

        if( psBandTree != nullptr )
        {
            if( psLastChild == nullptr )
            {
                CPLAddXMLChild( psDSTree, psBandTree );
            }
            else
            {
                psLastChild->psNext = psBandTree;
            }
            psLastChild = psBandTree;
        }
    }

/* -------------------------------------------------------------------- */
/*      We don't want to return anything if we had no metadata to       */
/*      attach.                                                         */
/* -------------------------------------------------------------------- */
    if( psDSTree->psChild == nullptr )
    {
        CPLDestroyXMLNode( psDSTree );
        psDSTree = nullptr;
    }

    return psDSTree;
}

/************************************************************************/
/*                           PamInitialize()                            */
/************************************************************************/

void GDALPamDataset::PamInitialize()

{
#ifdef PAM_ENABLED
    const char * const pszPamDefault = "YES";
#else
    const char * const pszPamDefault = "NO";
#endif

    if( psPam )
        return;

    if( !CPLTestBool( CPLGetConfigOption( "GDAL_PAM_ENABLED",
                                             pszPamDefault ) ) )
    {
        CPLDebug("GDAL", "PAM is disabled");
        nPamFlags |= GPF_DISABLED;
    }

    /* ERO 2011/04/13 : GPF_AUXMODE seems to be unimplemented */
    if( EQUAL( CPLGetConfigOption( "GDAL_PAM_MODE", "PAM" ), "AUX") )
        nPamFlags |= GPF_AUXMODE;

    psPam = new GDALDatasetPamInfo;
    for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = GetRasterBand(iBand+1);

        if( poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        cpl::down_cast<GDALPamRasterBand *>(poBand)->PamInitialize();
    }
}

/************************************************************************/
/*                              PamClear()                              */
/************************************************************************/

void GDALPamDataset::PamClear()

{
    if( psPam )
    {
        CPLFree( psPam->pszPamFilename );
        if( psPam->poSRS )
            psPam->poSRS->Release();
        if( psPam->poGCP_SRS )
            psPam->poGCP_SRS->Release();
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
        }

        delete psPam;
        psPam = nullptr;
    }
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamDataset::XMLInit( CPLXMLNode *psTree, const char *pszUnused )

{
/* -------------------------------------------------------------------- */
/*      Check for an SRS node.                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode* psSRSNode = CPLGetXMLNode(psTree, "SRS");
    if( psSRSNode )
    {
        if( psPam->poSRS )
            psPam->poSRS->Release();
        psPam->poSRS = new OGRSpatialReference();
        psPam->poSRS->SetFromUserInput( CPLGetXMLValue(psSRSNode, nullptr, ""),
                                        OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS );
        const char* pszMapping =
            CPLGetXMLValue(psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
        if( pszMapping )
        {
            char** papszTokens = CSLTokenizeStringComplex( pszMapping, ",", FALSE, FALSE);
            std::vector<int> anMapping;
            for( int i = 0; papszTokens && papszTokens[i]; i++ )
            {
                anMapping.push_back(atoi(papszTokens[i]));
            }
            CSLDestroy(papszTokens);
            psPam->poSRS->SetDataAxisToSRSAxisMapping(anMapping);
        }
        else
        {
            psPam->poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }

        const char* pszCoordinateEpoch =
            CPLGetXMLValue(psSRSNode, "coordinateEpoch", nullptr);
        if( pszCoordinateEpoch )
            psPam->poSRS->SetCoordinateEpoch(CPLAtof(pszCoordinateEpoch));
    }

/* -------------------------------------------------------------------- */
/*      Check for a GeoTransform node.                                  */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "GeoTransform", "")) > 0 )
    {
        const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");

        char **papszTokens =
            CSLTokenizeStringComplex( pszGT, ",", FALSE, FALSE );
        if( CSLCount(papszTokens) != 6 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "GeoTransform node does not have expected six values.");
        }
        else
        {
            for( int iTA = 0; iTA < 6; iTA++ )
                psPam->adfGeoTransform[iTA] = CPLAtof(papszTokens[iTA]);
            psPam->bHaveGeoTransform = TRUE;
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != nullptr )
    {
        if( psPam->poGCP_SRS )
            psPam->poGCP_SRS->Release();
        psPam->poGCP_SRS = nullptr;

        // Make sure any previous GCPs, perhaps from an .aux file, are cleared
        // if we have new ones.
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
            psPam->nGCPCount = 0;
            psPam->pasGCPList = nullptr;
        }

        GDALDeserializeGCPListFromXML( psGCPList,
                                       &(psPam->pasGCPList),
                                       &(psPam->nGCPCount),
                                       &(psPam->poGCP_SRS) );
    }

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    if( oMDMD.XMLInit( psTree, TRUE ) )
    {
        psPam->bHasMetadata = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Try loading ESRI xml encoded GeodataXform.                      */
/* -------------------------------------------------------------------- */
    if (psPam->poSRS == nullptr)
    {
        // ArcGIS 9.3: GeodataXform as a root element
        CPLXMLNode* psGeodataXform = CPLGetXMLNode(psTree, "=GeodataXform");
        CPLXMLNode *psValueAsXML = nullptr;
        if( psGeodataXform != nullptr )
        {
            char* apszMD[2];
            apszMD[0] = CPLSerializeXMLTree(psGeodataXform);
            apszMD[1] = nullptr;
            oMDMD.SetMetadata( apszMD, "xml:ESRI");
            CPLFree(apszMD[0]);
        }
        else
        {
            // ArcGIS 10: GeodataXform as content of xml:ESRI metadata domain.
            char** papszXML = oMDMD.GetMetadata( "xml:ESRI" );
            if (CSLCount(papszXML) == 1)
            {
                psValueAsXML = CPLParseXMLString( papszXML[0] );
                if( psValueAsXML )
                    psGeodataXform = CPLGetXMLNode(psValueAsXML, "=GeodataXform");
            }
        }

        if (psGeodataXform)
        {
            const char* pszESRI_WKT = CPLGetXMLValue(psGeodataXform,
                                "SpatialReference.WKT", nullptr);
            if (pszESRI_WKT)
            {
                psPam->poSRS = new OGRSpatialReference(nullptr);
                psPam->poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if( psPam->poSRS->importFromWkt(pszESRI_WKT) != OGRERR_NONE)
                {
                    delete psPam->poSRS;
                    psPam->poSRS = nullptr;
                }
            }

            // Parse GCPs
            const CPLXMLNode* psSourceGCPS = CPLGetXMLNode(psGeodataXform, "SourceGCPs");
            const CPLXMLNode* psTargetGCPs = CPLGetXMLNode(psGeodataXform, "TargetGCPs");
            if( psSourceGCPS && psTargetGCPs && !psPam->bHaveGeoTransform )
            {
                std::vector<double> adfSource;
                std::vector<double> adfTarget;
                bool ySourceAllNegative = true;
                for( auto psIter = psSourceGCPS->psChild; psIter; psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element && strcmp(psIter->pszValue, "Double") == 0 )
                    {
                        adfSource.push_back(CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                        if( (adfSource.size() % 2) == 0 && adfSource.back() > 0 )
                            ySourceAllNegative = false;
                    }
                }
                for( auto psIter = psTargetGCPs->psChild; psIter; psIter = psIter->psNext )
                {
                    if( psIter->eType == CXT_Element && strcmp(psIter->pszValue, "Double") == 0 )
                    {
                        adfTarget.push_back(CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                    }
                }
                if( !adfSource.empty() &&
                    adfSource.size() == adfTarget.size() &&
                    (adfSource.size() % 2) == 0 )
                {
                    std::vector<GDAL_GCP> asGCPs;
                    asGCPs.resize(adfSource.size() / 2);
                    char szEmptyString[] = { 0 };
                    for( size_t i = 0; i+1 < adfSource.size(); i+= 2 )
                    {
                        asGCPs[i/2].pszId = szEmptyString;
                        asGCPs[i/2].pszInfo = szEmptyString;
                        asGCPs[i/2].dfGCPPixel = adfSource[i];
                        asGCPs[i/2].dfGCPLine =
                            ySourceAllNegative ? -adfSource[i+1] : adfSource[i+1];
                        asGCPs[i/2].dfGCPX = adfTarget[i];
                        asGCPs[i/2].dfGCPY = adfTarget[i+1];
                        asGCPs[i/2].dfGCPZ = 0;
                    }
                    GDALPamDataset::SetGCPs( static_cast<int>(asGCPs.size()),
                                             &asGCPs[0],
                                             psPam->poSRS );
                    delete psPam->poSRS;
                    psPam->poSRS = nullptr;
                }
            }
        }
        if( psValueAsXML )
            CPLDestroyXMLNode(psValueAsXML);
    }

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psBandTree = psTree->psChild;
         psBandTree != nullptr;
         psBandTree = psBandTree->psNext )
    {
        if( psBandTree->eType != CXT_Element
            || !EQUAL(psBandTree->pszValue,"PAMRasterBand") )
            continue;

        const int nBand = atoi(CPLGetXMLValue( psBandTree, "band", "0"));

        if( nBand < 1 || nBand > GetRasterCount() )
            continue;

        GDALRasterBand *poBand = GetRasterBand(nBand);

        if( poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        GDALPamRasterBand *poPamBand = cpl::down_cast<GDALPamRasterBand *>(
            GetRasterBand(nBand) );

        poPamBand->XMLInit( psBandTree, pszUnused );
    }

/* -------------------------------------------------------------------- */
/*      Preserve Array information.                                     */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode* psIter = psTree->psChild;
                                            psIter; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Array") == 0 )
        {
            CPLXMLNode* psNextBackup = psIter->psNext;
            psIter->psNext = nullptr;
            psPam->m_apoOtherNodes.emplace_back(CPLXMLTreeCloser(CPLCloneXMLTree(psIter)));
            psIter->psNext = psNextBackup;
        }
    }

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.                                               */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_None;
}

/************************************************************************/
/*                        SetPhysicalFilename()                         */
/************************************************************************/

void GDALPamDataset::SetPhysicalFilename( const char *pszFilename )

{
    PamInitialize();

    if( psPam )
        psPam->osPhysicalFilename = pszFilename;
}

/************************************************************************/
/*                        GetPhysicalFilename()                         */
/************************************************************************/

const char *GDALPamDataset::GetPhysicalFilename()

{
    PamInitialize();

    if( psPam )
        return psPam->osPhysicalFilename;

    return "";
}

/************************************************************************/
/*                         SetSubdatasetName()                          */
/************************************************************************/

void GDALPamDataset::SetSubdatasetName( const char *pszSubdataset )

{
    PamInitialize();

    if( psPam )
        psPam->osSubdatasetName = pszSubdataset;
}

/************************************************************************/
/*                         GetSubdatasetName()                          */
/************************************************************************/

const char *GDALPamDataset::GetSubdatasetName()

{
    PamInitialize();

    if( psPam )
        return psPam->osSubdatasetName;

    return "";
}

/************************************************************************/
/*                          BuildPamFilename()                          */
/************************************************************************/

const char *GDALPamDataset::BuildPamFilename()

{
    if( psPam == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      What is the name of the physical file we are referencing?       */
/*      We allow an override via the psPam->pszPhysicalFile item.       */
/* -------------------------------------------------------------------- */
    if( psPam->pszPamFilename != nullptr )
        return psPam->pszPamFilename;

    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr )
        pszPhysicalFile = GetDescription();

    if( strlen(pszPhysicalFile) == 0 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Try a proxy lookup, otherwise just add .aux.xml.                */
/* -------------------------------------------------------------------- */
    const char *pszProxyPam = PamGetProxy( pszPhysicalFile );
    if( pszProxyPam != nullptr )
        psPam->pszPamFilename = CPLStrdup(pszProxyPam);
    else
    {
        if( !GDALCanFileAcceptSidecarFile(pszPhysicalFile) )
            return nullptr;
        psPam->pszPamFilename = static_cast<char*>(CPLMalloc(strlen(pszPhysicalFile)+10));
        strcpy( psPam->pszPamFilename, pszPhysicalFile );
        strcat( psPam->pszPamFilename, ".aux.xml" );
    }

    return psPam->pszPamFilename;
}

/************************************************************************/
/*                   IsPamFilenameAPotentialSiblingFile()               */
/************************************************************************/

int GDALPamDataset::IsPamFilenameAPotentialSiblingFile()
{
    if (psPam == nullptr)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Determine if the PAM filename is a .aux.xml file next to the    */
/*      physical file, or if it comes from the ProxyDB                  */
/* -------------------------------------------------------------------- */
    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr )
        pszPhysicalFile = GetDescription();

    size_t nLenPhysicalFile = strlen(pszPhysicalFile);
    int bIsSiblingPamFile = strncmp(psPam->pszPamFilename, pszPhysicalFile,
                                    nLenPhysicalFile) == 0 &&
                            strcmp(psPam->pszPamFilename + nLenPhysicalFile,
                                   ".aux.xml") == 0;

    return bIsSiblingPamFile;
}

/************************************************************************/
/*                             TryLoadXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadXML(char **papszSiblingFiles)

{
    PamInitialize();

    if( psPam == nullptr || (nPamFlags & GPF_DISABLED) != 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.  Generally when we get to this point is       */
/*      from a call at the end of the Open() method, and some calls     */
/*      may have already marked the PAM info as dirty (for instance     */
/*      setting metadata), but really everything to this point is       */
/*      reproducible, and so the PAM info should not really be          */
/*      thought of as dirty.                                            */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

/* -------------------------------------------------------------------- */
/*      Try reading the file.                                           */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      In case the PAM filename is a .aux.xml file next to the         */
/*      physical file and we have a siblings list, then we can skip     */
/*      stat'ing the filesystem.                                        */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    CPLXMLNode *psTree = nullptr;

    if( papszSiblingFiles != nullptr && IsPamFilenameAPotentialSiblingFile() &&
        GDALCanReliablyUseSiblingFileList(psPam->pszPamFilename) )
    {
        const int iSibling =
            CSLFindString( papszSiblingFiles,
                           CPLGetFilename(psPam->pszPamFilename) );
        if( iSibling >= 0 )
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            psTree = CPLParseXMLFile( psPam->pszPamFilename );
        }
    }
    else
    if( VSIStatExL( psPam->pszPamFilename, &sStatBuf,
                    VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0
        && VSI_ISREG( sStatBuf.st_mode ) )
    {
        CPLErrorStateBackuper oErrorStateBackuper;
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        psTree = CPLParseXMLFile( psPam->pszPamFilename );
    }

/* -------------------------------------------------------------------- */
/*      If we are looking for a subdataset, search for its subtree now. */
/* -------------------------------------------------------------------- */
    if( psTree && !psPam->osSubdatasetName.empty() )
    {
        CPLXMLNode *psSubTree = psTree->psChild;

        for( ;
             psSubTree != nullptr;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue,"Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            psSubTree = CPLGetXMLNode( psSubTree, "PAMDataset" );
            break;
        }

        if( psSubTree != nullptr )
            psSubTree = CPLCloneXMLTree( psSubTree );

        CPLDestroyXMLNode( psTree );
        psTree = psSubTree;
    }

/* -------------------------------------------------------------------- */
/*      If we fail, try .aux.                                           */
/* -------------------------------------------------------------------- */
    if( psTree == nullptr )
        return TryLoadAux(papszSiblingFiles);

/* -------------------------------------------------------------------- */
/*      Initialize ourselves from this XML tree.                        */
/* -------------------------------------------------------------------- */

    CPLString osVRTPath(CPLGetPath(psPam->pszPamFilename));
    const CPLErr eErr = XMLInit( psTree, osVRTPath );

    CPLDestroyXMLNode( psTree );

    if( eErr != CE_None )
        PamClear();

    return eErr;
}

/************************************************************************/
/*                             TrySaveXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TrySaveXML()

{
    nPamFlags &= ~GPF_DIRTY;

    if( psPam == nullptr || (nPamFlags & GPF_NOSAVE) != 0 || (nPamFlags & GPF_DISABLED) != 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Make sure we know the filename we want to store in.             */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Build the XML representation of the auxiliary metadata.          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = SerializeToXML( nullptr );

    if( psTree == nullptr )
    {
        /* If we have unset all metadata, we have to delete the PAM file */
        CPLPushErrorHandler( CPLQuietErrorHandler );
        VSIUnlink(psPam->pszPamFilename);
        CPLPopErrorHandler();
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      If we are working with a subdataset, we need to integrate       */
/*      the subdataset tree within the whole existing pam tree,         */
/*      after removing any old version of the same subdataset.          */
/* -------------------------------------------------------------------- */
    if( !psPam->osSubdatasetName.empty() )
    {
        CPLXMLNode *psOldTree = nullptr;

        VSIStatBufL sStatBuf;
        if( VSIStatExL( psPam->pszPamFilename, &sStatBuf,
                    VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0
            && VSI_ISREG( sStatBuf.st_mode ) )
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            psOldTree = CPLParseXMLFile( psPam->pszPamFilename );
        }

        if( psOldTree == nullptr )
            psOldTree = CPLCreateXMLNode( nullptr, CXT_Element, "PAMDataset" );

        CPLXMLNode* psSubTree = psOldTree->psChild;
        for( /* initialized above */;
             psSubTree != nullptr;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue,"Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            break;
        }

        if( psSubTree == nullptr )
        {
            psSubTree = CPLCreateXMLNode( psOldTree, CXT_Element,
                                          "Subdataset" );
            CPLCreateXMLNode(
                CPLCreateXMLNode( psSubTree, CXT_Attribute, "name" ),
                CXT_Text, psPam->osSubdatasetName );
        }

        CPLXMLNode *psOldPamDataset = CPLGetXMLNode( psSubTree, "PAMDataset");
        if( psOldPamDataset != nullptr )
        {
            CPLRemoveXMLChild( psSubTree, psOldPamDataset );
            CPLDestroyXMLNode( psOldPamDataset );
        }

        CPLAddXMLChild( psSubTree, psTree );
        psTree = psOldTree;
    }


/* -------------------------------------------------------------------- */
/*      Preserve other information.                                     */
/* -------------------------------------------------------------------- */
    for( const auto& poOtherNode: psPam->m_apoOtherNodes )
    {
        CPLAddXMLChild(psTree, CPLCloneXMLTree(poOtherNode.get()));
    }

/* -------------------------------------------------------------------- */
/*      Try saving the auxiliary metadata.                               */
/* -------------------------------------------------------------------- */

    CPLPushErrorHandler( CPLQuietErrorHandler );
    const int bSaved =
        CPLSerializeXMLTreeToFile( psTree, psPam->pszPamFilename );
    CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      If it fails, check if we have a proxy directory for auxiliary    */
/*      metadata to be stored in, and try to save there.                */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( bSaved )
        eErr = CE_None;
    else
    {
        const char *pszBasename = GetDescription();

        if( psPam->osPhysicalFilename.length() > 0 )
            pszBasename = psPam->osPhysicalFilename;

        const char *pszNewPam = nullptr;
        if( PamGetProxy(pszBasename) == nullptr
            && ((pszNewPam = PamAllocateProxy(pszBasename)) != nullptr))
        {
            CPLErrorReset();
            CPLFree( psPam->pszPamFilename );
            psPam->pszPamFilename = CPLStrdup(pszNewPam);
            eErr = TrySaveXML();
        }
        /* No way we can save into a /vsicurl resource */
        else if( !STARTS_WITH(psPam->pszPamFilename, "/vsicurl") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unable to save auxiliary information in %s.",
                      psPam->pszPamFilename );
            eErr = CE_Warning;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psTree );

    return eErr;
}

/************************************************************************/
/*                             CloneInfo()                              */
/************************************************************************/

CPLErr GDALPamDataset::CloneInfo( GDALDataset *poSrcDS, int nCloneFlags )

{
    const int bOnlyIfMissing = nCloneFlags & GCIF_ONLY_IF_MISSING;
    const int nSavedMOFlags = GetMOFlags();

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Suppress NotImplemented error messages - mainly needed if PAM   */
/*      disabled.                                                       */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags | GMO_IGNORE_UNIMPLEMENTED );

/* -------------------------------------------------------------------- */
/*      GeoTransform                                                    */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_GEOTRANSFORM )
    {
      double adfGeoTransform[6] = { 0.0 };

        if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
        {
            double adfOldGT[6] = { 0.0 };

            if( !bOnlyIfMissing || GetGeoTransform( adfOldGT ) != CE_None )
                SetGeoTransform( adfGeoTransform );
        }
    }

/* -------------------------------------------------------------------- */
/*      Projection                                                      */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_PROJECTION )
    {
        const auto poSRS = poSrcDS->GetSpatialRef();

        if( poSRS != nullptr )
        {
            if( !bOnlyIfMissing
                || GetSpatialRef() == nullptr )
                SetSpatialRef( poSRS );
        }
    }

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_GCPS )
    {
        if( poSrcDS->GetGCPCount() > 0 )
        {
            if( !bOnlyIfMissing || GetGCPCount() == 0 )
            {
                SetGCPs( poSrcDS->GetGCPCount(),
                         poSrcDS->GetGCPs(),
                         poSrcDS->GetGCPSpatialRef() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_METADATA )
    {
        for( const char* pszMDD: { "", "RPC", "json:ISIS3", "json:VICAR" } )
        {
            auto papszSrcMD = poSrcDS->GetMetadata(pszMDD);
            if(papszSrcMD  != nullptr )
            {
                if( !bOnlyIfMissing
                    || CSLCount(GetMetadata(pszMDD)) != CSLCount(papszSrcMD) )
                {
                    SetMetadata( papszSrcMD, pszMDD );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_PROCESS_BANDS )
    {
        for( int iBand = 0; iBand < GetRasterCount(); iBand++ )
        {
            GDALRasterBand *poBand = GetRasterBand(iBand+1);

            if( poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
                continue;

            if( poSrcDS->GetRasterCount() >= iBand+1 )
            {
                cpl::down_cast<GDALPamRasterBand *>(poBand)->
                    CloneInfo( poSrcDS->GetRasterBand(iBand+1), nCloneFlags );
            }
            else
                CPLDebug(
                    "GDALPamDataset",
                    "Skipping CloneInfo for band not in source, "
                    "this is a bit unusual!" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy masks.  These are really copied at a lower level using     */
/*      GDALDefaultOverviews, for formats with no native mask           */
/*      support but this is a convenient central point to put this      */
/*      for most drivers.                                               */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_MASK )
    {
        GDALDriver::DefaultCopyMasks( poSrcDS, this, FALSE );
    }

/* -------------------------------------------------------------------- */
/*      Restore MO flags.                                               */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags );

    return CE_None;
}
//! @endcond

/************************************************************************/
/*                            GetFileList()                             */
/*                                                                      */
/*      Add .aux.xml or .aux file into file list as appropriate.        */
/************************************************************************/

char **GDALPamDataset::GetFileList()

{
    char **papszFileList = GDALDataset::GetFileList();

    if( psPam && !psPam->osPhysicalFilename.empty()
        && GDALCanReliablyUseSiblingFileList(psPam->osPhysicalFilename.c_str())
        && CSLFindString( papszFileList, psPam->osPhysicalFilename ) == -1 )
    {
        papszFileList = CSLInsertString( papszFileList, 0,
                                         psPam->osPhysicalFilename );
    }

    if( psPam && psPam->pszPamFilename )
    {
        int bAddPamFile = nPamFlags & GPF_DIRTY;
        if (!bAddPamFile)
        {
            VSIStatBufL sStatBuf;
            if (oOvManager.GetSiblingFiles() != nullptr &&
                IsPamFilenameAPotentialSiblingFile() &&
                GDALCanReliablyUseSiblingFileList(psPam->pszPamFilename) )
            {
                bAddPamFile = CSLFindString(oOvManager.GetSiblingFiles(),
                                  CPLGetFilename(psPam->pszPamFilename)) >= 0;
            }
            else
            {
                bAddPamFile = VSIStatExL( psPam->pszPamFilename, &sStatBuf,
                                          VSI_STAT_EXISTS_FLAG ) == 0;
            }
        }
        if (bAddPamFile)
        {
            papszFileList = CSLAddString( papszFileList, psPam->pszPamFilename );
        }
    }

    if( psPam && !psPam->osAuxFilename.empty() &&
        GDALCanReliablyUseSiblingFileList(psPam->osAuxFilename.c_str()) &&
        CSLFindString( papszFileList, psPam->osAuxFilename ) == -1 )
    {
        papszFileList = CSLAddString( papszFileList, psPam->osAuxFilename );
    }
    return papszFileList;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALPamDataset::IBuildOverviews( const char *pszResampling,
                                        int nOverviews, int *panOverviewList,
                                        int nListBands, int *panBandList,
                                        GDALProgressFunc pfnProgress,
                                        void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Initialize PAM.                                                 */
/* -------------------------------------------------------------------- */
    PamInitialize();
    if( psPam == nullptr )
        return GDALDataset::IBuildOverviews( pszResampling,
                                             nOverviews, panOverviewList,
                                             nListBands, panBandList,
                                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      If we appear to have subdatasets and to have a physical         */
/*      filename, use that physical filename to derive a name for a     */
/*      new overview file.                                              */
/* -------------------------------------------------------------------- */
    if( oOvManager.IsInitialized() && psPam->osPhysicalFilename.length() != 0 )
    {
        return oOvManager.BuildOverviewsSubDataset(
            psPam->osPhysicalFilename, pszResampling,
            nOverviews, panOverviewList,
            nListBands, panBandList,
            pfnProgress, pProgressData );
    }

    return GDALDataset::IBuildOverviews( pszResampling,
                                         nOverviews, panOverviewList,
                                         nListBands, panBandList,
                                         pfnProgress, pProgressData );
}
//! @endcond

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *GDALPamDataset::GetSpatialRef() const

{
    if( psPam && psPam->poSRS )
        return psPam->poSRS;

    return GDALDataset::GetSpatialRef();
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GDALPamDataset::SetSpatialRef( const OGRSpatialReference* poSRS )

{
    PamInitialize();

    if( psPam == nullptr )
        return GDALDataset::SetSpatialRef( poSRS );

    if( psPam->poSRS )
        psPam->poSRS->Release();
    psPam->poSRS = poSRS ? poSRS->Clone() : nullptr;
    MarkPamDirty();

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::GetGeoTransform( double * padfTransform )

{
    if( psPam && psPam->bHaveGeoTransform )
    {
        memcpy( padfTransform, psPam->adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetGeoTransform( double * padfTransform )

{
    PamInitialize();

    if( psPam )
    {
        MarkPamDirty();
        psPam->bHaveGeoTransform = TRUE;
        memcpy( psPam->adfGeoTransform, padfTransform, sizeof(double) * 6 );
        return( CE_None );
    }

    return GDALDataset::SetGeoTransform( padfTransform );
}

/************************************************************************/
/*                        DeleteGeoTransform()                          */
/************************************************************************/

/** Remove geotransform from PAM.
 *
 * @since GDAL 3.4.1
 */
void GDALPamDataset::DeleteGeoTransform()

{
    PamInitialize();

    if( psPam && psPam->bHaveGeoTransform )
    {
        MarkPamDirty();
        psPam->bHaveGeoTransform = FALSE;
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GDALPamDataset::GetGCPCount()

{
    if( psPam && psPam->nGCPCount > 0 )
        return psPam->nGCPCount;

    return GDALDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *GDALPamDataset::GetGCPSpatialRef() const

{
    if( psPam && psPam->poGCP_SRS != nullptr )
        return psPam->poGCP_SRS;

    return GDALDataset::GetGCPSpatialRef();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GDALPamDataset::GetGCPs()

{
    if( psPam && psPam->nGCPCount > 0 )
        return psPam->pasGCPList;

    return GDALDataset::GetGCPs();
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr GDALPamDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                const OGRSpatialReference* poGCP_SRS )

{
    PamInitialize();

    if( psPam )
    {
        if( psPam->poGCP_SRS )
            psPam->poGCP_SRS->Release();
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
        }

        psPam->poGCP_SRS = poGCP_SRS ? poGCP_SRS->Clone() : nullptr;
        psPam->nGCPCount = nGCPCount;
        psPam->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );

        MarkPamDirty();

        return CE_None;
    }

    return GDALDataset::SetGCPs( nGCPCount, pasGCPList, poGCP_SRS );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadata( char **papszMetadata,
                                    const char *pszDomain )

{
    PamInitialize();

    if( psPam )
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

    return GDALDataset::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadataItem( const char *pszName,
                                        const char *pszValue,
                                        const char *pszDomain )

{
    PamInitialize();

    if( psPam )
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALPamDataset::GetMetadataItem( const char *pszName,
                                             const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      A request against the ProxyOverviewRequest is a special         */
/*      mechanism to request an overview filename be allocated in       */
/*      the proxy pool location.  The allocated name is saved as        */
/*      metadata as well as being returned.                             */
/* -------------------------------------------------------------------- */
    if( pszDomain != nullptr && EQUAL(pszDomain,"ProxyOverviewRequest") )
    {
        CPLString osPrelimOvr = GetDescription();
        osPrelimOvr += ":::OVR";

        const char *pszProxyOvrFilename = PamAllocateProxy( osPrelimOvr );
        if( pszProxyOvrFilename == nullptr )
            return nullptr;

        SetMetadataItem( "OVERVIEW_FILE", pszProxyOvrFilename, "OVERVIEWS" );

        return pszProxyOvrFilename;
    }

/* -------------------------------------------------------------------- */
/*      If the OVERVIEW_FILE metadata is requested, we intercept the    */
/*      request in order to replace ":::BASE:::" with the path to       */
/*      the physical file - if available.  This is primarily for the    */
/*      purpose of managing subdataset overview filenames as being      */
/*      relative to the physical file the subdataset comes              */
/*      from. (#3287).                                                  */
/* -------------------------------------------------------------------- */
    else if( pszDomain != nullptr
             && EQUAL(pszDomain,"OVERVIEWS")
             && EQUAL(pszName,"OVERVIEW_FILE") )
    {
        const char *pszOverviewFile =
            GDALDataset::GetMetadataItem( pszName, pszDomain );

        if( pszOverviewFile == nullptr
            || !STARTS_WITH_CI(pszOverviewFile, ":::BASE:::") )
            return pszOverviewFile;

        CPLString osPath;

        if( strlen(GetPhysicalFilename()) > 0 )
            osPath = CPLGetPath(GetPhysicalFilename());
        else
            osPath = CPLGetPath(GetDescription());

        return CPLFormFilename( osPath, pszOverviewFile + 10, nullptr );
    }

/* -------------------------------------------------------------------- */
/*      Everything else is a pass through.                              */
/* -------------------------------------------------------------------- */

    return GDALDataset::GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALPamDataset::GetMetadata( const char *pszDomain )

{
    // if( pszDomain == nullptr || !EQUAL(pszDomain,"ProxyOverviewRequest") )
    return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                             TryLoadAux()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALPamDataset::TryLoadAux(char **papszSiblingFiles)

{
/* -------------------------------------------------------------------- */
/*      Initialize PAM.                                                 */
/* -------------------------------------------------------------------- */
    PamInitialize();

    if( psPam == nullptr || (nPamFlags & GPF_DISABLED) != 0 )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      What is the name of the physical file we are referencing?       */
/*      We allow an override via the psPam->pszPhysicalFile item.       */
/* -------------------------------------------------------------------- */
    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr )
        pszPhysicalFile = GetDescription();

    if( strlen(pszPhysicalFile) == 0 )
        return CE_None;

    if( papszSiblingFiles && GDALCanReliablyUseSiblingFileList(pszPhysicalFile) )
    {
        CPLString osAuxFilename = CPLResetExtension( pszPhysicalFile, "aux");
        int iSibling = CSLFindString( papszSiblingFiles,
                                      CPLGetFilename(osAuxFilename) );
        if( iSibling < 0 )
        {
            osAuxFilename = pszPhysicalFile;
            osAuxFilename += ".aux";
            iSibling = CSLFindString( papszSiblingFiles,
                                      CPLGetFilename(osAuxFilename) );
            if( iSibling < 0 )
                return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to open .aux file.                                          */
/* -------------------------------------------------------------------- */
    GDALDataset *poAuxDS = GDALFindAssociatedAuxFile( pszPhysicalFile,
                                                      GA_ReadOnly, this );

    if( poAuxDS == nullptr )
        return CE_None;

    psPam->osAuxFilename = poAuxDS->GetDescription();

/* -------------------------------------------------------------------- */
/*      Do we have an SRS on the aux file?                              */
/* -------------------------------------------------------------------- */
    if( strlen(poAuxDS->GetProjectionRef()) > 0 )
        GDALPamDataset::SetProjection( poAuxDS->GetProjectionRef() );

/* -------------------------------------------------------------------- */
/*      Geotransform.                                                   */
/* -------------------------------------------------------------------- */
    if( poAuxDS->GetGeoTransform( psPam->adfGeoTransform ) == CE_None )
        psPam->bHaveGeoTransform = TRUE;

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( poAuxDS->GetGCPCount() > 0 )
    {
        psPam->nGCPCount = poAuxDS->GetGCPCount();
        psPam->pasGCPList = GDALDuplicateGCPs( psPam->nGCPCount,
                                               poAuxDS->GetGCPs() );
    }

/* -------------------------------------------------------------------- */
/*      Apply metadata. We likely ought to be merging this in rather    */
/*      than overwriting everything that was there.                     */
/* -------------------------------------------------------------------- */
    char **papszMD = poAuxDS->GetMetadata();
    if( CSLCount(papszMD) > 0 )
    {
        char **papszMerged =
            CSLMerge( CSLDuplicate(GetMetadata()), papszMD );
        GDALPamDataset::SetMetadata( papszMerged );
        CSLDestroy( papszMerged );
    }

    papszMD = poAuxDS->GetMetadata("XFORMS");
    if( CSLCount(papszMD) > 0 )
    {
        char **papszMerged =
            CSLMerge( CSLDuplicate(GetMetadata("XFORMS")), papszMD );
        GDALPamDataset::SetMetadata( papszMerged, "XFORMS" );
        CSLDestroy( papszMerged );
    }

/* ==================================================================== */
/*      Process bands.                                                  */
/* ==================================================================== */
    for( int iBand = 0; iBand < poAuxDS->GetRasterCount(); iBand++ )
    {
        if( iBand >= GetRasterCount() )
            break;

        GDALRasterBand * const poAuxBand = poAuxDS->GetRasterBand( iBand+1 );
        GDALRasterBand * const poBand = GetRasterBand( iBand+1 );

        papszMD = poAuxBand->GetMetadata();
        if( CSLCount(papszMD) > 0 )
        {
            char **papszMerged =
                CSLMerge( CSLDuplicate(poBand->GetMetadata()), papszMD );
            poBand->SetMetadata( papszMerged );
            CSLDestroy( papszMerged );
        }

        if( strlen(poAuxBand->GetDescription()) > 0 )
            poBand->SetDescription( poAuxBand->GetDescription() );

        if( poAuxBand->GetCategoryNames() != nullptr )
            poBand->SetCategoryNames( poAuxBand->GetCategoryNames() );

        if( poAuxBand->GetColorTable() != nullptr
            && poBand->GetColorTable() == nullptr )
            poBand->SetColorTable( poAuxBand->GetColorTable() );

        // histograms?
        double dfMin = 0.0;
        double dfMax = 0.0;
        int nBuckets = 0;
        GUIntBig *panHistogram=nullptr;

        if( poAuxBand->GetDefaultHistogram( &dfMin, &dfMax,
                                            &nBuckets, &panHistogram,
                                            FALSE, nullptr, nullptr ) == CE_None )
        {
            poBand->SetDefaultHistogram( dfMin, dfMax, nBuckets,
                                         panHistogram );
            CPLFree( panHistogram );
        }

        // RAT
        if( poAuxBand->GetDefaultRAT() != nullptr )
            poBand->SetDefaultRAT( poAuxBand->GetDefaultRAT() );

        // NoData
        int bSuccess = FALSE;
        const double dfNoDataValue = poAuxBand->GetNoDataValue( &bSuccess );
        if( bSuccess )
            poBand->SetNoDataValue( dfNoDataValue );
    }

    GDALClose( poAuxDS );

/* -------------------------------------------------------------------- */
/*      Mark PAM info as clean.                                         */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_Failure;
}
//! @endcond

/************************************************************************/
/*                        _GetProjectionRef()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
const char *GDALPamDataset::_GetProjectionRef()
{
    return GetProjectionRefFromSpatialRef(GDALPamDataset::GetSpatialRef());
}

/************************************************************************/
/*                          _SetProjection()                            */
/************************************************************************/

CPLErr GDALPamDataset::_SetProjection( const char *pszProjection )
{
    if( pszProjection && pszProjection[0] != '\0' )
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oSRS.importFromWkt(pszProjection) != OGRERR_NONE )
        {
            return CE_Failure;
        }
        return GDALPamDataset::SetSpatialRef(&oSRS);
    }
    else
    {
        return GDALPamDataset::SetSpatialRef(nullptr);
    }
}

/************************************************************************/
/*                        _GetGCPProjection()                           */
/************************************************************************/

const char *GDALPamDataset::_GetGCPProjection()
{
    return GetGCPProjectionFromSpatialRef(GDALPamDataset::GetGCPSpatialRef());
}

/************************************************************************/
/*                            _SetGCPs()                                */
/************************************************************************/

CPLErr GDALPamDataset::_SetGCPs( int nGCPCount,
                             const GDAL_GCP *pasGCPList,
                             const char *pszGCPProjection )

{
    if( pszGCPProjection && pszGCPProjection[0] != '\0' )
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( oSRS.importFromWkt(pszGCPProjection) != OGRERR_NONE )
        {
            return CE_Failure;
        }
        return GDALPamDataset::SetGCPs(nGCPCount, pasGCPList, &oSRS);
    }
    else
    {
        return GDALPamDataset::SetGCPs(nGCPCount, pasGCPList,
                       static_cast<const OGRSpatialReference*>(nullptr));
    }
}
//! @endcond

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void GDALPamDataset::ClearStatistics()
{
    PamInitialize();
    if( !psPam )
        return;
    for( int i = 1; i <= nBands; ++i )
    {
        bool bChanged = false;
        GDALRasterBand* poBand = GetRasterBand(i);
        char** papszOldMD = poBand->GetMetadata();
        char** papszNewMD = nullptr;
        for( char** papszIter = papszOldMD; papszIter && papszIter[0]; ++papszIter )
        {
            if( STARTS_WITH_CI(*papszIter, "STATISTICS_") )
            {
                MarkPamDirty();
                bChanged = true;
            }
            else
            {
                papszNewMD = CSLAddString(papszNewMD, *papszIter);
            }
        }
        if( bChanged )
        {
            poBand->SetMetadata(papszNewMD);
        }
        CSLDestroy(papszNewMD);
    }

    GDALDataset::ClearStatistics();
}
