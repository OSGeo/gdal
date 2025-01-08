/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Class for reading, parsing and handling a kmlfile.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_KML_KML_H_INCLUDED
#define OGR_KML_KML_H_INCLUDED

#ifdef HAVE_EXPAT

#include "ogr_expat.h"
#include "cpl_vsi.h"

// std
#include <iostream>
#include <string>
#include <vector>

#include "cpl_port.h"
#include "kmlutility.h"

class KMLNode;

typedef enum
{
    KML_VALIDITY_UNKNOWN,
    KML_VALIDITY_INVALID,
    KML_VALIDITY_VALID
} OGRKMLValidity;

class KML
{
  public:
    KML();
    virtual ~KML();
    bool open(const char *pszFilename);
    bool isValid();
    bool isHandled(std::string const &elem) const;
    virtual bool isLeaf(std::string const &elem) const;
    virtual bool isFeature(std::string const &elem) const;
    virtual bool isFeatureContainer(std::string const &elem) const;
    virtual bool isContainer(std::string const &elem) const;
    virtual bool isRest(std::string const &elem) const;
    virtual void findLayers(KMLNode *poNode, int bKeepEmptyContainers);

    bool hasOnlyEmpty() const;

    bool parse();
    void print(unsigned short what = 3);
    std::string getError() const;
    int classifyNodes();
    void eliminateEmpty();
    int getNumLayers() const;
    bool selectLayer(int);
    std::string getCurrentName() const;
    Nodetype getCurrentType() const;
    int is25D() const;
    int getNumFeatures();
    Feature *getFeature(std::size_t nNum, int &nLastAsked, int &nLastCount);

    void unregisterLayerIfMatchingThisNode(KMLNode *poNode);

  protected:
    void checkValidity();

    static void XMLCALL startElement(void *, const char *, const char **);
    static void XMLCALL startElementValidate(void *, const char *,
                                             const char **);
    static void XMLCALL dataHandler(void *, const char *, int);
    static void XMLCALL dataHandlerValidate(void *, const char *, int);
    static void XMLCALL endElement(void *, const char *);

    // Trunk of KMLnodes.
    KMLNode *poTrunk_;
    // Number of layers.
    int nNumLayers_;
    KMLNode **papoLayers_;

  private:
    // Depth of the DOM.
    unsigned int nDepth_;
    // KML version number.
    std::string sVersion_;
    // Set to KML_VALIDITY_VALID if the beginning of the file is detected as KML
    OGRKMLValidity validity;
    // File descriptor.
    VSILFILE *pKMLFile_;
    // Error text ("" when everything is OK").
    std::string sError_;
    // Current KMLNode.
    KMLNode *poCurrent_;

    XML_Parser oCurrentParser;
    int nDataHandlerCounter;
    int nWithoutEventCounter;
};

#endif  // HAVE_EXPAT

#endif /* OGR_KML_KML_H_INCLUDED */
