/******************************************************************************
 * $Id$
 *
 * Project:  DWG Translator
 * Purpose:  Definition of classes for OGR .dwg driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
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

#ifndef OGR_DWG_H_INCLUDED
#define OGR_DWG_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include <vector>
#include <map>
#include <set>
#include <queue>

#include "ogr_autocad_services.h"
#include "dwg_headers.h"

class OGRDWGDataSource;
class OGRDWGServices;

/************************************************************************/
/*                          DWGBlockDefinition                          */
/*                                                                      */
/*      Container for info about a block.                               */
/************************************************************************/

class DWGBlockDefinition
{
public:
    DWGBlockDefinition() : poGeometry(nullptr) {}
    ~DWGBlockDefinition();

    OGRGeometry                *poGeometry;
    std::vector<OGRFeature *>  apoFeatures;
};

/************************************************************************/
/*                         OGRDWGBlocksLayer()                          */
/************************************************************************/

class OGRDWGBlocksLayer final: public OGRLayer
{
    OGRDWGDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextFID;
    unsigned int        iNextSubFeature;

    std::map<CPLString,DWGBlockDefinition>::iterator oIt;

  public:
    explicit OGRDWGBlocksLayer( OGRDWGDataSource *poDS );
    ~OGRDWGBlocksLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRFeature *        GetNextUnfilteredFeature();
};

/************************************************************************/
/*                             OGRDWGLayer                              */
/************************************************************************/
class OGRDWGLayer final: public OGRLayer
{
    OGRDWGDataSource   *poDS;

    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextFID;

    std::set<CPLString> oIgnoredEntities;

    std::queue<OGRFeature*> apoPendingFeatures;
    void                ClearPendingFeatures();

    std::map<CPLString,CPLString> oStyleProperties;

    void                TranslateGenericProperties( OGRFeature *poFeature,
                                                    OdDbEntityPtr poEntity );
    void                PrepareLineStyle( OGRFeature *poFeature );
//    void                ApplyOCSTransformer( OGRGeometry * );

    OGRFeature *        TranslatePOINT( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateLINE( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateLWPOLYLINE( OdDbEntityPtr poEntity );
    OGRFeature *        Translate2DPOLYLINE( OdDbEntityPtr poEntity );
    OGRFeature *        Translate3DPOLYLINE( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateELLIPSE( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateARC( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateMTEXT( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateDIMENSION( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateCIRCLE( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateSPLINE( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateHATCH( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateTEXT( OdDbEntityPtr poEntity );
    OGRFeature *        TranslateINSERT( OdDbEntityPtr poEntity );
    OGRFeature *        Translate3DFACE(OdDbEntityPtr poEntity);

    void                FormatDimension( CPLString &osText, double dfValue );

    CPLString           TextUnescape( OdString oString, bool );

    OdDbBlockTableRecordPtr m_poBlock;
    OdDbObjectIteratorPtr   poEntIter;

  public:
    explicit OGRDWGLayer( OGRDWGDataSource *poDS );
    ~OGRDWGLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRFeature *        GetNextUnfilteredFeature();

    // internal
    void                SetBlockTable( OdDbBlockTableRecordPtr );
    static double       AngleCorrect( double dfAngle, double dfRatio );
};

/************************************************************************/
/*                           OGRDWGDataSource                           */
/************************************************************************/

class OGRDWGDataSource final: public OGRDataSource
{
    VSILFILE           *fp;

    CPLString           m_osName;
    std::vector<OGRLayer*> apoLayers;

    std::set<CPLString> attributeFields;

    int                 iEntitiesSectionOffset;

    std::map<CPLString,DWGBlockDefinition> oBlockMap;
    std::map<CPLString,CPLString> oHeaderVariables;

    CPLString           osEncoding;

    // indexed by layer name, then by property name.
    std::map< CPLString, std::map<CPLString,CPLString> >
                        oLayerTable;

    std::map<CPLString,CPLString> oLineTypeTable;

    int                 bInlineBlocks;
    int                 bAttributes;
    int                 bAllAttributes;

    OGRDWGServices     *poServices;
    OdDbDatabasePtr     poDb;

  public:
                        OGRDWGDataSource();
                        ~OGRDWGDataSource();

    OdDbDatabasePtr     GetDB() { return poDb; }

    int                 Open( OGRDWGServices *poServices,
                              const char * pszFilename, int bHeaderOnly=FALSE );

    const char          *GetName() override { return m_osName; }

    int                 GetLayerCount() override { return static_cast<int>(apoLayers.size()); }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    // The following is only used by OGRDWGLayer

    int                 InlineBlocks() { return bInlineBlocks; }
    int                 Attributes() { return bAttributes; }
    int                 AllAttributes() { return bAllAttributes; }
    void                AddStandardFields( OGRFeatureDefn *poDef );

    // Implemented in ogrdxf_blockmap.cpp
    void                ReadBlocksSection();
    void                ReadAttDefinitions();
    OGRGeometry        *SimplifyBlockGeometry( OGRGeometryCollection * );
    DWGBlockDefinition *LookupBlock( const char *pszName );
    std::map<CPLString,DWGBlockDefinition> &GetBlockMap() { return oBlockMap; }

    std::set<CPLString>& GetAttributes() { return attributeFields; }
    // Layer and other Table Handling (ogrdatasource.cpp)
    void                ReadLayerDefinitions();
    void                ReadLineTypeDefinitions();
    const char         *LookupLayerProperty( const char *pszLayer,
                                             const char *pszProperty );
    const char         *LookupLineType( const char *pszName );

    // Header variables.
    void                ReadHeaderSection();
    const char         *GetVariable(const char *pszName,
                                    const char *pszDefault=nullptr );

    const char         *GetEncoding() { return osEncoding; }
};

/************************************************************************/
/*                            OGRDWGServices                            */
/*                                                                      */
/*      Services implementation for OGR.  Eventually we should          */
/*      override the ExSystemServices IO to use VSI*L.                  */
/************************************************************************/
class OGRDWGServices : public ExSystemServices, public ExHostAppServices
{
protected:
  ODRX_USING_HEAP_OPERATORS(ExSystemServices);
};

/************************************************************************/
/*                             OGRDWGDriver                             */
/************************************************************************/

class OGRDWGDriver final: public OGRSFDriver
{
    OGRDWGServices *poServices;

  public:
    OGRDWGDriver();
    ~OGRDWGDriver();

    OGRDWGServices *GetServices() { return poServices; }

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;
    int         TestCapability( const char * ) override;
};

#endif /* ndef OGR_DWG_H_INCLUDED */
