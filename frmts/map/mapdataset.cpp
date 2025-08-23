/******************************************************************************
 *
 * Project:  OziExplorer .MAP Driver
 * Purpose:  GDALDataset driver for OziExplorer .MAP files
 * Author:   Jean-Claude Repetto, <jrepetto at @free dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2012, Jean-Claude Repetto
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"

/************************************************************************/
/* ==================================================================== */
/*                                MAPDataset                            */
/* ==================================================================== */
/************************************************************************/

class MAPDataset final : public GDALDataset
{
    GDALDataset *poImageDS{};

    OGRSpatialReference m_oSRS{};
    int bGeoTransformValid{};
    GDALGeoTransform m_gt{};
    int nGCPCount{};
    GDAL_GCP *pasGCPList{};
    OGRPolygon *poNeatLine{};
    CPLString osImgFilename{};

    CPL_DISALLOW_COPY_ASSIGN(MAPDataset)

  public:
    MAPDataset();
    virtual ~MAPDataset();

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual char **GetFileList() override;

    virtual int CloseDependentDatasets() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                         MAPWrapperRasterBand                         */
/* ==================================================================== */
/************************************************************************/
class MAPWrapperRasterBand final : public GDALProxyRasterBand
{
    GDALRasterBand *poBaseBand{};

    CPL_DISALLOW_COPY_ASSIGN(MAPWrapperRasterBand)

  protected:
    virtual GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/) const override;

  public:
    explicit MAPWrapperRasterBand(GDALRasterBand *poBaseBandIn)
    {
        this->poBaseBand = poBaseBandIn;
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    ~MAPWrapperRasterBand()
    {
    }
};

GDALRasterBand *
MAPWrapperRasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return poBaseBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             MAPDataset                               */
/* ==================================================================== */
/************************************************************************/

MAPDataset::MAPDataset()
    : poImageDS(nullptr), bGeoTransformValid(false), nGCPCount(0),
      pasGCPList(nullptr), poNeatLine(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                            ~MAPDataset()                             */
/************************************************************************/

MAPDataset::~MAPDataset()

{
    if (poImageDS != nullptr)
    {
        GDALClose(poImageDS);
        poImageDS = nullptr;
    }

    if (nGCPCount)
    {
        GDALDeinitGCPs(nGCPCount, pasGCPList);
        CPLFree(pasGCPList);
    }

    if (poNeatLine != nullptr)
    {
        delete poNeatLine;
        poNeatLine = nullptr;
    }
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int MAPDataset::CloseDependentDatasets()
{
    int bRet = GDALDataset::CloseDependentDatasets();
    if (poImageDS != nullptr)
    {
        GDALClose(poImageDS);
        poImageDS = nullptr;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int MAPDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 200 ||
        !poOpenInfo->IsExtensionEqualToCI("MAP"))
        return FALSE;

    if (strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
               "OziExplorer Map Data File") == nullptr)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MAPDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("MAP");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    auto poDS = std::make_unique<MAPDataset>();

    /* -------------------------------------------------------------------- */
    /*      Try to load and parse the .MAP file.                            */
    /* -------------------------------------------------------------------- */

    char *pszWKT = nullptr;
    bool bOziFileOK = CPL_TO_BOOL(
        GDALLoadOziMapFile(poOpenInfo->pszFilename, poDS->m_gt.data(), &pszWKT,
                           &poDS->nGCPCount, &poDS->pasGCPList));
    if (pszWKT)
    {
        poDS->m_oSRS.importFromWkt(pszWKT);
        CPLFree(pszWKT);
    }

    if (bOziFileOK && poDS->nGCPCount == 0)
        poDS->bGeoTransformValid = TRUE;

    /* We need to read again the .map file because the GDALLoadOziMapFile
       function does not returns all required data . An API change is necessary
       : maybe in GDAL 2.0 ? */

    const CPLStringList aosLines(
        CSLLoad2(poOpenInfo->pszFilename, 200, 200, nullptr));
    if (aosLines.empty())
    {
        return nullptr;
    }

    const int nLines = aosLines.size();
    if (nLines < 3)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      We need to open the image in order to establish                 */
    /*      details like the band count and types.                          */
    /* -------------------------------------------------------------------- */
    poDS->osImgFilename = aosLines[2];
    if (CPLHasPathTraversal(poDS->osImgFilename.c_str()))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Path traversal detected in %s",
                 poDS->osImgFilename.c_str());
        return nullptr;
    }

    const CPLString osPath = CPLGetPathSafe(poOpenInfo->pszFilename);
    if (CPLIsFilenameRelative(poDS->osImgFilename))
    {
        poDS->osImgFilename =
            CPLFormCIFilenameSafe(osPath, poDS->osImgFilename, nullptr);
    }
    else
    {
        VSIStatBufL sStat;
        if (VSIStatL(poDS->osImgFilename, &sStat) != 0)
        {
            poDS->osImgFilename = CPLGetFilename(poDS->osImgFilename);
            poDS->osImgFilename =
                CPLFormCIFilenameSafe(osPath, poDS->osImgFilename, nullptr);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try and open the file.                                          */
    /* -------------------------------------------------------------------- */
    poDS->poImageDS = GDALDataset::Open(poDS->osImgFilename,
                                        GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR);
    if (poDS->poImageDS == nullptr || poDS->poImageDS->GetRasterCount() == 0)
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Attach the bands.                                               */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->poImageDS->GetRasterXSize();
    poDS->nRasterYSize = poDS->poImageDS->GetRasterYSize();
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        return nullptr;
    }

    for (int iBand = 1; iBand <= poDS->poImageDS->GetRasterCount(); iBand++)
        poDS->SetBand(iBand, std::make_unique<MAPWrapperRasterBand>(
                                 poDS->poImageDS->GetRasterBand(iBand)));

    /* -------------------------------------------------------------------- */
    /*      Add the neatline/cutline, if required                           */
    /* -------------------------------------------------------------------- */

    /* First, we need to check if it is necessary to define a neatline */
    bool bNeatLine = false;
    for (int iLine = 10; iLine < nLines; iLine++)
    {
        if (STARTS_WITH_CI(aosLines[iLine], "MMPXY,"))
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(aosLines[iLine], ",",
                                   CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

            if (aosTokens.size() != 4)
            {
                continue;
            }

            const int x = atoi(aosTokens[2]);
            const int y = atoi(aosTokens[3]);
            if ((x != 0 && x != poDS->nRasterXSize) ||
                (y != 0 && y != poDS->nRasterYSize))
            {
                bNeatLine = true;
                break;
            }
        }
    }

    /* Create and fill the neatline polygon */
    if (bNeatLine)
    {
        poDS->poNeatLine =
            new OGRPolygon(); /* Create a polygon to store the neatline */
        OGRLinearRing *poRing = new OGRLinearRing();

        if (poDS->bGeoTransformValid) /* Compute the projected coordinates of
                                         the corners */
        {
            for (int iLine = 10; iLine < nLines; iLine++)
            {
                if (STARTS_WITH_CI(aosLines[iLine], "MMPXY,"))
                {
                    const CPLStringList aosTokens(CSLTokenizeString2(
                        aosLines[iLine], ",",
                        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

                    if (aosTokens.size() != 4)
                    {
                        continue;
                    }

                    const double x = CPLAtofM(aosTokens[2]);
                    const double y = CPLAtofM(aosTokens[3]);
                    const double X =
                        poDS->m_gt[0] + x * poDS->m_gt[1] + y * poDS->m_gt[2];
                    const double Y =
                        poDS->m_gt[3] + x * poDS->m_gt[4] + y * poDS->m_gt[5];
                    poRing->addPoint(X, Y);
                    CPLDebug("CORNER MMPXY", "%f, %f, %f, %f", x, y, X, Y);
                }
            }
        }
        else /* Convert the geographic coordinates to projected coordinates */
        {
            std::unique_ptr<OGRCoordinateTransformation> poTransform;
            if (!poDS->m_oSRS.IsEmpty())
            {
                OGRSpatialReference *poLongLat = poDS->m_oSRS.CloneGeogCS();
                if (poLongLat)
                {
                    poLongLat->SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                    poTransform.reset(OGRCreateCoordinateTransformation(
                        poLongLat, &poDS->m_oSRS));
                    poLongLat->Release();
                }
            }

            for (int iLine = 10; iLine < nLines; iLine++)
            {
                if (STARTS_WITH_CI(aosLines[iLine], "MMPLL,"))
                {
                    CPLDebug("MMPLL", "%s", aosLines[iLine]);

                    const CPLStringList aosTokens(CSLTokenizeString2(
                        aosLines[iLine], ",",
                        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

                    if (aosTokens.size() != 4)
                    {
                        continue;
                    }

                    double dfLon = CPLAtofM(aosTokens[2]);
                    double dfLat = CPLAtofM(aosTokens[3]);

                    if (poTransform)
                        poTransform->Transform(1, &dfLon, &dfLat);
                    poRing->addPoint(dfLon, dfLat);
                    CPLDebug("CORNER MMPLL", "%f, %f", dfLon, dfLat);
                }
            }
        }

        poRing->closeRings();
        poDS->poNeatLine->addRingDirectly(poRing);

        char *pszNeatLineWkt = nullptr;
        poDS->poNeatLine->exportToWkt(&pszNeatLineWkt);
        CPLDebug("NEATLINE", "%s", pszNeatLineWkt);
        poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }

    return poDS.release();
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MAPDataset::GetSpatialRef() const
{
    return (!m_oSRS.IsEmpty() && nGCPCount == 0) ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MAPDataset::GetGeoTransform(GDALGeoTransform &gt) const

{
    gt = m_gt;

    return (nGCPCount == 0) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           GetGCPCount()                              */
/************************************************************************/

int MAPDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *MAPDataset::GetGCPSpatialRef() const
{
    return (!m_oSRS.IsEmpty() && nGCPCount != 0) ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *MAPDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **MAPDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    papszFileList = CSLAddString(papszFileList, osImgFilename);

    return papszFileList;
}

/************************************************************************/
/*                          GDALRegister_MAP()                          */
/************************************************************************/

void GDALRegister_MAP()

{
    if (GDALGetDriverByName("MAP") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MAP");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OziExplorer .MAP");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/map.html");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = MAPDataset::Open;
    poDriver->pfnIdentify = MAPDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
