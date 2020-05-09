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

#ifdef HAVE_EXPAT
#include "ogr_expat.h"
#endif

#ifdef HAVE_EXPAT

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

/************************************************************************/
/*                           OGRLVBAGLayer                              */
/************************************************************************/

class OGRLVBAGLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;
    OGRFeature         *poFeature;
    VSILFILE           *fp;
    int                 nNextFID;
    
    XMLParserUniquePtr  oParser;
    
    bool                bSchemaOlny;
    bool                bHasReadSchema;
    
    int                 nCurrentDepth;
    int                 nGeometryElementDepth;
    int                 nFeatureCollectionDepth;
    int                 nFeatureElementDepth;
    int                 nAttributeElementDepth;
    
    CPLString           osElementString;
    bool                bCollectData;

    char                aBuf[BUFSIZ];

    void                ConfigureParser();
    void                ParseDocument();
    bool                IsParserFinished(XML_Status status);
    
    void                StartElementCbk(const char *, const char **);
    void                EndElementCbk(const char *);
    void                DataHandlerCbk(const char *, int);

    void                StartDataCollect();
    void                StopDataCollect();

public:
    OGRLVBAGLayer( const char *pszFilename, VSILFILE *fp );
    ~OGRLVBAGLayer();

    void                ResetReading() override;
    OGRFeature*         GetNextFeature() override;

    OGRFeatureDefn*     GetLayerDefn() override;

    int                 TestCapability( const char * ) override;
};

#endif /* HAVE_EXPAT */

/************************************************************************/
/*                          OGRLVBAGDataSource                          */
/************************************************************************/

class OGRLVBAGDataSource final: public GDALDataset
{
    std::unique_ptr<OGRLayer>       poLayer;
    VSILFILE            *fp;

public:
                        OGRLVBAGDataSource();
                        ~OGRLVBAGDataSource();

    int                 Open( const char* pszFilename,
                              int bUpdate,
                              VSILFILE* fpIn );

    int                 GetLayerCount() override { return poLayer != nullptr ? 1 : 0; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
};

#endif  // ndef OGR_LVBAG_H_INCLUDED
