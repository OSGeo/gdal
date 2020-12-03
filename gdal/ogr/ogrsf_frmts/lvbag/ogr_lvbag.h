/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Definition of classes for OGR LVBAG driver.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
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

#ifndef OGR_LVBAG_H_INCLUDED
#define OGR_LVBAG_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_expat.h"
#include "ogrlayerpool.h"

namespace OGRLVBAG {

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

}

/************************************************************************/
/*                           OGRLVBAGLayer                              */
/************************************************************************/

class OGRLVBAGLayer final: public OGRAbstractProxiedLayer, public OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>
{
    CPL_DISALLOW_COPY_ASSIGN(OGRLVBAGLayer)

    OGRFeatureDefn     *poFeatureDefn;
    OGRFeature         *m_poFeature = nullptr;
    VSILFILE           *fp;
    int                 nNextFID;
    CPLString           osFilename;
    
    typedef enum
    {
        FD_OPENED,
        FD_CLOSED,
        FD_CANNOT_REOPEN
    } FileDescriptorState;
    
    FileDescriptorState eFileDescriptorsState;

    OGRExpatUniquePtr   oParser;
    
    bool                bSchemaOnly;
    bool                bHasReadSchema;
    bool                bFixInvalidData;
    bool                bLegacyId;
    
    int                 nCurrentDepth;
    int                 nGeometryElementDepth;
    int                 nFeatureCollectionDepth;
    int                 nFeatureElementDepth;
    int                 nAttributeElementDepth;
    
    CPLString           osElementString;
    CPLString           osAttributeString;
    bool                bCollectData;

    char                aBuf[BUFSIZ];

    void                AddSpatialRef(OGRwkbGeometryType eTypeIn);
    void                AddOccurrenceFieldDefn();
    void                AddIdentifierFieldDefn();
    void                AddDocumentFieldDefn();
    void                CreateFeatureDefn(const char *);

    void                ConfigureParser();
    void                ParseDocument();
    bool                IsParserFinished(XML_Status status);
    
    void                StartElementCbk(const char *, const char **);
    void                EndElementCbk(const char *);
    void                DataHandlerCbk(const char *, int);

    void                StartDataCollect();
    void                StopDataCollect();

    bool                TouchLayer();
    void                CloseUnderlyingLayer() override;

    OGRFeature*         GetNextRawFeature();

    friend class OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>;
    friend class OGRLVBAGDataSource;

public:
    explicit OGRLVBAGLayer( const char *pszFilename, OGRLayerPool* poPoolIn, char **papszOpenOptions );
    ~OGRLVBAGLayer();

    void                ResetReading() override;
    OGRFeature*         GetNextFeature() override;

    OGRFeatureDefn*     GetLayerDefn() override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                          OGRLVBAGDataSource                          */
/************************************************************************/

class OGRLVBAGDataSource final: public GDALDataset
{
    OGRLVBAG::LayerPoolUniquePtr poPool;
    OGRLVBAG::LayerVector papoLayers;

    void                TryCoalesceLayers();

    friend GDALDataset *OGRLVBAGDriverOpen( GDALOpenInfo* poOpenInfo );

public:
                        OGRLVBAGDataSource();

    int                 Open( const char* pszFilename, char **papszOpenOptions );

    int                 GetLayerCount() override;
    OGRLayer*           GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
};

#endif  // ndef OGR_LVBAG_H_INCLUDED
