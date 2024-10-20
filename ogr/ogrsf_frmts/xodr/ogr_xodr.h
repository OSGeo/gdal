/******************************************************************************
 * Project:  OpenGIS Simple Features for OpenDRIVE
 * Purpose:  Definition of OGR driver components for OpenDRIVE.
 * Author:   Michael Scholz, German Aerospace Center (DLR)
 *           Gülsen Bardak, German Aerospace Center (DLR)
 *
 ******************************************************************************
 * Copyright 2024 German Aerospace Center (DLR), Institute of Transportation Systems
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once
#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include <iostream>
#include <OpenDriveMap.h>
#include <pugixml/pugixml.hpp>
#include <vector>

struct RoadElements
{
    /* Map of road to its original OpenDRIVE ID for fast lookup. */
    std::map<std::string, odr::Road> roads{};
    std::vector<odr::Line3D> referenceLines{};

    std::vector<odr::Lane> lanes{};
    std::vector<odr::LaneSection> laneSections{};
    std::vector<std::string> laneRoadIDs{};
    std::vector<odr::Mesh3D> laneMeshes{};

    std::vector<odr::Line3D> laneLinesInner{};
    std::vector<odr::Line3D> laneLinesOuter{};

    std::vector<odr::RoadMark> roadMarks{};
    std::vector<odr::Mesh3D> roadMarkMeshes{};

    std::vector<odr::RoadObject> roadObjects{};
    std::vector<odr::Mesh3D> roadObjectMeshes{};

    std::vector<odr::RoadSignal> roadSignals{};
    std::vector<odr::Mesh3D> roadSignalMeshes{};
};

/*--------------------------------------------------------------------*/
/*---------------------  Layer declarations  -------------------------*/
/*--------------------------------------------------------------------*/

class OGRXODRLayer : public OGRLayer
{
  private:
    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn.get();
    }

    /**
     * Initializes XODR road elements and iterators.
    */
    void resetRoadElementIterators();

  protected:
    RoadElements m_roadElements{};
    bool m_bDissolveTIN{false};
    OGRSpatialReference m_oSRS{};
    /* Unique feature ID which is automatically incremented for any new road feature creation. */
    int m_nNextFID{0};

    std::map<std::string, odr::Road>::iterator m_roadIter{};
    std::vector<odr::Line3D>::iterator m_referenceLineIter{};

    std::vector<odr::Lane>::iterator m_laneIter{};
    std::vector<odr::LaneSection>::iterator m_laneSectionIter{};
    std::vector<std::string>::iterator m_laneRoadIDIter{};
    std::vector<odr::Mesh3D>::iterator m_laneMeshIter{};

    std::vector<odr::Line3D>::iterator m_laneLinesInnerIter{};
    std::vector<odr::Line3D>::iterator m_laneLinesOuterIter{};

    std::vector<odr::RoadMark>::iterator m_roadMarkIter{};
    std::vector<odr::Mesh3D>::iterator m_roadMarkMeshIter{};

    std::vector<odr::RoadObject>::iterator m_roadObjectIter{};
    std::vector<odr::Mesh3D>::iterator m_roadObjectMeshesIter{};

    std::vector<odr::RoadSignal>::iterator m_roadSignalIter{};
    std::vector<odr::Mesh3D>::iterator m_roadSignalMeshesIter{};

    std::unique_ptr<OGRFeatureDefn> m_poFeatureDefn{};

    /**
     * Builds an ordinary TIN from libOpenDRIVE's mesh.
    */
    std::unique_ptr<OGRTriangulatedSurface>
    triangulateSurface(const odr::Mesh3D &mesh);

  public:
    OGRXODRLayer(const RoadElements &xodrRoadElements,
                 const std::string &proj4Defn);
    /**
     * \param dissolveTriangulatedSurface True if original triangulated surface meshes from
     * libOpenDRIVE are to be dissolved into simpler geometries.
     * Only applicable for layer types derived from meshes.
    */
    OGRXODRLayer(const RoadElements &xodrRoadElements,
                 const std::string &proj4Defn,
                 const bool dissolveTriangulatedSurface);
    void ResetReading() override;
};

class OGRXODRLayerReferenceLine
    : public OGRXODRLayer,
      public OGRGetNextFeatureThroughRaw<OGRXODRLayerReferenceLine>
{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "ReferenceLine";

    OGRXODRLayerReferenceLine(const RoadElements &xodrRoadElements,
                              const std::string &proj4Defn);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerReferenceLine)
};

class OGRXODRLayerLaneBorder
    : public OGRXODRLayer,
      public OGRGetNextFeatureThroughRaw<OGRXODRLayerLaneBorder>
{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "LaneBorder";

    OGRXODRLayerLaneBorder(const RoadElements &xodrRoadElements,
                           const std::string &proj4Defn);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerLaneBorder)
};

class OGRXODRLayerRoadMark
    : public OGRXODRLayer,
      public OGRGetNextFeatureThroughRaw<OGRXODRLayerRoadMark>

{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "RoadMark";

    OGRXODRLayerRoadMark(const RoadElements &xodrRoadElements,
                         const std::string &proj4Defn,
                         const bool dissolveTriangulatedSurface);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerRoadMark)
};

class OGRXODRLayerRoadObject
    : public OGRXODRLayer,
      public OGRGetNextFeatureThroughRaw<OGRXODRLayerRoadObject>
{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "RoadObject";

    OGRXODRLayerRoadObject(const RoadElements &xodrRoadElements,
                           const std::string &proj4Defn);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerRoadObject)
};

class OGRXODRLayerRoadSignal
    : public OGRXODRLayer,
      public OGRGetNextFeatureThroughRaw<OGRXODRLayerRoadSignal>
{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "RoadSignal";

    OGRXODRLayerRoadSignal(const RoadElements &xodrRoadElements,
                           const std::string &proj4Defn,
                           const bool dissolveTriangulatedSurface);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerRoadSignal)
};

class OGRXODRLayerLane : public OGRXODRLayer,
                         public OGRGetNextFeatureThroughRaw<OGRXODRLayerLane>
{
  protected:
    OGRFeature *GetNextRawFeature();

  public:
    const std::string FEATURE_CLASS_NAME = "Lane";

    OGRXODRLayerLane(const RoadElements &xodrRoadElements,
                     const std::string &proj4Defn,
                     const bool dissolveTriangulatedSurface);
    virtual int TestCapability(const char *pszCap) override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRXODRLayerLane)
};

/*--------------------------------------------------------------------*/
/*--------------------  Data source declarations ----------------------*/
/*--------------------------------------------------------------------*/

class OGRXODRDataSource : public GDALDataset
{
  private:
    std::vector<std::unique_ptr<OGRXODRLayer>> m_apoLayers{};

    /**
     * Approximation factor for sampling of continuous geometry functions into discrete
     * OGC Simple Feature geometries.
    */
    double m_dfEpsilon{1.0};

    /**
     * Retrieves all necessary road elements from the underlying OpenDRIVE structure.
     *
     * \param roads Roads of the dataset.
    */
    RoadElements createRoadElements(const std::vector<odr::Road> &roads);

  public:
    bool Open(const char *pszFilename, CSLConstList openOptions);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;

    virtual int TestCapability(const char *pszCap) override;
};
