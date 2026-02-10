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
#include "gdalalg_vector_pipeline.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "ogrsf_frmts.h"

#include <cmath>
#include <limits>
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
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    m_outputLayerName = "pixels";

    if (standaloneStep)
    {
        AddRasterInputArgs(false, false);
        AddVectorOutputArgs(false, false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddOutputLayerNameArg(/* hiddenForCLI = */ false,
                              /* shortNameOutputLayerAllowed = */ false);
    }

    AddBandArg(&m_bands);
    AddArg("geometry-type", 0, _("Geometry type"), &m_geomTypeName)
        .SetChoices("none", "point", "polygon")
        .SetDefault(m_geomTypeName);
    AddArg("skip-nodata", 0, _("Omit NoData pixels from the result"),
           &m_skipNoData);
    AddArg("include-xy", 0, _("Include fields for cell center coordinates"),
           &m_includeXY);
    AddArg("include-row-col", 0, _("Include columns for row and column"),
           &m_includeRowCol);
}

GDALRasterAsFeaturesAlgorithm::~GDALRasterAsFeaturesAlgorithm() = default;

GDALRasterAsFeaturesAlgorithmStandalone::
    ~GDALRasterAsFeaturesAlgorithmStandalone() = default;

struct RasterAsFeaturesOptions
{
    OGRwkbGeometryType geomType{wkbNone};
    bool includeXY{false};
    bool includeRowCol{false};
    bool skipNoData{false};
    std::vector<int> bands{};
    std::string outputLayerName{};
};

class GDALRasterAsFeaturesLayer final
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<GDALRasterAsFeaturesLayer>
{
  public:
    static constexpr const char *ROW_FIELD = "ROW";
    static constexpr const char *COL_FIELD = "COL";
    static constexpr const char *X_FIELD = "CENTER_X";
    static constexpr const char *Y_FIELD = "CENTER_Y";

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(GDALRasterAsFeaturesLayer)

    GDALRasterAsFeaturesLayer(GDALDataset &ds, RasterAsFeaturesOptions options)
        : m_ds(ds), m_it(GDALRasterBand::WindowIterator(
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

        SetDescription(options.outputLayerName.c_str());
        m_defn = new OGRFeatureDefn(options.outputLayerName.c_str());
        if (options.geomType == wkbNone)
        {
            m_defn->SetGeomType(wkbNone);
        }
        else
        {
            m_defn->GetGeomFieldDefn(0)->SetType(options.geomType);
            m_defn->GetGeomFieldDefn(0)->SetSpatialRef(ds.GetSpatialRef());
        }
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

    int TestCapability(const char *pszCap) const override
    {
        return EQUAL(pszCap, OLCFastFeatureCount) &&
               m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
               !m_excludeNoDataPixels;
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
            !m_excludeNoDataPixels)
        {
            return static_cast<GIntBig>(m_ds.GetRasterXSize()) *
                   m_ds.GetRasterYSize();
        }
        return OGRLayer::GetFeatureCount(bForce);
    }

    OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_defn;
    }

    OGRFeature *GetNextRawFeature()
    {
        if (m_row >= m_window.nYSize && !NextWindow())
        {
            return nullptr;
        }

        std::unique_ptr<OGRFeature> feature;

        while (m_row < m_window.nYSize)
        {
            const double *pSrcVal = reinterpret_cast<double *>(m_buf.data()) +
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
                const auto geomType = m_defn->GetGeomType();
                if (geomType == wkbPoint)
                {
                    double x, y;
                    m_gt.Apply(pixel + 0.5, line + 0.5, &x, &y);

                    geom = std::make_unique<OGRPoint>(x, y);
                    geom->assignSpatialReference(
                        m_defn->GetGeomFieldDefn(0)->GetSpatialRef());
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
                    geom->assignSpatialReference(
                        m_defn->GetGeomFieldDefn(0)->GetSpatialRef());
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

        if (!m_bands.empty())
        {
            const auto nBufTypeSize = GDALGetDataTypeSizeBytes(m_bufType);
            if constexpr (sizeof(int) < sizeof(size_t))
            {
                if (m_window.nYSize > 0 &&
                    static_cast<size_t>(m_window.nXSize) >
                        std::numeric_limits<size_t>::max() / m_window.nYSize)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Failed to allocate buffer");
                    return false;
                }
            }
            const size_t nPixelCount =
                static_cast<size_t>(m_window.nXSize) * m_window.nYSize;
            if (static_cast<size_t>(nBandCount) * nBufTypeSize >
                std::numeric_limits<size_t>::max() / nPixelCount)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Failed to allocate buffer");
                return false;
            }
            const size_t nBufSize = nPixelCount * nBandCount * nBufTypeSize;
            if (m_buf.size() < nBufSize)
            {
                try
                {
                    m_buf.resize(nBufSize);
                }
                catch (const std::exception &)
                {
                    CPLError(CE_Failure, CPLE_OutOfMemory,
                             "Failed to allocate buffer");
                    return false;
                }
            }

            const auto nPixelSpace =
                static_cast<GSpacing>(nBandCount) * nBufTypeSize;
            const auto eErr = m_ds.RasterIO(
                GF_Read, m_window.nXOff, m_window.nYOff, m_window.nXSize,
                m_window.nYSize, m_buf.data(), m_window.nXSize, m_window.nYSize,
                m_bufType, static_cast<int>(m_bands.size()), m_bands.data(),
                nPixelSpace, nPixelSpace * m_window.nXSize, nBufTypeSize,
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
    std::vector<GByte> m_buf{};
    const GDALDataType m_bufType = GDT_Float64;
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
}

bool GDALRasterAsFeaturesAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();

    RasterAsFeaturesOptions options;
    options.geomType = m_geomTypeName == "point"     ? wkbPoint
                       : m_geomTypeName == "polygon" ? wkbPolygon
                                                     : wkbNone;
    options.includeRowCol = m_includeRowCol;
    options.includeXY = m_includeXY;
    options.skipNoData = m_skipNoData;
    options.outputLayerName = m_outputLayerName;

    if (!m_bands.empty())
    {
        options.bands = std::move(m_bands);
    }

    auto poLayer =
        std::make_unique<GDALRasterAsFeaturesLayer>(*poSrcDS, options);
    auto poRetDS = std::make_unique<GDALVectorOutputDataset>();
    poRetDS->AddLayer(std::move(poLayer));

    m_outputDataset.Set(std::move(poRetDS));

    return true;
}

//! @endcond
