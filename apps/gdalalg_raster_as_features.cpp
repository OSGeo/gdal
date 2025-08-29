/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "as-features" step of "gdal pipeline"
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences, LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_as_features.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogrsf_frmts.h"

#include <cmath>
#include <optional>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

GDALRasterAsFeaturesAlgorithm::GDALRasterAsFeaturesAlgorithm(
    bool standaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE)),
      m_bands{}, m_geomTypeName("none"), m_skipNoData(false),
      m_includeXY(false), m_includeRowCol(false)
{
    m_outputLayerName = "pixels";

    // TODO: This block is copied from gdalalg_raster_polygonize. Avoid duplication?
    if (standaloneStep)
    {
        AddOutputFormatArg(&m_format).AddMetadataItem(
            GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
        AddOpenOptionsArg(&m_openOptions);
        AddInputFormatsArg(&m_inputFormats)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});

        AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);
        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
        AddCreationOptionsArg(&m_creationOptions);
        AddLayerCreationOptionsArg(&m_layerCreationOptions);
        AddOverwriteArg(&m_overwrite);
        AddUpdateArg(&m_update);
        AddOverwriteLayerArg(&m_overwriteLayer);
        AddAppendLayerArg(&m_appendLayer);
        AddLayerNameArg(&m_outputLayerName)
            .AddAlias("nln")
            .SetDefault(m_outputLayerName);
    }

    AddBandArg(&m_bands);
    AddArg("geometry-type", 0, _("Geometry type"), &m_geomTypeName)
        .SetChoices("none", "point", "polygon")
        .SetDefault("none");
    AddArg("skip-nodata", 0, _("Omit NoData pixels from the result"),
           &m_skipNoData);
    AddArg("include-xy", 0, _("Include fields for cell center coordinates"),
           &m_includeXY);
    AddArg("include-row-col", 0, _("Include columns for row and column"),
           &m_includeRowCol);
}

GDALRasterAsFeaturesAlgorithm::~GDALRasterAsFeaturesAlgorithm() = default;

bool GDALRasterAsFeaturesAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

GDALRasterAsFeaturesAlgorithmStandalone::
    ~GDALRasterAsFeaturesAlgorithmStandalone() = default;

struct RasterAsFeaturesOptions
{
    OGRwkbGeometryType geomType{wkbNone};
    bool includeXY{false};
    bool includeRowCol{false};
    bool skipNoData{false};
    std::vector<int> bands{};
};

class GDALRasterToVectorPipelineOutputDataset final : public GDALDataset
{

  public:
    int GetLayerCount() const override
    {
        return static_cast<int>(m_layers.size());
    }

    const OGRLayer *GetLayer(int idx) const override
    {
        return m_layers[idx].get();
    }

    int TestCapability(const char *) const override;

    void AddLayer(std::unique_ptr<OGRLayer> layer)
    {
        m_layers.emplace_back(std::move(layer));
    }

  private:
    std::vector<std::unique_ptr<OGRLayer>> m_layers{};
};

int GDALRasterToVectorPipelineOutputDataset::TestCapability(const char *) const
{
    return 0;
}

class GDALRasterAsFeaturesLayer final : public OGRLayer
{
  public:
    static constexpr const char *ROW_FIELD = "ROW";
    static constexpr const char *COL_FIELD = "COL";
    static constexpr const char *X_FIELD = "CENTER_X";
    static constexpr const char *Y_FIELD = "CENTER_Y";

    GDALRasterAsFeaturesLayer(GDALDataset &ds, RasterAsFeaturesOptions options)
        : m_ds(ds), m_bufType(GDT_Float64),
          m_it(GDALRasterBand::WindowIterator(
              m_ds.GetRasterXSize(), m_ds.GetRasterYSize(),
              m_ds.GetRasterXSize(), m_ds.GetRasterYSize(), 0, 0)),
          m_end(GDALRasterBand::WindowIterator(
              m_ds.GetRasterXSize(), m_ds.GetRasterYSize(),
              m_ds.GetRasterXSize(), m_ds.GetRasterYSize(), 1, 0)),
          m_includeXY(options.includeXY),
          m_includeRowCol(options.includeRowCol),
          m_excludeNoDataPixels(options.skipNoData)
    {
        // TODO: Handle Int64, UInt64
        m_ds.GetGeoTransform(m_gt);

        int nBands = m_ds.GetRasterCount();
        m_bands.resize(nBands);
        for (int i = 1; i <= nBands; i++)
        {
            m_bands[i - 1] = i;
        }

        // TODO: Handle per-band NoData values
        if (nBands > 0)
        {
            int hasNoData;
            double noData = m_ds.GetRasterBand(1)->GetNoDataValue(&hasNoData);
            if (hasNoData)
            {
                m_noData = noData;
            }
        }

        m_defn = new OGRFeatureDefn();
        m_defn->GetGeomFieldDefn(0)->SetType(options.geomType);
        m_defn->GetGeomFieldDefn(0)->SetSpatialRef(ds.GetSpatialRef());
        m_defn->Reference();

        if (m_includeXY)
        {
            auto xField = std::make_unique<OGRFieldDefn>(X_FIELD, OFTReal);
            auto yField = std::make_unique<OGRFieldDefn>(Y_FIELD, OFTReal);
            m_defn->AddFieldDefn(std::move(xField));
            m_defn->AddFieldDefn(std::move(yField));
        }
        if (m_includeRowCol)
        {
            auto rowField =
                std::make_unique<OGRFieldDefn>(ROW_FIELD, OFTInteger);
            auto colField =
                std::make_unique<OGRFieldDefn>(COL_FIELD, OFTInteger);
            m_defn->AddFieldDefn(std::move(rowField));
            m_defn->AddFieldDefn(std::move(colField));
        }
        for (int band : m_bands)
        {
            CPLString fieldName = CPLSPrintf("BAND_%d", band);
            auto bandField =
                std::make_unique<OGRFieldDefn>(fieldName.c_str(), OFTReal);
            m_defn->AddFieldDefn(std::move(bandField));
            m_bandFields.push_back(m_defn->GetFieldIndex(fieldName));
        }

        GDALRasterAsFeaturesLayer::ResetReading();
    }

    ~GDALRasterAsFeaturesLayer() override;

    void ResetReading() override
    {
        if (m_ds.GetRasterCount() > 0)
        {
            GDALRasterBand *poFirstBand = m_ds.GetRasterBand(1);
            CPLAssert(poFirstBand);  // appease clang scan-build
            m_it = poFirstBand->IterateWindows().begin();
            m_end = poFirstBand->IterateWindows().end();
        }
    }

    int TestCapability(const char *) const override
    {
        return 0;
    }

    OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_defn;
    }

    OGRFeature *GetNextFeature() override
    {
        if (m_row >= m_window.nYSize && !NextWindow())
        {
            return nullptr;
        }

        std::unique_ptr<OGRFeature> feature;

        while (m_row < m_window.nYSize)
        {
            const double *pSrcVal = static_cast<double *>(m_buf) +
                                    (m_bands.size() * m_row * m_window.nXSize +
                                     m_col * m_bands.size());

            const bool emitFeature =
                !m_excludeNoDataPixels || !IsNoData(*pSrcVal);

            if (emitFeature)
            {
                feature.reset(OGRFeature::CreateFeature(m_defn));

                for (int fieldPos : m_bandFields)
                {
                    feature->SetField(fieldPos, *pSrcVal);
                    pSrcVal++;
                }

                const double line = m_window.nYOff + m_row;
                const double pixel = m_window.nXOff + m_col;

                if (m_includeRowCol)
                {
                    feature->SetField(ROW_FIELD, static_cast<GIntBig>(line));
                    feature->SetField(COL_FIELD, static_cast<GIntBig>(pixel));
                }
                if (m_includeXY)
                {
                    double x, y;
                    m_gt.Apply(pixel + 0.5, line + 0.5, &x, &y);
                    feature->SetField(X_FIELD, x);
                    feature->SetField(Y_FIELD, y);
                }

                std::unique_ptr<OGRGeometry> geom;
                const auto geomType = m_defn->GetGeomFieldDefn(0)->GetType();
                if (geomType == wkbPoint)
                {
                    double x, y;
                    m_gt.Apply(pixel + 0.5, line + 0.5, &x, &y);

                    geom = std::make_unique<OGRPoint>(x, y);
                }
                else if (geomType == wkbPolygon)
                {
                    double x, y;

                    auto lr = std::make_unique<OGRLinearRing>();

                    m_gt.Apply(pixel, line, &x, &y);
                    lr->addPoint(x, y);
                    m_gt.Apply(pixel, line + 1, &x, &y);
                    lr->addPoint(x, y);
                    m_gt.Apply(pixel + 1, line + 1, &x, &y);
                    lr->addPoint(x, y);
                    m_gt.Apply(pixel + 1, line, &x, &y);
                    lr->addPoint(x, y);
                    m_gt.Apply(pixel, line, &x, &y);
                    lr->addPoint(x, y);

                    auto poly = std::make_unique<OGRPolygon>();
                    poly->addRing(std::move(lr));
                    geom = std::move(poly);
                }

                feature->SetGeometry(std::move(geom));
            }

            m_col += 1;
            if (m_col >= m_window.nXSize)
            {
                m_col = 0;
                m_row++;
            }

            if (feature)
            {
                return feature.release();
            }
        }

        return nullptr;
    }

    CPL_DISALLOW_COPY_ASSIGN(GDALRasterAsFeaturesLayer)

  private:
    bool IsNoData(double x) const
    {
        if (!m_noData.has_value())
        {
            return false;
        }

        return m_noData.value() == x ||
               (std::isnan(m_noData.value()) && std::isnan(x));
    }

    bool NextWindow()
    {
        int nBandCount = static_cast<int>(m_bands.size());

        if (m_it == m_end)
        {
            return false;
        }

        if (m_ds.GetRasterXSize() == 0 || m_ds.GetRasterYSize() == 0)
        {
            return false;
        }

        m_window = *m_it;
        ++m_it;

        if (m_buf == nullptr && !m_bands.empty())
        {
            m_buf =
                VSI_MALLOC3_VERBOSE(m_window.nXSize, m_window.nYSize,
                                    static_cast<size_t>(nBandCount) *
                                        GDALGetDataTypeSizeBytes(m_bufType));
            if (m_buf == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to allocate buffer");
                return false;
            }
        }

        for (size_t i = 0; i < m_bands.size(); i++)
        {
            auto eErr =
                m_ds.GetRasterBand(m_bands[i])
                    ->RasterIO(GF_Read, m_window.nXOff, m_window.nYOff,
                               m_window.nXSize, m_window.nYSize,
                               static_cast<GByte *>(m_buf) +
                                   i * GDALGetDataTypeSizeBytes(m_bufType),
                               m_window.nXSize, m_window.nYSize, m_bufType,
                               static_cast<GSpacing>(nBandCount) *
                                   GDALGetDataTypeSizeBytes(m_bufType),
                               static_cast<GSpacing>(nBandCount) *
                                   GDALGetDataTypeSizeBytes(m_bufType) *
                                   m_window.nXSize,
                               nullptr);

            if (eErr != CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to read raster data");
                return false;
            }
        }

        m_row = 0;
        m_col = 0;

        return true;
    }

    GDALDataset &m_ds;
    void *m_buf{nullptr};
    GDALDataType m_bufType;
    GDALGeoTransform m_gt{};
    std::optional<double> m_noData{std::nullopt};

    std::vector<int> m_bands{};
    std::vector<int> m_bandFields{};

    GDALRasterBand::WindowIterator m_it;
    GDALRasterBand::WindowIterator m_end;
    GDALRasterWindow m_window{};

    int m_row{0};
    int m_col{0};

    OGRFeatureDefn *m_defn{nullptr};
    bool m_includeXY;
    bool m_includeRowCol;
    bool m_excludeNoDataPixels;
};

GDALRasterAsFeaturesLayer::~GDALRasterAsFeaturesLayer()
{
    m_defn->Release();
    if (m_buf != nullptr)
    {
        CPLFree(m_buf);
    }
}

bool GDALRasterAsFeaturesAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();

    GDALDataset *poDstDS = m_outputDataset.GetDatasetRef();
    std::string outputFilename = m_outputDataset.GetName();

    std::unique_ptr<GDALDataset> poRetDS;
    if (!poDstDS)
    {
        if (m_standaloneStep && m_format.empty())
        {
            const auto aosFormats =
                CPLStringList(GDALGetOutputDriversForDatasetName(
                    m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                    /* bSingleMatch = */ true,
                    /* bWarn = */ true));
            if (aosFormats.size() != 1)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot guess driver for %s",
                            m_outputDataset.GetName().c_str());
                return false;
            }
            m_format = aosFormats[0];

            auto poDriver =
                GetGDALDriverManager()->GetDriverByName(m_format.c_str());

            poRetDS.reset(
                poDriver->Create(outputFilename.c_str(), 0, 0, 0, GDT_Unknown,
                                 CPLStringList(m_creationOptions).List()));
        }
        else
        {
            poRetDS =
                std::make_unique<GDALRasterToVectorPipelineOutputDataset>();
        }

        if (!poRetDS)
            return false;

        poDstDS = poRetDS.get();
    }

    RasterAsFeaturesOptions options;
    options.geomType = m_geomTypeName == "point"     ? wkbPoint
                       : m_geomTypeName == "polygon" ? wkbPolygon
                                                     : wkbNone;
    options.includeRowCol = m_includeRowCol;
    options.includeXY = m_includeXY;
    options.skipNoData = m_skipNoData;

    if (!m_bands.empty())
    {
        options.bands = std::move(m_bands);
    }

    auto poLayer =
        std::make_unique<GDALRasterAsFeaturesLayer>(*poSrcDS, options);

    poDstDS->CopyLayer(poLayer.get(), m_outputLayerName.c_str());

    if (poRetDS)
    {
        m_outputDataset.Set(std::move(poRetDS));
    }

    return true;
}

//! @endcond
