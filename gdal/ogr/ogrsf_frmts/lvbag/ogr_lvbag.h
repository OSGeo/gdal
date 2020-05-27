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

namespace OGRLVBAG {

/**
 * Deleter for unique pointer.
 *
 * This functor will free the XML kept resources once the unique pointer goes
 * out of scope or is reset.
 */

struct XMLParserUniquePtrDeleter
{
    void operator()(XML_Parser oParser) const noexcept
    {
        XML_ParserFree(oParser);
    }
};

/**
 * XML parser unique pointer type.
 *
 * The XML parser unique pointer holds the resources used by the parser for as long
 * as the pointers stays inscope. Once the pointer leaves operation scope the deleter
 * is called and the resources are freed.
 */

typedef std::unique_ptr<XML_ParserStruct, XMLParserUniquePtrDeleter> XMLParserUniquePtr;

/**
 * Vector holding pointers to OGRLayer.
 */
using LayerVector = std::vector<std::unique_ptr<OGRLayer>>;

}

/************************************************************************/
/*                           OGRLVBAGLayer                              */
/************************************************************************/

class OGRLVBAGLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRLVBAGLayer>
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRFeature         *poFeature;
    VSILFILE           *fp;
    int                 nNextFID;
    
    OGRLVBAG::XMLParserUniquePtr  oParser;
    
    bool                bSchemaOnly;
    bool                bHasReadSchema;
    
    int                 nCurrentDepth;
    int                 nGeometryElementDepth;
    int                 nFeatureCollectionDepth;
    int                 nFeatureElementDepth;
    int                 nAttributeElementDepth;
    
    CPLString           osElementString;
    bool                bCollectData;

    char                aBuf[BUFSIZ];

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

    OGRFeature *        GetNextRawFeature();

public:
    explicit OGRLVBAGLayer( const char *pszFilename );
    ~OGRLVBAGLayer();

    void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRLVBAGLayer)

    OGRFeatureDefn*     GetLayerDefn() override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                          OGRLVBAGDataSource                          */
/************************************************************************/

class OGRLVBAGDataSource final: public GDALDataset
{
    OGRLVBAG::LayerVector papoLayers;

    void                TryCoalesceLayers();

public:
                        OGRLVBAGDataSource();

    int                 Open( const char* pszFilename );

    int                 GetLayerCount() override;
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
};

#endif  // ndef OGR_LVBAG_H_INCLUDED
