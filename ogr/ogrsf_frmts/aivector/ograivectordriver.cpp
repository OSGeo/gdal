/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Artificial Intelligence powered driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

/************************************************************************/
/*                       OGRAIVectorIdentify()                          */
/************************************************************************/

static int OGRAIVectorIdentify(GDALOpenInfo *poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "AIVector:") ||
           poOpenInfo->IsSingleAllowedDriver("AIVector");
}

/************************************************************************/
/*                         OGRAIVectorOpen()                            */
/************************************************************************/

static GDALDataset *OGRAIVectorOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRAIVectorIdentify(poOpenInfo))
        return nullptr;

    class MyLayer final : public OGRLayer,
                          public OGRGetNextFeatureThroughRaw<MyLayer>
    {
        OGRFeatureDefn *m_poLayerDefn = nullptr;
        bool m_bReturnedFeature = false;

        CPL_DISALLOW_COPY_ASSIGN(MyLayer)

      public:
        MyLayer()
        {
            m_poLayerDefn = new OGRFeatureDefn("result");
            SetDescription(m_poLayerDefn->GetName());
            m_poLayerDefn->Reference();
            OGRFieldDefn oFieldDefn("name", OFTString);
            m_poLayerDefn->AddFieldDefn(&oFieldDefn);
            OGRSpatialReference *poSRS = new OGRSpatialReference(
                "GEOGCS[\"I don't know\",\n"
                "    DATUM[\"I don't care\",\n"
                "        SPHEROID[\"GRS 1980\",6378137,298.257222101,\n"
                "            AUTHORITY[\"EPSG\",\"7019\"]]],\n"
                "    PRIMEM[\"Greenwich\",0,\n"
                "        AUTHORITY[\"EPSG\",\"8901\"]],\n"
                "    UNIT[\"degree\",0.0174532925199433,\n"
                "        AUTHORITY[\"EPSG\",\"9122\"]],\n"
                "    AUTHORITY[\"AI\",\"TOTALLY_MADE_UP\"]]");
            m_poLayerDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Release();
        }

        ~MyLayer() override
        {
            m_poLayerDefn->Release();
        }

        void ResetReading() override
        {
            m_bReturnedFeature = false;
        }

        OGRFeatureDefn *GetLayerDefn() override
        {
            return m_poLayerDefn;
        }
        DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(MyLayer)

        OGRFeature *GetNextRawFeature()
        {
            if (m_bReturnedFeature)
                return nullptr;
            m_bReturnedFeature = true;
            OGRFeature *poFeature = new OGRFeature(m_poLayerDefn);
            poFeature->SetFID(0);
            poFeature->SetField(0, "Null Island: the place to be");
            OGRPoint *poPoint = new OGRPoint(0, 0);
            poPoint->assignSpatialReference(GetSpatialRef());
            poFeature->SetGeometryDirectly(poPoint);
            return poFeature;
        }

        int TestCapability(const char *) override
        {
            return false;
        }
    };

    class MyDataset final : public GDALDataset
    {
        MyLayer m_oLayer{};

      public:
        MyDataset() = default;

        int GetLayerCount() override
        {
            return 1;
        }

        OGRLayer *GetLayer(int idx) override
        {
            return idx == 0 ? &m_oLayer : nullptr;
        }
    };

    return new MyDataset();
}

/************************************************************************/
/*                       RegisterOGRAIVector()                          */
/************************************************************************/

void RegisterOGRAIVector()
{
    if (!GDAL_CHECK_VERSION("AIVector"))
        return;

    if (GDALGetDriverByName("AIVector") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("AIVector");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Artificial Intelligence powered vector driver");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/aivector.html");

    poDriver->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, "AIVector:");

    poDriver->pfnOpen = OGRAIVectorOpen;
    poDriver->pfnIdentify = OGRAIVectorIdentify;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
