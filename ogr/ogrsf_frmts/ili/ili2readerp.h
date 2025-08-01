/******************************************************************************
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Private Declarations for Reader code.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ILI2READERP_H_INCLUDED
#define CPL_ILI2READERP_H_INCLUDED

#include "xercesc_headers.h"
#include "ogr_xerces.h"

#include "ili2reader.h"
#include "ogr_ili2.h"

#include <string>
#include <set>

namespace gdal
{
namespace ili2
{
int cmpStr(const std::string &s1, const std::string &s2);

std::string ltrim(const std::string &tmpstr);
std::string rtrim(const std::string &tmpstr);
std::string trim(const std::string &tmpstr);
}  // namespace ili2
}  // namespace gdal

class ILI2Reader;

/************************************************************************/
/*                            ILI2Handler                                */
/************************************************************************/
class ILI2Handler : public DefaultHandler
{
    ILI2Reader *m_poReader;

    int level;

    DOMDocument *dom_doc;
    DOMElement *dom_elem;

    int m_nEntityCounter;

  public:
    explicit ILI2Handler(ILI2Reader *poReader);
    ~ILI2Handler();

    void startDocument() override;
    void endDocument() override;

    void startElement(const XMLCh *const uri, const XMLCh *const localname,
                      const XMLCh *const qname,
                      const Attributes &attrs) override;
    void endElement(const XMLCh *const uri, const XMLCh *const localname,
                    const XMLCh *const qname) override;
    void characters(const XMLCh *const chars,
                    const XMLSize_t length) override;  // xerces 3

    void startEntity(const XMLCh *const name) override;

    void fatalError(const SAXParseException &) override;
};

/************************************************************************/
/*                              ILI2Reader                               */
/************************************************************************/

class ILI2Reader : public IILI2Reader
{
  private:
    int SetupParser();
    void CleanupParser();

    char *m_pszFilename;

    std::list<std::string> m_missAttrs;

    ILI2Handler *m_poILI2Handler;
    SAX2XMLReader *m_poSAXReader;
    int m_bReadStarted;

    std::list<OGRLayer *> m_listLayer;

    bool m_bXercesInitialized;

    ILI2Reader(ILI2Reader &) = delete;
    ILI2Reader &operator=(const ILI2Reader &) = delete;
    ILI2Reader(ILI2Reader &&) = delete;
    ILI2Reader &operator=(ILI2Reader &&) = delete;

  public:
    ILI2Reader();
    ~ILI2Reader();

    void SetSourceFile(const char *pszFilename) override;
    int ReadModel(OGRILI2DataSource *, ImdReader *poImdReader,
                  const char *modelFilename) override;
    int SaveClasses(const char *pszFile) override;

    std::list<OGRLayer *> GetLayers() override;
    int GetLayerCount() override;
    OGRLayer *GetLayer(const char *pszName);

    int AddFeature(DOMElement *elem);
    void SetFieldValues(OGRFeature *feature, DOMElement *elem);
    const char *GetLayerName(/*IOM_BASKET model, IOM_OBJECT table*/);
    void AddField(OGRLayer *layer /*, IOM_BASKET model, IOM_OBJECT obj*/);
    static OGRCircularString *getArc(DOMElement *elem);
    static OGRGeometry *getGeometry(DOMElement *elem, int type);
    static void setFieldDefn(OGRFeatureDefn *featureDef, DOMElement *elem);
};

#endif
