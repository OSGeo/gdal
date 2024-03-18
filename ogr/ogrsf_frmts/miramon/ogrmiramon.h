/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API to create a MiraMon layer
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
    OGRSpatialReference *m_poSRS = nullptr;
    OGRFeatureDefn *poFeatureDefn;

    GUIntBig iNextFID;

    // Pointer to one of three possible MiraMon layers: points,
    // arcs or polygons. Every time a feature is read this pointer
    // points to the appropiate layer
    struct MiraMonVectLayerInfo *phMiraMonLayer;

    // When writing a layer
    struct MiraMonVectLayerInfo hMiraMonLayerPNT;  // MiraMon points layer
    struct MiraMonVectLayerInfo hMiraMonLayerARC;  // MiraMon arcs layer
    struct MiraMonVectLayerInfo hMiraMonLayerPOL;  // MiraMon polygons layer

    // When reading a layer or the result of writing is only a DBF
    struct MiraMonVectLayerInfo hMiraMonLayerReadOrNonGeom;

    struct MiraMonFeature hMMFeature;  // Feature reading/writing

    bool bUpdate;

    // Ratio used to enhance certain aspects of memory
    // In some memory settings, a block of 256 or 512 bytes is used.
    // This parameter can be adjusted to achieve
    // nMemoryRatio*256 or nMemoryRatio*512.
    // For example, nMemoryRatio=2 in powerful computers and
    // nMemoryRatio=0.5 in less powerful computers.
    // By increasing this parameter, more memory will be required,
    // but there will be fewer read/write operations to the disk.
    double nMMMemoryRatio;

    VSILFILE *m_fp = nullptr;

    // Array of string or doubles used in the field features processing
    char **papszValues;
    double *padfValues;

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

    OGRMiraMonLayer(const char *pszFilename, VSILFILE *fp,
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
};

/************************************************************************/
/*                           OGRMiraMonDataSource                           */
/************************************************************************/

class OGRMiraMonDataSource final : public OGRDataSource
{
    OGRMiraMonLayer **papoLayers;
    int nLayers;
    char *pszRootName;
    char *pszDSName;
    bool bUpdate;
    struct MiraMonVectMapInfo MMMap;

  public:
    OGRMiraMonDataSource();
    ~OGRMiraMonDataSource();

    int Open(const char *pszFilename, VSILFILE *fp,
             const OGRSpatialReference *poSRS, int bUpdate,
             CSLConstList papszOpenOptions);
    int Create(const char *pszFilename, char **papszOptions);

    const char *GetName() override
    {
        return pszDSName;
    }
    int GetLayerCount() override
    {
        return nLayers;
    }
    OGRLayer *GetLayer(int) override;

    OGRLayer *ICreateLayer(const char *pszLayerName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    int TestCapability(const char *) override;
};

#endif /* OGRMIRAMON_H_INCLUDED */
