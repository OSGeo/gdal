/******************************************************************************
 * Project:  GDAL
 * Purpose:  Implements The Two-Arm Chains EdgeTracing Algorithm
 * Author:   kikitte.lee
 *
 ******************************************************************************
 * Copyright (c) 2023, kikitte.lee <kikitte.lee@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef POLYGONIZE_POLYGONIZER_H_INCLUDED
#define POLYGONIZE_POLYGONIZER_H_INCLUDED

/*! @cond Doxygen_Suppress */

// Implements Junhua Teng, Fahui Wang, Yu Liu: An Efficient Algorithm for
// Raster-to-Vector Data Conversion: https://doi.org/10.1080/10824000809480639

#include <array>
#include <cstdint>
#include <vector>
#include <limits>
#include <map>

#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace gdal
{
namespace polygonizer
{

using IndexType = std::uint32_t;
using Point = std::array<IndexType, 2>;
using Arc = std::vector<Point>;

struct IndexedArc
{
    Arc *poArc;
    std::size_t iIndex;
};

/**
 * A raster polygon(RPolygon) is formed by a list of arcs in order.
 * Each arc has two properties:
 *     1. does the arc follows the right-hand rule respect to the area it bounds
 *     2. the next arc of the current arc
 */
struct RPolygon
{
    IndexType iBottomRightRow{0};
    IndexType iBottomRightCol{0};

    struct ArcStruct
    {
        std::unique_ptr<Arc> poArc{};
        // each element is the next arc index of the current arc
        unsigned nConnection = 0;
        // does arc follows the right-hand rule with
        bool bFollowRighthand = false;

        ArcStruct(unsigned nConnectionIn, bool bFollowRighthandIn)
            : poArc(std::make_unique<Arc>()), nConnection(nConnectionIn),
              bFollowRighthand(bFollowRighthandIn)
        {
        }
    };

    // arc object list
    std::vector<ArcStruct> oArcs{};

    RPolygon() = default;

    RPolygon(const RPolygon &) = delete;

    RPolygon &operator=(const RPolygon &) = delete;

    /**
     * create a new arc object
     */
    IndexedArc newArc(bool bFollowRighthand);

    /**
     * set the next arc index of the current arc
     */
    void setArcConnection(const IndexedArc &oArc, const IndexedArc &oNextArc);

    /**
     * update the bottom-right most cell index of the current polygon
     */
    void updateBottomRightPos(IndexType iRow, IndexType iCol);
};

/**
 * Arm class is used to record the tracings of both arcs and polygons.
 */
struct TwoArm
{
    IndexType iRow{0};
    IndexType iCol{0};

    RPolygon *poPolyInside{nullptr};
    RPolygon *poPolyAbove{nullptr};
    RPolygon *poPolyLeft{nullptr};

    IndexedArc oArcHorOuter{};
    IndexedArc oArcHorInner{};
    IndexedArc oArcVerInner{};
    IndexedArc oArcVerOuter{};

    bool bSolidHorizontal{false};
    bool bSolidVertical{false};
};

template <typename DataType> class PolygonReceiver
{
  public:
    PolygonReceiver() = default;

    PolygonReceiver(const PolygonReceiver<DataType> &) = delete;

    virtual ~PolygonReceiver() = default;

    PolygonReceiver<DataType> &
    operator=(const PolygonReceiver<DataType> &) = delete;

    virtual void receive(RPolygon *poPolygon, DataType nPolygonCellValue) = 0;
};

/**
 * Polygonizer is used to manage polygon memory and do the edge tracing process
 */
template <typename PolyIdType, typename DataType> class Polygonizer
{
  public:
    static constexpr PolyIdType THE_OUTER_POLYGON_ID =
        std::numeric_limits<PolyIdType>::max();

  private:
    using PolygonMap = std::map<PolyIdType, RPolygon *>;
    using PolygonMapEntry = typename PolygonMap::value_type;

    PolyIdType nInvalidPolyId_;
    RPolygon *poTheOuterPolygon_{nullptr};
    PolygonMap oPolygonMap_{};

    PolygonReceiver<DataType> *poPolygonReceiver_;

    RPolygon *getPolygon(PolyIdType nPolygonId);

    RPolygon *createPolygon(PolyIdType nPolygonId);

    void destroyPolygon(PolyIdType nPolygonId);

  public:
    explicit Polygonizer(PolyIdType nInvalidPolyId,
                         PolygonReceiver<DataType> *poPolygonReceiver);

    Polygonizer(const Polygonizer<PolyIdType, DataType> &) = delete;

    ~Polygonizer();

    Polygonizer<PolyIdType, DataType> &
    operator=(const Polygonizer<PolyIdType, DataType> &) = delete;

    RPolygon *getTheOuterPolygon() const
    {
        return poTheOuterPolygon_;
    }

    bool processLine(const PolyIdType *panThisLineId,
                     const DataType *panLastLineVal, TwoArm *poThisLineArm,
                     TwoArm *poLastLineArm, IndexType nCurrentRow,
                     IndexType nCols);
};

/**
 * Write raster polygon object to OGR layer.
 */
template <typename DataType>
class OGRPolygonWriter : public PolygonReceiver<DataType>
{
    OGRLayer *poOutLayer_ = nullptr;
    int iPixValField_;
    double *padfGeoTransform_;
    std::unique_ptr<OGRFeature> poFeature_{};
    OGRPolygon *poPolygon_ =
        nullptr;  // = poFeature_->GetGeometryRef(), owned by poFeature

    CPLErr eErr_{CE_None};

  public:
    OGRPolygonWriter(OGRLayerH hOutLayer, int iPixValField,
                     double *padfGeoTransform);

    OGRPolygonWriter(const OGRPolygonWriter<DataType> &) = delete;

    ~OGRPolygonWriter() = default;

    OGRPolygonWriter<DataType> &
    operator=(const OGRPolygonWriter<DataType> &) = delete;

    void receive(RPolygon *poPolygon, DataType nPolygonCellValue) override;

    inline CPLErr getErr()
    {
        return eErr_;
    }
};

}  // namespace polygonizer
}  // namespace gdal

/*! @endcond */

#endif /* POLYGONIZE_POLYGONIZER_H_INCLUDED */
