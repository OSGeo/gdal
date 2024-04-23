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
 * For GDAL < 3.5, setting this option to OFF may have unwanted side-effects on
 * drivers that rely on PAM functionality.
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
 *
 * In some situations where a derived dataset (e.g. used by
 * GDALMDArray::AsClassicDataset()) is linked to a physical file, the name of
 * the derived dataset is set with the SetDerivedSubdatasetName() method.
 *
 * \code
 *      poDS->SetDescription( poOpenInfo->pszFilename );
 *      poDS->SetPhysicalFilename( poDS->pszFilename );
 *      poDS->SetDerivedDatasetName( osDerivedDatasetName );
 *
 *      poDS->TryLoadXML();
 * \endcode
 */
class GDALPamDataset;

GDALPamDataset::GDALPamDataset()
{
    SetMOFlags(GetMOFlags() | GMO_PAM_CLASS);
}

/************************************************************************/
/*                          ~GDALPamDataset()                           */
/************************************************************************/

GDALPamDataset::~GDALPamDataset()

{
    if (IsMarkedSuppressOnClose())
    {
        if (psPam && psPam->pszPamFilename != nullptr)
            VSIUnlink(psPam->pszPamFilename);
    }
    else if (nPamFlags & GPF_DIRTY)
    {
        CPLDebug("GDALPamDataset", "In destructor with dirty metadata.");
        GDALPamDataset::TrySaveXML();
    }

    PamClear();
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

CPLErr GDALPamDataset::FlushCache(bool bAtClosing)

{
    CPLErr eErr = GDALDataset::FlushCache(bAtClosing);
    if (nPamFlags & GPF_DIRTY)
    {
        if (TrySaveXML() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                            MarkPamDirty()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALPamDataset::MarkPamDirty()
{
    if ((nPamFlags & GPF_DIRTY) == 0 &&
        CPLTestBool(CPLGetConfigOption("GDAL_PAM_ENABLE_MARK_DIRTY", "YES")))
    {
        nPamFlags |= GPF_DIRTY;
    }
}

// @endcond

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLXMLNode *GDALPamDataset::SerializeToXML(const char *pszUnused)

{
    if (psPam == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Setup root node and attributes.                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset");

    /* -------------------------------------------------------------------- */
    /*      SRS                                                             */
    /* -------------------------------------------------------------------- */
    if (psPam->poSRS && !psPam->poSRS->IsEmpty())
    {
        char *pszWKT = nullptr;
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            if (psPam->poSRS->exportToWkt(&pszWKT) != OGRERR_NONE)
            {
                CPLFree(pszWKT);
                pszWKT = nullptr;
                const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
                psPam->poSRS->exportToWkt(&pszWKT, apszOptions);
            }
        }
        CPLXMLNode *psSRSNode =
            CPLCreateXMLElementAndValue(psDSTree, "SRS", pszWKT);
        CPLFree(pszWKT);
        const auto &mapping = psPam->poSRS->GetDataAxisToSRSAxisMapping();
        CPLString osMapping;
        for (size_t i = 0; i < mapping.size(); ++i)
        {
            if (!osMapping.empty())
                osMapping += ",";
            osMapping += CPLSPrintf("%d", mapping[i]);
        }
        CPLAddXMLAttributeAndValue(psSRSNode, "dataAxisToSRSAxisMapping",
                                   osMapping.c_str());

        const double dfCoordinateEpoch = psPam->poSRS->GetCoordinateEpoch();
        if (dfCoordinateEpoch > 0)
        {
            std::string osCoordinateEpoch = CPLSPrintf("%f", dfCoordinateEpoch);
            if (osCoordinateEpoch.find('.') != std::string::npos)
            {
                while (osCoordinateEpoch.back() == '0')
                    osCoordinateEpoch.resize(osCoordinateEpoch.size() - 1);
            }
            CPLAddXMLAttributeAndValue(psSRSNode, "coordinateEpoch",
                                       osCoordinateEpoch.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      GeoTransform.                                                   */
    /* -------------------------------------------------------------------- */
    if (psPam->bHaveGeoTransform)
    {
        CPLString oFmt;
        oFmt.Printf("%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                    psPam->adfGeoTransform[0], psPam->adfGeoTransform[1],
                    psPam->adfGeoTransform[2], psPam->adfGeoTransform[3],
                    psPam->adfGeoTransform[4], psPam->adfGeoTransform[5]);
        CPLSetXMLValue(psDSTree, "GeoTransform", oFmt);
    }

    /* -------------------------------------------------------------------- */
    /*      Metadata.                                                       */
    /* -------------------------------------------------------------------- */
    if (psPam->bHasMetadata)
    {
        CPLXMLNode *psMD = oMDMD.Serialize();
        if (psMD != nullptr)
        {
            CPLAddXMLChild(psDSTree, psMD);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      GCPs                                                            */
    /* -------------------------------------------------------------------- */
    if (!psPam->asGCPs.empty())
    {
        GDALSerializeGCPListToXML(psDSTree, psPam->asGCPs, psPam->poGCP_SRS);
    }

    /* -------------------------------------------------------------------- */
    /*      Process bands.                                                  */
    /* -------------------------------------------------------------------- */

    // Find last child
    CPLXMLNode *psLastChild = psDSTree->psChild;
    for (; psLastChild != nullptr && psLastChild->psNext;
         psLastChild = psLastChild->psNext)
    {
    }

    for (int iBand = 0; iBand < GetRasterCount(); iBand++)
    {
        GDALRasterBand *const poBand = GetRasterBand(iBand + 1);

        if (poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS))
            continue;

        CPLXMLNode *const psBandTree =
            cpl::down_cast<GDALPamRasterBand *>(poBand)->SerializeToXML(
                pszUnused);

        if (psBandTree != nullptr)
        {
            if (psLastChild == nullptr)
            {
                CPLAddXMLChild(psDSTree, psBandTree);
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
    if (psDSTree->psChild == nullptr)
    {
        CPLDestroyXMLNode(psDSTree);
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
    const char *const pszPamDefault = "YES";
#else
    const char *const pszPamDefault = "NO";
#endif

    if (psPam)
        return;

    if (!CPLTestBool(CPLGetConfigOption("GDAL_PAM_ENABLED", pszPamDefault)))
    {
        CPLDebug("GDAL", "PAM is disabled");
        nPamFlags |= GPF_DISABLED;
    }

    /* ERO 2011/04/13 : GPF_AUXMODE seems to be unimplemented */
    if (EQUAL(CPLGetConfigOption("GDAL_PAM_MODE", "PAM"), "AUX"))
        nPamFlags |= GPF_AUXMODE;

    psPam = new GDALDatasetPamInfo;
    for (int iBand = 0; iBand < GetRasterCount(); iBand++)
    {
        GDALRasterBand *poBand = GetRasterBand(iBand + 1);

        if (poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS))
            continue;

        cpl::down_cast<GDALPamRasterBand *>(poBand)->PamInitialize();
    }
}

/************************************************************************/
/*                              PamClear()                              */
/************************************************************************/

void GDALPamDataset::PamClear()

{
    if (psPam)
    {
        CPLFree(psPam->pszPamFilename);
        if (psPam->poSRS)
            psPam->poSRS->Release();
        if (psPam->poGCP_SRS)
            psPam->poGCP_SRS->Release();

        delete psPam;
        psPam = nullptr;
    }
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamDataset::XMLInit(const CPLXMLNode *psTree, const char *pszUnused)

{
    /* -------------------------------------------------------------------- */
    /*      Check for an SRS node.                                          */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psSRSNode = CPLGetXMLNode(psTree, "SRS"))
    {
        if (psPam->poSRS)
            psPam->poSRS->Release();
        psPam->poSRS = new OGRSpatialReference();
        psPam->poSRS->SetFromUserInput(
            CPLGetXMLValue(psSRSNode, nullptr, ""),
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS);
        const char *pszMapping =
            CPLGetXMLValue(psSRSNode, "dataAxisToSRSAxisMapping", nullptr);
        if (pszMapping)
        {
            char **papszTokens =
                CSLTokenizeStringComplex(pszMapping, ",", FALSE, FALSE);
            std::vector<int> anMapping;
            for (int i = 0; papszTokens && papszTokens[i]; i++)
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

        const char *pszCoordinateEpoch =
            CPLGetXMLValue(psSRSNode, "coordinateEpoch", nullptr);
        if (pszCoordinateEpoch)
            psPam->poSRS->SetCoordinateEpoch(CPLAtof(pszCoordinateEpoch));
    }

    /* -------------------------------------------------------------------- */
    /*      Check for a GeoTransform node.                                  */
    /* -------------------------------------------------------------------- */
    const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
    if (strlen(pszGT) > 0)
    {
        const CPLStringList aosTokens(
            CSLTokenizeStringComplex(pszGT, ",", FALSE, FALSE));
        if (aosTokens.size() != 6)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "GeoTransform node does not have expected six values.");
        }
        else
        {
            for (int iTA = 0; iTA < 6; iTA++)
                psPam->adfGeoTransform[iTA] = CPLAtof(aosTokens[iTA]);
            psPam->bHaveGeoTransform = TRUE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check for GCPs.                                                 */
    /* -------------------------------------------------------------------- */
    if (const CPLXMLNode *psGCPList = CPLGetXMLNode(psTree, "GCPList"))
    {
        if (psPam->poGCP_SRS)
            psPam->poGCP_SRS->Release();
        psPam->poGCP_SRS = nullptr;

        // Make sure any previous GCPs, perhaps from an .aux file, are cleared
        // if we have new ones.
        psPam->asGCPs.clear();
        GDALDeserializeGCPListFromXML(psGCPList, psPam->asGCPs,
                                      &(psPam->poGCP_SRS));
    }

    /* -------------------------------------------------------------------- */
    /*      Apply any dataset level metadata.                               */
    /* -------------------------------------------------------------------- */
    if (oMDMD.XMLInit(psTree, TRUE))
    {
        psPam->bHasMetadata = TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      Try loading ESRI xml encoded GeodataXform.                      */
    /* -------------------------------------------------------------------- */
    {
        // previously we only tried to load GeodataXform if we didn't already
        // encounter a valid SRS at this stage. But in some cases a PAMDataset
        // may have both a SRS child element AND a GeodataXform with a SpatialReference
        // child element. In this case we should prioritize the GeodataXform
        // over the root PAMDataset SRS node.

        // ArcGIS 9.3: GeodataXform as a root element
        const CPLXMLNode *psGeodataXform =
            CPLGetXMLNode(psTree, "=GeodataXform");
        CPLXMLTreeCloser oTreeValueAsXML(nullptr);
        if (psGeodataXform != nullptr)
        {
            char *apszMD[2];
            apszMD[0] = CPLSerializeXMLTree(psGeodataXform);
            apszMD[1] = nullptr;
            oMDMD.SetMetadata(apszMD, "xml:ESRI");
            CPLFree(apszMD[0]);
        }
        else
        {
            // ArcGIS 10: GeodataXform as content of xml:ESRI metadata domain.
            char **papszXML = oMDMD.GetMetadata("xml:ESRI");
            if (CSLCount(papszXML) == 1)
            {
                oTreeValueAsXML.reset(CPLParseXMLString(papszXML[0]));
                if (oTreeValueAsXML)
                    psGeodataXform =
                        CPLGetXMLNode(oTreeValueAsXML.get(), "=GeodataXform");
            }
        }

        if (psGeodataXform)
        {
            const char *pszESRI_WKT =
                CPLGetXMLValue(psGeodataXform, "SpatialReference.WKT", nullptr);
            if (pszESRI_WKT)
            {
                auto poSRS = std::make_unique<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->importFromWkt(pszESRI_WKT) != OGRERR_NONE)
                {
                    poSRS.reset();
                }
                delete psPam->poSRS;
                psPam->poSRS = poSRS.release();
            }

            // Parse GCPs
            const CPLXMLNode *psSourceGCPS =
                CPLGetXMLNode(psGeodataXform, "SourceGCPs");
            const CPLXMLNode *psTargetGCPs =
                CPLGetXMLNode(psGeodataXform, "TargetGCPs");
            const CPLXMLNode *psCoeffX =
                CPLGetXMLNode(psGeodataXform, "CoeffX");
            const CPLXMLNode *psCoeffY =
                CPLGetXMLNode(psGeodataXform, "CoeffY");
            if (psSourceGCPS && psTargetGCPs && !psPam->bHaveGeoTransform)
            {
                std::vector<double> adfSource;
                std::vector<double> adfTarget;
                bool ySourceAllNegative = true;
                for (auto psIter = psSourceGCPS->psChild; psIter;
                     psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "Double") == 0)
                    {
                        adfSource.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                        if ((adfSource.size() % 2) == 0 && adfSource.back() > 0)
                            ySourceAllNegative = false;
                    }
                }
                for (auto psIter = psTargetGCPs->psChild; psIter;
                     psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "Double") == 0)
                    {
                        adfTarget.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                    }
                }
                if (!adfSource.empty() &&
                    adfSource.size() == adfTarget.size() &&
                    (adfSource.size() % 2) == 0)
                {
                    std::vector<gdal::GCP> asGCPs;
                    for (size_t i = 0; i + 1 < adfSource.size(); i += 2)
                    {
                        asGCPs.emplace_back("", "",
                                            /* pixel = */ adfSource[i],
                                            /* line = */
                                            ySourceAllNegative
                                                ? -adfSource[i + 1]
                                                : adfSource[i + 1],
                                            /* X = */ adfTarget[i],
                                            /* Y = */ adfTarget[i + 1]);
                    }
                    GDALPamDataset::SetGCPs(static_cast<int>(asGCPs.size()),
                                            gdal::GCP::c_ptr(asGCPs),
                                            psPam->poSRS);
                    delete psPam->poSRS;
                    psPam->poSRS = nullptr;
                }
            }
            else if (psCoeffX && psCoeffY && !psPam->bHaveGeoTransform &&
                     EQUAL(
                         CPLGetXMLValue(psGeodataXform, "PolynomialOrder", ""),
                         "1"))
            {
                std::vector<double> adfCoeffX;
                std::vector<double> adfCoeffY;
                for (auto psIter = psCoeffX->psChild; psIter;
                     psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "Double") == 0)
                    {
                        adfCoeffX.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                    }
                }
                for (auto psIter = psCoeffY->psChild; psIter;
                     psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "Double") == 0)
                    {
                        adfCoeffY.push_back(
                            CPLAtof(CPLGetXMLValue(psIter, nullptr, "0")));
                    }
                }
                if (adfCoeffX.size() == 3 && adfCoeffY.size() == 3)
                {
                    psPam->adfGeoTransform[0] = adfCoeffX[0];
                    psPam->adfGeoTransform[1] = adfCoeffX[1];
                    // Looking at the example of https://github.com/qgis/QGIS/issues/53125#issuecomment-1567650082
                    // when comparing the .pgwx world file and .png.aux.xml file,
                    // it appears that the sign of the coefficients for the line
                    // terms must be negated (which is a bit in line with the
                    // negation of dfGCPLine in the above GCP case)
                    psPam->adfGeoTransform[2] = -adfCoeffX[2];
                    psPam->adfGeoTransform[3] = adfCoeffY[0];
                    psPam->adfGeoTransform[4] = adfCoeffY[1];
                    psPam->adfGeoTransform[5] = -adfCoeffY[2];

                    // Looking at the example of https://github.com/qgis/QGIS/issues/53125#issuecomment-1567650082
                    // when comparing the .pgwx world file and .png.aux.xml file,
                    // one can see that they have the same origin, so knowing
                    // that world file uses a center-of-pixel convention,
                    // correct from center of pixel to top left of pixel
                    psPam->adfGeoTransform[0] -=
                        0.5 * psPam->adfGeoTransform[1];
                    psPam->adfGeoTransform[0] -=
                        0.5 * psPam->adfGeoTransform[2];
                    psPam->adfGeoTransform[3] -=
                        0.5 * psPam->adfGeoTransform[4];
                    psPam->adfGeoTransform[3] -=
                        0.5 * psPam->adfGeoTransform[5];

                    psPam->bHaveGeoTransform = TRUE;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Process bands.                                                  */
    /* -------------------------------------------------------------------- */
    for (const CPLXMLNode *psBandTree = psTree->psChild; psBandTree;
         psBandTree = psBandTree->psNext)
    {
        if (psBandTree->eType != CXT_Element ||
            !EQUAL(psBandTree->pszValue, "PAMRasterBand"))
            continue;

        const int nBand = atoi(CPLGetXMLValue(psBandTree, "band", "0"));

        if (nBand < 1 || nBand > GetRasterCount())
            continue;

        GDALRasterBand *poBand = GetRasterBand(nBand);

        if (poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS))
            continue;

        GDALPamRasterBand *poPamBand =
            cpl::down_cast<GDALPamRasterBand *>(GetRasterBand(nBand));

        poPamBand->XMLInit(psBandTree, pszUnused);
    }

    /* -------------------------------------------------------------------- */
    /*      Preserve Array information.                                     */
    /* -------------------------------------------------------------------- */
    for (const CPLXMLNode *psIter = psTree->psChild; psIter;
         psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "Array") == 0)
        {
            CPLXMLNode sArrayTmp = *psIter;
            sArrayTmp.psNext = nullptr;
            psPam->m_apoOtherNodes.emplace_back(
                CPLXMLTreeCloser(CPLCloneXMLTree(&sArrayTmp)));
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

void GDALPamDataset::SetPhysicalFilename(const char *pszFilename)

{
    PamInitialize();

    if (psPam)
        psPam->osPhysicalFilename = pszFilename;
}

/************************************************************************/
/*                        GetPhysicalFilename()                         */
/************************************************************************/

const char *GDALPamDataset::GetPhysicalFilename()

{
    PamInitialize();

    if (psPam)
        return psPam->osPhysicalFilename;

    return "";
}

/************************************************************************/
/*                         SetSubdatasetName()                          */
/************************************************************************/

/* Mutually exclusive with SetDerivedDatasetName() */
void GDALPamDataset::SetSubdatasetName(const char *pszSubdataset)

{
    PamInitialize();

    if (psPam)
        psPam->osSubdatasetName = pszSubdataset;
}

/************************************************************************/
/*                        SetDerivedDatasetName()                        */
/************************************************************************/

/* Mutually exclusive with SetSubdatasetName() */
void GDALPamDataset::SetDerivedDatasetName(const char *pszDerivedDataset)

{
    PamInitialize();

    if (psPam)
        psPam->osDerivedDatasetName = pszDerivedDataset;
}

/************************************************************************/
/*                         GetSubdatasetName()                          */
/************************************************************************/

const char *GDALPamDataset::GetSubdatasetName()

{
    PamInitialize();

    if (psPam)
        return psPam->osSubdatasetName;

    return "";
}

/************************************************************************/
/*                          BuildPamFilename()                          */
/************************************************************************/

const char *GDALPamDataset::BuildPamFilename()

{
    if (psPam == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      What is the name of the physical file we are referencing?       */
    /*      We allow an override via the psPam->pszPhysicalFile item.       */
    /* -------------------------------------------------------------------- */
    if (psPam->pszPamFilename != nullptr)
        return psPam->pszPamFilename;

    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if (strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr)
        pszPhysicalFile = GetDescription();

    if (strlen(pszPhysicalFile) == 0)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Try a proxy lookup, otherwise just add .aux.xml.                */
    /* -------------------------------------------------------------------- */
    const char *pszProxyPam = PamGetProxy(pszPhysicalFile);
    if (pszProxyPam != nullptr)
        psPam->pszPamFilename = CPLStrdup(pszProxyPam);
    else
    {
        if (!GDALCanFileAcceptSidecarFile(pszPhysicalFile))
            return nullptr;
        psPam->pszPamFilename =
            static_cast<char *>(CPLMalloc(strlen(pszPhysicalFile) + 10));
        strcpy(psPam->pszPamFilename, pszPhysicalFile);
        strcat(psPam->pszPamFilename, ".aux.xml");
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

    if (strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr)
        pszPhysicalFile = GetDescription();

    size_t nLenPhysicalFile = strlen(pszPhysicalFile);
    int bIsSiblingPamFile =
        strncmp(psPam->pszPamFilename, pszPhysicalFile, nLenPhysicalFile) ==
            0 &&
        strcmp(psPam->pszPamFilename + nLenPhysicalFile, ".aux.xml") == 0;

    return bIsSiblingPamFile;
}

/************************************************************************/
/*                             TryLoadXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadXML(char **papszSiblingFiles)

{
    PamInitialize();

    if (psPam == nullptr || (nPamFlags & GPF_DISABLED) != 0)
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
    if (!BuildPamFilename())
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      In case the PAM filename is a .aux.xml file next to the         */
    /*      physical file and we have a siblings list, then we can skip     */
    /*      stat'ing the filesystem.                                        */
    /* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;
    CPLXMLNode *psTree = nullptr;

    if (papszSiblingFiles != nullptr && IsPamFilenameAPotentialSiblingFile() &&
        GDALCanReliablyUseSiblingFileList(psPam->pszPamFilename))
    {
        const int iSibling = CSLFindString(
            papszSiblingFiles, CPLGetFilename(psPam->pszPamFilename));
        if (iSibling >= 0)
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            psTree = CPLParseXMLFile(psPam->pszPamFilename);
        }
    }
    else if (VSIStatExL(psPam->pszPamFilename, &sStatBuf,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
             VSI_ISREG(sStatBuf.st_mode))
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        psTree = CPLParseXMLFile(psPam->pszPamFilename);
    }

    /* -------------------------------------------------------------------- */
    /*      If we are looking for a subdataset, search for its subtree now. */
    /* -------------------------------------------------------------------- */
    if (psTree)
    {
        std::string osSubNode;
        std::string osSubNodeValue;
        if (!psPam->osSubdatasetName.empty())
        {
            osSubNode = "Subdataset";
            osSubNodeValue = psPam->osSubdatasetName;
        }
        else if (!psPam->osDerivedDatasetName.empty())
        {
            osSubNode = "DerivedDataset";
            osSubNodeValue = psPam->osDerivedDatasetName;
        }
        if (!osSubNode.empty())
        {
            CPLXMLNode *psSubTree = psTree->psChild;

            for (; psSubTree != nullptr; psSubTree = psSubTree->psNext)
            {
                if (psSubTree->eType != CXT_Element ||
                    !EQUAL(psSubTree->pszValue, osSubNode.c_str()))
                    continue;

                if (!EQUAL(CPLGetXMLValue(psSubTree, "name", ""),
                           osSubNodeValue.c_str()))
                    continue;

                psSubTree = CPLGetXMLNode(psSubTree, "PAMDataset");
                break;
            }

            if (psSubTree != nullptr)
                psSubTree = CPLCloneXMLTree(psSubTree);

            CPLDestroyXMLNode(psTree);
            psTree = psSubTree;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If we fail, try .aux.                                           */
    /* -------------------------------------------------------------------- */
    if (psTree == nullptr)
        return TryLoadAux(papszSiblingFiles);

    /* -------------------------------------------------------------------- */
    /*      Initialize ourselves from this XML tree.                        */
    /* -------------------------------------------------------------------- */

    CPLString osVRTPath(CPLGetPath(psPam->pszPamFilename));
    const CPLErr eErr = XMLInit(psTree, osVRTPath);

    CPLDestroyXMLNode(psTree);

    if (eErr != CE_None)
        PamClear();

    return eErr;
}

/************************************************************************/
/*                             TrySaveXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TrySaveXML()

{
    nPamFlags &= ~GPF_DIRTY;

    if (psPam == nullptr || (nPamFlags & GPF_NOSAVE) != 0 ||
        (nPamFlags & GPF_DISABLED) != 0)
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      Make sure we know the filename we want to store in.             */
    /* -------------------------------------------------------------------- */
    if (!BuildPamFilename())
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      Build the XML representation of the auxiliary metadata.          */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psTree = SerializeToXML(nullptr);

    if (psTree == nullptr)
    {
        /* If we have unset all metadata, we have to delete the PAM file */
        CPLPushErrorHandler(CPLQuietErrorHandler);
        VSIUnlink(psPam->pszPamFilename);
        CPLPopErrorHandler();
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      If we are working with a subdataset, we need to integrate       */
    /*      the subdataset tree within the whole existing pam tree,         */
    /*      after removing any old version of the same subdataset.          */
    /* -------------------------------------------------------------------- */
    std::string osSubNode;
    std::string osSubNodeValue;
    if (!psPam->osSubdatasetName.empty())
    {
        osSubNode = "Subdataset";
        osSubNodeValue = psPam->osSubdatasetName;
    }
    else if (!psPam->osDerivedDatasetName.empty())
    {
        osSubNode = "DerivedDataset";
        osSubNodeValue = psPam->osDerivedDatasetName;
    }
    if (!osSubNode.empty())
    {
        CPLXMLNode *psOldTree = nullptr;

        VSIStatBufL sStatBuf;
        if (VSIStatExL(psPam->pszPamFilename, &sStatBuf,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
            VSI_ISREG(sStatBuf.st_mode))
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            psOldTree = CPLParseXMLFile(psPam->pszPamFilename);
        }

        if (psOldTree == nullptr)
            psOldTree = CPLCreateXMLNode(nullptr, CXT_Element, "PAMDataset");

        CPLXMLNode *psSubTree = psOldTree->psChild;
        for (/* initialized above */; psSubTree != nullptr;
             psSubTree = psSubTree->psNext)
        {
            if (psSubTree->eType != CXT_Element ||
                !EQUAL(psSubTree->pszValue, osSubNode.c_str()))
                continue;

            if (!EQUAL(CPLGetXMLValue(psSubTree, "name", ""),
                       osSubNodeValue.c_str()))
                continue;

            break;
        }

        if (psSubTree == nullptr)
        {
            psSubTree =
                CPLCreateXMLNode(psOldTree, CXT_Element, osSubNode.c_str());
            CPLCreateXMLNode(CPLCreateXMLNode(psSubTree, CXT_Attribute, "name"),
                             CXT_Text, osSubNodeValue.c_str());
        }

        CPLXMLNode *psOldPamDataset = CPLGetXMLNode(psSubTree, "PAMDataset");
        if (psOldPamDataset != nullptr)
        {
            CPLRemoveXMLChild(psSubTree, psOldPamDataset);
            CPLDestroyXMLNode(psOldPamDataset);
        }

        CPLAddXMLChild(psSubTree, psTree);
        psTree = psOldTree;
    }

    /* -------------------------------------------------------------------- */
    /*      Preserve other information.                                     */
    /* -------------------------------------------------------------------- */
    for (const auto &poOtherNode : psPam->m_apoOtherNodes)
    {
        CPLAddXMLChild(psTree, CPLCloneXMLTree(poOtherNode.get()));
    }

    /* -------------------------------------------------------------------- */
    /*      Try saving the auxiliary metadata.                               */
    /* -------------------------------------------------------------------- */

    CPLPushErrorHandler(CPLQuietErrorHandler);
    const int bSaved = CPLSerializeXMLTreeToFile(psTree, psPam->pszPamFilename);
    CPLPopErrorHandler();

    /* -------------------------------------------------------------------- */
    /*      If it fails, check if we have a proxy directory for auxiliary    */
    /*      metadata to be stored in, and try to save there.                */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if (bSaved)
        eErr = CE_None;
    else
    {
        const char *pszBasename = GetDescription();

        if (psPam->osPhysicalFilename.length() > 0)
            pszBasename = psPam->osPhysicalFilename;

        const char *pszNewPam = nullptr;
        if (PamGetProxy(pszBasename) == nullptr &&
            ((pszNewPam = PamAllocateProxy(pszBasename)) != nullptr))
        {
            CPLErrorReset();
            CPLFree(psPam->pszPamFilename);
            psPam->pszPamFilename = CPLStrdup(pszNewPam);
            eErr = TrySaveXML();
        }
        /* No way we can save into a /vsicurl resource */
        else if (!STARTS_WITH(psPam->pszPamFilename, "/vsicurl"))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unable to save auxiliary information in %s.",
                     psPam->pszPamFilename);
            eErr = CE_Warning;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    CPLDestroyXMLNode(psTree);

    return eErr;
}

/************************************************************************/
/*                             CloneInfo()                              */
/************************************************************************/

CPLErr GDALPamDataset::CloneInfo(GDALDataset *poSrcDS, int nCloneFlags)

{
    const int bOnlyIfMissing = nCloneFlags & GCIF_ONLY_IF_MISSING;
    const int nSavedMOFlags = GetMOFlags();

    PamInitialize();

    /* -------------------------------------------------------------------- */
    /*      Suppress NotImplemented error messages - mainly needed if PAM   */
    /*      disabled.                                                       */
    /* -------------------------------------------------------------------- */
    SetMOFlags(nSavedMOFlags | GMO_IGNORE_UNIMPLEMENTED);

    /* -------------------------------------------------------------------- */
    /*      GeoTransform                                                    */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_GEOTRANSFORM)
    {
        double adfGeoTransform[6] = {0.0};

        if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
        {
            double adfOldGT[6] = {0.0};

            if (!bOnlyIfMissing || GetGeoTransform(adfOldGT) != CE_None)
                SetGeoTransform(adfGeoTransform);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Projection                                                      */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_PROJECTION)
    {
        const auto poSRS = poSrcDS->GetSpatialRef();

        if (poSRS != nullptr)
        {
            if (!bOnlyIfMissing || GetSpatialRef() == nullptr)
                SetSpatialRef(poSRS);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      GCPs                                                            */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_GCPS)
    {
        if (poSrcDS->GetGCPCount() > 0)
        {
            if (!bOnlyIfMissing || GetGCPCount() == 0)
            {
                SetGCPs(poSrcDS->GetGCPCount(), poSrcDS->GetGCPs(),
                        poSrcDS->GetGCPSpatialRef());
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Metadata                                                        */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_METADATA)
    {
        for (const char *pszMDD : {"", "RPC", "json:ISIS3", "json:VICAR"})
        {
            auto papszSrcMD = poSrcDS->GetMetadata(pszMDD);
            if (papszSrcMD != nullptr)
            {
                if (!bOnlyIfMissing ||
                    CSLCount(GetMetadata(pszMDD)) != CSLCount(papszSrcMD))
                {
                    SetMetadata(papszSrcMD, pszMDD);
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Process bands.                                                  */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_PROCESS_BANDS)
    {
        for (int iBand = 0; iBand < GetRasterCount(); iBand++)
        {
            GDALRasterBand *poBand = GetRasterBand(iBand + 1);

            if (poBand == nullptr || !(poBand->GetMOFlags() & GMO_PAM_CLASS))
                continue;

            if (poSrcDS->GetRasterCount() >= iBand + 1)
            {
                cpl::down_cast<GDALPamRasterBand *>(poBand)->CloneInfo(
                    poSrcDS->GetRasterBand(iBand + 1), nCloneFlags);
            }
            else
                CPLDebug("GDALPamDataset",
                         "Skipping CloneInfo for band not in source, "
                         "this is a bit unusual!");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy masks.  These are really copied at a lower level using     */
    /*      GDALDefaultOverviews, for formats with no native mask           */
    /*      support but this is a convenient central point to put this      */
    /*      for most drivers.                                               */
    /* -------------------------------------------------------------------- */
    if (nCloneFlags & GCIF_MASK)
    {
        GDALDriver::DefaultCopyMasks(poSrcDS, this, FALSE);
    }

    /* -------------------------------------------------------------------- */
    /*      Restore MO flags.                                               */
    /* -------------------------------------------------------------------- */
    SetMOFlags(nSavedMOFlags);

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

    if (psPam && !psPam->osPhysicalFilename.empty() &&
        GDALCanReliablyUseSiblingFileList(psPam->osPhysicalFilename.c_str()) &&
        CSLFindString(papszFileList, psPam->osPhysicalFilename) == -1)
    {
        papszFileList =
            CSLInsertString(papszFileList, 0, psPam->osPhysicalFilename);
    }

    if (psPam && psPam->pszPamFilename)
    {
        int bAddPamFile = nPamFlags & GPF_DIRTY;
        if (!bAddPamFile)
        {
            VSIStatBufL sStatBuf;
            if (oOvManager.GetSiblingFiles() != nullptr &&
                IsPamFilenameAPotentialSiblingFile() &&
                GDALCanReliablyUseSiblingFileList(psPam->pszPamFilename))
            {
                bAddPamFile =
                    CSLFindString(oOvManager.GetSiblingFiles(),
                                  CPLGetFilename(psPam->pszPamFilename)) >= 0;
            }
            else
            {
                bAddPamFile = VSIStatExL(psPam->pszPamFilename, &sStatBuf,
                                         VSI_STAT_EXISTS_FLAG) == 0;
            }
        }
        if (bAddPamFile)
        {
            papszFileList = CSLAddString(papszFileList, psPam->pszPamFilename);
        }
    }

    if (psPam && !psPam->osAuxFilename.empty() &&
        GDALCanReliablyUseSiblingFileList(psPam->osAuxFilename.c_str()) &&
        CSLFindString(papszFileList, psPam->osAuxFilename) == -1)
    {
        papszFileList = CSLAddString(papszFileList, psPam->osAuxFilename);
    }
    return papszFileList;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALPamDataset::IBuildOverviews(
    const char *pszResampling, int nOverviews, const int *panOverviewList,
    int nListBands, const int *panBandList, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)

{
    /* -------------------------------------------------------------------- */
    /*      Initialize PAM.                                                 */
    /* -------------------------------------------------------------------- */
    PamInitialize();
    if (psPam == nullptr)
        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
            pfnProgress, pProgressData, papszOptions);

    /* -------------------------------------------------------------------- */
    /*      If we appear to have subdatasets and to have a physical         */
    /*      filename, use that physical filename to derive a name for a     */
    /*      new overview file.                                              */
    /* -------------------------------------------------------------------- */
    if (oOvManager.IsInitialized() && psPam->osPhysicalFilename.length() != 0)
    {
        return oOvManager.BuildOverviewsSubDataset(
            psPam->osPhysicalFilename, pszResampling, nOverviews,
            panOverviewList, nListBands, panBandList, pfnProgress,
            pProgressData, papszOptions);
    }

    return GDALDataset::IBuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
        pfnProgress, pProgressData, papszOptions);
}

//! @endcond

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *GDALPamDataset::GetSpatialRef() const

{
    if (psPam && psPam->poSRS)
        return psPam->poSRS;

    return GDALDataset::GetSpatialRef();
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GDALPamDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    PamInitialize();

    if (psPam == nullptr)
        return GDALDataset::SetSpatialRef(poSRS);

    if (psPam->poSRS)
        psPam->poSRS->Release();
    psPam->poSRS = poSRS ? poSRS->Clone() : nullptr;
    MarkPamDirty();

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::GetGeoTransform(double *padfTransform)

{
    if (psPam && psPam->bHaveGeoTransform)
    {
        memcpy(padfTransform, psPam->adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    return GDALDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetGeoTransform(double *padfTransform)

{
    PamInitialize();

    if (psPam)
    {
        MarkPamDirty();
        psPam->bHaveGeoTransform = TRUE;
        memcpy(psPam->adfGeoTransform, padfTransform, sizeof(double) * 6);
        return (CE_None);
    }

    return GDALDataset::SetGeoTransform(padfTransform);
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

    if (psPam && psPam->bHaveGeoTransform)
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
    if (psPam && !psPam->asGCPs.empty())
        return static_cast<int>(psPam->asGCPs.size());

    return GDALDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *GDALPamDataset::GetGCPSpatialRef() const

{
    if (psPam && psPam->poGCP_SRS != nullptr)
        return psPam->poGCP_SRS;

    return GDALDataset::GetGCPSpatialRef();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GDALPamDataset::GetGCPs()

{
    if (psPam && !psPam->asGCPs.empty())
        return gdal::GCP::c_ptr(psPam->asGCPs);

    return GDALDataset::GetGCPs();
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr GDALPamDataset::SetGCPs(int nGCPCount, const GDAL_GCP *pasGCPList,
                               const OGRSpatialReference *poGCP_SRS)

{
    PamInitialize();

    if (psPam)
    {
        if (psPam->poGCP_SRS)
            psPam->poGCP_SRS->Release();
        psPam->poGCP_SRS = poGCP_SRS ? poGCP_SRS->Clone() : nullptr;
        psPam->asGCPs = gdal::GCP::fromC(pasGCPList, nGCPCount);
        MarkPamDirty();

        return CE_None;
    }

    return GDALDataset::SetGCPs(nGCPCount, pasGCPList, poGCP_SRS);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadata(char **papszMetadata, const char *pszDomain)

{
    PamInitialize();

    if (psPam)
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

    return GDALDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadataItem(const char *pszName,
                                       const char *pszValue,
                                       const char *pszDomain)

{
    PamInitialize();

    if (psPam)
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

    return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALPamDataset::GetMetadataItem(const char *pszName,
                                            const char *pszDomain)

{
    /* -------------------------------------------------------------------- */
    /*      A request against the ProxyOverviewRequest is a special         */
    /*      mechanism to request an overview filename be allocated in       */
    /*      the proxy pool location.  The allocated name is saved as        */
    /*      metadata as well as being returned.                             */
    /* -------------------------------------------------------------------- */
    if (pszDomain != nullptr && EQUAL(pszDomain, "ProxyOverviewRequest"))
    {
        CPLString osPrelimOvr = GetDescription();
        osPrelimOvr += ":::OVR";

        const char *pszProxyOvrFilename = PamAllocateProxy(osPrelimOvr);
        if (pszProxyOvrFilename == nullptr)
            return nullptr;

        SetMetadataItem("OVERVIEW_FILE", pszProxyOvrFilename, "OVERVIEWS");

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
    else if (pszDomain != nullptr && EQUAL(pszDomain, "OVERVIEWS") &&
             EQUAL(pszName, "OVERVIEW_FILE"))
    {
        const char *pszOverviewFile =
            GDALDataset::GetMetadataItem(pszName, pszDomain);

        if (pszOverviewFile == nullptr ||
            !STARTS_WITH_CI(pszOverviewFile, ":::BASE:::"))
            return pszOverviewFile;

        CPLString osPath;

        if (strlen(GetPhysicalFilename()) > 0)
            osPath = CPLGetPath(GetPhysicalFilename());
        else
            osPath = CPLGetPath(GetDescription());

        return CPLFormFilename(osPath, pszOverviewFile + 10, nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Everything else is a pass through.                              */
    /* -------------------------------------------------------------------- */

    return GDALDataset::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALPamDataset::GetMetadata(const char *pszDomain)

{
    // if( pszDomain == nullptr || !EQUAL(pszDomain,"ProxyOverviewRequest") )
    return GDALDataset::GetMetadata(pszDomain);
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

    if (psPam == nullptr || (nPamFlags & GPF_DISABLED) != 0)
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      What is the name of the physical file we are referencing?       */
    /*      We allow an override via the psPam->pszPhysicalFile item.       */
    /* -------------------------------------------------------------------- */
    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if (strlen(pszPhysicalFile) == 0 && GetDescription() != nullptr)
        pszPhysicalFile = GetDescription();

    if (strlen(pszPhysicalFile) == 0)
        return CE_None;

    if (papszSiblingFiles && GDALCanReliablyUseSiblingFileList(pszPhysicalFile))
    {
        CPLString osAuxFilename = CPLResetExtension(pszPhysicalFile, "aux");
        int iSibling =
            CSLFindString(papszSiblingFiles, CPLGetFilename(osAuxFilename));
        if (iSibling < 0)
        {
            osAuxFilename = pszPhysicalFile;
            osAuxFilename += ".aux";
            iSibling =
                CSLFindString(papszSiblingFiles, CPLGetFilename(osAuxFilename));
            if (iSibling < 0)
                return CE_None;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to open .aux file.                                          */
    /* -------------------------------------------------------------------- */
    GDALDataset *poAuxDS =
        GDALFindAssociatedAuxFile(pszPhysicalFile, GA_ReadOnly, this);

    if (poAuxDS == nullptr)
        return CE_None;

    psPam->osAuxFilename = poAuxDS->GetDescription();

    /* -------------------------------------------------------------------- */
    /*      Do we have an SRS on the aux file?                              */
    /* -------------------------------------------------------------------- */
    if (strlen(poAuxDS->GetProjectionRef()) > 0)
        GDALPamDataset::SetProjection(poAuxDS->GetProjectionRef());

    /* -------------------------------------------------------------------- */
    /*      Geotransform.                                                   */
    /* -------------------------------------------------------------------- */
    if (poAuxDS->GetGeoTransform(psPam->adfGeoTransform) == CE_None)
        psPam->bHaveGeoTransform = TRUE;

    /* -------------------------------------------------------------------- */
    /*      GCPs                                                            */
    /* -------------------------------------------------------------------- */
    if (poAuxDS->GetGCPCount() > 0)
    {
        psPam->asGCPs =
            gdal::GCP::fromC(poAuxDS->GetGCPs(), poAuxDS->GetGCPCount());
    }

    /* -------------------------------------------------------------------- */
    /*      Apply metadata. We likely ought to be merging this in rather    */
    /*      than overwriting everything that was there.                     */
    /* -------------------------------------------------------------------- */
    char **papszMD = poAuxDS->GetMetadata();
    if (CSLCount(papszMD) > 0)
    {
        char **papszMerged = CSLMerge(CSLDuplicate(GetMetadata()), papszMD);
        GDALPamDataset::SetMetadata(papszMerged);
        CSLDestroy(papszMerged);
    }

    papszMD = poAuxDS->GetMetadata("XFORMS");
    if (CSLCount(papszMD) > 0)
    {
        char **papszMerged =
            CSLMerge(CSLDuplicate(GetMetadata("XFORMS")), papszMD);
        GDALPamDataset::SetMetadata(papszMerged, "XFORMS");
        CSLDestroy(papszMerged);
    }

    /* ==================================================================== */
    /*      Process bands.                                                  */
    /* ==================================================================== */
    for (int iBand = 0; iBand < poAuxDS->GetRasterCount(); iBand++)
    {
        if (iBand >= GetRasterCount())
            break;

        GDALRasterBand *const poAuxBand = poAuxDS->GetRasterBand(iBand + 1);
        GDALRasterBand *const poBand = GetRasterBand(iBand + 1);

        papszMD = poAuxBand->GetMetadata();
        if (CSLCount(papszMD) > 0)
        {
            char **papszMerged =
                CSLMerge(CSLDuplicate(poBand->GetMetadata()), papszMD);
            poBand->SetMetadata(papszMerged);
            CSLDestroy(papszMerged);
        }

        if (strlen(poAuxBand->GetDescription()) > 0)
            poBand->SetDescription(poAuxBand->GetDescription());

        if (poAuxBand->GetCategoryNames() != nullptr)
            poBand->SetCategoryNames(poAuxBand->GetCategoryNames());

        if (poAuxBand->GetColorTable() != nullptr &&
            poBand->GetColorTable() == nullptr)
            poBand->SetColorTable(poAuxBand->GetColorTable());

        // histograms?
        double dfMin = 0.0;
        double dfMax = 0.0;
        int nBuckets = 0;
        GUIntBig *panHistogram = nullptr;

        if (poAuxBand->GetDefaultHistogram(&dfMin, &dfMax, &nBuckets,
                                           &panHistogram, FALSE, nullptr,
                                           nullptr) == CE_None)
        {
            poBand->SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);
            CPLFree(panHistogram);
        }

        // RAT
        if (poAuxBand->GetDefaultRAT() != nullptr)
            poBand->SetDefaultRAT(poAuxBand->GetDefaultRAT());

        // NoData
        int bSuccess = FALSE;
        const double dfNoDataValue = poAuxBand->GetNoDataValue(&bSuccess);
        if (bSuccess)
            poBand->SetNoDataValue(dfNoDataValue);
    }

    GDALClose(poAuxDS);

    /* -------------------------------------------------------------------- */
    /*      Mark PAM info as clean.                                         */
    /* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_Failure;
}

//! @endcond

/************************************************************************/
/*                          ClearStatistics()                           */
/************************************************************************/

void GDALPamDataset::ClearStatistics()
{
    PamInitialize();
    if (!psPam)
        return;
    for (int i = 1; i <= nBands; ++i)
    {
        bool bChanged = false;
        GDALRasterBand *poBand = GetRasterBand(i);
        CPLStringList aosNewMD;
        for (const char *pszStr :
             cpl::Iterate(CSLConstList(poBand->GetMetadata())))
        {
            if (STARTS_WITH_CI(pszStr, "STATISTICS_"))
            {
                MarkPamDirty();
                bChanged = true;
            }
            else
            {
                aosNewMD.AddString(pszStr);
            }
        }
        if (bChanged)
        {
            poBand->SetMetadata(aosNewMD.List());
        }
    }

    GDALDataset::ClearStatistics();
}
