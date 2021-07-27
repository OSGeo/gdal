/******************************************************************************
 *
 * Project:  MongoDB Translator
 * Purpose:  Implements OGRMongoDBDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2014-2019, Even Rouault <even dot rouault at spatialys dot com>
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "mongocxxv3_headers.h"

#include "cpl_time.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"

#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

extern "C" void CPL_DLL RegisterOGRMongoDBv3();

// g++ -Wall -Wextra -std=c++11 -fPIC -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -I$HOME/install-mongocxx-3.4.0/include/bsoncxx/v_noabi  -I$HOME/install-mongocxx-3.4.0/include/mongocxx/v_noabi ogr/ogrsf_frmts/mongodbv3/ogrmongodbv3driver.cpp -shared -o ogr_MongoDBv3.so -L$HOME/install-mongocxx-3.4.0/lib -lmongocxx -lgdal

using bsoncxx::builder::basic::kvp;

static mongocxx::instance* g_pInst = nullptr;
static bool g_bCanInstantiateMongo = true;

namespace {
typedef struct _IntOrMap IntOrMap;

struct _IntOrMap
{
    int bIsMap;
    union
    {
        int nField;
        std::map< CPLString, IntOrMap*>* poMap;
    } u;
};
} // namespace

class OGRMongoDBv3Layer;

class OGRMongoDBv3Dataset final: public GDALDataset
{
        friend class OGRMongoDBv3Layer;

        mongocxx::client                                m_oConn{};
        CPLString                                       m_osDatabase{};
        std::vector<std::unique_ptr<OGRMongoDBv3Layer>> m_apoLayers{};
        bool                                            m_bFlattenNestedAttributes = false;
        int                                             m_nBatchSize = 0;
        int                                             m_nFeatureCountToEstablishFeatureDefn = 0;
        bool                                            m_bJSonField = false;
        CPLString                                       m_osFID{};
        bool                                            m_bUseOGRMetadata = true;
        bool                                            m_bBulkInsert = true;

        void                        CreateLayers(mongocxx::database& db);

    public:
        OGRMongoDBv3Dataset() = default;

        int       GetLayerCount() override { return static_cast<int>(m_apoLayers.size()); }
        OGRLayer* GetLayer(int) override;
        OGRLayer* GetLayerByName(const char* pszLayerName) override;

        OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect ) override;
        void        ReleaseResultSet( OGRLayer * poLayer ) override;

        OGRLayer* ICreateLayer( const char *pszName,
                                  OGRSpatialReference *poSpatialRef,
                                  OGRwkbGeometryType eGType,
                                  char ** papszOptions ) override;
        OGRErr    DeleteLayer( int iLayer ) override;
        int       TestCapability( const char * pszCap ) override;


        bool Open(GDALOpenInfo* poOpenInfo);
};

class OGRMongoDBv3Layer final: public OGRLayer
{
        friend class OGRMongoDBv3Dataset;

        OGRMongoDBv3Dataset*                  m_poDS = nullptr;
        OGRFeatureDefn*                       m_poFeatureDefn = nullptr;
        bool                                  m_bHasEstablishedFeatureDefn = false;
        mongocxx::database                    m_oDb{};
        mongocxx::collection                  m_oColl{};
        CPLString                             m_osFID{};
        bsoncxx::document::value              m_oQueryAttr{bsoncxx::builder::basic::make_document()};
        bsoncxx::document::value              m_oQuerySpat{bsoncxx::builder::basic::make_document()};
        bool                                  m_bLayerMetadataUpdatable = false;
        bool                                  m_bUpdateLayerMetadata = false;
        bool                                  m_bDotAsNestedField = true;
        bool                                  m_bIgnoreSourceID = false;
        bool                                  m_bCreateSpatialIndex = true;
        GIntBig                                m_nIndex = 0;
        GIntBig                                m_nNextFID = 0;
        std::unique_ptr<mongocxx::cursor>      m_poCursor{};
        std::unique_ptr<mongocxx::cursor::iterator> m_poIterator{};

        std::vector< std::vector<CPLString> > m_aaosFieldPaths{};

        std::vector< std::vector<CPLString> >    m_aaosGeomFieldPaths{};
        std::vector< CPLString >                 m_aosGeomIndexes{};
        std::vector< std::unique_ptr<OGRCoordinateTransformation> > m_apoCT{};

        std::map< CPLString, CPLString> CollectGeomIndices();
        bool                            ReadOGRMetadata(std::map< CPLString, CPLString>& oMapIndices);
        void                            EstablishFeatureDefn();
        void                            WriteOGRMetadata();
        void                            AddOrUpdateField(const char* pszAttrName,
                                                        const bsoncxx::document::element& elt,
                                                        char chNestedAttributeSeparator,
                                                        std::vector<CPLString>& aosPaths,
                                                        std::map< CPLString, CPLString>& oMapIndices);
        std::unique_ptr<OGRFeature>     Translate(const bsoncxx::document::view& doc);
        bsoncxx::document::value        BuildQuery();

        void                     SerializeField(bsoncxx::builder::basic::document& b,
                                                OGRFeature *poFeature,
                                                int iField,
                                                const char* pszJSonField);
        void                     SerializeGeometry(bsoncxx::builder::basic::document& b,
                                                    OGRGeometry* poGeom, int iField,
                                                    const char* pszJSonField);
        void                     SerializeRecursive(bsoncxx::builder::basic::document& b,
                                        OGRFeature *poFeature,
                                        std::map< CPLString, IntOrMap*>& aoMap );
        static void                     InsertInMap(IntOrMap* rootMap,
                                                std::map< std::vector<CPLString>, IntOrMap*>& aoMap,
                                                const std::vector<CPLString>& aosFieldPathFull,
                                                int nField);
        bsoncxx::document::value BuildBSONObjFromFeature(OGRFeature* poFeature, bool bUpdate);
        std::vector<bsoncxx::document::value> m_aoDocsToInsert{};

    public:
            OGRMongoDBv3Layer(OGRMongoDBv3Dataset* poDS,
                              const std::string& osDbName,
                              const std::string& osCollection);
            ~OGRMongoDBv3Layer() override;

            void            ResetReading() override;
            const char*     GetFIDColumn() override;
            OGRFeature*     GetNextFeature() override;
            OGRFeature*     GetFeature(GIntBig nFID) override;
            OGRErr          DeleteFeature(GIntBig nFID) override;
            GIntBig         GetFeatureCount(int bForce) override;
            OGRErr          SetAttributeFilter(const char* pszFilter) override;
            void            SetSpatialFilter( OGRGeometry *poGeom ) override { SetSpatialFilter(0, poGeom); }
            void            SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override;
            int             TestCapability( const char* pszCap ) override;
            OGRFeatureDefn* GetLayerDefn() override;
            OGRErr          CreateField( OGRFieldDefn *poFieldIn, int ) override;
            OGRErr          CreateGeomField( OGRGeomFieldDefn *poFieldIn, int ) override;
            OGRErr          ICreateFeature( OGRFeature *poFeature ) override;
            OGRErr          ISetFeature( OGRFeature *poFeature ) override;

            OGRErr          SyncToDisk() override;

};

/************************************************************************/
/*                         OGRMongoDBv3Layer()                          */
/************************************************************************/

OGRMongoDBv3Layer::OGRMongoDBv3Layer(OGRMongoDBv3Dataset* poDS,
                                     const std::string& osDbName,
                                     const std::string& osCollection) :
    m_poDS(poDS)
{
    CPLString osLayerName;
    m_oDb = m_poDS->m_oConn[osDbName];
    m_oColl = m_oDb[osCollection];
    if( m_poDS->m_osDatabase == osDbName )
        osLayerName = osCollection;
    else
        osLayerName = osDbName + "." + osCollection;
    m_poFeatureDefn = new OGRFeatureDefn(osLayerName);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);
    SetDescription(m_poFeatureDefn->GetName());

    OGRFieldDefn oFieldDefn("_id", OFTString);
    std::vector<CPLString> aosPath;
    aosPath.push_back("_id");
    m_aaosFieldPaths.push_back(aosPath);
    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
}

/************************************************************************/
/*                         ~OGRMongoDBv3Layer()                         */
/************************************************************************/

OGRMongoDBv3Layer::~OGRMongoDBv3Layer()
{
    OGRMongoDBv3Layer::SyncToDisk();

    if( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            WriteOGRMetadata()                        */
/************************************************************************/

void OGRMongoDBv3Layer::WriteOGRMetadata()
{
    //CPLDebug("MongoDBv3", "WriteOGRMetadata(%s)", m_poFeatureDefn->GetName());
    if( !m_bUpdateLayerMetadata )
        return;
    m_bUpdateLayerMetadata = false;

    try
    {
        bsoncxx::builder::basic::document b;

        b.append(kvp("layer", m_oColl.name()));

        if( !m_osFID.empty() )
        {
            b.append(kvp("fid", m_osFID));
        }

        bsoncxx::builder::basic::array fields;

        CPLAssert( static_cast<int>(m_aaosFieldPaths.size()) == m_poFeatureDefn->GetFieldCount() );
        for(int i=1;i<m_poFeatureDefn->GetFieldCount();i++)
        {
            OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            const char* pszFieldName = poFieldDefn->GetNameRef();
            if( EQUAL(pszFieldName, "_json") )
                continue;
            bsoncxx::builder::basic::array path;
            for(int j=0;j<static_cast<int>(m_aaosFieldPaths[i].size());j++)
                path.append(m_aaosFieldPaths[i][j]);
            bsoncxx::builder::basic::document rec;
            rec.append(kvp("name", pszFieldName));
            OGRFieldType eType = poFieldDefn->GetType();
            rec.append(kvp("type", OGR_GetFieldTypeName(eType)));
            if( eType == OFTInteger && poFieldDefn->GetSubType() == OFSTBoolean )
            {
                rec.append(kvp("subtype", "Boolean"));
            }
            rec.append(kvp("path", path.extract()));
            fields.append(rec.extract());
        }
        b.append(kvp("fields", fields.extract()));

        bsoncxx::builder::basic::array geomfields;
        CPLAssert( static_cast<int>(m_aaosGeomFieldPaths.size()) == m_poFeatureDefn->GetGeomFieldCount() );
        for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
        {
            OGRGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            const char* pszFieldName = poGeomFieldDefn->GetNameRef();
            bsoncxx::builder::basic::array path;
            for(int j=0;j<static_cast<int>(m_aaosGeomFieldPaths[i].size());j++)
                path.append(m_aaosGeomFieldPaths[i][j]);
            const char* pszGeomType = OGRToOGCGeomType(poGeomFieldDefn->GetType());
            bsoncxx::builder::basic::document rec;
            rec.append(kvp("name", pszFieldName));
            rec.append(kvp("type", pszGeomType));
            rec.append(kvp("path", path.extract()));
            geomfields.append(rec.extract());
        }
        b.append(kvp("geomfields", geomfields.extract()));

        {
            bsoncxx::builder::basic::document filter{};
            filter.append(kvp("layer", m_oColl.name()));
            m_oDb["_ogr_metadata"].find_one_and_delete(filter.extract());
        }

        m_oDb["_ogr_metadata"].insert_one( b.extract() );
    }
    catch( const std::exception &ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "WriteOGRMetadata()", ex.what());
    }
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::SyncToDisk()
{
    try
    {
        if( !m_aoDocsToInsert.empty() )
        {
            m_oColl.insert_many( m_aoDocsToInsert.begin(), m_aoDocsToInsert.end() );
            m_aoDocsToInsert.clear();
        }
    }
    catch( const std::exception &ex )
    {
        m_aoDocsToInsert.clear();
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "CreateFeature()", ex.what());
        return OGRERR_FAILURE;
    }

    WriteOGRMetadata();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMongoDBv3Layer::ResetReading()
{
    m_poCursor.reset();
    m_poIterator.reset();
    m_nIndex = 0;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRMongoDBv3Layer::GetLayerDefn()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();

    return m_poFeatureDefn;
}

/************************************************************************/
/*                    OGRMongoDBv3GetFieldTypeFromBSON()                */
/************************************************************************/

static
OGRFieldType OGRMongoDBv3GetFieldTypeFromBSON(
                                        const bsoncxx::document::element& elt,
                                        OGRFieldSubType& eSubType )
{
    eSubType = OFSTNone;

    auto eBSONType = elt.type();
    if( eBSONType == bsoncxx::type::k_bool )
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }
    else if( eBSONType == bsoncxx::type::k_double )
        return OFTReal;
    else if( eBSONType == bsoncxx::type::k_int32 )
        return OFTInteger;
    else if( eBSONType == bsoncxx::type::k_int64 )
        return OFTInteger64;
    else if( eBSONType == bsoncxx::type::k_utf8 )
        return OFTString;
    else if( eBSONType == bsoncxx::type::k_array )
    {
        auto arrayView = elt.get_array().value;
        if( arrayView.empty() )
            return OFTStringList; /* we don't know, so let's assume it is a string list */
        OGRFieldType eType = OFTIntegerList;
        bool bOnlyBoolean = true;
        for (auto&& subElt : arrayView)
        {
            eBSONType = subElt.type();

            bOnlyBoolean &= (eBSONType == bsoncxx::type::k_bool);
            if (eBSONType == bsoncxx::type::k_double)
                eType = OFTRealList;
            else if (eType == OFTIntegerList && eBSONType == bsoncxx::type::k_int64)
                eType = OFTInteger64List;
            else if (eBSONType != bsoncxx::type::k_int32 &&
                     eBSONType != bsoncxx::type::k_int64 &&
                     eBSONType != bsoncxx::type::k_bool)
                return OFTStringList;
        }
        if( bOnlyBoolean )
            eSubType = OFSTBoolean;
        return eType;
    }
    else if( eBSONType == bsoncxx::type::k_date )
        return OFTDateTime;
    else if( eBSONType == bsoncxx::type::k_binary )
        return OFTBinary;
    else
        return OFTString; /* null, object */
}

/************************************************************************/
/*                         AddOrUpdateField()                           */
/************************************************************************/

void OGRMongoDBv3Layer::AddOrUpdateField(const char* pszAttrName,
                                         const bsoncxx::document::element& elt,
                                         char chNestedAttributeSeparator,
                                         std::vector<CPLString>& aosPaths,
                                         std::map< CPLString, CPLString>& oMapIndices)
{
    const auto eBSONType = elt.type();
    if( eBSONType == bsoncxx::type::k_null ||
        eBSONType == bsoncxx::type::k_undefined ||
        eBSONType == bsoncxx::type::k_minkey ||
        eBSONType == bsoncxx::type::k_maxkey )
        return;

    if( eBSONType == bsoncxx::type::k_document )
    {
        bsoncxx::document::view doc{elt.get_document()};
        auto eltType = doc["type"];
        if( eltType && eltType.type() == bsoncxx::type::k_utf8 )
        {
            OGRwkbGeometryType eGeomType = OGRFromOGCGeomType(
                std::string(eltType.get_utf8().value).c_str());
            if( eGeomType != wkbUnknown )
            {
                int nIndex = m_poFeatureDefn->GetGeomFieldIndex(pszAttrName);
                if( nIndex < 0 )
                {
                    OGRGeomFieldDefn fldDefn( pszAttrName, eGeomType );
                    OGRSpatialReference* poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
                    fldDefn.SetSpatialRef(poSRS);
                    poSRS->Release();
                    m_poFeatureDefn->AddGeomFieldDefn( &fldDefn );

                    aosPaths.push_back(std::string(elt.key()));
                    m_aaosGeomFieldPaths.push_back(aosPaths);
                    if( oMapIndices.find(pszAttrName) == oMapIndices.end() )
                        m_aosGeomIndexes.push_back(oMapIndices[pszAttrName]);
                    else
                        m_aosGeomIndexes.push_back("none");
                    m_apoCT.push_back(nullptr);
                }
                else
                {
                    OGRGeomFieldDefn* poFDefn = m_poFeatureDefn->GetGeomFieldDefn(nIndex);
                    if( poFDefn->GetType() != eGeomType )
                        poFDefn->SetType(wkbUnknown);
                }
            }
        }
        else if( m_poDS->m_bFlattenNestedAttributes )
        {
            if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
                return;
            aosPaths.push_back(std::string(elt.key()));
            for (auto&& subElt : doc)
            {
                char szSeparator[2];
                szSeparator[0] = chNestedAttributeSeparator;
                szSeparator[1] = 0;
                CPLString osAttrName(pszAttrName);
                osAttrName += szSeparator;
                osAttrName += std::string(subElt.key());

                std::vector<CPLString> aosNewPaths(aosPaths);
                AddOrUpdateField(osAttrName, subElt,
                                 chNestedAttributeSeparator,
                                 aosNewPaths, oMapIndices);
            }
            return;
        }
    }
    else if( eBSONType == bsoncxx::type::k_array )
    {
        if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
            return;
        if( oMapIndices.find(pszAttrName) != oMapIndices.end() &&
            oMapIndices[pszAttrName] == "2d" )
        {
            OGRGeomFieldDefn fldDefn( pszAttrName, wkbPoint );
            OGRSpatialReference* poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
            fldDefn.SetSpatialRef(poSRS);
            poSRS->Release();
            m_poFeatureDefn->AddGeomFieldDefn( &fldDefn );

            aosPaths.push_back(std::string(elt.key()));
            m_aaosGeomFieldPaths.push_back(aosPaths);
            m_aosGeomIndexes.push_back("2d");
            m_apoCT.push_back(nullptr);
        }
    }

    if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
        return;

    OGRFieldSubType eNewSubType;
    OGRFieldType eNewType = OGRMongoDBv3GetFieldTypeFromBSON( elt, eNewSubType );

    int nIndex = m_poFeatureDefn->GetFieldIndex(pszAttrName);
    if( nIndex < 0 )
    {
        OGRFieldDefn fldDefn( pszAttrName, eNewType );
        fldDefn.SetSubType(eNewSubType);
        if( eNewSubType == OFSTBoolean )
            fldDefn.SetWidth(1);
        m_poFeatureDefn->AddFieldDefn( &fldDefn );

        aosPaths.push_back(std::string(elt.key()));
        m_aaosFieldPaths.push_back(aosPaths);
    }
    else
    {
        OGRFieldDefn* poFDefn = m_poFeatureDefn->GetFieldDefn(nIndex);
        OGRUpdateFieldType(poFDefn, eNewType, eNewSubType);
    }
}

/************************************************************************/
/*                         CollectGeomIndices()                         */
/************************************************************************/

std::map< CPLString, CPLString> OGRMongoDBv3Layer::CollectGeomIndices()
{
    std::map< CPLString, CPLString> oMapIndices;
    try
    {
        auto cursor = m_oColl.list_indexes();
        for (auto&& l_index : cursor)
        {
            //std::string s(bsoncxx::to_json(l_index));
            //CPLDebug("MongoDBv3", "%s", s.c_str());
            auto key = l_index["key"];
            if( key && key.type() == bsoncxx::type::k_document )
            {
                bsoncxx::document::view keyDoc{key.get_document()};
                for (auto&& field : keyDoc)
                {
                    if( field.type() == bsoncxx::type::k_utf8 )
                    {
                        std::string v(field.get_utf8().value);
                        if( v == "2d" || v == "2dsphere" )
                        {
                            std::string idxColName(field.key());
                            //CPLDebug("MongoDBv3", "Index %s for %s of %s",
                            //         v.c_str(),
                            //         idxColName.c_str(),
                            //         m_poFeatureDefn->GetName());
                            oMapIndices[idxColName] = v;
                        }
                    }
                }
            }
        }
    }
    catch( const std::exception& ex )
    {
        CPLDebug("MongoDBv3", "Error when listing indices: %s", ex.what());
    }
    return oMapIndices;
}

/************************************************************************/
/*                          ReadOGRMetadata()                           */
/************************************************************************/

bool OGRMongoDBv3Layer::ReadOGRMetadata(std::map< CPLString, CPLString>& oMapIndices)
{
    try
    {
        bsoncxx::builder::basic::document filter{};
        filter.append(kvp("layer", m_oColl.name()));
        auto docOpt = m_oDb["_ogr_metadata"].find_one(filter.extract());
        if( docOpt )
        {
            auto doc = docOpt->view();
            auto fid = doc["fid"];
            if( fid && fid.type() == bsoncxx::type::k_utf8 )
                m_osFID = std::string(fid.get_utf8().value);

            auto fields = doc["fields"];
            if( fields && fields.type() == bsoncxx::type::k_array )
            {
                auto arrayView(fields.get_array().value);
                for( const auto& elt: arrayView )
                {
                    if( elt.type() == bsoncxx::type::k_document )
                    {
                        auto obj2_doc(elt.get_document());
                        auto obj2(obj2_doc.view());
                        auto name = obj2["name"];
                        auto type = obj2["type"];
                        auto subtype = obj2["subtype"];
                        auto path = obj2["path"];
                        if( name && name.type() == bsoncxx::type::k_utf8 &&
                            type && type.type() == bsoncxx::type::k_utf8 &&
                            path && path.type() == bsoncxx::type::k_array )
                        {
                            if( std::string(name.get_utf8().value) == "_id" )
                                continue;
                            OGRFieldType eType(OFTString);
                            for(int j=0; j<=OFTMaxType;j++)
                            {
                                if( EQUAL(OGR_GetFieldTypeName(static_cast<OGRFieldType>(j)),
                                          std::string(type.get_utf8().value).c_str()) )
                                {
                                    eType = static_cast<OGRFieldType>(j);
                                    break;
                                }
                            }

                            std::vector<CPLString> aosPaths;
                            auto oPathArray(path.get_array().value);
                            bool ok = true;
                            for( const auto& eltPath: oPathArray )
                            {
                                if( eltPath.type() != bsoncxx::type::k_utf8 )
                                {
                                    ok = false;
                                    break;
                                }
                                aosPaths.push_back(std::string(eltPath.get_utf8().value));
                            }
                            if( !ok )
                                continue;

                            OGRFieldDefn oFieldDefn(std::string(name.get_utf8().value).c_str(), eType);
                            if( subtype && subtype.type() == bsoncxx::type::k_utf8 &&
                                std::string(subtype.get_utf8().value) == "Boolean" )
                                oFieldDefn.SetSubType(OFSTBoolean);
                            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);

                            m_aaosFieldPaths.push_back(aosPaths);
                        }
                    }
                }
            }

            auto geomfields = doc["geomfields"];
            if( geomfields && geomfields.type() == bsoncxx::type::k_array )
            {
                auto arrayView(geomfields.get_array().value);
                for( const auto& elt: arrayView )
                {
                    if( elt.type() == bsoncxx::type::k_document )
                    {
                        auto obj2_doc(elt.get_document());
                        auto obj2(obj2_doc.view());
                        auto name = obj2["name"];
                        auto type = obj2["type"];
                        auto path = obj2["path"];
                        if( name && name.type() == bsoncxx::type::k_utf8 &&
                            type && type.type() == bsoncxx::type::k_utf8 &&
                            path && path.type() == bsoncxx::type::k_array )
                        {

                            std::vector<CPLString> aosPaths;
                            auto oPathArray(path.get_array().value);
                            bool ok = true;
                            for( const auto& eltPath: oPathArray )
                            {
                                if( eltPath.type() != bsoncxx::type::k_utf8 )
                                {
                                    ok = false;
                                    break;
                                }
                                aosPaths.push_back(std::string(eltPath.get_utf8().value));
                            }
                            if( !ok )
                                continue;

                            OGRwkbGeometryType eType(OGRFromOGCGeomType(std::string(type.get_utf8().value).c_str()));
                            OGRGeomFieldDefn oFieldDefn(std::string(name.get_utf8().value).c_str(), eType);
                            OGRSpatialReference* poSRS = new OGRSpatialReference();
                            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                            poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
                            oFieldDefn.SetSpatialRef(poSRS);
                            poSRS->Release();
                            m_poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

                            m_aaosGeomFieldPaths.push_back(aosPaths);
                            if( oMapIndices.find(oFieldDefn.GetNameRef()) != oMapIndices.end() )
                                m_aosGeomIndexes.push_back(oMapIndices[oFieldDefn.GetNameRef()]);
                            else
                                m_aosGeomIndexes.push_back("none");
                            //CPLDebug("MongoDBv3", "Layer %s: m_aosGeomIndexes[%d] = %s",
                            //         m_poFeatureDefn->GetName(),
                            //         m_poFeatureDefn->GetGeomFieldCount()-1,
                            //         m_aosGeomIndexes[m_poFeatureDefn->GetGeomFieldCount()-1].c_str());
                            m_apoCT.push_back(nullptr);
                        }
                    }
                }
            }

            m_bLayerMetadataUpdatable = true;
            return true;
        }
    }
    catch( const std::exception& ex )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s: %s",
                    "ReadOGRMetadata()", ex.what());
    }
    return false;
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

void OGRMongoDBv3Layer::EstablishFeatureDefn()
{
    if( m_bHasEstablishedFeatureDefn )
        return;

    std::map< CPLString, CPLString> oMapIndices(CollectGeomIndices());

    int nCount = m_poDS->m_nFeatureCountToEstablishFeatureDefn;
    if( m_poDS->m_bUseOGRMetadata )
    {
        if( ReadOGRMetadata(oMapIndices) )
            nCount = 0;
    }

    if( nCount != 0 )
    {
        try
        {
            mongocxx::options::find options;
            if( nCount > 0 )
            {
                options.limit(nCount);
            }
            if( m_poDS->m_nBatchSize > 0 )
            {
                options.batch_size(m_poDS->m_nBatchSize);
            }

            auto cursor = m_oColl.find({}, options);
            for (auto&& doc : cursor)
            {
                //std::string s(bsoncxx::to_json(doc));
                //CPLDebug("MongoDBv3", "%s", s.c_str());
                for (auto&& field : doc)
                {
                    std::vector<CPLString> aosPaths;
                    std::string osKey(field.key());
                    if( osKey == m_poDS->m_osFID )
                    {
                        m_osFID = osKey;
                    }
                    else
                    {
                        AddOrUpdateField(osKey.c_str(), field,
                                         '.', aosPaths, oMapIndices);
                    }
                }
            }
        }
        catch( const std::exception& ex )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "EstablishFeatureDefn()", ex.what());
        }
    }

    if( m_poDS->m_bJSonField )
    {
        OGRFieldDefn fldDefn("_json", OFTString);
        m_poFeatureDefn->AddFieldDefn( &fldDefn );
        std::vector<CPLString> aosPaths;
        m_aaosFieldPaths.push_back(aosPaths);
    }

    m_bHasEstablishedFeatureDefn = true;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char* OGRMongoDBv3Layer::GetFIDColumn()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    return m_osFID.c_str();
}

/************************************************************************/
/*                            BuildQuery()                              */
/************************************************************************/

bsoncxx::document::value OGRMongoDBv3Layer::BuildQuery()
{
    bsoncxx::builder::basic::document b{};
    auto queryAttrView(m_oQueryAttr.view());
    for( const auto& field: queryAttrView )
    {
        b.append(kvp(field.key(), field.get_value()));
    }
    auto querySpatView(m_oQuerySpat.view());
    for( const auto& field: querySpatView )
    {
        b.append(kvp(field.key(), field.get_value()));
    }
    return b.extract();
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRMongoDBv3Layer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery != nullptr ||
        (m_poFilterGeom != nullptr && !TestCapability(OLCFastSpatialFilter)) )
    {
        return OGRLayer::GetFeatureCount(bForce);
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    SyncToDisk();

    try
    {
        return static_cast<GIntBig>(m_oColl.count_documents(BuildQuery()));
    }
    catch( const std::exception& ex )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s: %s",
                 "GetFeatureCount()", ex.what());
        return OGRLayer::GetFeatureCount(bForce);
    }
}

/************************************************************************/
/*                             Stringify()                              */
/************************************************************************/

static CPLString Stringify(const bsoncxx::types::value& val)
{
    const auto eBSONType = val.type();
    if( eBSONType == bsoncxx::type::k_utf8 )
    {
        return std::string( val.get_utf8().value );
    }
    else if( eBSONType == bsoncxx::type::k_int32 )
        return CPLSPrintf("%d", val.get_int32().value);
    else if( eBSONType == bsoncxx::type::k_int64 )
        return CPLSPrintf(CPL_FRMT_GIB, static_cast<GIntBig>(val.get_int64().value));
    else if( eBSONType == bsoncxx::type::k_double )
        return CPLSPrintf("%.16g", val.get_double().value);
    else if( eBSONType == bsoncxx::type::k_oid )
        return val.get_oid().value.to_string();
    else if( eBSONType == bsoncxx::type::k_bool )
        return CPLSPrintf("%d", val.get_bool().value);
    else if( eBSONType == bsoncxx::type::k_date )
    {
        GIntBig secsandmillis = static_cast<GIntBig>(val.get_date().to_int64());
        struct tm tm;
        GIntBig secs = secsandmillis / 1000;
        int millis = static_cast<int>(secsandmillis % 1000);
        if( millis < 0 )
        {
            secs --;
            millis += 1000;
        }
        CPLUnixTimeToYMDHMS(secs, &tm);
        return CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                          tm.tm_year + 1900,
                          tm.tm_mon + 1,
                          tm.tm_mday,
                          tm.tm_hour,
                          tm.tm_min,
                          tm.tm_sec,
                          millis);
    }
    else if( eBSONType == bsoncxx::type::k_document )
    {
        return CPLString(bsoncxx::to_json(val.get_document().value));
    }
    else
    {
        // This looks like a hack, but to_json() only works on documents
        // so we have to wrap our value as a fake { "v": val } document...
        bsoncxx::builder::basic::document b{};
        b.append(kvp("v", val));
        CPLString ret(bsoncxx::to_json(b.extract()));
        CPLAssert(ret.find("{ \"v\" : ") == 0);
        CPLAssert(ret.substr(ret.size() - 2) == " }");
        ret = ret.substr(strlen("{ \"v\" : "), ret.size() - strlen("{ \"v\" : ") - 2);
        return ret;
    }
}

/************************************************************************/
/*                   OGRMongoDBV3ReaderSetField()                       */
/************************************************************************/

static void OGRMongoDBV3ReaderSetField(OGRFeature* poFeature,
                                       const char* pszAttrName,
                                       const bsoncxx::document::element& elt,
                                       bool bFlattenNestedAttributes,
                                       char chNestedAttributeSeparator)
{
    int nGeomFieldIndex;
    auto eBSONType = elt.type();

    if( eBSONType == bsoncxx::type::k_document &&
        (nGeomFieldIndex = poFeature->GetGeomFieldIndex(pszAttrName)) >= 0 )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRGeometry* poGeom = OGRGeometry::FromHandle(
            OGR_G_CreateGeometryFromJson( Stringify(elt.get_value()) ));
        CPLPopErrorHandler();
        if( poGeom != nullptr )
        {
            poGeom->assignSpatialReference(
                poFeature->GetDefnRef()->GetGeomFieldDefn(nGeomFieldIndex)->GetSpatialRef() );
            poFeature->SetGeomFieldDirectly(nGeomFieldIndex, poGeom);
        }
        return;
    }
    else if( eBSONType == bsoncxx::type::k_array &&
        (nGeomFieldIndex = poFeature->GetGeomFieldIndex(pszAttrName)) >= 0 )
    {
        auto arrayView = elt.get_array().value;
        const unsigned nSize = static_cast<unsigned>(
            std::distance(arrayView.begin(), arrayView.end()));
        if( nSize == 2 )
        {
            auto x = arrayView[0];
            auto y = arrayView[1];
            if( x.type() == bsoncxx::type::k_double && y.type() == bsoncxx::type::k_double )
            {
                OGRGeometry* poGeom = new OGRPoint( x.get_double().value, y.get_double().value );
                poGeom->assignSpatialReference(
                    poFeature->GetDefnRef()->GetGeomFieldDefn(nGeomFieldIndex)->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly(nGeomFieldIndex, poGeom);
            }
        }
        return;
    }

    if( bFlattenNestedAttributes && eBSONType == bsoncxx::type::k_document )
    {
        auto doc(elt.get_document().value);
        for (auto&& field : doc)
        {
            CPLString osAttrName(pszAttrName);
            osAttrName += chNestedAttributeSeparator;
            std::string keyName(field.key());
            osAttrName += keyName;
            OGRMongoDBV3ReaderSetField(poFeature,
                                       osAttrName, field,
                                       bFlattenNestedAttributes,
                                       chNestedAttributeSeparator);
        }
        return ;
    }

    int nField = poFeature->GetFieldIndex(pszAttrName);
    if( nField < 0 )
        return;
    OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(nField);
    CPLAssert( nullptr != poFieldDefn );
    OGRFieldType eType = poFieldDefn->GetType();
    if( eBSONType == bsoncxx::type::k_null )
        poFeature->SetFieldNull( nField );
    else if( eBSONType == bsoncxx::type::k_int32 )
        poFeature->SetField( nField, elt.get_int32().value );
    else if( eBSONType == bsoncxx::type::k_int64 )
        poFeature->SetField( nField, static_cast<GIntBig>(elt.get_int64().value) );
    else if( eBSONType == bsoncxx::type::k_double )
        poFeature->SetField( nField, elt.get_double().value );
    else if( eBSONType == bsoncxx::type::k_minkey && eType == OFTReal )
        poFeature->SetField( nField, -std::numeric_limits<double>::infinity() );
    else if( eBSONType == bsoncxx::type::k_maxkey && eType == OFTReal )
        poFeature->SetField( nField, std::numeric_limits<double>::infinity() );
     else if( eBSONType == bsoncxx::type::k_minkey && eType == OFTInteger )
        poFeature->SetField( nField, INT_MIN );
    else if( eBSONType == bsoncxx::type::k_maxkey  && eType == OFTInteger )
        poFeature->SetField( nField, INT_MAX );
    else if( eBSONType == bsoncxx::type::k_minkey && eType == OFTInteger64 )
        poFeature->SetField( nField, std::numeric_limits<GIntBig>::min() );
    else if( eBSONType == bsoncxx::type::k_maxkey  && eType == OFTInteger64 )
        poFeature->SetField( nField, std::numeric_limits<GIntBig>::max() );
    else if( eBSONType == bsoncxx::type::k_array )
    {
        auto arrayView = elt.get_array().value;
        const unsigned nSize = static_cast<unsigned>(
            std::distance(arrayView.begin(), arrayView.end()));
        if( eType == OFTStringList )
        {
            char** papszValues = static_cast<char**>(CPLCalloc(nSize + 1, sizeof(char*)));
            unsigned int i = 0;
            for( auto&& subElt : arrayView )
            {
                papszValues[i] = CPLStrdup(Stringify(subElt.get_value()));
                ++i;
            }
            poFeature->SetField( nField, papszValues );
            CSLDestroy(papszValues);
        }
        else if( eType == OFTRealList )
        {
            double* padfValues = static_cast<double*>(
                CPLMalloc(nSize * sizeof(double)));
            unsigned int i = 0;
            for( auto&& subElt : arrayView )
            {
                eBSONType = subElt.type();
                if( eBSONType == bsoncxx::type::k_int32 )
                    padfValues[i] = subElt.get_int32().value;
                else if( eBSONType == bsoncxx::type::k_int64 )
                    padfValues[i] = static_cast<double>(subElt.get_int64().value);
                else if( eBSONType == bsoncxx::type::k_double )
                    padfValues[i] = subElt.get_double().value;
                else if( eBSONType == bsoncxx::type::k_minkey )
                    padfValues[i] = -std::numeric_limits<double>::infinity();
                else if( eBSONType == bsoncxx::type::k_maxkey )
                    padfValues[i] = std::numeric_limits<double>::infinity();
                else
                    padfValues[i] = CPLAtof(Stringify(subElt.get_value()));
                ++i;
            }
            poFeature->SetField( nField, nSize, padfValues );
            CPLFree(padfValues);
        }
        else if( eType == OFTIntegerList )
        {
            int* panValues = static_cast<int*>(CPLMalloc(nSize * sizeof(int)));
            unsigned int i = 0;
            for( auto&& subElt : arrayView )
            {
                eBSONType = subElt.type();
                if( eBSONType == bsoncxx::type::k_int32 )
                    panValues[i] = subElt.get_int32().value;
                else if( eBSONType == bsoncxx::type::k_int64 )
                {
                    GIntBig nVal = subElt.get_int64().value;
                    if( nVal < INT_MIN )
                        panValues[i] = INT_MIN;
                    else if( nVal > INT_MAX )
                        panValues[i] = INT_MAX;
                    else
                        panValues[i] = static_cast<int>(nVal);
                }
                else if( eBSONType == bsoncxx::type::k_double )
                {
                    double dfVal = subElt.get_double().value;
                    if( dfVal < INT_MIN )
                        panValues[i] = INT_MIN;
                    else if( dfVal > INT_MAX )
                        panValues[i] = INT_MAX;
                    else
                        panValues[i] = static_cast<int>(dfVal);
                }
                else if( eBSONType == bsoncxx::type::k_minkey )
                    panValues[i] = INT_MIN;
                else if( eBSONType == bsoncxx::type::k_maxkey )
                    panValues[i] = INT_MAX;
                else
                    panValues[i] = atoi(Stringify(subElt.get_value()));
                ++i;
            }
            poFeature->SetField( nField, nSize, panValues );
            CPLFree(panValues);
        }
        else if( eType == OFTInteger64List )
        {
            GIntBig* panValues = static_cast<GIntBig*>(
                CPLMalloc(nSize * sizeof(GIntBig)));
            unsigned int i = 0;
            for( auto&& subElt : arrayView )
            {
                eBSONType = subElt.type();
                if( eBSONType == bsoncxx::type::k_int32 )
                    panValues[i] = subElt.get_int32().value;
                else if( eBSONType == bsoncxx::type::k_int64 )
                    panValues[i] = subElt.get_int64().value;
                else if( eBSONType == bsoncxx::type::k_double )
                {
                    double dfVal = subElt.get_double().value;
                    if( dfVal < std::numeric_limits<GIntBig>::min() )
                        panValues[i] = std::numeric_limits<GIntBig>::min();
                    else if( dfVal > static_cast<double>(std::numeric_limits<GIntBig>::max()) )
                        panValues[i] = std::numeric_limits<GIntBig>::max();
                    else
                        panValues[i] = static_cast<GIntBig>(dfVal);
                }
                else if( eBSONType == bsoncxx::type::k_minkey )
                    panValues[i] = std::numeric_limits<GIntBig>::min();
                else if( eBSONType == bsoncxx::type::k_maxkey )
                    panValues[i] = std::numeric_limits<GIntBig>::max();
                else
                    panValues[i] = CPLAtoGIntBig(Stringify(subElt.get_value()));
                ++i;
            }
            poFeature->SetField( nField, nSize, panValues );
            CPLFree(panValues);
        }
    }
    else if( eBSONType == bsoncxx::type::k_utf8 )
    {
        std::string s( elt.get_utf8().value );
        poFeature->SetField( nField, s.c_str() );
    }
    else if( eBSONType == bsoncxx::type::k_oid )
        poFeature->SetField( nField, elt.get_oid().value.to_string().c_str() );
    else if( eBSONType == bsoncxx::type::k_bool )
        poFeature->SetField( nField, elt.get_bool().value );
    else if( eBSONType == bsoncxx::type::k_binary )
    {
        const auto v(elt.get_binary());
        int len = static_cast<int>(v.size);
        const GByte *pabyData = v.bytes;
        poFeature->SetField( nField, len, pabyData);
    }
    else
        poFeature->SetField( nField, Stringify(elt.get_value()) );
}

/************************************************************************/
/*                            Translate()                               */
/************************************************************************/

std::unique_ptr<OGRFeature> OGRMongoDBv3Layer::Translate(
                                            const bsoncxx::document::view& doc)
{
    std::unique_ptr<OGRFeature> poFeature(new OGRFeature(m_poFeatureDefn));
    for (auto&& field : doc)
    {
        std::string fieldName(field.key());
        if( !m_poDS->m_osFID.empty() && EQUAL(m_osFID, fieldName.c_str()) )
        {
            const auto eBSONType = field.type();
            if( eBSONType == bsoncxx::type::k_int32 )
            {
                poFeature->SetFID(field.get_int32().value);
            }
            else if( eBSONType == bsoncxx::type::k_int64 )
            {
                poFeature->SetFID(field.get_int64().value);
            }
            else if( eBSONType == bsoncxx::type::k_double )
            {
                double dfV = field.get_double().value;
                if( dfV >= static_cast<double>(
                            std::numeric_limits<GIntBig>::min()) &&
                    dfV <= static_cast<double>(
                            std::numeric_limits<GIntBig>::max()) )
                {
                    auto nV = static_cast<GIntBig>(dfV);
                    if( static_cast<double>(nV) == dfV )
                    {
                        poFeature->SetFID(nV);
                    }
                }
            }
        }
        else
        {
            OGRMongoDBV3ReaderSetField( poFeature.get(),
                                        fieldName.c_str(),
                                        field,
                                        m_poDS->m_bFlattenNestedAttributes,
                                        '.' );
        }

        if( m_poDS->m_bJSonField )
        {
            poFeature->SetField("_json", bsoncxx::to_json(doc).c_str());
        }
    }
    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRMongoDBv3Layer::GetNextFeature()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( !m_aoDocsToInsert.empty() )
        SyncToDisk();

    try
    {
        if( !m_poCursor )
        {
            mongocxx::options::find options;
            if( m_poDS->m_nBatchSize > 0 )
            {
                options.batch_size(m_poDS->m_nBatchSize);
            }
            m_poCursor.reset(new mongocxx::cursor(m_oColl.find(BuildQuery(), options)));
            m_poIterator.reset(new mongocxx::cursor::iterator(m_poCursor->begin()));
        }
        if( !m_poCursor || !m_poIterator )
            return nullptr;

        while( *m_poIterator != m_poCursor->end() )
        {
            auto poFeature(Translate(**m_poIterator));
            if( poFeature->GetFID() < 0 )
                poFeature->SetFID(++m_nIndex);

            (*m_poIterator) ++;

            if((m_poFilterGeom == nullptr
                || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature.get() )) )
            {
                return poFeature.release();
            }
        }
    }
    catch( const std::exception& ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                    "GetNextFeature()", ex.what());
    }
    return nullptr;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature* OGRMongoDBv3Layer::GetFeature(GIntBig nFID)
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( !m_aoDocsToInsert.empty() )
        SyncToDisk();

    if( m_osFID.empty() )
    {
        auto oQueryAttrBak = m_oQueryAttr;
        auto oQuerySpatBak = m_oQuerySpat;
        m_oQueryAttr = bsoncxx::builder::basic::make_document();
        m_oQuerySpat = bsoncxx::builder::basic::make_document();
        OGRFeature* poFeature = OGRLayer::GetFeature(nFID);
        m_oQueryAttr = oQueryAttrBak;
        m_oQuerySpat = oQuerySpatBak;
        return poFeature;
    }

    try
    {
        bsoncxx::builder::basic::document b{};
        b.append( kvp( std::string(m_osFID), static_cast<int64_t>(nFID) ) );
        auto obj = m_oColl.find_one(b.extract());
        if( !obj )
            return nullptr;

        auto poFeature = Translate(obj->view());
        poFeature->SetFID(nFID);
        return poFeature.release();
    }
    catch( const std::exception &ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "GetFeature()", ex.what());
        return nullptr;
    }
}

/************************************************************************/
/*                             DeleteFeature()                          */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::DeleteFeature(GIntBig nFID)
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( !m_aoDocsToInsert.empty() )
        SyncToDisk();
    if( m_osFID.empty() )
        return OGRERR_FAILURE;

    try
    {
        bsoncxx::builder::basic::document b{};
        b.append( kvp( std::string(m_osFID), static_cast<int64_t>(nFID) ) );
        auto obj = m_oColl.find_one_and_delete(b.extract());
        //if( obj )
        //{
        //    std::string s(bsoncxx::to_json(obj->view()));
        //    CPLDebug("MongoDBv3", "%s", s.c_str());
        //}
        return obj ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    }
    catch( const std::exception &ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "DeleteFeature()", ex.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::CreateField( OGRFieldDefn *poFieldIn, int )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    const char* pszFieldName = poFieldIn->GetNameRef();
    if( m_poFeatureDefn->GetFieldIndex(pszFieldName) >= 0 )
    {
        if( !EQUAL(pszFieldName, "_id") &&
            !EQUAL(pszFieldName, "_json") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateField() called with an already existing field name: %s",
                     pszFieldName);
        }
        return OGRERR_FAILURE;
    }

    m_poFeatureDefn->AddFieldDefn( poFieldIn );

    std::vector<CPLString> aosPaths;
    if( m_bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(pszFieldName, ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPaths.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPaths.push_back(pszFieldName);
    m_aaosFieldPaths.push_back(aosPaths);

    m_bUpdateLayerMetadata = m_bLayerMetadataUpdatable;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, int )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( m_poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( EQUAL(oFieldDefn.GetNameRef(), "") )
        oFieldDefn.SetName("geometry");

    m_poFeatureDefn->AddGeomFieldDefn( &oFieldDefn );

    std::vector<CPLString> aosPaths;
    if( m_bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(oFieldDefn.GetNameRef(), ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPaths.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPaths.push_back(oFieldDefn.GetNameRef());
    m_aaosGeomFieldPaths.push_back(aosPaths);
    m_aosGeomIndexes.push_back("none");

    std::unique_ptr<OGRCoordinateTransformation> poCT;
    if( oFieldDefn.GetSpatialRef() != nullptr )
    {
        OGRSpatialReference oSRS_WGS84;
        oSRS_WGS84.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        oSRS_WGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( !oSRS_WGS84.IsSame(oFieldDefn.GetSpatialRef()) )
        {
            poCT.reset(OGRCreateCoordinateTransformation( oFieldDefn.GetSpatialRef(), &oSRS_WGS84 ));
            if( poCT.get() == nullptr )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "On-the-fly reprojection to WGS84 long/lat would be "
                          "needed, but instantiation of transformer failed" );
            }
        }
    }
    m_apoCT.push_back(std::move(poCT));

    if( m_bCreateSpatialIndex )
    {
        //CPLDebug("MongoDBv3", "Create spatial index for %s of %s",
        //         poFieldIn->GetNameRef(), m_poFeatureDefn->GetName());
        try
        {
            const char* pszIndexType;
            if( wkbFlatten(poFieldIn->GetType()) != wkbPoint )
                pszIndexType = "2dsphere";
            else
                pszIndexType = CPLGetConfigOption("OGR_MONGODB_SPAT_INDEX_TYPE", "2dsphere");

            bsoncxx::builder::basic::document b{};
            b.append(kvp(
                std::string(oFieldDefn.GetNameRef()),
                std::string(pszIndexType)));
            m_oColl.create_index(b.extract());

            m_aosGeomIndexes.back() = pszIndexType;
        }
        catch( const std::exception &ex )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "Index creation", ex.what());
        }
    }

    m_bUpdateLayerMetadata = m_bLayerMetadataUpdatable;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SerializeField()                           */
/************************************************************************/

void OGRMongoDBv3Layer::SerializeField(bsoncxx::builder::basic::document& b,
                                     OGRFeature *poFeature,
                                     int iField,
                                     const char* pszJSonField)
{
    OGRFieldType eType = m_poFeatureDefn->GetFieldDefn(iField)->GetType();
    std::string osFieldName(pszJSonField);
    if( poFeature->IsFieldNull(iField) )
    {
        b.append( kvp(osFieldName, bsoncxx::types::b_null{}) );
    }
    else if( eType == OFTInteger )
    {
        if( m_poFeatureDefn->GetFieldDefn(iField)->GetSubType() == OFSTBoolean )
            b.append( kvp(osFieldName, CPL_TO_BOOL(poFeature->GetFieldAsInteger(iField)) ));
        else
            b.append( kvp(osFieldName, poFeature->GetFieldAsInteger(iField) ));
    }
    else if( eType == OFTInteger64 )
        b.append( kvp(osFieldName, static_cast<int64_t>(
            poFeature->GetFieldAsInteger64(iField))) );
    else if( eType == OFTReal )
        b.append( kvp(osFieldName, poFeature->GetFieldAsDouble(iField) ));
    else if( eType == OFTString )
        b.append( kvp(osFieldName, poFeature->GetFieldAsString(iField) ));
    else if( eType == OFTStringList )
    {
        char** papszValues = poFeature->GetFieldAsStringList(iField);
        bsoncxx::builder::basic::array arrayBuilder;
        for(int i=0; papszValues[i]; i++)
            arrayBuilder.append( papszValues[i] );
        b.append( kvp(osFieldName, arrayBuilder.extract() ));
    }
    else if( eType == OFTIntegerList )
    {
        int nSize;
        const int* panValues = poFeature->GetFieldAsIntegerList(iField, &nSize);
        bsoncxx::builder::basic::array arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( panValues[i] );
        b.append( kvp(osFieldName, arrayBuilder.extract() ));
    }
    else if( eType == OFTInteger64List )
    {
        int nSize;
        const GIntBig* panValues = poFeature->GetFieldAsInteger64List(iField, &nSize);
        bsoncxx::builder::basic::array arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( static_cast<int64_t>(panValues[i]) );
        b.append( kvp(osFieldName, arrayBuilder.extract() ));
    }
    else if( eType == OFTRealList )
    {
        int nSize;
        const double* padfValues = poFeature->GetFieldAsDoubleList(iField, &nSize);
        bsoncxx::builder::basic::array arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( padfValues[i] );
        b.append( kvp(osFieldName, arrayBuilder.extract() ));
    }
    else if( eType == OFTBinary )
    {
        int nSize;
        const GByte* pabyData = poFeature->GetFieldAsBinary(iField, &nSize);
        bsoncxx::types::b_binary bin;
        bin.sub_type = bsoncxx::binary_sub_type::k_binary;
        bin.size = nSize;
        bin.bytes = pabyData;
        b.append( kvp(osFieldName, bin) );
    }
    else if( eType == OFTDate || eType == OFTDateTime || eType == OFTTime )
    {
        struct tm tm;
        int nYear, nMonth, nDay, nHour, nMinute, nTZ;
        float fSecond;
        poFeature->GetFieldAsDateTime(iField, &nYear, &nMonth, &nDay,
                                      &nHour, &nMinute, &fSecond, &nTZ);
        tm.tm_year = nYear - 1900;
        tm.tm_mon = nMonth - 1;
        tm.tm_mday = nDay;
        tm.tm_hour = nHour;
        tm.tm_min = nMinute;
        tm.tm_sec = static_cast<int>(fSecond);
        GIntBig millis = 1000 * CPLYMDHMSToUnixTime(&tm) +
                        static_cast<GIntBig>(1000 * fmod(fSecond, 1));
        b.append( kvp(osFieldName, bsoncxx::types::b_date(
            static_cast<std::chrono::milliseconds>(millis)) ) );
    }
}

/************************************************************************/
/*                        SerializeGeometry()                           */
/************************************************************************/

void OGRMongoDBv3Layer::SerializeGeometry(bsoncxx::builder::basic::document& b,
                                          OGRGeometry* poGeom, int iField,
                                          const char* pszJSonField)
{
    std::string osFieldName(pszJSonField);
    if( m_aosGeomIndexes[iField] == "2d" &&
        wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        bsoncxx::builder::basic::array arrayBuilder;
        OGRPoint* poPoint = poGeom->toPoint();
        arrayBuilder.append( poPoint->getX() );
        arrayBuilder.append( poPoint->getY() );
        b.append( kvp(osFieldName, arrayBuilder.extract()) );
    }
    else
    {
        char* pszJSon = OGR_G_ExportToJson(OGRGeometry::ToHandle(poGeom));
        if( pszJSon )
        {
            //CPLDebug("MongoDBv3", "%s", pszJSon);
            auto obj(bsoncxx::from_json(pszJSon));
            b.append( kvp(osFieldName, obj) );
        }
        CPLFree(pszJSon);
    }
}

/************************************************************************/
/*                       SerializeRecursive()                           */
/************************************************************************/

void OGRMongoDBv3Layer::SerializeRecursive(bsoncxx::builder::basic::document& b,
                                         OGRFeature *poFeature,
                                         std::map< CPLString, IntOrMap*>& aoMap )
{
    std::map< CPLString, IntOrMap* >::iterator oIter = aoMap.begin();
    for( ; oIter != aoMap.end(); ++oIter)
    {
        IntOrMap* intOrMap = oIter->second;
        if( intOrMap->bIsMap )
        {
            bsoncxx::builder::basic::document subB;
            SerializeRecursive(subB, poFeature, *(intOrMap->u.poMap));
            b.append( kvp( std::string(oIter->first), subB.extract()) );
            delete intOrMap->u.poMap;
        }
        else
        {
            int i = intOrMap->u.nField;
            if( i >= 0)
            {
                SerializeField(b, poFeature, i, oIter->first.c_str());
            }
            else
            {
                i = -i - 1;
                OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
                SerializeGeometry(b, poGeom, i, oIter->first.c_str());
            }
        }
        delete intOrMap;
    }
}

/************************************************************************/
/*                           InsertInMap()                              */
/************************************************************************/

void OGRMongoDBv3Layer::InsertInMap(IntOrMap* rootMap,
                                  std::map< std::vector<CPLString>, IntOrMap*>& aoMap,
                                  const std::vector<CPLString>& aosFieldPathFull,
                                  int nField)
{
    std::vector<CPLString> aosFieldPath;
    std::vector<CPLString> aosFieldPathPrev;
    for(int j=0; j< static_cast<int>(aosFieldPathFull.size()) - 1; j++)
    {
        aosFieldPath.push_back(aosFieldPathFull[j]);
        if( aoMap.find(aosFieldPath) == aoMap.end() )
        {
            IntOrMap* intOrMap = new IntOrMap;
            intOrMap->bIsMap = TRUE;
            intOrMap->u.poMap = new std::map< CPLString, IntOrMap*>;
            aoMap[aosFieldPath] = intOrMap;
        }
        if( j > 0 )
        {
            std::map< CPLString, IntOrMap* >* poPrevMap = aoMap[aosFieldPathPrev]->u.poMap;
            (*poPrevMap)[aosFieldPathFull[j]] = aoMap[aosFieldPath];
        }
        else
            (*(rootMap->u.poMap))[aosFieldPathFull[j]] = aoMap[aosFieldPath];
        aosFieldPathPrev.push_back(aosFieldPathFull[j]);
    }
    IntOrMap* intOrMap = new IntOrMap;
    intOrMap->bIsMap = FALSE;
    intOrMap->u.nField = nField;
    std::map< CPLString, IntOrMap* >* poPrevMap = aoMap[aosFieldPathPrev]->u.poMap;
    const CPLString& osLastComponent(aosFieldPathFull.back());
    CPLAssert( (*poPrevMap).find(osLastComponent) == (*poPrevMap).end() );
    (*(poPrevMap))[osLastComponent] = intOrMap;
}

/************************************************************************/
/*                       BuildBSONObjFromFeature()                      */
/************************************************************************/

bsoncxx::document::value OGRMongoDBv3Layer::BuildBSONObjFromFeature(OGRFeature* poFeature, bool bUpdate)
{
    bsoncxx::builder::basic::document b{};

    int nJSonFieldIndex = m_poFeatureDefn->GetFieldIndex("_json");
    if( nJSonFieldIndex >= 0 && poFeature->IsFieldSetAndNotNull(nJSonFieldIndex) )
    {
        CPLString osJSon(poFeature->GetFieldAsString(nJSonFieldIndex));

        auto obj(bsoncxx::from_json(osJSon));
        auto obj_view(obj.view());
        if( (m_bIgnoreSourceID || !obj_view["_id"]) && !bUpdate )
        {
            bsoncxx::oid generated;
            b.append(kvp("_id", generated));
            poFeature->SetField(0, generated.to_string().c_str());
        }
        for( const auto& field: obj_view )
        {
            b.append(kvp(field.key(), field.get_value()));
        }
        return b.extract();
    }

    if( poFeature->GetFID() >= 0 && !m_osFID.empty() )
    {
        b.append(kvp( std::string(m_osFID),
                      static_cast<int64_t>(poFeature->GetFID()) ));
    }

    CPLAssert(static_cast<int>(m_aaosFieldPaths.size()) == m_poFeatureDefn->GetFieldCount());

    if( !poFeature->IsFieldSetAndNotNull(0) || (!bUpdate && m_bIgnoreSourceID) )
    {
        bsoncxx::oid generated;
        b.append(kvp("_id", generated));
        poFeature->SetField(0, generated.to_string().c_str());
    }
    else
        b.append(kvp(
            "_id", bsoncxx::oid(poFeature->GetFieldAsString(0)) ));

    IntOrMap* rootMap = new IntOrMap;
    rootMap->bIsMap = TRUE;
    rootMap->u.poMap = new std::map< CPLString, IntOrMap*>;
    std::map< std::vector<CPLString>, IntOrMap*> aoMap;

    for(int i=1;i<m_poFeatureDefn->GetFieldCount();i++)
    {
        if( !poFeature->IsFieldSet(i) )
            continue;

        if( m_aaosFieldPaths[i].size() > 1 )
        {
            InsertInMap(rootMap, aoMap, m_aaosFieldPaths[i], i);
        }
        else
        {
            const char* pszFieldName = m_poFeatureDefn->GetFieldDefn(i)->GetNameRef();
            SerializeField(b, poFeature, i, pszFieldName);
        }
    }

    CPLAssert(static_cast<int>(m_aaosGeomFieldPaths.size()) == m_poFeatureDefn->GetGeomFieldCount());
    CPLAssert(static_cast<int>(m_apoCT.size()) == m_poFeatureDefn->GetGeomFieldCount());
    for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
    {
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == nullptr )
            continue;
        if( !bUpdate && m_apoCT[i] != nullptr )
            poGeom->transform( m_apoCT[i].get() );

        if( m_aaosGeomFieldPaths[i].size() > 1 )
        {
            InsertInMap(rootMap, aoMap, m_aaosGeomFieldPaths[i],  -i-1);
        }
        else
        {
            const char* pszFieldName = m_poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef();
            SerializeGeometry(b, poGeom, i, pszFieldName);
        }
    }

    SerializeRecursive(b, poFeature, *(rootMap->u.poMap));
    delete rootMap->u.poMap;
    delete rootMap;

    return b.extract();
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::ICreateFeature( OGRFeature *poFeature )
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();

    try
    {
        if( poFeature->GetFID() < 0 )
        {
            if( m_nNextFID == 0 )
                m_nNextFID = GetFeatureCount(false);
            poFeature->SetFID(++m_nNextFID);
        }

        auto bsonObj( BuildBSONObjFromFeature(poFeature, false) );

        if( m_poDS->m_bBulkInsert )
        {
            constexpr size_t knMAX_DOCS_IN_BULK = 1000;
            if( m_aoDocsToInsert.size() == knMAX_DOCS_IN_BULK )
                SyncToDisk();
            m_aoDocsToInsert.emplace_back( std::move(bsonObj) );
        }
        else
        {
            //std::string s(bsoncxx::to_json(bsonObj));
            //CPLDebug("MongoDBv3", "%s", s.c_str());
            m_oColl.insert_one( std::move(bsonObj) );
        }

        return OGRERR_NONE;
    }
    catch( const std::exception &ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "CreateFeature()", ex.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ISetFeature()                             */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::ISetFeature( OGRFeature *poFeature )
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( !m_aoDocsToInsert.empty() )
        SyncToDisk();

    if( !poFeature->IsFieldSetAndNotNull(0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "_id field not set");
        return OGRERR_FAILURE;
    }

    try
    {
        auto bsonObj( BuildBSONObjFromFeature(poFeature, true) );
        auto view(bsonObj.view());

        bsoncxx::builder::basic::document filterBuilder{};
        filterBuilder.append( kvp("_id", view["_id"].get_oid().value ) );
        if( !m_osFID.empty() )
            filterBuilder.append( kvp( std::string(m_osFID),
                                static_cast<int64_t>(poFeature->GetFID()) ) );

        auto filter(filterBuilder.extract());
        auto ret = m_oColl.find_one_and_replace( std::move(filter), std::move(bsonObj) );
        //if( ret )
        //{
        //    std::string s(bsoncxx::to_json(ret->view()));
        //    CPLDebug("MongoDBv3", "%s", s.c_str());
        //}
        return ret ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    }
    catch( const std::exception &ex )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "SetFeature()", ex.what());
        return OGRERR_FAILURE;
    }
}
/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRMongoDBv3Layer::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return true;
    }
    if( EQUAL(pszCap,OLCRandomRead) )
    {
        EstablishFeatureDefn();
        return !m_osFID.empty();
    }
    if( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        EstablishFeatureDefn();
        for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
        {
            if( m_aosGeomIndexes[i] == "none" )
            {
                return false;
            }
        }
        return true;
    }
    if( EQUAL(pszCap, OLCCreateField) ||
             EQUAL(pszCap, OLCCreateGeomField) ||
             EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->GetAccess() == GA_Update;
    }
    else if( EQUAL(pszCap,OLCDeleteFeature) )
    {
        EstablishFeatureDefn();
        return m_poDS->GetAccess() == GA_Update &&
               !m_osFID.empty();
    }

    return false;
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr OGRMongoDBv3Layer::SetAttributeFilter(const char* pszFilter)
{
    m_oQueryAttr = bsoncxx::builder::basic::make_document();

    if( pszFilter != nullptr && pszFilter[0] == '{' )
    {
        OGRLayer::SetAttributeFilter(nullptr);
        try
        {
            m_oQueryAttr = bsoncxx::from_json(pszFilter);
            return OGRERR_NONE;
        }
        catch( const std::exception &ex )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "SetAttributeFilter()", ex.what());
            return OGRERR_FAILURE;
        }
    }
    return OGRLayer::SetAttributeFilter(pszFilter);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRMongoDBv3Layer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    m_oQuerySpat = bsoncxx::builder::basic::make_document();
    if( InstallFilter( poGeomIn ) && poGeomIn )
    {
        OGREnvelope sEnvelope;
        poGeomIn->getEnvelope(&sEnvelope);
        if( sEnvelope.MaxX == sEnvelope.MinX )
            sEnvelope.MaxX += 1e-10;
        if( sEnvelope.MaxY == sEnvelope.MinY )
            sEnvelope.MaxY += 1e-10;

        if( sEnvelope.MinX < -180 )
            sEnvelope.MinX = -180;
        if( sEnvelope.MinY < -90 )
            sEnvelope.MinY = -90;
        if( sEnvelope.MaxX > 180 )
            sEnvelope.MaxX = 180;
        if( sEnvelope.MaxY > 90 )
            sEnvelope.MaxY = 90;
        if( sEnvelope.MinX == -180 && sEnvelope.MinY == -90 &&
            sEnvelope.MaxX == 180 && sEnvelope.MaxY == 90 )
        {
            return;
        }

        try
        {
            if( m_aosGeomIndexes[m_iGeomFieldFilter] == "2dsphere" )
            {
                m_oQuerySpat = bsoncxx::from_json(CPLSPrintf("{ \"%s\" : { \"$geoIntersects\" : "
                "{ \"$geometry\" : { \"type\" : \"Polygon\" , \"coordinates\" : [["
                "[%.16g,%.16g],[%.16g,%.16g],[%.16g,%.16g],[%.16g,%.16g],[%.16g,%.16g]]] } } } }",
                              m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef(),
                              sEnvelope.MinX, sEnvelope.MinY,
                              sEnvelope.MaxX, sEnvelope.MinY,
                              sEnvelope.MaxX, sEnvelope.MaxY,
                              sEnvelope.MinX, sEnvelope.MaxY,
                              sEnvelope.MinX, sEnvelope.MinY));
            }
            else if( m_aosGeomIndexes[m_iGeomFieldFilter] == "2d" )
            {
                m_oQuerySpat = bsoncxx::from_json(CPLSPrintf("{ \"%s\" : { \"$geoWithin\" : "
                "{ \"$box\" : [ [ %.16g , %.16g ] , [ %.16g , %.16g ] ] } } }",
                              m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef(),
                              sEnvelope.MinX, sEnvelope.MinY,
                              sEnvelope.MaxX, sEnvelope.MaxY));
            }
        }
        catch( const std::exception &ex )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: %s", "SetSpatialFilter()", ex.what());
        }
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* OGRMongoDBv3Dataset::GetLayer(int nIndex)
{
    if( nIndex < 0 || nIndex >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[nIndex].get();
}

/************************************************************************/
/*                         GetLayerByName()                             */
/************************************************************************/

OGRLayer *OGRMongoDBv3Dataset::GetLayerByName(const char* pszLayerName)
{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
    if( poLayer != nullptr )
        return poLayer;

    for( const auto& l_poLayer: m_apoLayers )
    {
        l_poLayer->SyncToDisk();
    }

    CPLString osDatabase;
    if( m_osDatabase.empty() )
    {
        const char* pszDot = strchr(pszLayerName, '.');
        if( pszDot == nullptr )
            return nullptr;
        osDatabase = pszLayerName;
        osDatabase.resize(pszDot - pszLayerName);
        pszLayerName = pszDot + 1;
    }
    else
        osDatabase = m_osDatabase;

    for(int i=0;i<2;i++)
    {
        try
        {
            auto db = m_oConn.database(osDatabase);
            auto aosCollections = db.list_collection_names();
            for( const auto& osCollection: aosCollections )
            {
                if( EQUAL(osCollection.c_str(),pszLayerName) )
                {
                    m_apoLayers.emplace_back(
                        new OGRMongoDBv3Layer(this,
                                              osDatabase,
                                              osCollection.c_str()));
                    return m_apoLayers.back().get();
                }
            }
        }
        catch( const std::exception &ex)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Command failed: %s", ex.what());
        }
        if( i == 0 )
        {
            if( m_osDatabase.empty() )
                break;
            const char* pszDot = strchr(pszLayerName, '.');
            if( pszDot == nullptr )
                break;
            osDatabase = pszLayerName;
            osDatabase.resize(pszDot - pszLayerName);
            pszLayerName = pszDot + 1;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                           CreateLayers()                             */
/************************************************************************/

void OGRMongoDBv3Dataset::CreateLayers(mongocxx::database& db)
{
    std::string dbName(db.name());
    auto aosCollections = db.list_collection_names();
    for( const auto& osCollection: aosCollections )
    {
        if( osCollection != "_ogr_metadata" )
        {
            m_apoLayers.emplace_back(
                new OGRMongoDBv3Layer(this, dbName.c_str(), osCollection));
        }
    }
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool OGRMongoDBv3Dataset::Open(GDALOpenInfo* poOpenInfo)
{
    eAccess = poOpenInfo->eAccess;

    const char* pszHost = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "HOST", "localhost");
    const char* pszPort = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "PORT", "27017");
    const char* pszURI = CSLFetchNameValue(
        poOpenInfo->papszOpenOptions, "URI");
    if( pszURI == nullptr )
    {
        if( STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodbv3:") )
            pszURI = poOpenInfo->pszFilename + strlen("mongodbv3:");
        else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb:") ||
                STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb+srv:") )
            pszURI = poOpenInfo->pszFilename;
    }
    const char* pszUser = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "USER");
    const char* pszPassword = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "PASSWORD");
    if( (pszUser != nullptr && pszPassword == nullptr) ||
        (pszUser == nullptr && pszPassword != nullptr) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "USER and PASSWORD open options must be both specified.");
        return false;
    }

    try
    {
        mongocxx::uri uri;
        if( pszURI && pszURI[0] )
        {
            uri = mongocxx::uri(pszURI);
        }
        else
        {
            CPLString osURI("mongodb://");
            if( pszUser && pszPassword )
            {
                osURI += pszUser;
                osURI += ':';
                osURI += pszPassword;
                osURI += '@';
            }
            osURI += pszHost;
            osURI += ':';
            osURI += pszPort;
            uri = mongocxx::uri(osURI);
        }
        m_osDatabase = uri.database();
        if( m_osDatabase.empty() )
        {
            m_osDatabase = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "DBNAME", "");
        }

        CPLString osPEMKeyFile = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "SSL_PEM_KEY_FILE", "");
        CPLString osPEMKeyPassword = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "SSL_PEM_KEY_PASSWORD", "");
        CPLString osCAFile = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "SSL_CA_FILE", "");
        CPLString osCRLFile = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "SSL_CRL_FILE", "");
        bool bAllowInvalidCertificates =
            CPLFetchBool(poOpenInfo->papszOpenOptions, "SSL_ALLOW_INVALID_CERTIFICATES", false);

        mongocxx::options::client client_options;
        if( !osPEMKeyFile.empty() || !osPEMKeyPassword.empty() ||
            !osCAFile.empty() || !osCRLFile.empty() ||
            bAllowInvalidCertificates )
        {
            mongocxx::options::ssl ssl_options;
            if( !osPEMKeyFile.empty() )
                ssl_options.pem_file(osPEMKeyFile);
            if( !osPEMKeyPassword.empty() )
                ssl_options.pem_password(osPEMKeyPassword);
            if( !osCAFile.empty() )
                ssl_options.ca_file(osCAFile);
            if( !osCRLFile.empty() )
                ssl_options.crl_file(osCRLFile);
            ssl_options.allow_invalid_certificates(bAllowInvalidCertificates);
            client_options.ssl_opts(ssl_options);
        }

        m_oConn = mongocxx::client(uri, client_options);

        try
        {
            auto db = m_oConn[m_osDatabase.empty() ? "admin" : m_osDatabase.c_str()];
            auto ret = db.run_command(bsoncxx::from_json("{ \"buildInfo\" : 1 }"));
            std::string s(bsoncxx::to_json(ret.view()));
            CPLDebug("MongoDBv3", "%s", s.c_str());
        }
        catch (const std::exception& ex)
        {
            CPLDebug("MongoDBv3", "buildInfo(): %s", ex.what());
        }

        if( m_osDatabase.empty() )
        {
            auto dbs = m_oConn.list_databases();
            for( const auto& dbBson: dbs )
            {
                std::string dbName(dbBson["name"].get_utf8().value);
                if( dbName == "admin" || dbName == "config" || dbName == "local" )
                {
                    continue;
                }
                CPLDebug("MongoDBv3", "Iterating over database %s", dbName.c_str() );
                auto db = m_oConn.database(dbName);
                CreateLayers(db);
            }
        }
        else
        {
            auto db = m_oConn.database(m_osDatabase);
            CreateLayers(db);
        }
    }
    catch (const std::exception& ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", ex.what());
        return false;
    }

    m_nBatchSize = atoi(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "BATCH_SIZE", "0"));
    m_nFeatureCountToEstablishFeatureDefn = atoi(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN", "100"));
    m_bJSonField = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "JSON_FIELD", false);
    m_bFlattenNestedAttributes =
        CPLFetchBool(poOpenInfo->papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", true);
    m_osFID = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "FID", "ogc_fid");
    m_bUseOGRMetadata =
        CPLFetchBool( poOpenInfo->papszOpenOptions, "USE_OGR_METADATA", true);
    m_bBulkInsert = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "BULK_INSERT", true);

    return true;
}

/************************************************************************/
/*                            ICreateLayer()                            */
/************************************************************************/

OGRLayer* OGRMongoDBv3Dataset::ICreateLayer( const char *pszName,
                                              OGRSpatialReference *poSpatialRef,
                                              OGRwkbGeometryType eGType,
                                              char ** papszOptions )
{
    if( m_osDatabase.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create layer/collection when dataset opened without explicit database");
        return nullptr;
    }

    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return nullptr;
    }

    for(int i=0; i<static_cast<int>(m_apoLayers.size()); i++)
    {
        if( EQUAL(m_apoLayers[i]->GetName(), pszName) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != nullptr
                && !EQUAL(CSLFetchNameValue(papszOptions,"OVERWRITE"),"NO") )
            {
                DeleteLayer( i );
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszName );
                return nullptr;
            }
        }
    }

    try
    {
        m_oConn[m_osDatabase].create_collection(std::string(pszName));
    }
    catch( const std::exception& ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", ex.what());
        return nullptr;
    }

    m_apoLayers.emplace_back(
        new OGRMongoDBv3Layer(this, m_osDatabase, pszName));
    auto poLayer = m_apoLayers.back().get();

    poLayer->m_osFID = CSLFetchNameValueDef(papszOptions, "FID", "ogc_fid");
    poLayer->m_bLayerMetadataUpdatable =
        CPLFetchBool(papszOptions, "WRITE_OGR_METADATA", true);
    poLayer->m_bUpdateLayerMetadata = poLayer->m_bLayerMetadataUpdatable;
    poLayer->m_bDotAsNestedField =
        CPLFetchBool(papszOptions, "DOT_AS_NESTED_FIELD", true);
    poLayer->m_bIgnoreSourceID =
        CPLFetchBool(papszOptions, "IGNORE_SOURCE_ID", false);
    poLayer->m_bCreateSpatialIndex =
        CPLFetchBool(papszOptions, "SPATIAL_INDEX", true);

    if( eGType != wkbNone )
    {
        const char* pszGeometryName = CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry");
        OGRGeomFieldDefn oFieldDefn(pszGeometryName, eGType);
        oFieldDefn.SetSpatialRef(poSpatialRef);
        poLayer->CreateGeomField(&oFieldDefn, FALSE);
    }

    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRMongoDBv3Dataset::DeleteLayer( int iLayer )

{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()) )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = m_apoLayers[iLayer]->GetName();
    CPLDebug( "MongoDB", "DeleteLayer(%s)", osLayerName.c_str() );

    try
    {
        {
            bsoncxx::builder::basic::document b{};
            std::string colName(m_apoLayers[iLayer]->m_oColl.name());
            b.append(kvp("layer", colName));
            m_apoLayers[iLayer]->m_oDb["_ogr_metadata"].find_one_and_delete(b.extract());
        }

        m_apoLayers[iLayer]->m_oColl.drop();

        m_apoLayers.erase( m_apoLayers.begin() + iLayer );

        return OGRERR_NONE;
    }
    catch( const std::exception &ex)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "DeleteLayer()", ex.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMongoDBv3Dataset::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer)
        || EQUAL(pszCap,ODsCDeleteLayer)
        || EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return eAccess == GA_Update;
    else
        return FALSE;
}

/************************************************************************/
/*                    OGRMongoDBv3SingleFeatureLayer                    */
/************************************************************************/

class OGRMongoDBv3SingleFeatureLayer final: public OGRLayer
{
    OGRFeatureDefn     *m_poFeatureDefn;
    CPLString           osVal;
    int                 iNextShapeId;
    public:
       explicit OGRMongoDBv3SingleFeatureLayer( const char *pszVal );
       ~OGRMongoDBv3SingleFeatureLayer() { m_poFeatureDefn->Release(); }
       void             ResetReading() override { iNextShapeId = 0; }
       OGRFeature      *GetNextFeature() override;
       OGRFeatureDefn  *GetLayerDefn() override { return m_poFeatureDefn; }
       int              TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                   OGRMongoDBv3SingleFeatureLayer()                   */
/************************************************************************/

OGRMongoDBv3SingleFeatureLayer::OGRMongoDBv3SingleFeatureLayer( const char *pszVal )
{
    m_poFeatureDefn = new OGRFeatureDefn( "RESULT" );
    m_poFeatureDefn->Reference();
    OGRFieldDefn oField( "_json", OFTString );
    m_poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    osVal = pszVal;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGRMongoDBv3SingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetField(0, osVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer* OGRMongoDBv3Dataset::ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect )
{
    for( const auto& poLayer: m_apoLayers )
    {
        poLayer->SyncToDisk();
    }

/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELLAYER:") )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        for( int iLayer = 0; iLayer < static_cast<int>(m_apoLayers.size()); iLayer++ )
        {
            if( EQUAL(m_apoLayers[iLayer]->GetName(),
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case WRITE_OGR_METADATA command.                        */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "WRITE_OGR_METADATA ") )
    {
        if( eAccess != GA_Update )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
            return nullptr;
        }
        const char* pszLayerName = pszSQLCommand + strlen("WRITE_OGR_METADATA ");
        auto poLayer = static_cast<OGRMongoDBv3Layer*>(GetLayerByName(pszLayerName));
        if( poLayer == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Layer %s not found", pszLayerName);
            return nullptr;
        }
        poLayer->GetLayerDefn(); // force schema discovery
        poLayer->m_bLayerMetadataUpdatable = true;
        poLayer->m_bUpdateLayerMetadata = true;
        poLayer->SyncToDisk();

        return nullptr;
    }

    if( pszDialect != nullptr && EQUAL(pszDialect, "MONGODB") )
    {
        if( m_osDatabase.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot run ExecuteSQL() when dataset opened without explicit database");
            return nullptr;
        }
        try
        {
            auto ret = m_oConn[m_osDatabase].run_command(bsoncxx::from_json(pszSQLCommand));
            return new OGRMongoDBv3SingleFeatureLayer(bsoncxx::to_json(ret).c_str());
        }
        catch( const std::exception &ex)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Command failed: %s", ex.what());
            return nullptr;
        }
    }
    else
    {
        return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
    }
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRMongoDBv3Dataset::ReleaseResultSet( OGRLayer * poLayer )
{
    delete poLayer;
}

/************************************************************************/
/*                   OGRMongoDBv3DriverIdentify()                       */
/************************************************************************/

static int OGRMongoDBv3DriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "MongoDBv3:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb+srv:") ||
           (STARTS_WITH_CI(poOpenInfo->pszFilename, "mongodb:") &&
            GDALGetDriverByName("MONGODB") == nullptr);
}

/************************************************************************/
/*                     OGRMongoDBv3DriverOpen()                         */
/************************************************************************/

static GDALDataset* OGRMongoDBv3DriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRMongoDBv3DriverIdentify(poOpenInfo) )
        return nullptr;

    {
        static std::mutex oMutex;
        std::lock_guard<std::mutex> oLock(oMutex);
        if( g_pInst == nullptr )
        {
            if( !g_bCanInstantiateMongo )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "MongoDB client has been previously shut down and "
                         "can no longer be reinitialized");
                return nullptr;
            }

#ifdef use_logger
            class logger final : public mongocxx::logger
            {
            public:
                explicit logger() {}

                void operator()(mongocxx::log_level level,
                                bsoncxx::stdx::string_view /*domain*/,
                                bsoncxx::stdx::string_view message) noexcept override {
                    if (level >= mongocxx::log_level::k_trace)
                        return;
                    std::string tmp(message);
                    CPLDebug("MongoDBv3", "%s", tmp.c_str());
                }
            };
#endif
            g_pInst = new mongocxx::instance(
#ifdef use_logger
                bsoncxx::stdx::make_unique<logger>()
#endif
            );
        }
    }

    auto poDS = new OGRMongoDBv3Dataset();
    if( !poDS->Open(poOpenInfo) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                        OGRMongoDBv3DriverUnload()                    */
/************************************************************************/

extern "C" int GDALIsInGlobalDestructor();

static void OGRMongoDBv3DriverUnload( GDALDriver* )
{
    if( g_pInst != nullptr && !GDALIsInGlobalDestructor() )
    {
        delete g_pInst;
        g_pInst = nullptr;
        g_bCanInstantiateMongo = false;
    }
}
/************************************************************************/
/*                       RegisterOGRMongoDBv3()                         */
/************************************************************************/

void RegisterOGRMongoDBv3()
{
    if( GDALGetDriverByName( "MongoDBv3" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MongoDBv3" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "MongoDB (using libmongocxx v3 client)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/mongodbv3.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "MongoDBv3:" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing collection with the layer name to be created' default='NO'/>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='geometry'/>"
"  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
"  <Option name='FID' type='string' description='Field name, with integer values, to use as FID' default='ogc_fid'/>"
"  <Option name='WRITE_OGR_METADATA' type='boolean' description='Whether to create a description of layer fields in the _ogr_metadata collection' default='YES'/>"
"  <Option name='DOT_AS_NESTED_FIELD' type='boolean' description='Whether to consider dot character in field name as sub-document' default='YES'/>"
"  <Option name='IGNORE_SOURCE_ID' type='boolean' description='Whether to ignore _id field in features passed to CreateFeature()' default='NO'/>"
"</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URI' type='string' description='Connection URI' />"
"  <Option name='HOST' type='string' description='Server hostname' />"
"  <Option name='PORT' type='integer' description='Server port' />"
"  <Option name='DBNAME' type='string' description='Database name' />"
"  <Option name='USER' type='string' description='User name' />"
"  <Option name='PASSWORD' type='string' description='User password' />"
"  <Option name='SSL_PEM_KEY_FILE' type='string' description='SSL PEM certificate/key filename' />"
"  <Option name='SSL_PEM_KEY_PASSWORD' type='string' description='SSL PEM key password' />"
"  <Option name='SSL_CA_FILE' type='string' description='SSL Certification Authority filename' />"
"  <Option name='SSL_CRL_FILE' type='string' description='SSL Certification Revocation List filename' />"
"  <Option name='SSL_ALLOW_INVALID_CERTIFICATES' type='boolean' description='Whether to allow connections to servers with invalid certificates' default='NO'/>"
"  <Option name='BATCH_SIZE' type='integer' description='Number of features to retrieve per batch'/>"
"  <Option name='FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN' type='integer' description='Number of features to retrieve to establish feature definition. -1 = unlimited' default='100'/>"
"  <Option name='JSON_FIELD' type='boolean' description='Whether to include a field with the full document as JSON' default='NO'/>"
"  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' description='Whether to recursively explore nested objects and produce flatten OGR attributes' default='YES'/>"
"  <Option name='FID' type='string' description='Field name, with integer values, to use as FID' default='ogc_fid'/>"
"  <Option name='USE_OGR_METADATA' type='boolean' description='Whether to use the _ogr_metadata collection to read layer metadata' default='YES'/>"
"  <Option name='BULK_INSERT' type='boolean' description='Whether to use bulk insert for feature creation' default='YES'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time IntegerList Integer64List RealList StringList Binary" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRMongoDBv3DriverOpen;
    poDriver->pfnIdentify = OGRMongoDBv3DriverIdentify;
    poDriver->pfnUnloadDriver = OGRMongoDBv3DriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
