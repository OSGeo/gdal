/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  NULL driver.
 * Author:   Even Rouault, <even dot rouault at spatialys dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012-2017, Even Rouault, <even dot rouault at spatialys dot
 *org>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"
#include "gdal_frmts.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                          GDALNullDataset                             */
/************************************************************************/

class GDALNullDataset final : public GDALDataset
{
    int m_nLayers;
    OGRLayer **m_papoLayers;

    CPL_DISALLOW_COPY_ASSIGN(GDALNullDataset)

  public:
    GDALNullDataset();
    ~GDALNullDataset() override;

    int GetLayerCount() const override
    {
        return m_nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) const override;

    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    CPLErr SetGeoTransform(const GDALGeoTransform &gt) override;

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
};

/************************************************************************/
/*                           GDALNullLayer                              */
/************************************************************************/

class GDALNullRasterBand final : public GDALRasterBand
{
  public:
    explicit GDALNullRasterBand(GDALDataType eDT);

    CPLErr SetCategoryNames(CPL_UNUSED char **papszNames) override
    {
        return CE_None;
    }

    CPLErr SetNoDataValue(CPL_UNUSED double dfNoData) override
    {
        return CE_None;
    }

    CPLErr SetNoDataValueAsInt64(CPL_UNUSED int64_t nNoData) override
    {
        return CE_None;
    }

    CPLErr SetNoDataValueAsUInt64(CPL_UNUSED uint64_t nNoData) override
    {
        return CE_None;
    }

    CPLErr DeleteNoDataValue() override
    {
        return CE_None;
    }

    CPLErr SetColorTable(CPL_UNUSED GDALColorTable *poCT) override
    {
        return CE_None;
    }

    CPLErr
    SetColorInterpretation(CPL_UNUSED GDALColorInterp eColorInterp) override
    {
        return CE_None;
    }

    CPLErr SetOffset(CPL_UNUSED double dfNewOffset) override
    {
        return CE_None;
    }

    CPLErr SetScale(CPL_UNUSED double dfNewScale) override
    {
        return CE_None;
    }

    CPLErr SetUnitType(CPL_UNUSED const char *pszNewValue) override
    {
        return CE_None;
    }

    CPLErr SetDefaultRAT(const GDALRasterAttributeTable *) override
    {
        return CE_None;
    }

    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/*                           GDALNullLayer                              */
/************************************************************************/

class GDALNullLayer final : public OGRLayer
{
    OGRFeatureDefn *poFeatureDefn;
    OGRSpatialReference *poSRS = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALNullLayer)

  public:
    GDALNullLayer(const char *pszLayerName, const OGRSpatialReference *poSRS,
                  OGRwkbGeometryType eType);
    ~GDALNullLayer() override;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return poSRS;
    }

    void ResetReading() override
    {
    }

    int TestCapability(const char *) const override;

    OGRFeature *GetNextFeature() override
    {
        return nullptr;
    }

    OGRErr ICreateFeature(OGRFeature *) override
    {
        return OGRERR_NONE;
    }

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
};

/************************************************************************/
/*                           GDALNullRasterBand()                       */
/************************************************************************/

GDALNullRasterBand::GDALNullRasterBand(GDALDataType eDT)
{
    eDataType = eDT;
    nBlockXSize = 256;
    nBlockYSize = 256;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALNullRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                     int nXSize, int nYSize, void *pData,
                                     int nBufXSize, int nBufYSize,
                                     GDALDataType eBufType,
                                     GSpacing nPixelSpace, GSpacing nLineSpace,
                                     GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag == GF_Write)
        return CE_None;
    if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour &&
        (nBufXSize != nXSize || nBufYSize != nYSize))
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg);
    }
    if (nPixelSpace == GDALGetDataTypeSizeBytes(eBufType) &&
        nLineSpace == nPixelSpace * nBufXSize)
    {
        memset(pData, 0, static_cast<size_t>(nLineSpace) * nBufYSize);
    }
    else
    {
        for (int iY = 0; iY < nBufYSize; iY++)
        {
            double dfZero = 0;
            GDALCopyWords(&dfZero, GDT_Float64, 0,
                          reinterpret_cast<GByte *>(pData) + iY * nLineSpace,
                          eBufType, static_cast<int>(nPixelSpace), nBufXSize);
        }
    }
    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALNullRasterBand::IReadBlock(int, int, void *pData)
{
    memset(pData, 0,
           static_cast<size_t>(nBlockXSize) * nBlockYSize *
               GDALGetDataTypeSizeBytes(eDataType));
    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr GDALNullRasterBand::IWriteBlock(int, int, void *)
{
    return CE_None;
}

/************************************************************************/
/*                          GDALNullDataset()                           */
/************************************************************************/

GDALNullDataset::GDALNullDataset() : m_nLayers(0), m_papoLayers(nullptr)
{
    eAccess = GA_Update;
}

/************************************************************************/
/*                         ~GDALNullDataset()                           */
/************************************************************************/

GDALNullDataset::~GDALNullDataset()
{
    for (int i = 0; i < m_nLayers; i++)
        delete m_papoLayers[i];
    CPLFree(m_papoLayers);
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *GDALNullDataset::ICreateLayer(const char *pszLayerName,
                                        const OGRGeomFieldDefn *poGeomFieldDefn,
                                        CSLConstList /*papszOptions */)
{
    const auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    m_papoLayers = static_cast<OGRLayer **>(
        CPLRealloc(m_papoLayers, sizeof(OGRLayer *) * (m_nLayers + 1)));
    m_papoLayers[m_nLayers] = new GDALNullLayer(pszLayerName, poSRS, eType);
    m_nLayers++;
    return m_papoLayers[m_nLayers - 1];
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int GDALNullDataset::TestCapability(const char *pszCap) const

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

const OGRLayer *GDALNullDataset::GetLayer(int iLayer) const

{
    if (iLayer < 0 || iLayer >= m_nLayers)
        return nullptr;
    else
        return m_papoLayers[iLayer];
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr GDALNullDataset::SetSpatialRef(const OGRSpatialReference *)

{
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALNullDataset::SetGeoTransform(const GDALGeoTransform &)

{
    return CE_None;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

GDALDataset *GDALNullDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "NULL:"))
        return nullptr;

    const char *pszStr = poOpenInfo->pszFilename + strlen("NULL:");
    char **papszTokens = CSLTokenizeString2(pszStr, ",", 0);
    int nXSize = atoi(CSLFetchNameValueDef(papszTokens, "width", "512"));
    int nYSize = atoi(CSLFetchNameValueDef(papszTokens, "height", "512"));
    int nBands = atoi(CSLFetchNameValueDef(papszTokens, "bands", "1"));
    const char *pszDTName = CSLFetchNameValueDef(papszTokens, "type", "Byte");
    GDALDataType eDT = GDT_UInt8;
    for (int iType = 1; iType < GDT_TypeCount; iType++)
    {
        if (GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr &&
            EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                  pszDTName))
        {
            eDT = static_cast<GDALDataType>(iType);
            break;
        }
    }
    CSLDestroy(papszTokens);

    return Create("", nXSize, nYSize, nBands, eDT, nullptr);
}

/************************************************************************/
/*                              Create()                                */
/************************************************************************/

GDALDataset *GDALNullDataset::Create(const char *, int nXSize, int nYSize,
                                     int nBandsIn, GDALDataType eType, char **)
{
    GDALNullDataset *poDS = new GDALNullDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    for (int i = 0; i < nBandsIn; i++)
        poDS->SetBand(i + 1, new GDALNullRasterBand(eType));
    return poDS;
}

/************************************************************************/
/*                           GDALNullLayer()                            */
/************************************************************************/

GDALNullLayer::GDALNullLayer(const char *pszLayerName,
                             const OGRSpatialReference *poSRSIn,
                             OGRwkbGeometryType eType)
    : poFeatureDefn(new OGRFeatureDefn(pszLayerName))
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->SetGeomType(eType);
    poFeatureDefn->Reference();

    if (poSRSIn)
        poSRS = poSRSIn->Clone();
}

/************************************************************************/
/*                           ~GDALNullLayer()                           */
/************************************************************************/

GDALNullLayer::~GDALNullLayer()
{
    poFeatureDefn->Release();

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int GDALNullLayer::TestCapability(const char *pszCap) const

{
    if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                             CreateField()                            */
/************************************************************************/

OGRErr GDALNullLayer::CreateField(const OGRFieldDefn *poField, int)
{
    poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                        GDALRegister_NULL()                           */
/************************************************************************/

void GDALRegister_NULL()

{
    if (GDALGetDriverByName("NULL") != nullptr)
        return;

    GDALDriver *poDriver;
    poDriver = new GDALDriver();

    poDriver->SetDescription("NULL");
    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "NULL:");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "NULL");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Binary IntegerList Integer64List "
                              "RealList StringList");

    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = GDALNullDataset::Open;
    poDriver->pfnCreate = GDALNullDataset::Create;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
