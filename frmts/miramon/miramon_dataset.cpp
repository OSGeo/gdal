/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRDataset class: responsible for generating the
 *           main dataset or the subdatasets as needed.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "gdal_frmts.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                GDALRegister_MiraMon()                                */
/************************************************************************/
void GDALRegister_MiraMon()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MiraMon Raster Images");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/miramon.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                            MMRDataset()                              */
/************************************************************************/

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRRel.
    auto pMMfRel = std::make_unique<MMRRel>(poOpenInfo->pszFilename, true);
    if (!pMMfRel->IsValid())
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, probably it's not a MiraMon file.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    if (pMMfRel->GetNBands() == 0)
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, it has zero usable bands.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    m_pMMRRel = std::move(pMMfRel);

    // General Dataset information available
    nRasterXSize = m_pMMRRel->GetColumnsNumberFromREL();
    nRasterYSize = m_pMMRRel->GetRowsNumberFromREL();
    ReadProjection();
    nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (m_nNSubdataSets)
    {
        CreateSubdatasetsFromBands();
        // Fills adfGeoTransform if documented
        UpdateGeoTransform();
    }
    else
    {
        if (!CreateRasterBands())
            return;

        // GeoTransform of a subdataset is always the same than the first band
        if (m_pMMRRel->GetNBands() >= 1)
        {
            MMRBand *poBand = m_pMMRRel->GetBand(m_pMMRRel->GetNBands() - 1);
            if (poBand)
                m_gt = poBand->m_gt;
        }
    }

    // Make sure we don't try to do any pam stuff with this dataset.
    nPamFlags |= GPF_NOSAVE;

    // We have a valid DataSet.
    m_bIsValid = true;
}

/************************************************************************/
/*                           ~MMRDataset()                              */
/************************************************************************/

MMRDataset::~MMRDataset()

{
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int MMRDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    // Checking for subdataset
    int nIdentifyResult =
        MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename);
    if (nIdentifyResult != GDAL_IDENTIFY_FALSE)
        return nIdentifyResult;

    // Checking for MiraMon raster file
    return MMRRel::IdentifyFile(poOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The MiraMonRaster driver does not support update "
                 "access to existing datasets.");
        return nullptr;
    }

    // Create the Dataset (with bands or Subdatasets).
    auto poDS = std::make_unique<MMRDataset>(poOpenInfo);
    if (!poDS->IsValid())
        return nullptr;

    // Set description
    poDS->SetDescription(poOpenInfo->pszFilename);

    return poDS.release();
}

bool MMRDataset::CreateRasterBands()
{
    MMRBand *pBand;

    for (int nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
    {
        // Establish raster band info.
        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        nRasterXSize = pBand->GetWidth();
        nRasterYSize = pBand->GetHeight();
        pBand->UpdateGeoTransform();  // Fills adfGeoTransform for this band

        auto poRasterBand = std::make_unique<MMRRasterBand>(this, nBands + 1);
        if (!poRasterBand->IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create a RasterBand from '%s'",
                     m_pMMRRel->GetRELNameChar());

            return false;
        }

        SetBand(nBands + 1, std::move(poRasterBand));

        MMRRasterBand *poBand =
            cpl::down_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        if (!pBand->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem("DESCRIPTION",
                                    pBand->GetFriendlyDescription());
        }
    }
    // Some metadata items must be preserved just in case to be restored
    // if they are preserved through translations.
    m_pMMRRel->RELToGDALMetadata(this);

    return true;
}

void MMRDataset::ReadProjection()

{
    if (!m_pMMRRel)
        return;

    CPLString osSRS;

    if (!m_pMMRRel->GetMetadataValue("SPATIAL_REFERENCE_SYSTEM:HORIZONTAL",
                                     "HorizontalSystemIdentifier", osSRS) ||
        osSRS.empty())
        return;

    char szResult[MM_MAX_ID_SNY + 10];
    int nResult = ReturnEPSGCodeSRSFromMMIDSRS(osSRS.c_str(), szResult);
    if (nResult == 1 || szResult[0] == '\0')
        return;

    int nEPSG;
    if (1 == sscanf(szResult, "%d", &nEPSG))
        m_oSRS.importFromEPSG(nEPSG);

    return;
}

/************************************************************************/
/*                           SUBDATASETS                                */
/************************************************************************/
// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    m_nNSubdataSets = 0;
    if (!m_pMMRRel.get())
        return;

    m_nNSubdataSets = 1;
    int nIBand = 0;
    MMRBand *pBand = m_pMMRRel->GetBand(nIBand);
    if (!pBand)
        return;

    pBand->AssignSubDataSet(m_nNSubdataSets);
    MMRBand *pNextBand;
    for (; nIBand < m_pMMRRel->GetNBands() - 1; nIBand++)
    {
        if (IsNextBandInANewDataSet(nIBand))
        {
            m_nNSubdataSets++;
            pNextBand = m_pMMRRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(m_nNSubdataSets);
        }
        else
        {
            pNextBand = m_pMMRRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (m_nNSubdataSets == 1)
    {
        m_nNSubdataSets = 0;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                break;
            pBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;
    MMRBand *pBand;

    for (int iSubdataset = 1; iSubdataset <= m_nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        if (nIBand == m_pMMRRel->GetNBands())
            break;

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return;

        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        pBand->GetRELFileName().c_str(),
                        pBand->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        pBand->GetBandName().c_str());
        nIBand++;

        for (; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(
                CPLSPrintf(",\"%s\"", pBand->GetRawBandFileName().c_str()));
            osDSDesc.append(
                CPLSPrintf(",\"%s\"", pBand->GetBandName().c_str()));
        }

        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubdataset), osDSName);
        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubdataset), osDSDesc);
    }

    if (oSubdatasetList.Count() > 0)
    {
        // Add metadata to the main dataset
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
    }
}

bool MMRDataset::IsNextBandInANewDataSet(int nIBand) const
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= m_pMMRRel->GetNBands())
        return false;

    MMRBand *pThisBand = m_pMMRRel->GetBand(nIBand);
    MMRBand *pNextBand = m_pMMRRel->GetBand(nIBand + 1);
    if (!pThisBand || !pNextBand)
        return false;

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->GetWidth() != pNextBand->GetWidth())
        return true;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->GetHeight() != pNextBand->GetHeight())
        return true;

    // Two images with different bounding box are assigned to different subdatasets
    if (pThisBand->GetBoundingBoxMinX() != pNextBand->GetBoundingBoxMinX())
        return true;
    if (pThisBand->GetBoundingBoxMaxX() != pNextBand->GetBoundingBoxMaxX())
        return true;
    if (pThisBand->GetBoundingBoxMinY() != pNextBand->GetBoundingBoxMinY())
        return true;
    if (pThisBand->GetBoundingBoxMaxY() != pNextBand->GetBoundingBoxMaxY())
        return true;

    // One image has NoData values and the other does not;
    // they are assigned to different subdatasets
    if (pThisBand->BandHasNoData() != pNextBand->BandHasNoData())
        return true;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->GetNoDataValue() != pNextBand->GetNoDataValue())
        return true;

    return false;
}

/************************************************************************/
/*                          UpdateGeoTransform()                     */
/************************************************************************/
int MMRDataset::UpdateGeoTransform()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    if (!m_pMMRRel)
        return 1;

    CPLString osMinX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinX", osMinX) ||
        osMinX.empty())
        return 1;

    if (1 != CPLsscanf(osMinX, "%lf", &(m_gt[0])))
        m_gt[0] = 0.0;

    int nNCols = m_pMMRRel->GetColumnsNumberFromREL();
    if (nNCols <= 0)
        return 1;

    CPLString osMaxX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxX", osMaxX) ||
        osMaxX.empty())
        return 1;

    double dfMaxX;
    if (1 != CPLsscanf(osMaxX, "%lf", &dfMaxX))
        dfMaxX = 1.0;

    m_gt[1] = (dfMaxX - m_gt[0]) / nNCols;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinY", osMinY) ||
        osMinY.empty())
        return 1;

    CPLString osMaxY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxY", osMaxY) ||
        osMaxY.empty())
        return 1;

    int nNRows = m_pMMRRel->GetRowsNumberFromREL();
    if (nNRows <= 0)
        return 1;

    double dfMaxY;
    if (1 != CPLsscanf(osMaxY, "%lf", &dfMaxY))
        dfMaxY = 1.0;

    m_gt[3] = dfMaxY;
    m_gt[4] = 0.0;

    double dfMinY;
    if (1 != CPLsscanf(osMinY, "%lf", &dfMinY))
        dfMinY = 0.0;
    m_gt[5] = (dfMinY - m_gt[3]) / nNRows;

    return 0;
}

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

CPLErr MMRDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    if (m_gt[0] != 0.0 || m_gt[1] != 1.0 || m_gt[2] != 0.0 || m_gt[3] != 0.0 ||
        m_gt[4] != 0.0 || m_gt[5] != 1.0)
    {
        gt = m_gt;
        return CE_None;
    }

    return GDALDataset::GetGeoTransform(gt);
}
