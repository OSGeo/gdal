/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Definition of classes for OGR LVBAG driver.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_LVBAG_H_INCLUDED
#define OGR_LVBAG_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_expat.h"
#include "ogrlayerpool.h"

namespace OGRLVBAG
{

typedef enum
{
    LYR_RAW,
    LYR_UNION
} LayerType;

/**
 * Layer pool unique pointer.
 */
using LayerPoolUniquePtr = std::unique_ptr<OGRLayerPool>;

/**
 * Vector holding pointers to OGRLayer.
 */
using LayerVector = std::vector<std::pair<LayerType, OGRLayerUniquePtr>>;

}  // namespace OGRLVBAG

/************************************************************************/
/*                            OGRLVBAGLayer                             */
/************************************************************************/
constexpr int PARSER_BUF_SIZE = 8192;

class OGRLVBAGLayer final : public OGRAbstractProxiedLayer,
                            public OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>
{
    CPL_DISALLOW_COPY_ASSIGN(OGRLVBAGLayer)

    OGRFeatureDefn *poFeatureDefn;
    OGRFeature *m_poFeature = nullptr;
    VSILFILE *fp;
    CPLString osFilename;

    typedef enum
    {
        FD_OPENED,
        FD_CLOSED,
        FD_CANNOT_REOPEN
    } FileDescriptorState;

    FileDescriptorState eFileDescriptorsState;

    OGRExpatUniquePtr oParser;

    mutable bool bSchemaOnly;
    bool bHasReadSchema;
    bool bFixInvalidData;
    bool bLegacyId;

    typedef enum
    {
        ADDRESS_PRIMARY,
        ADDRESS_SECONDARY,
    } AddressRefState;

    int nNextFID;
    int nCurrentDepth;
    int nGeometryElementDepth;
    int nFeatureCollectionDepth;
    int nFeatureElementDepth;
    int nAttributeElementDepth;

    AddressRefState eAddressRefState;

    CPLString osElementString;
    CPLString osAttributeString;
    bool bCollectData;

    std::vector<char> aBuf = std::vector<char>(PARSER_BUF_SIZE);

    void AddSpatialRef(OGRwkbGeometryType eTypeIn);
    void AddOccurrenceFieldDefn();
    void AddIdentifierFieldDefn();
    void AddDocumentFieldDefn();
    void CreateFeatureDefn(const char *);

    void ConfigureParser();
    void ParseDocument();
    bool IsParserFinished(XML_Status status);

    void StartElementCbk(const char *, const char **);
    void EndElementCbk(const char *);
    void DataHandlerCbk(const char *, int);

    void StartDataCollect();
    void StopDataCollect();

    bool TouchLayer();
    void CloseUnderlyingLayer() override;

    OGRFeature *GetNextRawFeature();

    friend class OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>;
    friend class OGRLVBAGDataSource;

  public:
    explicit OGRLVBAGLayer(const char *pszFilename, OGRLayerPool *poPoolIn,
                           char **papszOpenOptions);
    ~OGRLVBAGLayer() override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    const OGRFeatureDefn *GetLayerDefn() const override;

    int TestCapability(const char *) const override;
};

/************************************************************************/
/*                          OGRLVBAGDataSource                          */
/************************************************************************/

class OGRLVBAGDataSource final : public GDALDataset
{
    OGRLVBAG::LayerPoolUniquePtr poPool;
    OGRLVBAG::LayerVector papoLayers;

    void TryCoalesceLayers();

    friend GDALDataset *OGRLVBAGDriverOpen(GDALOpenInfo *poOpenInfo);

  public:
    OGRLVBAGDataSource();

    int Open(const char *pszFilename, char **papszOpenOptions);

    int GetLayerCount() const override;
    const OGRLayer *GetLayer(int) const override;

    int TestCapability(const char *) const override;
};

#endif  // ndef OGR_LVBAG_H_INCLUDED
