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

/*! @cond Doxygen_Suppress */

#include "polygonize_polygonizer.h"

#include <algorithm>

namespace gdal
{
namespace polygonizer
{
IndexedArc RPolygon::newArc(bool bFollowRighthand)
{
    const std::size_t iArcIndex = oArcs.size();
    const auto &oNewArc =
        oArcs.emplace_back(static_cast<unsigned>(iArcIndex), bFollowRighthand);
    return IndexedArc{oNewArc.poArc.get(), iArcIndex};
}

void RPolygon::setArcConnection(const IndexedArc &oArc,
                                const IndexedArc &oNextArc)
{
    oArcs[oArc.iIndex].nConnection = static_cast<unsigned>(oNextArc.iIndex);
}

void RPolygon::updateBottomRightPos(IndexType iRow, IndexType iCol)
{
    iBottomRightRow = iRow;
    iBottomRightCol = iCol;
}

/**
 * Process different kinds of Arm connections.
 */
static void ProcessArmConnections(TwoArm *poCurrent, TwoArm *poAbove,
                                  TwoArm *poLeft)
{
    poCurrent->poPolyInside->updateBottomRightPos(poCurrent->iRow,
                                                  poCurrent->iCol);
    poCurrent->bSolidVertical = poCurrent->poPolyInside != poLeft->poPolyInside;
    poCurrent->bSolidHorizontal =
        poCurrent->poPolyInside != poAbove->poPolyInside;
    poCurrent->poPolyAbove = poAbove->poPolyInside;
    poCurrent->poPolyLeft = poLeft->poPolyInside;

    constexpr int BIT_CUR_HORIZ = 0;
    constexpr int BIT_CUR_VERT = 1;
    constexpr int BIT_LEFT = 2;
    constexpr int BIT_ABOVE = 3;

    const int nArmConnectionType =
        (static_cast<int>(poAbove->bSolidVertical) << BIT_ABOVE) |
        (static_cast<int>(poLeft->bSolidHorizontal) << BIT_LEFT) |
        (static_cast<int>(poCurrent->bSolidVertical) << BIT_CUR_VERT) |
        (static_cast<int>(poCurrent->bSolidHorizontal) << BIT_CUR_HORIZ);

    constexpr int VIRTUAL = 0;
    constexpr int SOLID = 1;

    constexpr int ABOVE_VIRTUAL = VIRTUAL << BIT_ABOVE;
    constexpr int ABOVE_SOLID = SOLID << BIT_ABOVE;

    constexpr int LEFT_VIRTUAL = VIRTUAL << BIT_LEFT;
    constexpr int LEFT_SOLID = SOLID << BIT_LEFT;

    constexpr int CUR_VERT_VIRTUAL = VIRTUAL << BIT_CUR_VERT;
    constexpr int CUR_VERT_SOLID = SOLID << BIT_CUR_VERT;

    constexpr int CUR_HORIZ_VIRTUAL = VIRTUAL << BIT_CUR_HORIZ;
    constexpr int CUR_HORIZ_SOLID = SOLID << BIT_CUR_HORIZ;

    /**
     * There are 12 valid connection types depending on the arm types(virtual or solid)
     * The following diagram illustrates these kinds of connection types, ⇢⇣ means virtual arm, →↓ means solid arm.
     *     ⇣        ⇣          ⇣         ⇣        ↓
     *    ⇢ →      → →        → ⇢       → →      ⇢ →
     *     ↓        ⇣          ↓         ↓        ⇣
     *   type=3    type=5    type=6    type=7    type=9
     *
     *     ↓        ↓          ↓         ↓          ↓
     *    ⇢ ⇢      ⇢ →        → ⇢       → →        → ⇢
     *     ↓        ↓          ⇣         ⇣          ↓
     *   type=10  type=11    type=12    type=13   type=14
     *
     *     ↓        ⇣
     *    → →      ⇢ ⇢
     *     ↓        ⇣
     *   type=15  type=0
     *
     *   For each connection type, we may create new arc, ,
     *   Depending on the connection type, we may do the following things:
     *       1. Create new arc. If the arc is closed to the inner polygon, it is called "Inner Arc", otherwise "Outer Arc"
     *       2. Pass an arc to the next arm.
     *       3. "Close" two arcs. If two arcs meet at the bottom right corner of a cell, close them by recording the arc connection.
     *       4. Add grid position(row, col) to an arc.
     */

    switch (nArmConnectionType)
    {
        case ABOVE_VIRTUAL | LEFT_VIRTUAL | CUR_VERT_VIRTUAL |
            CUR_HORIZ_VIRTUAL:  // 0
            // nothing to do
            break;

        case ABOVE_VIRTUAL | LEFT_VIRTUAL | CUR_VERT_SOLID |
            CUR_HORIZ_SOLID:  // 3
            // add inner arcs
            poCurrent->oArcVerInner = poCurrent->poPolyInside->newArc(true);
            poCurrent->oArcHorInner = poCurrent->poPolyInside->newArc(false);
            poCurrent->poPolyInside->setArcConnection(poCurrent->oArcHorInner,
                                                      poCurrent->oArcVerInner);
            poCurrent->oArcVerInner.poArc->emplace_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            // add outer arcs
            poCurrent->oArcHorOuter = poAbove->poPolyInside->newArc(true);
            poCurrent->oArcVerOuter = poAbove->poPolyInside->newArc(false);
            poAbove->poPolyInside->setArcConnection(poCurrent->oArcVerOuter,
                                                    poCurrent->oArcHorOuter);
            poCurrent->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_VIRTUAL | LEFT_SOLID | CUR_VERT_VIRTUAL |
            CUR_HORIZ_SOLID:  // 5
            // pass arcs
            poCurrent->oArcHorInner = poLeft->oArcHorInner;
            poCurrent->oArcHorOuter = poLeft->oArcHorOuter;

            break;
        case ABOVE_VIRTUAL | LEFT_SOLID | CUR_VERT_SOLID |
            CUR_HORIZ_VIRTUAL:  // 6
            // pass arcs
            poCurrent->oArcVerInner = poLeft->oArcHorOuter;
            poCurrent->oArcVerOuter = poLeft->oArcHorInner;
            poCurrent->oArcVerInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poCurrent->oArcVerOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_VIRTUAL | LEFT_SOLID | CUR_VERT_SOLID |
            CUR_HORIZ_SOLID:  // 7
            // pass arcs
            poCurrent->oArcHorOuter = poLeft->oArcHorOuter;
            poCurrent->oArcVerOuter = poLeft->oArcHorInner;
            poLeft->oArcHorInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            // add inner arcs
            poCurrent->oArcVerInner = poCurrent->poPolyInside->newArc(true);
            poCurrent->oArcHorInner = poCurrent->poPolyInside->newArc(false);
            poCurrent->poPolyInside->setArcConnection(poCurrent->oArcHorInner,
                                                      poCurrent->oArcVerInner);
            poCurrent->oArcVerInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_SOLID | LEFT_VIRTUAL | CUR_VERT_VIRTUAL |
            CUR_HORIZ_SOLID:  // 9
            // pass arcs
            poCurrent->oArcHorOuter = poAbove->oArcVerInner;
            poCurrent->oArcHorInner = poAbove->oArcVerOuter;
            poCurrent->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poCurrent->oArcHorInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_SOLID | LEFT_VIRTUAL | CUR_VERT_SOLID |
            CUR_HORIZ_VIRTUAL:  // 10
            // pass arcs
            poCurrent->oArcVerInner = poAbove->oArcVerInner;
            poCurrent->oArcVerOuter = poAbove->oArcVerOuter;

            break;
        case ABOVE_SOLID | LEFT_VIRTUAL | CUR_VERT_SOLID |
            CUR_HORIZ_SOLID:  // 11
            // pass arcs
            poCurrent->oArcHorOuter = poAbove->oArcVerInner;
            poCurrent->oArcVerOuter = poAbove->oArcVerOuter;
            poCurrent->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            // add inner arcs
            poCurrent->oArcVerInner = poCurrent->poPolyInside->newArc(true);
            poCurrent->oArcHorInner = poCurrent->poPolyInside->newArc(false);
            poCurrent->poPolyInside->setArcConnection(poCurrent->oArcHorInner,
                                                      poCurrent->oArcVerInner);
            poCurrent->oArcVerInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_SOLID | LEFT_SOLID | CUR_VERT_VIRTUAL |
            CUR_HORIZ_VIRTUAL:  // 12
            // close arcs
            poLeft->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poLeft->poPolyAbove->setArcConnection(poLeft->oArcHorOuter,
                                                  poAbove->oArcVerOuter);
            // close arcs
            poAbove->oArcVerInner.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poCurrent->poPolyInside->setArcConnection(poAbove->oArcVerInner,
                                                      poLeft->oArcHorInner);

            break;
        case ABOVE_SOLID | LEFT_SOLID | CUR_VERT_VIRTUAL |
            CUR_HORIZ_SOLID:  // 13
            // close arcs
            poLeft->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poLeft->poPolyAbove->setArcConnection(poLeft->oArcHorOuter,
                                                  poAbove->oArcVerOuter);
            // pass arcs
            poCurrent->oArcHorOuter = poAbove->oArcVerInner;
            poCurrent->oArcHorInner = poLeft->oArcHorInner;
            poCurrent->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_SOLID | LEFT_SOLID | CUR_VERT_SOLID |
            CUR_HORIZ_VIRTUAL:  // 14
            // close arcs
            poLeft->oArcHorOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});
            poLeft->poPolyAbove->setArcConnection(poLeft->oArcHorOuter,
                                                  poAbove->oArcVerOuter);
            // pass arcs
            poCurrent->oArcVerInner = poAbove->oArcVerInner;
            poCurrent->oArcVerOuter = poLeft->oArcHorInner;
            poCurrent->oArcVerOuter.poArc->push_back(
                Point{poCurrent->iRow, poCurrent->iCol});

            break;
        case ABOVE_SOLID | LEFT_SOLID | CUR_VERT_SOLID | CUR_HORIZ_SOLID:  // 15
            // Tow pixels of the main diagonal belong to the same polygon
            if (poAbove->poPolyLeft == poCurrent->poPolyInside)
            {
                // pass arcs
                poCurrent->oArcVerInner = poLeft->oArcHorOuter;
                poCurrent->oArcHorInner = poAbove->oArcVerOuter;
                poCurrent->oArcVerInner.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
                poCurrent->oArcHorInner.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
            }
            else
            {
                // close arcs
                poLeft->oArcHorOuter.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
                poLeft->poPolyAbove->setArcConnection(poLeft->oArcHorOuter,
                                                      poAbove->oArcVerOuter);
                // add inner arcs
                poCurrent->oArcVerInner = poCurrent->poPolyInside->newArc(true);
                poCurrent->oArcHorInner =
                    poCurrent->poPolyInside->newArc(false);
                poCurrent->poPolyInside->setArcConnection(
                    poCurrent->oArcHorInner, poCurrent->oArcVerInner);
                poCurrent->oArcVerInner.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
            }

            // Tow pixels of the secondary diagonal belong to the same polygon
            if (poAbove->poPolyInside == poLeft->poPolyInside)
            {
                // close arcs
                poAbove->poPolyInside->setArcConnection(poAbove->oArcVerInner,
                                                        poLeft->oArcHorInner);
                poAbove->oArcVerInner.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
                // add outer arcs
                poCurrent->oArcHorOuter = poAbove->poPolyInside->newArc(true);
                poCurrent->oArcVerOuter = poAbove->poPolyInside->newArc(false);
                poCurrent->oArcHorOuter.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
                poAbove->poPolyInside->setArcConnection(
                    poCurrent->oArcVerOuter, poCurrent->oArcHorOuter);
            }
            else
            {
                // pass arcs
                poCurrent->oArcHorOuter = poAbove->oArcVerInner;
                poCurrent->oArcVerOuter = poLeft->oArcHorInner;
                poCurrent->oArcHorOuter.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
                poCurrent->oArcVerOuter.poArc->push_back(
                    Point{poCurrent->iRow, poCurrent->iCol});
            }

            break;

        case ABOVE_VIRTUAL | LEFT_VIRTUAL | CUR_VERT_VIRTUAL |
            CUR_HORIZ_SOLID:  // 1
        case ABOVE_VIRTUAL | LEFT_VIRTUAL | CUR_VERT_SOLID |
            CUR_HORIZ_VIRTUAL:  // 2
        case ABOVE_VIRTUAL | LEFT_SOLID | CUR_VERT_VIRTUAL |
            CUR_HORIZ_VIRTUAL:  // 4
        default:
            // Impossible case
            CPLAssert(false);
            break;
    }
}

template <typename PolyIdType, typename DataType>
Polygonizer<PolyIdType, DataType>::Polygonizer(
    PolyIdType nInvalidPolyId, PolygonReceiver<DataType> *poPolygonReceiver)
    : nInvalidPolyId_(nInvalidPolyId), poPolygonReceiver_(poPolygonReceiver)
{
    poTheOuterPolygon_ = createPolygon(THE_OUTER_POLYGON_ID);
}

template <typename PolyIdType, typename DataType>
Polygonizer<PolyIdType, DataType>::~Polygonizer()
{
    // cppcheck-suppress constVariableReference
    for (auto &pair : oPolygonMap_)
    {
        delete pair.second;
    }
}

template <typename PolyIdType, typename DataType>
RPolygon *Polygonizer<PolyIdType, DataType>::getPolygon(PolyIdType nPolygonId)
{
    const auto oIter = oPolygonMap_.find(nPolygonId);
    if (oIter == oPolygonMap_.end())
    {
        return createPolygon(nPolygonId);
    }
    else
    {
        return oIter->second;
    }
}

template <typename PolyIdType, typename DataType>
RPolygon *
Polygonizer<PolyIdType, DataType>::createPolygon(PolyIdType nPolygonId)
{
    auto polygon = new RPolygon();
    oPolygonMap_[nPolygonId] = polygon;
    return polygon;
}

template <typename PolyIdType, typename DataType>
void Polygonizer<PolyIdType, DataType>::destroyPolygon(PolyIdType nPolygonId)
{
    const auto oIter = oPolygonMap_.find(nPolygonId);
    CPLAssert(oIter != oPolygonMap_.end());
    delete oIter->second;
    oPolygonMap_.erase(oIter);
}

template <typename PolyIdType, typename DataType>
bool Polygonizer<PolyIdType, DataType>::processLine(
    const PolyIdType *panThisLineId, const DataType *panLastLineVal,
    TwoArm *poThisLineArm, TwoArm *poLastLineArm, const IndexType nCurrentRow,
    const IndexType nCols)
{
    TwoArm *poCurrent, *poAbove, *poLeft;

    try
    {
        poCurrent = poThisLineArm + 1;
        poCurrent->iRow = nCurrentRow;
        poCurrent->iCol = 0;
        poCurrent->poPolyInside = getPolygon(panThisLineId[0]);
        poAbove = poLastLineArm + 1;
        poLeft = poThisLineArm;
        poLeft->poPolyInside = poTheOuterPolygon_;
        ProcessArmConnections(poCurrent, poAbove, poLeft);
        for (IndexType col = 1; col < nCols; ++col)
        {
            IndexType iArmIndex = col + 1;
            poCurrent = poThisLineArm + iArmIndex;
            poCurrent->iRow = nCurrentRow;
            poCurrent->iCol = col;
            poCurrent->poPolyInside = getPolygon(panThisLineId[col]);
            poAbove = poLastLineArm + iArmIndex;
            poLeft = poThisLineArm + iArmIndex - 1;
            ProcessArmConnections(poCurrent, poAbove, poLeft);
        }
        poCurrent = poThisLineArm + nCols + 1;
        poCurrent->iRow = nCurrentRow;
        poCurrent->iCol = nCols;
        poCurrent->poPolyInside = poTheOuterPolygon_;
        poAbove = poLastLineArm + nCols + 1;
        poAbove->poPolyInside = poTheOuterPolygon_;
        poLeft = poThisLineArm + nCols;
        ProcessArmConnections(poCurrent, poAbove, poLeft);

        /**
         *
         * Find those polygons haven't been processed on this line as we can be sure they are completed
         *
         */
        std::vector<PolygonMapEntry> oCompletedPolygons;
        for (auto &entry : oPolygonMap_)
        {
            RPolygon *poPolygon = entry.second;

            if (poPolygon->iBottomRightRow + 1 == nCurrentRow)
            {
                oCompletedPolygons.push_back(entry);
            }
        }
        // cppcheck-suppress constVariableReference
        for (auto &entry : oCompletedPolygons)
        {
            PolyIdType nPolyId = entry.first;
            RPolygon *poPolygon = entry.second;

            // emit valid polygon only
            if (nPolyId != nInvalidPolyId_)
            {
                poPolygonReceiver_->receive(
                    poPolygon, panLastLineVal[poPolygon->iBottomRightCol]);
            }

            destroyPolygon(nPolyId);
        }
        return true;
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Out of memory in Polygonizer::processLine");
        return false;
    }
}

template <typename DataType>
OGRPolygonWriter<DataType>::OGRPolygonWriter(OGRLayerH hOutLayer,
                                             int iPixValField,
                                             double *padfGeoTransform)
    : PolygonReceiver<DataType>(), poOutLayer_(OGRLayer::FromHandle(hOutLayer)),
      iPixValField_(iPixValField), padfGeoTransform_(padfGeoTransform)
{
    poFeature_ = std::make_unique<OGRFeature>(poOutLayer_->GetLayerDefn());
    poPolygon_ = new OGRPolygon();
    poFeature_->SetGeometryDirectly(poPolygon_);
}

template <typename DataType>
void OGRPolygonWriter<DataType>::receive(RPolygon *poPolygon,
                                         DataType nPolygonCellValue)
{
    std::vector<bool> oAccessedArc(poPolygon->oArcs.size(), false);
    double *padfGeoTransform = padfGeoTransform_;

    OGRLinearRing *poFirstRing = poPolygon_->getExteriorRing();
    if (poFirstRing && poPolygon_->getNumInteriorRings() == 0)
    {
        poFirstRing->empty();
    }
    else
    {
        poFirstRing = nullptr;
        poPolygon_->empty();
    }

    auto AddRingToPolygon =
        [this, &poPolygon, &oAccessedArc,
         padfGeoTransform](std::size_t iFirstArcIndex, OGRLinearRing *poRing)
    {
        std::unique_ptr<OGRLinearRing> poNewRing;
        if (!poRing)
        {
            poNewRing = std::make_unique<OGRLinearRing>();
            poRing = poNewRing.get();
        }

        auto AddArcToRing =
            [&poPolygon, poRing, padfGeoTransform](std::size_t iArcIndex)
        {
            const auto &oArc = poPolygon->oArcs[iArcIndex];
            const bool bArcFollowRighthand = oArc.bFollowRighthand;
            const int nArcPointCount = static_cast<int>(oArc.poArc->size());
            int nDstPointIdx = poRing->getNumPoints();
            poRing->setNumPoints(nDstPointIdx + nArcPointCount,
                                 /* bZeroizeNewContent = */ false);
            if (poRing->getNumPoints() < nDstPointIdx + nArcPointCount)
            {
                return false;
            }
            for (int i = 0; i < nArcPointCount; ++i)
            {
                const Point &oPixel =
                    (*oArc.poArc)[bArcFollowRighthand
                                      ? i
                                      : (nArcPointCount - i - 1)];

                const double dfX = padfGeoTransform[0] +
                                   oPixel[1] * padfGeoTransform[1] +
                                   oPixel[0] * padfGeoTransform[2];
                const double dfY = padfGeoTransform[3] +
                                   oPixel[1] * padfGeoTransform[4] +
                                   oPixel[0] * padfGeoTransform[5];

                poRing->setPoint(nDstPointIdx, dfX, dfY);
                ++nDstPointIdx;
            }
            return true;
        };

        if (!AddArcToRing(iFirstArcIndex))
        {
            return false;
        }

        std::size_t iArcIndex = iFirstArcIndex;
        std::size_t iNextArcIndex = poPolygon->oArcs[iArcIndex].nConnection;
        oAccessedArc[iArcIndex] = true;
        while (iNextArcIndex != iFirstArcIndex)
        {
            if (!AddArcToRing(iNextArcIndex))
            {
                return false;
            }
            iArcIndex = iNextArcIndex;
            iNextArcIndex = poPolygon->oArcs[iArcIndex].nConnection;
            oAccessedArc[iArcIndex] = true;
        }

        // close ring manually
        poRing->closeRings();

        if (poNewRing)
            poPolygon_->addRingDirectly(poNewRing.release());
        return true;
    };

    for (size_t i = 0; i < oAccessedArc.size(); ++i)
    {
        if (!oAccessedArc[i])
        {
            if (!AddRingToPolygon(i, poFirstRing))
            {
                eErr_ = CE_Failure;
                return;
            }
            poFirstRing = nullptr;
        }
    }

    // Create the feature object
    poFeature_->SetFID(OGRNullFID);
    if (iPixValField_ >= 0)
        poFeature_->SetField(iPixValField_,
                             static_cast<double>(nPolygonCellValue));

    // Write the to the layer.
    if (poOutLayer_->CreateFeature(poFeature_.get()) != OGRERR_NONE)
        eErr_ = CE_Failure;

    // Shouldn't happen for well behaved drivers, but better check...
    else if (poFeature_->GetGeometryRef() != poPolygon_)
    {
        poPolygon_ = new OGRPolygon();
        poFeature_->SetGeometryDirectly(poPolygon_);
    }
}

}  // namespace polygonizer
}  // namespace gdal

/*! @endcond */
