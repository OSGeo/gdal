/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Declarations for OGR wrapper classes for GML, and GML<->OGR
 *           translation of geometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GML_H_INCLUDED
#define OGR_GML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "gmlreader.h"
#include "gmlutils.h"

#include <memory>
#include <set>
#include <vector>

class OGRGMLDataSource;

typedef enum
{
    STANDARD,
    SEQUENTIAL_LAYERS,
    INTERLEAVED_LAYERS
} ReadMode;

/************************************************************************/
/*                            OGRGMLLayer                               */
/************************************************************************/

class OGRGMLLayer final : public OGRLayer
{
    OGRFeatureDefn *poFeatureDefn;

    GIntBig m_iNextGMLId = 0;
    bool m_bInvalidFIDFound = false;
    char *m_pszFIDPrefix = nullptr;
    std::set<GIntBig> m_oSetFIDs{};

    bool bWriter;

    OGRGMLDataSource *poDS;

    GMLFeatureClass *poFClass;

    std::unique_ptr<OGRGML_SRSCache, decltype(&OGRGML_SRSCache_Destroy)>
        m_srsCache{OGRGML_SRSCache_Create(), OGRGML_SRSCache_Destroy};

    bool bUseOldFIDFormat;

    bool bFaceHoleNegative;

    CPL_DISALLOW_COPY_ASSIGN(OGRGMLLayer)

  public:
    OGRGMLLayer(const char *pszName, bool bWriter, OGRGMLDataSource *poDS);

    ~OGRGMLLayer() override;

    GDALDataset *GetDataset() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poField,
                                   int bApproxOK = TRUE) override;

    int TestCapability(const char *) const override;
};

/************************************************************************/
/*                           OGRGMLDataSource                           */
/************************************************************************/

class OGRGMLDataSource final : public GDALDataset
{
    OGRLayer **papoLayers;
    int nLayers;

    OGRLayer *TranslateGMLSchema(GMLFeatureClass *);

    char **papszCreateOptions;

    // output related parameters
    VSILFILE *fpOutput;
    bool bFpOutputIsNonSeekable;
    bool bFpOutputSingleFile;
    bool m_bWriteError = false;
    OGREnvelope3D sBoundingRect{};
    bool bBBOX3D;
    int nBoundedByLocation;

    int nSchemaInsertLocation;
    bool bIsOutputGML3;
    bool bIsOutputGML3Deegree; /* if TRUE, then bIsOutputGML3 is also TRUE */
    bool bIsOutputGML32;       /* if TRUE, then bIsOutputGML3 is also TRUE */
    OGRGMLSRSNameFormat eSRSNameFormat;
    bool bWriteSpaceIndentation;

    //! Whether all geometry fields of all layers have the same SRS (or no SRS at all)
    bool m_bWriteGlobalSRS = true;

    //! The global SRS (may be null), that is valid only if m_bWriteGlobalSRS == true
    std::unique_ptr<OGRSpatialReference> m_poWriteGlobalSRS{};

    //! Whether at least one geometry field has been created
    bool m_bWriteGlobalSRSInit = false;

    // input related parameters.
    CPLString osFilename{};
    CPLString osXSDFilename{};
    bool m_bUnlinkXSDFilename = false;

    IGMLReader *poReader;
    bool bOutIsTempFile;

    void InsertHeader();

    bool bExposeGMLId;
    bool bExposeFid;
    bool bIsWFS;

    bool bUseGlobalSRSName;

    bool m_bInvertAxisOrderIfLatLong;
    bool m_bConsiderEPSGAsURN;
    GMLSwapCoordinatesEnum m_eSwapCoordinates;
    bool m_bGetSecondaryGeometryOption;

    ReadMode eReadMode;
    GMLFeature *poStoredGMLFeature;
    OGRGMLLayer *poLastReadLayer;

    bool bEmptyAsNull;

    OGRSpatialReference m_oStandaloneGeomSRS{};
    std::unique_ptr<OGRGeometry> m_poStandaloneGeom{};

    std::vector<std::string> m_aosGMLExtraElements{};

    void FindAndParseTopElements(VSILFILE *fp);
    void SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);

    void BuildJointClassFromXSD();
    void BuildJointClassFromScannedSchema();

    void WriteTopElements();

    // Analyze the OGR_SCHEMA open options and apply changes to the GML reader, return false in case of a critical error
    bool DealWithOgrSchemaOpenOption(const GDALOpenInfo *poOpenInfo);

    CPL_DISALLOW_COPY_ASSIGN(OGRGMLDataSource)

  public:
    OGRGMLDataSource();
    ~OGRGMLDataSource() override;

    bool Open(GDALOpenInfo *poOpenInfo);
    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override;
    bool Create(const char *pszFile, char **papszOptions);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    using GDALDataset::GetLayer;
    const OGRLayer *GetLayer(int) const override;
    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *) const override;

    VSILFILE *GetOutputFP() const
    {
        return fpOutput;
    }

    IGMLReader *GetReader() const
    {
        return poReader;
    }

    void GrowExtents(OGREnvelope3D *psGeomBounds, int nCoordDimension);

    int ExposeId() const
    {
        return bExposeGMLId || bExposeFid;
    }

    void PrintLine(VSILFILE *fp, const char *fmt, ...)
        CPL_PRINT_FUNC_FORMAT(3, 4);

    bool IsGML3Output() const
    {
        return bIsOutputGML3;
    }

    bool IsGML3DeegreeOutput() const
    {
        return bIsOutputGML3Deegree;
    }

    bool IsGML32Output() const
    {
        return bIsOutputGML32;
    }

    /** Returns whether a writing error has occurred */
    inline bool HasWriteError() const
    {
        return m_bWriteError;
    }

    OGRGMLSRSNameFormat GetSRSNameFormat() const
    {
        return eSRSNameFormat;
    }

    bool WriteSpaceIndentation() const
    {
        return bWriteSpaceIndentation;
    }

    const char *GetGlobalSRSName();

    bool GetInvertAxisOrderIfLatLong() const
    {
        return m_bInvertAxisOrderIfLatLong;
    }

    bool GetConsiderEPSGAsURN() const
    {
        return m_bConsiderEPSGAsURN;
    }

    GMLSwapCoordinatesEnum GetSwapCoordinates() const
    {
        return m_eSwapCoordinates;
    }

    bool GetSecondaryGeometryOption() const
    {
        return m_bGetSecondaryGeometryOption;
    }

    ReadMode GetReadMode() const
    {
        return eReadMode;
    }

    void SetStoredGMLFeature(GMLFeature *poStoredGMLFeatureIn)
    {
        poStoredGMLFeature = poStoredGMLFeatureIn;
    }

    GMLFeature *PeekStoredGMLFeature() const
    {
        return poStoredGMLFeature;
    }

    OGRGMLLayer *GetLastReadLayer() const
    {
        return poLastReadLayer;
    }

    void SetLastReadLayer(OGRGMLLayer *poLayer)
    {
        poLastReadLayer = poLayer;
    }

    const char *GetAppPrefix() const;
    bool RemoveAppPrefix() const;
    bool WriteFeatureBoundedBy() const;
    const char *GetSRSDimensionLoc() const;
    bool GMLFeatureCollection() const;

    void DeclareNewWriteSRS(const OGRSpatialReference *poSRS);

    bool HasWriteGlobalSRS() const
    {
        return m_bWriteGlobalSRS;
    }

    OGRLayer *ExecuteSQL(const char *pszSQLCommand,
                         OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poResultsSet) override;

    static bool CheckHeader(const char *pszStr);
};

#endif /* OGR_GML_H_INCLUDED */
