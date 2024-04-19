/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C++ classes for the MiraMon driver
 * Author:   Abel Pau
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
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

#ifndef OGRMIRAMON_H_INCLUDED
#define OGRMIRAMON_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "cpl_string.h"
#include "mm_wrlayr.h"

/************************************************************************/
/*                             OGRMiraMonLayer                          */
/************************************************************************/

class OGRMiraMonLayer final
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGRMiraMonLayer>
{
    GDALDataset *m_poDS;
    OGRSpatialReference *m_poSRS;
    OGRFeatureDefn *m_poFeatureDefn;

    GUIntBig m_iNextFID;

    // Pointer to one of three possible MiraMon layers: points,
    // arcs or polygons. Every time a feature is read this pointer
    // points to the appropriate layer
    struct MiraMonVectLayerInfo *phMiraMonLayer;

    // When writing a layer
    struct MiraMonVectLayerInfo hMiraMonLayerPNT;  // MiraMon points layer
    struct MiraMonVectLayerInfo hMiraMonLayerARC;  // MiraMon arcs layer
    struct MiraMonVectLayerInfo hMiraMonLayerPOL;  // MiraMon polygons layer

    // When reading a layer or the result of writing is only a DBF
    struct MiraMonVectLayerInfo hMiraMonLayerReadOrNonGeom;

    struct MiraMonFeature hMMFeature;  // Feature reading/writing

    bool m_bUpdate;

    VSILFILE *m_fp = nullptr;

    // Array of doubles used in the field features processing
    double *padfValues;
    GInt64 *pnInt64Values;

    OGRFeature *GetNextRawFeature();
    OGRFeature *GetFeature(GIntBig nFeatureId) override;
    void GoToFieldOfMultipleRecord(MM_INTERNAL_FID iFID,
                                   MM_EXT_DBF_N_RECORDS nIRecord,
                                   MM_EXT_DBF_N_FIELDS nIField);

    OGRErr MMDumpVertices(OGRGeometryH hGeom, MM_BOOLEAN bExternalRing,
                          MM_BOOLEAN bUseVFG);
    OGRErr MMProcessGeometry(OGRGeometryH poGeom, OGRFeature *poFeature,
                             MM_BOOLEAN bcalculateRecord);
    OGRErr MMProcessMultiGeometry(OGRGeometryH hGeom, OGRFeature *poFeature);
    OGRErr MMLoadGeometry(OGRGeometryH hGeom);
    OGRErr MMWriteGeometry();
    GIntBig GetFeatureCount(int bForce) override;

  public:
    bool bValidFile;

    OGRMiraMonLayer(GDALDataset *poDS, const char *pszFilename, VSILFILE *fp,
                    const OGRSpatialReference *poSRS, int bUpdate,
                    CSLConstList papszOpenOptions,
                    struct MiraMonVectMapInfo *MMMap);
    virtual ~OGRMiraMonLayer();

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRMiraMonLayer)

    OGRErr TranslateFieldsToMM();
    OGRErr TranslateFieldsValuesToMM(OGRFeature *poFeature);
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override;

    OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    int TestCapability(const char *) override;
    void AddToFileList(CPLStringList &oFileList);

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                           OGRMiraMonDataSource                       */
/************************************************************************/

class OGRMiraMonDataSource final : public GDALDataset
{
    std::vector<std::unique_ptr<OGRMiraMonLayer>> m_apoLayers;
    std::string m_osRootName{};
    bool m_bUpdate = false;
    struct MiraMonVectMapInfo m_MMMap;

  public:
    OGRMiraMonDataSource();
    ~OGRMiraMonDataSource();

    bool Open(const char *pszFilename, VSILFILE *fp,
              const OGRSpatialReference *poSRS, CSLConstList papszOpenOptions);
    bool Create(const char *pszFilename, CSLConstList papszOptions);

    int GetLayerCount() override
    {
        return static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int) override;
    char **GetFileList() override;

    OGRLayer *ICreateLayer(const char *pszLayerName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) override;
};

#endif /* OGRMIRAMON_H_INCLUDED */
