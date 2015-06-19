/******************************************************************************
 * $Id$
 *
 * Project:  MongoDB Translator
 * Purpose:  Implements OGRMongoDBDriver.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#ifdef WIN32
#include <winsock2.h>
#undef min
#undef max
#endif
#include "mongo/client/dbclient.h" // for the driver
#include "ogr_p.h"
#include "cpl_time.h"
#include <limits>

// g++ -DDEBUG -g -Wall -fPIC -shared -o ogr_MongoDB.so -I/home/even/boost_1_53_0 -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mongodb ogr/ogrsf_frmts/mongodb/*.c* -L. -lgdal -I/home/even/mongo-cxx-1.0.2-install/include -L/home/even/mongo-cxx-1.0.2-install/lib -lmongoclient -L/home/even/boost_1_53_0/stage/lib -lboost_system -lboost_thread -lboost_regex
CPL_CVSID("$Id$");

#define MAX_DOCS_IN_BULK                1000

extern "C" void RegisterOGRMongoDB();

using namespace mongo;
using mongo::client::Options;

static int bMongoInitialized = -1;
static CPLString osStaticPEMKeyFile;
static CPLString osStaticPEMKeyPassword;
static CPLString osStaticCAFile;
static CPLString osStaticCRLFile;
static int bStaticAllowInvalidCertificates = FALSE;
static int bStaticAllowInvalidHostnames = FALSE;
static int bStaticFIPSMode = FALSE;

class OGRMongoDBDataSource;

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

class OGRMongoDBLayer: public OGRLayer
{
            OGRMongoDBDataSource    *m_poDS;
            OGRFeatureDefn          *m_poFeatureDefn;
            CPLString                m_osDatabase;
            CPLString                m_osCollection;
            CPLString                m_osQualifiedCollection;
            int                      m_bHasEstablishedFeatureDefn;
            GIntBig                  m_nIndex, m_nNextFID;
            std::auto_ptr<DBClientCursor> m_poCursor;
            int                      m_bCursorValid;
            BSONObj                  m_oQueryAttr, m_oQuerySpat;
            CPLString                m_osFID;
            int                      m_bLayerMetadataUpdatable;
            int                      m_bUpdateLayerMetadata;
            int                      m_bDotAsNestedField;
            int                      m_bIgnoreSourceID;
            int                      m_bCreateSpatialIndex;
            BulkOperationBuilder*    m_poBulkBuilder;
            int                      m_nFeaturesInBulk;
            
            std::vector< std::vector<CPLString> > m_aaosFieldPaths;

            std::vector< std::vector<CPLString> > m_aaosGeomFieldPaths;
            std::vector< CPLString > m_aosGeomIndexes;
            std::vector< OGRCoordinateTransformation* > m_apoCT;

            std::map< CPLString, CPLString> CollectGeomIndices();
            int                      ReadOGRMetadata(std::map< CPLString, CPLString>& oMapIndices);
            void                     EstablishFeatureDefn();
            void                     WriteOGRMetadata();
            OGRFeature*              Translate(BSONObj& obj);
            void                     AddOrUpdateField(const char* pszAttrName,
                                       const BSONElement* poElt,
                                       char chNestedAttributeSeparator,
                                       std::vector<CPLString>& aosPaths,
                                       std::map< CPLString, CPLString>& oMapIndices);

            void                     SerializeField(BSONObjBuilder& b,
                                                    OGRFeature *poFeature,
                                                    int iField,
                                                    const char* pszJSonField);
            void                     SerializeGeometry(BSONObjBuilder& b,
                                                       OGRGeometry* poGeom, int iField,
                                                       const char* pszJSonField);
            void                     SerializeRecursive(BSONObjBuilder& b,
                                         OGRFeature *poFeature,
                                         std::map< CPLString, IntOrMap*>& aoMap );
            void                     InsertInMap(IntOrMap* rootMap,
                                                  std::map< std::vector<CPLString>, IntOrMap*>& aoMap,
                                                  const std::vector<CPLString>& aosFieldPathFull,
                                                  int nField);
            BSONObj                  BuildBSONObjFromFeature(OGRFeature* poFeature, int bUpdate);
            BSONObj                  BuildQuery();

public:
            OGRMongoDBLayer(OGRMongoDBDataSource* m_poDS,
                            const char* pszDatabase,
                            const char* pszCollection);
           ~OGRMongoDBLayer();
           
            virtual OGRFeatureDefn* GetLayerDefn();
            virtual const char* GetName() { return m_poFeatureDefn->GetName(); }
            virtual void        ResetReading();
            virtual OGRFeature* GetNextFeature();
            virtual OGRFeature* GetFeature(GIntBig nFID);
            virtual OGRErr      DeleteFeature(GIntBig nFID);
            virtual int         TestCapability(const char* pszCap);
            virtual GIntBig     GetFeatureCount(int bForce);
            virtual OGRErr      SetAttributeFilter(const char* pszFilter);
            virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
            virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );
            virtual const char* GetFIDColumn();
            virtual OGRErr      CreateField( OGRFieldDefn *poFieldIn, int bApproxOK );
            virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poFieldIn, int bApproxOK );
            virtual OGRErr      ICreateFeature(OGRFeature* poFeature);
            virtual OGRErr      ISetFeature(OGRFeature* poFeature);

            virtual OGRErr      SyncToDisk();

            const CPLString&    GetDatabase() const { return m_osDatabase; }
            const CPLString&    GetCollection() const { return m_osCollection; }
            const CPLString&    GetQualifiedCollection() const { return m_osQualifiedCollection; }
            void                SetFID(const CPLString& m_osFIDIn) { m_osFID = m_osFIDIn; }
            void                SetCreateLayerMetadata(int bFlag) { m_bLayerMetadataUpdatable = bFlag; m_bUpdateLayerMetadata = bFlag; }
            void                SetDotAsNestedField(int bFlag) { m_bDotAsNestedField = bFlag; }
            void                SetIgnoreSourceID(int bFlag) { m_bIgnoreSourceID = bFlag; }
            void                SetCreateSpatialIndex(int bFlag) { m_bCreateSpatialIndex = bFlag; }
};


class OGRMongoDBDataSource: public GDALDataset
{
            DBClientBase *m_poConn;
            CPLString     m_osDatabase;
            std::vector<OGRMongoDBLayer*> m_apoLayers;
            int           m_nBatchSize;
            int           m_bFlattenNestedAttributes;
            int           m_nFeatureCountToEstablishFeatureDefn;
            int           m_bJSonField;
            CPLString     m_osFID;
            int           m_bUseOGRMetadata;
            int           m_bBulkInsert;
            
            static int Initialize(char** papszOpenOptions);
            int        ListLayers(const char* pszDatabase);

public:
            OGRMongoDBDataSource();
            ~OGRMongoDBDataSource();
            
            int Open(const char* pszFilename, GDALAccess eAccess, char** papszOpenOptions);
            virtual int GetLayerCount() { return (int)m_apoLayers.size(); }
            virtual OGRLayer* GetLayer(int nIdx);
            virtual int         TestCapability(const char* pszCap);
            virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                             OGRSpatialReference *poSpatialRef = NULL,
                                             OGRwkbGeometryType eGType = wkbUnknown,
                                             char ** papszOptions = NULL );
            virtual OGRErr      DeleteLayer( int iLayer );
            virtual OGRLayer   *GetLayerByName(const char* pszLayerName);

            virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect );
            virtual void        ReleaseResultSet( OGRLayer * poLayer );

            const CPLString& GetDatabase() const { return m_osDatabase; }
            DBClientBase    *GetConn() const { return m_poConn; }
            int              GetBatchSize() const { return m_nBatchSize; }
            int              GetFlattenNestedAttributes() const { return m_bFlattenNestedAttributes; }
            int              GetFeatureCountToEstablishFeatureDefn() const { return m_nFeatureCountToEstablishFeatureDefn; }
            int              JSonField() const { return m_bJSonField; }
            int              UseOGRMetadata() const { return m_bUseOGRMetadata; }
            int              BulkInsert() const { return m_bBulkInsert; }
            const CPLString& GetFID() const { return m_osFID; }
};
     
/************************************************************************/
/*                            OGRMongoDBLayer()                         */
/************************************************************************/

OGRMongoDBLayer::OGRMongoDBLayer(OGRMongoDBDataSource* m_poDS,
                            const char* pszDatabase,
                            const char* pszCollection)
{
    this->m_poDS = m_poDS;
    m_osDatabase = pszDatabase;
    m_osCollection = pszCollection;
    m_osQualifiedCollection = CPLSPrintf("%s.%s", m_osDatabase.c_str(), m_osCollection.c_str());
    if( m_poDS->GetDatabase().size() )
        m_poFeatureDefn = new OGRFeatureDefn(pszCollection);
    else
        m_poFeatureDefn = new OGRFeatureDefn(m_osQualifiedCollection);
    m_poFeatureDefn->SetGeomType(wkbNone);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->Reference();
    m_bHasEstablishedFeatureDefn = FALSE;
    m_bCursorValid = FALSE;
    m_nIndex = 0;
    m_nNextFID = 0;
    m_bLayerMetadataUpdatable = FALSE;
    m_bUpdateLayerMetadata = FALSE;
    m_bDotAsNestedField = TRUE;
    m_bIgnoreSourceID = FALSE;
    m_bCreateSpatialIndex = TRUE;
    m_poBulkBuilder = NULL;
    m_nFeaturesInBulk = 0;
    
    OGRFieldDefn oFieldDefn("_id", OFTString);
    std::vector<CPLString> aosPath;
    aosPath.push_back("_id");
    m_aaosFieldPaths.push_back(aosPath);
    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
}

/************************************************************************/
/*                            ~OGRMongoDBLayer()                        */
/************************************************************************/

OGRMongoDBLayer::~OGRMongoDBLayer()
{
    SyncToDisk();

    for(int i=0;i<(int)m_apoCT.size();i++)
        delete m_apoCT[i];
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            WriteOGRMetadata()                        */
/************************************************************************/

void OGRMongoDBLayer::WriteOGRMetadata()
{
    //CPLDebug("MongoDB", "WriteOGRMetadata(%s)", m_osQualifiedCollection.c_str());
    if( !m_bUpdateLayerMetadata )
        return;
    m_bUpdateLayerMetadata = FALSE;

    try
    {
        BSONObjBuilder b;

        b.append("layer", m_osCollection.c_str());

        if( m_osFID.size() )
        {
            b.append( "fid", m_osFID.c_str() );
        }

        BSONArrayBuilder fields;

        CPLAssert( (int)m_aaosFieldPaths.size() == m_poFeatureDefn->GetFieldCount() );
        for(int i=1;i<m_poFeatureDefn->GetFieldCount();i++)
        {
            OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            const char* pszFieldName = poFieldDefn->GetNameRef();
            if( EQUAL(pszFieldName, "_json") )
                continue;
            BSONArrayBuilder path;
            for(int j=0;j<(int)m_aaosFieldPaths[i].size();j++)
                path.append(m_aaosFieldPaths[i][j]);
            OGRFieldType eType = poFieldDefn->GetType();
            if( eType == OFTInteger && poFieldDefn->GetSubType() == OFSTBoolean )
                fields.append(BSON("name" << pszFieldName <<
                                "type" << OGR_GetFieldTypeName(eType) <<
                                "subtype" << "Boolean" <<
                                "path" << path.arr()));
            else
                fields.append(BSON("name" << pszFieldName <<
                                "type" << OGR_GetFieldTypeName(eType) <<
                                "path" << path.arr()));
        }
        b.append("fields", fields.arr());

        BSONArrayBuilder geomfields;
        CPLAssert( (int)m_aaosGeomFieldPaths.size() == m_poFeatureDefn->GetGeomFieldCount() );
        for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
        {
            OGRGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
            const char* pszFieldName = poGeomFieldDefn->GetNameRef();
            BSONArrayBuilder path;
            for(int j=0;j<(int)m_aaosGeomFieldPaths[i].size();j++)
                path.append(m_aaosGeomFieldPaths[i][j]);
            const char* pszGeomType = OGRToOGCGeomType(poGeomFieldDefn->GetType());
            geomfields.append(BSON("name" << pszFieldName <<
                                    "type" << pszGeomType <<
                                    "path" << path.arr()));
        }
        b.append("geomfields", geomfields.arr());

        m_poDS->GetConn()->findAndRemove(
            CPLSPrintf("%s._ogr_metadata", m_osDatabase.c_str()),
            BSON("layer" << m_osCollection.c_str()), 
            BSONObj(),
            BSONObj());
        m_poDS->GetConn()->insert( CPLSPrintf("%s._ogr_metadata", m_osDatabase.c_str()), b.obj() );
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "WriteOGRMetadata()", e.what());
    }
}

/************************************************************************/
/*                              SyncToDisk()                            */
/************************************************************************/

OGRErr OGRMongoDBLayer::SyncToDisk()
{
    OGRErr eErr = OGRERR_NONE;
    if( m_poBulkBuilder != NULL )
    {
        WriteResult writeResult;
        try
        {
            m_poBulkBuilder->execute(NULL, &writeResult);
            eErr = writeResult.hasErrors() ? OGRERR_FAILURE: OGRERR_NONE;
        }
        catch( const DBException &e )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                    "Bulk write", e.what());
            eErr = OGRERR_FAILURE;
        }
        delete m_poBulkBuilder;
        m_poBulkBuilder = NULL;
        m_nFeaturesInBulk = 0;
    }

    WriteOGRMetadata();

    return eErr;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMongoDBLayer::ResetReading()
{
    m_bCursorValid = FALSE;
    m_nIndex = 0;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRMongoDBLayer::GetLayerDefn()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    return m_poFeatureDefn;
}

/************************************************************************/
/*                     OGRMongoDBGetFieldTypeFromBSON()                 */
/************************************************************************/

static
OGRFieldType OGRMongoDBGetFieldTypeFromBSON( const BSONElement* poElt,
                                             OGRFieldSubType& eSubType )
{
    eSubType = OFSTNone;

    BSONType eBSONType = poElt->type();
    if( eBSONType == Bool )
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }
    else if( eBSONType == NumberDouble )
        return OFTReal;
    else if( eBSONType == NumberInt )
        return OFTInteger;
    else if( eBSONType == NumberLong )
        return OFTInteger64;
    else if( eBSONType == String )
        return OFTString;
    else if( eBSONType == Array )
    {
        std::vector<BSONElement> oArray = poElt->Array();
        int nSize = (int)oArray.size();
        if (nSize == 0)
            return OFTStringList; /* we don't know, so let's assume it's a string list */
        OGRFieldType eType = OFTIntegerList;
        int bOnlyBoolean = TRUE;
        for(int i=0;i<nSize;i++)
        {
            BSONElement& elt = oArray[i];
            eBSONType = elt.type();
        
            bOnlyBoolean &= (eBSONType == Bool);
            if (eBSONType == NumberDouble)
                eType = OFTRealList;
            else if (eType == OFTIntegerList && eBSONType == NumberLong)
                eType = OFTInteger64List;
            else if (eBSONType != NumberInt &&
                     eBSONType != NumberLong &&
                     eBSONType != Bool)
                return OFTStringList;
        }
        if( bOnlyBoolean )
            eSubType = OFSTBoolean;
        return eType;
    }
    else if( eBSONType == Date )
        return OFTDateTime;
    else if( eBSONType == BinData )
        return OFTBinary;
    else
        return OFTString; /* null, object */
}

/************************************************************************/
/*                         AddOrUpdateField()                           */
/************************************************************************/

void OGRMongoDBLayer::AddOrUpdateField(const char* pszAttrName,
                                       const BSONElement* poElt,
                                       char chNestedAttributeSeparator,
                                       std::vector<CPLString>& aosPaths,
                                       std::map< CPLString, CPLString>& oMapIndices)
{
    
    BSONType eBSONType = poElt->type();
    if( eBSONType == jstNULL || eBSONType == Undefined ||
        eBSONType == MinKey || eBSONType == MaxKey )
        return;

    if( eBSONType == Object )
    {
        BSONObj obj(poElt->Obj());
        BSONElement eltType = obj.getField("type");
        OGRwkbGeometryType eGeomType;
        if( !eltType.eoo() && eltType.type() == String &&
            (eGeomType = OGRFromOGCGeomType(eltType.String().c_str())) != wkbUnknown )
        {
            int nIndex = m_poFeatureDefn->GetGeomFieldIndex(pszAttrName);
            if( nIndex < 0 )
            {
                OGRGeomFieldDefn fldDefn( pszAttrName, eGeomType );
                OGRSpatialReference* poSRS = new OGRSpatialReference();
                poSRS->SetFromUserInput(SRS_WKT_WGS84);
                fldDefn.SetSpatialRef(poSRS);
                poSRS->Release();
                m_poFeatureDefn->AddGeomFieldDefn( &fldDefn );

                aosPaths.push_back(poElt->fieldName());
                m_aaosGeomFieldPaths.push_back(aosPaths);
                if( oMapIndices.find(pszAttrName) == oMapIndices.end() )
                    m_aosGeomIndexes.push_back(oMapIndices[pszAttrName]);
                else
                    m_aosGeomIndexes.push_back("none");
                m_apoCT.push_back(NULL);
            }
            else
            {
                OGRGeomFieldDefn* poFDefn = m_poFeatureDefn->GetGeomFieldDefn(nIndex);
                if( poFDefn->GetType() != eGeomType )
                    poFDefn->SetType(wkbUnknown);
            }
        }
        else if( m_poDS->GetFlattenNestedAttributes() )
        {
            if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
                return;
            aosPaths.push_back(poElt->fieldName());
            for( BSONObj::iterator i(obj); i.more(); )
            {
                BSONElement elt(i.next());
                char szSeparator[2];
                szSeparator[0] = chNestedAttributeSeparator;
                szSeparator[1] = 0;
                CPLString osAttrName(CPLSPrintf("%s%s%s", pszAttrName, szSeparator,
                                                elt.fieldName()));

                std::vector<CPLString> aosNewPaths(aosPaths);
                AddOrUpdateField(osAttrName, &elt,chNestedAttributeSeparator,
                                 aosNewPaths, oMapIndices);
            }
            return;
        }
    }
    else if( eBSONType == Array )
    {
        if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
            return;
        if( oMapIndices.find(pszAttrName) != oMapIndices.end() &&
            oMapIndices[pszAttrName] == "2d" )
        {
            OGRGeomFieldDefn fldDefn( pszAttrName, wkbPoint );
            OGRSpatialReference* poSRS = new OGRSpatialReference();
            poSRS->SetFromUserInput(SRS_WKT_WGS84);
            fldDefn.SetSpatialRef(poSRS);
            poSRS->Release();
            m_poFeatureDefn->AddGeomFieldDefn( &fldDefn );

            aosPaths.push_back(poElt->fieldName());
            m_aaosGeomFieldPaths.push_back(aosPaths);
            m_aosGeomIndexes.push_back("2d");
            m_apoCT.push_back(NULL);
        }
    }
    
    if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
        return;

    OGRFieldSubType eSubType;
    OGRFieldType eNewType = OGRMongoDBGetFieldTypeFromBSON( poElt, eSubType );

    int nIndex = m_poFeatureDefn->GetFieldIndex(pszAttrName);
    if( nIndex < 0 )
    {
        OGRFieldDefn fldDefn( pszAttrName, eNewType );
        fldDefn.SetSubType(eSubType);
        if( eSubType == OFSTBoolean )
            fldDefn.SetWidth(1);
        m_poFeatureDefn->AddFieldDefn( &fldDefn );

        aosPaths.push_back(poElt->fieldName());
        m_aaosFieldPaths.push_back(aosPaths);
    }
    else
    {
        OGRFieldDefn* poFDefn = m_poFeatureDefn->GetFieldDefn(nIndex);
        OGRFieldType eType = poFDefn->GetType();
        if( eType == OFTInteger )
        {
            if( eNewType == OFTInteger &&
                poFDefn->GetSubType() == OFSTBoolean && eSubType != OFSTBoolean )
            {
                poFDefn->SetSubType(OFSTNone);
            }
            else if( eNewType == OFTInteger64 || eNewType == OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTIntegerList || eNewType == OFTInteger64List ||
                     eNewType == OFTRealList || eNewType == OFTStringList )
            {
                if( eNewType != OFTIntegerList || eSubType != OFSTBoolean )
                    poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType != OFTInteger )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTInteger64 )
        {
            if( eNewType == OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTIntegerList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if( eNewType == OFTInteger64List ||
                     eNewType == OFTRealList || eNewType == OFTStringList )
            {
                if( eNewType != OFTIntegerList )
                    poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTReal )
        {
            if( eNewType == OFTIntegerList || eNewType == OFTInteger64List ||
                eNewType == OFTRealList )
            {
                poFDefn->SetType(OFTRealList);
            }
            else if( eNewType == OFTStringList )
            {
                poFDefn->SetType(OFTStringList);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTIntegerList )
        {
            if( eNewType == OFTIntegerList &&
                poFDefn->GetSubType() == OFSTBoolean && eSubType != OFSTBoolean )
            {
                poFDefn->SetSubType(OFSTNone);
            }
            else if( eNewType == OFTInteger64 || eNewType == OFTInteger64List )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if( eNewType == OFTReal || eNewType == OFTRealList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if( eNewType != OFTInteger && eNewType != OFTIntegerList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTStringList);
            }
        }
        else if( eType == OFTInteger64List )
        {
            if( eNewType == OFTReal || eNewType == OFTRealList )
                poFDefn->SetType(OFTRealList);
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTIntegerList && eNewType != OFTInteger64List )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTStringList);
            }
        }
        else if( eType == OFTRealList )
        {
            if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                eNewType != OFTReal &&
                eNewType != OFTIntegerList && eNewType != OFTInteger64List &&
                eNewType != OFTRealList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTStringList);
            }
        }
        else if( eType == OFTDateTime )
        {
            if( eNewType != OFTDateTime )
            {
                poFDefn->SetType(OFTString);
            }
        }
    }
}

/************************************************************************/
/*                         CollectGeomIndices()                         */
/************************************************************************/

std::map< CPLString, CPLString> OGRMongoDBLayer::CollectGeomIndices()
{
    std::map< CPLString, CPLString> oMapIndices;
    try
    {
        std::auto_ptr<DBClientCursor> cursor =
            m_poDS->GetConn()->enumerateIndexes(m_osQualifiedCollection);
        while( cursor->more() )
        {
            BSONObj obj = cursor->nextSafe();
            BSONElement key = obj.getField("key");
            if( !key.eoo() && key.type() == Object )
            {
                for( BSONObj::iterator i(key.Obj()); i.more(); )
                {
                    BSONElement elt(i.next());
                    if( elt.type() == String &&
                        (elt.String() == "2d" || elt.String() == "2dsphere") )
                    {
                        //CPLDebug("MongoDB", "Index %s for %s of %s",
                        //         elt.String().c_str(), elt.fieldName(), m_osQualifiedCollection.c_str());
                        oMapIndices[elt.fieldName()] = elt.String().c_str();
                    }
                }
            }
        }
    }
    catch( const DBException &e )
    {
        CPLDebug("MongoDB", "Error when listing indices: %s", e.what());
    }
    return oMapIndices;
}

/************************************************************************/
/*                          ReadOGRMetadata()                           */
/************************************************************************/

int OGRMongoDBLayer::ReadOGRMetadata(std::map< CPLString, CPLString>& oMapIndices)
{
    try
    {
        std::auto_ptr<DBClientCursor> cursor = m_poDS->GetConn()->query(
            CPLSPrintf("%s._ogr_metadata", m_osDatabase.c_str()),
            BSON("layer" << m_osCollection.c_str()), 1);
        if( cursor->more() )
        {
            BSONObj obj = cursor->nextSafe();

            BSONElement fid = obj.getField("fid");
            if( !fid.eoo() && fid.type() == String )
                m_osFID = fid.String();
            
            BSONElement fields = obj.getField("fields");
            if( !fields.eoo() && fields.type() == Array )
            {
                std::vector<BSONElement> oArray = fields.Array();
                int nSize = (int)oArray.size();
                for(int i=0;i<nSize;i++)
                {
                    BSONElement& elt = oArray[i];
                    if( elt.type() == Object )
                    {
                        BSONObj obj2(elt.Obj());
                        BSONElement name = obj2.getField("name");
                        BSONElement type = obj2.getField("type");
                        BSONElement subtype = obj2.getField("subtype");
                        BSONElement path = obj2.getField("path");
                        if( !name.eoo() && name.type() == String &&
                            !type.eoo() && type.type() == String &&
                            !path.eoo() && path.type() == Array )
                        {
                            if( name.String() == "_id" )
                                continue;
                            OGRFieldType eType(OFTString);
                            for(int i=0; i<=OFTMaxType;i++)
                            {
                                if( EQUAL(OGR_GetFieldTypeName((OGRFieldType)i),
                                            type.String().c_str()) )
                                {
                                    eType = (OGRFieldType)i;
                                    break;
                                }
                            }
                            OGRFieldDefn oFieldDefn(name.String().c_str(), eType);
                            if( !subtype.eoo() && subtype.type() == String &&
                                subtype.String() == "Boolean" )
                                oFieldDefn.SetSubType(OFSTBoolean);
                            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                            
                            std::vector<CPLString> aosPaths;
                            std::vector<BSONElement> oPathArray = path.Array();
                            for(int j=0;j<(int)oPathArray.size();j++)
                            {
                                BSONElement& eltPath = oPathArray[j];
                                aosPaths.push_back(eltPath.String().c_str());
                            }
                            m_aaosFieldPaths.push_back(aosPaths);
                        }
                    }
                }
            }
            
            BSONElement geomfields = obj.getField("geomfields");
            if( !geomfields.eoo() && geomfields.type() == Array )
            {
                std::vector<BSONElement> oArray = geomfields.Array();
                int nSize = (int)oArray.size();
                for(int i=0;i<nSize;i++)
                {
                    BSONElement& elt = oArray[i];
                    if( elt.type() == Object )
                    {
                        BSONObj obj2(elt.Obj());
                        BSONElement name = obj2.getField("name");
                        BSONElement type = obj2.getField("type");
                        BSONElement path = obj2.getField("path");
                        if( !name.eoo() && name.type() == String &&
                            !type.eoo() && type.type() == String &&
                            !path.eoo() && path.type() == Array )
                        {
                            OGRwkbGeometryType eType(OGRFromOGCGeomType(type.String().c_str()));
                            OGRGeomFieldDefn oFieldDefn(name.String().c_str(), eType);
                            OGRSpatialReference* poSRS = new OGRSpatialReference();
                            poSRS->SetFromUserInput(SRS_WKT_WGS84);
                            oFieldDefn.SetSpatialRef(poSRS);
                            poSRS->Release();
                            m_poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);
                            
                            std::vector<CPLString> aosPaths;
                            std::vector<BSONElement> oPathArray = path.Array();
                            for(int j=0;j<(int)oPathArray.size();j++)
                            {
                                BSONElement& eltPath = oPathArray[j];
                                aosPaths.push_back(eltPath.String().c_str());
                            }
                            m_aaosGeomFieldPaths.push_back(aosPaths);
                            if( oMapIndices.find(oFieldDefn.GetNameRef()) != oMapIndices.end() )
                                m_aosGeomIndexes.push_back(oMapIndices[oFieldDefn.GetNameRef()]);
                            else
                                m_aosGeomIndexes.push_back("none");
                            //CPLDebug("MongoDB", "Layer %s: m_aosGeomIndexes[%d] = %s",
                            //         m_osQualifiedCollection.c_str(),
                            //         m_poFeatureDefn->GetGeomFieldCount()-1,
                            //         m_aosGeomIndexes[m_poFeatureDefn->GetGeomFieldCount()-1].c_str());
                            m_apoCT.push_back(NULL);
                        }
                    }
                }
            }
            
            m_bLayerMetadataUpdatable = TRUE;
            return TRUE;
        }
    }
    catch( const DBException &e )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s: %s",
                    "ReadOGRMetadata()", e.what());
    }
    return FALSE;
}

/************************************************************************/
/*                         EstablishFeatureDefn()                       */
/************************************************************************/

void OGRMongoDBLayer::EstablishFeatureDefn()
{
    if( m_bHasEstablishedFeatureDefn )
        return;
    m_bHasEstablishedFeatureDefn = TRUE;

    std::map< CPLString, CPLString> oMapIndices(CollectGeomIndices());

    int nCount = m_poDS->GetFeatureCountToEstablishFeatureDefn();
    if( m_poDS->UseOGRMetadata() )
    {
        if( ReadOGRMetadata(oMapIndices) )
            nCount = 0;
    }

    if( nCount != 0 )
    {
        if( nCount < 0 )
            nCount = 0; /* unlimited */
        
        try
        {
            std::auto_ptr<DBClientCursor> cursor = m_poDS->GetConn()->query(
                                                m_osQualifiedCollection,
                                                BSONObj(),
                                                nCount,
                                                0, /* nToSkip */
                                                NULL, /* fieldsToReturn */
                                                0, /* queryOptions */
                                                m_poDS->GetBatchSize());
            while ( cursor->more() )
            {
                BSONObj obj = cursor->nextSafe();
                for( BSONObj::iterator i(obj); i.more(); )
                {
                    BSONElement elt(i.next());
                    if( EQUAL(elt.fieldName(), m_poDS->GetFID()) )
                    {
                        m_osFID = elt.fieldName();
                    }
                    else
                    {
                        std::vector<CPLString> aosPaths;
                        AddOrUpdateField(elt.fieldName(), &elt,
                                         '.', aosPaths, oMapIndices);
                    }
                }
            }
        }
        catch( const DBException &e )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "EstablishFeatureDefn()", e.what());
        }
    }

    if( m_poDS->JSonField() )
    {
        OGRFieldDefn fldDefn("_json", OFTString);
        m_poFeatureDefn->AddFieldDefn( &fldDefn );
        std::vector<CPLString> aosPaths;
        m_aaosFieldPaths.push_back(aosPaths);
    }
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char* OGRMongoDBLayer::GetFIDColumn()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    return m_osFID.c_str();
}

/************************************************************************/
/*                            BuildQuery()                              */
/************************************************************************/

BSONObj OGRMongoDBLayer::BuildQuery()
{
    BSONObjBuilder b;
    b.appendElements(m_oQueryAttr);
    b.appendElementsUnique(m_oQuerySpat);
    return b.obj();
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

GIntBig OGRMongoDBLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery != NULL ||
        (m_poFilterGeom != NULL && !TestCapability(OLCFastSpatialFilter)) )
    {
        return OGRLayer::GetFeatureCount(bForce);
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( m_poBulkBuilder )
        SyncToDisk();

    try
    {
         return (GIntBig) m_poDS->GetConn()->count(m_osQualifiedCollection,
                                                 BuildQuery());
    }
    catch( const DBException &e )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "%s: %s",
                 "GetFeatureCount()", e.what());
        return OGRLayer::GetFeatureCount(bForce);
    }
}

/************************************************************************/
/*                             Stringify()                              */
/************************************************************************/

static CPLString Stringify(const BSONElement& elt)
{
    BSONType eBSONType = elt.type();
    if( eBSONType == String )
        return elt.String();
    else if( eBSONType == NumberInt )
        return CPLSPrintf("%d", elt.Int());
    else if( eBSONType == NumberLong )
        return CPLSPrintf(CPL_FRMT_GIB, elt.Long());
    else if( eBSONType == NumberDouble )
        return CPLSPrintf("%.16g", elt.Double());
    else if( eBSONType == jstOID )
        return elt.OID().toString().c_str();
    else if( eBSONType == Bool )
        return CPLSPrintf("%d", elt.Bool());
    else if( eBSONType == Date )
    {
        GIntBig secsandmillis = (GIntBig)elt.Date().millis;
        struct tm tm;
        GIntBig secs = secsandmillis / 1000;
        int millis = (int)(secsandmillis % 1000);
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
        // Doesn't work with dates < 1970
        //return dateToISOStringUTC(elt.Date()).c_str();
    }
    else
        return elt.jsonString(Strict, false).c_str();
}

/************************************************************************/
/*                   OGRMongoDBReaderSetField()                         */
/************************************************************************/

static void OGRMongoDBReaderSetField( OGRLayer* poLayer,
                                      OGRFeature* poFeature,
                                      const char* pszAttrName,
                                      const BSONElement* poElt,
                                      bool bFlattenNestedAttributes,
                                      char chNestedAttributeSeparator )
{
    int nGeomFieldIndex;
    if( poElt->type() == Object &&
        (nGeomFieldIndex = poFeature->GetGeomFieldIndex(pszAttrName)) >= 0 )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRGeometry* poGeom = (OGRGeometry*)OGR_G_CreateGeometryFromJson( Stringify(*poElt) );
        CPLPopErrorHandler();
        if( poGeom != NULL )
        {
            poGeom->assignSpatialReference(
                poFeature->GetDefnRef()->GetGeomFieldDefn(nGeomFieldIndex)->GetSpatialRef() );
            poFeature->SetGeomFieldDirectly(nGeomFieldIndex, poGeom);
        }
        return;
    }
    else if( poElt->type() == Array &&
        (nGeomFieldIndex = poFeature->GetGeomFieldIndex(pszAttrName)) >= 0 )
    {
        std::vector<BSONElement> oArray = poElt->Array();
        int nSize = (int)oArray.size();
        if( nSize == 2 )
        {
            BSONElement& x = oArray[0];
            BSONElement& y = oArray[1];
            if( x.type() == NumberDouble && y.type() == NumberDouble )
            {
                OGRGeometry* poGeom = new OGRPoint( x.Double(), y.Double() );
                poGeom->assignSpatialReference(
                    poFeature->GetDefnRef()->GetGeomFieldDefn(nGeomFieldIndex)->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly(nGeomFieldIndex, poGeom);
            }
        }
        return;
    }
        
    if( bFlattenNestedAttributes && poElt->type() == Object )
    {
        BSONObj obj(poElt->Obj());
        for( BSONObj::iterator i(obj); i.more(); )
        {
            BSONElement elt(i.next());
            char szSeparator[2];
            szSeparator[0] = chNestedAttributeSeparator;
            szSeparator[1] = 0;
            CPLString osAttrName(CPLSPrintf("%s%s%s", pszAttrName, szSeparator,
                                            elt.fieldName()));
            OGRMongoDBReaderSetField(poLayer, poFeature,
                                     osAttrName, &elt,
                                     bFlattenNestedAttributes,
                                     chNestedAttributeSeparator);
        }
        return ;
    }
    
    int nField = poFeature->GetFieldIndex(pszAttrName);
    if( nField < 0 )
        return;
    OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(nField);
    CPLAssert( NULL != poFieldDefn );
    BSONType eBSONType = poElt->type();
    OGRFieldType eType = poFieldDefn->GetType();
    if( eBSONType == jstNULL )
        return;
    
    if( eBSONType == NumberInt )
        poFeature->SetField( nField, poElt->Int() );
    else if( eBSONType == NumberLong )
        poFeature->SetField( nField, poElt->Long() );
    else if( eBSONType == NumberDouble )
        poFeature->SetField( nField, poElt->Double() );
    else if( eBSONType == MinKey && eType == OFTReal )
        poFeature->SetField( nField, -std::numeric_limits<double>::infinity() );
    else if( eBSONType == MaxKey && eType == OFTReal )
        poFeature->SetField( nField, std::numeric_limits<double>::infinity() );
    else if( eBSONType == MinKey && eType == OFTInteger )
        poFeature->SetField( nField, INT_MIN );
    else if( eBSONType == MaxKey && eType == OFTInteger )
        poFeature->SetField( nField, INT_MAX );
    else if( eBSONType == MinKey && eType == OFTInteger64 )
        poFeature->SetField( nField, std::numeric_limits<GIntBig>::min() );
    else if( eBSONType == MaxKey && eType == OFTInteger64 )
        poFeature->SetField( nField, std::numeric_limits<GIntBig>::max() );
    else if( eBSONType == Array )
    {
        std::vector<BSONElement> oArray = poElt->Array();
        int nSize = (int)oArray.size();
        if( eType == OFTStringList )
        {
            char** papszValues = (char**)CPLCalloc(nSize + 1, sizeof(char*));
            for(int i=0;i<nSize;i++)
            {
                BSONElement& elt = oArray[i];
                eBSONType = elt.type();
                papszValues[i] = CPLStrdup(Stringify(elt));
            }
            poFeature->SetField( nField, papszValues );
            CSLDestroy(papszValues);
        }
        else if( eType == OFTRealList )
        {
            double* padfValues = (double*)CPLMalloc(nSize * sizeof(double));
            for(int i=0;i<nSize;i++)
            {
                BSONElement& elt = oArray[i];
                eBSONType = elt.type();
                if( eBSONType == NumberInt )
                    padfValues[i] = elt.Int();
                else if( eBSONType == NumberLong )
                    padfValues[i] = (double)elt.Long();
                else if( eBSONType == NumberDouble )
                    padfValues[i] = elt.Double();
                else if( eBSONType == MinKey )
                    padfValues[i] = -std::numeric_limits<double>::infinity();
                else if( eBSONType == MaxKey )
                    padfValues[i] = std::numeric_limits<double>::infinity();
                else
                    padfValues[i] = CPLAtof(Stringify(elt));
            }
            poFeature->SetField( nField, nSize, padfValues );
            CPLFree(padfValues);
        }
        else if( eType == OFTIntegerList )
        {
            int* panValues = (int*)CPLMalloc(nSize * sizeof(int));
            for(int i=0;i<nSize;i++)
            {
                BSONElement& elt = oArray[i];
                eBSONType = elt.type();
                if( eBSONType == NumberInt )
                    panValues[i] = elt.Int();
                else if( eBSONType == NumberLong )
                {
                    GIntBig nVal = elt.Long();
                    if( nVal < INT_MIN )
                        panValues[i] = INT_MIN;
                    else if( nVal > INT_MAX )
                        panValues[i] = INT_MAX;
                    else
                        panValues[i] = (int)nVal;
                }
                else if( eBSONType == NumberDouble )
                {
                    double dfVal = elt.Double();
                    if( dfVal < INT_MIN )
                        panValues[i] = INT_MIN;
                    else if( dfVal > INT_MAX )
                        panValues[i] = INT_MAX;
                    else
                        panValues[i] = (int)dfVal;
                }
                else if( eBSONType == MinKey )
                    panValues[i] = INT_MIN;
                else if( eBSONType == MaxKey )
                    panValues[i] = INT_MAX;
                else
                    panValues[i] = atoi(Stringify(elt));
            }
            poFeature->SetField( nField, nSize, panValues );
            CPLFree(panValues);
        }
        else if( eType == OFTInteger64List )
        {
            GIntBig* panValues = (GIntBig*)CPLMalloc(nSize * sizeof(GIntBig));
            for(int i=0;i<nSize;i++)
            {
                BSONElement& elt = oArray[i];
                eBSONType = elt.type();
                if( eBSONType == NumberInt )
                    panValues[i] = elt.Int();
                else if( eBSONType == NumberLong )
                    panValues[i] = elt.Long();
                else if( eBSONType == NumberDouble )
                {
                    double dfVal = elt.Double();
                    if( dfVal < std::numeric_limits<GIntBig>::min() )
                        panValues[i] = std::numeric_limits<GIntBig>::min();
                    else if( dfVal > std::numeric_limits<GIntBig>::max() )
                        panValues[i] = std::numeric_limits<GIntBig>::max();
                    else
                        panValues[i] = (int)dfVal;
                }
                else if( eBSONType == MinKey )
                    panValues[i] = std::numeric_limits<GIntBig>::min();
                else if( eBSONType == MaxKey )
                    panValues[i] = std::numeric_limits<GIntBig>::max();
                else
                    panValues[i] = CPLAtoGIntBig(Stringify(elt));
            }
            poFeature->SetField( nField, nSize, panValues );
            CPLFree(panValues);
        }
    }
    else if( eBSONType == String )
        poFeature->SetField( nField, poElt->String().c_str() );
    else if( eBSONType == jstOID )
        poFeature->SetField( nField, poElt->OID().toString().c_str() );
    else if( eBSONType == Bool )
        poFeature->SetField( nField, poElt->Bool() );
    else if( eBSONType == BinData )
    {
        int len;
        const char *pabyData = poElt->binDataClean(len);
        poFeature->SetField( nField, len, (GByte*)pabyData);
    }
    else
        poFeature->SetField( nField, Stringify(*poElt) );
}

/************************************************************************/
/*                            Translate()                               */
/************************************************************************/

OGRFeature* OGRMongoDBLayer::Translate(BSONObj& obj)
{
    OGRFeature* poFeature = new OGRFeature(GetLayerDefn());
    try
    {
        for( BSONObj::iterator i(obj); i.more(); )
        {
            BSONElement elt(i.next());
            if( m_osFID.size() && EQUAL(m_osFID, elt.fieldName()) )
            {
                BSONType eBSONType = elt.type();
                if( eBSONType == NumberInt )
                    poFeature->SetFID(elt.Int());
                else if( eBSONType == NumberLong )
                    poFeature->SetFID(elt.Long());
                else if( eBSONType == NumberDouble )
                    poFeature->SetFID((GIntBig)elt.Double());
            }
            else
            {
                OGRMongoDBReaderSetField(this, poFeature,
                                         elt.fieldName(),
                                         &elt,
                                         m_poDS->GetFlattenNestedAttributes(),
                                         '.');
            }
        }

        if( m_poDS->JSonField() )
        {
            poFeature->SetField("_json", obj.jsonString().c_str());
        }
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "Translate()", e.what());
    }
    return poFeature;
}

/************************************************************************/
/*                            GetNextFeature()                          */
/************************************************************************/

OGRFeature* OGRMongoDBLayer::GetNextFeature()
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( m_poBulkBuilder )
        SyncToDisk();

    try
    {
        if( !m_bCursorValid )
        {
            m_poCursor = m_poDS->GetConn()->query(m_osQualifiedCollection,
                                              BuildQuery(), 
                                              0, /* nToReturn */
                                              0, /* nToSkip */
                                              NULL, /* fieldsToReturn */
                                              0, /* queryOptions */
                                              m_poDS->GetBatchSize());
            m_bCursorValid = TRUE;
        }
    
        while(TRUE)
        {
            if( !m_poCursor->more() )
                return NULL;
            BSONObj obj = m_poCursor->nextSafe();
            
            OGRFeature* poFeature = Translate(obj);
            if( poFeature->GetFID() < 0 )
                poFeature->SetFID(++m_nIndex);
            
            if((m_poFilterGeom == NULL
                || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            {
                return poFeature;
            }
            else
                delete poFeature;
        }
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "GetNextFeature()", e.what());
        return NULL;
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature* OGRMongoDBLayer::GetFeature(GIntBig nFID)
{
    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( m_poBulkBuilder )
        SyncToDisk();
        
    if( m_osFID.size() == 0 )
    {
        BSONObj oQueryAttrBak(m_oQueryAttr), oQuerySpatBak(m_oQuerySpat);
        OGRFeature* poFeature = OGRLayer::GetFeature(nFID);
        m_oQueryAttr = oQueryAttrBak;
        m_oQuerySpat = oQuerySpatBak;
        return poFeature;
    }
    
    try
    {
        std::auto_ptr<DBClientCursor> cursor = m_poDS->GetConn()->query(
            m_osQualifiedCollection,
            BSON(m_osFID.c_str() << nFID), 
            1);
        if( !cursor->more() )
            return NULL;
        BSONObj obj = cursor->nextSafe();
        
        OGRFeature* poFeature = Translate(obj);
        poFeature->SetFID(nFID);
        return poFeature;        
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "GetFeature()", e.what());
        return NULL;
    }
}

/************************************************************************/
/*                             DeleteFeature()                          */
/************************************************************************/

OGRErr OGRMongoDBLayer::DeleteFeature(GIntBig nFID)
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( m_poBulkBuilder )
        SyncToDisk();
    if( m_osFID.size() == 0 )
        return OGRERR_FAILURE;
    
    try
    {
        BSONObj obj = m_poDS->GetConn()->findAndRemove(
            m_osQualifiedCollection,
            BSON(m_osFID.c_str() << nFID), 
            BSONObj(),
            BSONObj());
        return (obj.isEmpty()) ? OGRERR_NON_EXISTING_FEATURE : OGRERR_NONE;
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "DeleteFeature()", e.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMongoDBLayer::CreateField( OGRFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( m_poFeatureDefn->GetFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        if( !EQUAL(poFieldIn->GetNameRef(), "_id") &&
            !EQUAL(poFieldIn->GetNameRef(), "_json") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateField() called with an already existing field name: %s",
                     poFieldIn->GetNameRef());
        }
        return OGRERR_FAILURE;
    }

    m_poFeatureDefn->AddFieldDefn( poFieldIn );

    std::vector<CPLString> aosPaths;
    if( m_bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(poFieldIn->GetNameRef(), ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPaths.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPaths.push_back(poFieldIn->GetNameRef());
    m_aaosFieldPaths.push_back(aosPaths);

    m_bUpdateLayerMetadata = m_bLayerMetadataUpdatable;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRMongoDBLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

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
    
    OGRCoordinateTransformation* poCT = NULL;
    if( oFieldDefn.GetSpatialRef() != NULL )
    {
        OGRSpatialReference oSRS_WGS84;
        oSRS_WGS84.SetFromUserInput(SRS_WKT_WGS84);
        if( !oSRS_WGS84.IsSame(oFieldDefn.GetSpatialRef()) )
        {
            poCT = OGRCreateCoordinateTransformation( oFieldDefn.GetSpatialRef(), &oSRS_WGS84 );
            if( poCT == NULL )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "On-the-fly reprojection to WGS84 longlat would be needed, but instanciation of transformer failed");
            }
        }
    }
    m_apoCT.push_back(poCT);

    if( m_bCreateSpatialIndex )
    {
        //CPLDebug("MongoDB", "Create spatial index for %s of %s",
        //         poFieldIn->GetNameRef(), m_osQualifiedCollection.c_str());
        try
        {
            const char* pszIndexType;
            if( wkbFlatten(poFieldIn->GetType()) != wkbPoint )
                pszIndexType = "2dsphere";
            else
                pszIndexType = CPLGetConfigOption("OGR_MONGODB_SPAT_INDEX_TYPE", "2dsphere");
            m_poDS->GetConn()->createIndex(m_osQualifiedCollection,
                                         BSON( oFieldDefn.GetNameRef() << pszIndexType ));
            m_aosGeomIndexes[m_aosGeomIndexes.size()-1] = pszIndexType;
        }
        catch( const DBException &e )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "Index creation", e.what());
        }
    }

    m_bUpdateLayerMetadata = m_bLayerMetadataUpdatable;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SerializeField()                           */
/************************************************************************/

void OGRMongoDBLayer::SerializeField(BSONObjBuilder& b,
                                     OGRFeature *poFeature,
                                     int i,
                                     const char* pszJSonField)
{
    OGRFieldType eType = m_poFeatureDefn->GetFieldDefn(i)->GetType();
    if( eType == OFTInteger )
    {
        if( m_poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
            b.append( pszJSonField, (bool)poFeature->GetFieldAsInteger(i) );
        else
            b.append( pszJSonField, poFeature->GetFieldAsInteger(i) );
    }
    else if( eType == OFTInteger64 )
        b.append( pszJSonField, poFeature->GetFieldAsInteger64(i) );
    else if( eType == OFTReal )
        b.append( pszJSonField, poFeature->GetFieldAsDouble(i) );
    else if( eType == OFTString )
        b.append( pszJSonField, poFeature->GetFieldAsString(i) );
    else if( eType == OFTStringList )
    {
        char** papszValues = poFeature->GetFieldAsStringList(i);
        BSONArrayBuilder arrayBuilder;
        for(int i=0; papszValues[i]; i++)
            arrayBuilder.append( papszValues[i] );
        b.append( pszJSonField, arrayBuilder.arr() );
    }
    else if( eType == OFTIntegerList )
    {
        int nSize;
        const int* panValues = poFeature->GetFieldAsIntegerList(i, &nSize);
        BSONArrayBuilder arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( panValues[i] );
        b.append( pszJSonField, arrayBuilder.arr() );
    }
    else if( eType == OFTInteger64List )
    {
        int nSize;
        const GIntBig* panValues = poFeature->GetFieldAsInteger64List(i, &nSize);
        BSONArrayBuilder arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( panValues[i] );
        b.append( pszJSonField, arrayBuilder.arr() );
    }
    else if( eType == OFTRealList )
    {
        int nSize;
        const double* padfValues = poFeature->GetFieldAsDoubleList(i, &nSize);
        BSONArrayBuilder arrayBuilder;
        for(int i=0; i<nSize; i++)
            arrayBuilder.append( padfValues[i] );
        b.append( pszJSonField, arrayBuilder.arr() );
    }
    else if( eType == OFTBinary )
    {
        int nSize;
        const GByte* pabyData = poFeature->GetFieldAsBinary(i, &nSize);
        b.appendBinData( pszJSonField, nSize, BinDataGeneral, (const void*)pabyData);
    }
    else if( eType == OFTDate || eType == OFTDateTime || eType == OFTTime )
    {
        struct tm tm;
        int nYear, nMonth, nDay, nHour, nMinute, nTZ;
        float fSecond;
        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                      &nHour, &nMinute, &fSecond, &nTZ);
        tm.tm_year = nYear - 1900;
        tm.tm_mon = nMonth - 1;
        tm.tm_mday = nDay;
        tm.tm_hour = nHour;
        tm.tm_min = nMinute;
        tm.tm_sec = (int)fSecond;
        GIntBig millis = 1000 * CPLYMDHMSToUnixTime(&tm) + (GIntBig)(1000 * fmod(fSecond, 1));
        b.append( pszJSonField, Date_t((GUIntBig)millis) );
        //char* pszDT = OGRGetXMLDateTime(poFeature->GetRawFieldRef(i));
        //StatusWith<Date_t> d = dateFromISOString(pszDT);
        //if( d.isOK() )
        //    b.append( pszJSonField, d.getValue() );
        //CPLFree(pszDT);
    }
}

/************************************************************************/
/*                       OGRLocaleSafeFromJSON()                        */
/************************************************************************/

static BSONObj OGRLocaleSafeFromJSON(const char* pszJSon)
{
    CPLThreadLocaleC oCLocale;
    return fromjson(pszJSon);
}

/************************************************************************/
/*                        SerializeGeometry()                           */
/************************************************************************/

void OGRMongoDBLayer::SerializeGeometry(BSONObjBuilder& b,
                                        OGRGeometry* poGeom, int iField,
                                        const char* pszJSonField)
{
    if( m_aosGeomIndexes[iField] == "2d" &&
        wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        BSONArrayBuilder arrayBuilder;
        arrayBuilder.append( ((OGRPoint*)poGeom)->getX() );
        arrayBuilder.append( ((OGRPoint*)poGeom)->getY() );
        b.append( pszJSonField, arrayBuilder.arr() );
    }
    else
    {
        char* pszJSon = OGR_G_ExportToJson((OGRGeometryH)poGeom);
        if( pszJSon )
            b.append(pszJSonField, OGRLocaleSafeFromJSON(pszJSon));
        CPLFree(pszJSon);
    }
}

/************************************************************************/
/*                       SerializeRecursive()                           */
/************************************************************************/

void OGRMongoDBLayer::SerializeRecursive(BSONObjBuilder& b,
                                         OGRFeature *poFeature,
                                         std::map< CPLString, IntOrMap*>& aoMap )
{
    std::map< CPLString, IntOrMap* >::iterator oIter = aoMap.begin();
    for( ; oIter != aoMap.end(); ++oIter)
    {
        IntOrMap* intOrMap = oIter->second;
        if( intOrMap->bIsMap )
        {
            BSONObjBuilder subB;
            SerializeRecursive(subB, poFeature, *(intOrMap->u.poMap));
            b.append( oIter->first.c_str(), subB.obj() );
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

void OGRMongoDBLayer::InsertInMap(IntOrMap* rootMap,
                                  std::map< std::vector<CPLString>, IntOrMap*>& aoMap,
                                  const std::vector<CPLString>& aosFieldPathFull,
                                  int nField)
{
    std::vector<CPLString> aosFieldPath;
    std::vector<CPLString> aosFieldPathPrev;
    for(int j=0; j<(int)aosFieldPathFull.size() - 1; j++)
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
    const CPLString& osLastComponent(aosFieldPathFull[aosFieldPathFull.size() - 1]);
    CPLAssert( (*poPrevMap).find(osLastComponent) == (*poPrevMap).end() );
    (*(poPrevMap))[osLastComponent] = intOrMap;
}

/************************************************************************/
/*                       BuildBSONObjFromFeature()                      */
/************************************************************************/

BSONObj OGRMongoDBLayer::BuildBSONObjFromFeature(OGRFeature* poFeature, int bUpdate)
{
    BSONObjBuilder b;

    int nJSonFieldIndex = m_poFeatureDefn->GetFieldIndex("_json");
    if( nJSonFieldIndex >= 0 && poFeature->IsFieldSet(nJSonFieldIndex) )
    {
        CPLString osJSon(poFeature->GetFieldAsString(nJSonFieldIndex));

        // Workaround bug in  JParse::dateObject() with { "$numberLong": "-123456" }
        // that cannot be parsed successfully
        while(TRUE)
        {
            size_t i = osJSon.find("{ \"$date\" : { \"$numberLong\" : \"-");
            if( i == std::string::npos )
                break;
            size_t j = osJSon.find("\" }", i+strlen("{ \"$date\" : { \"$numberLong\" : \"-"));
            if( j == std::string::npos )
                break;
            GIntBig negNumber = CPLAtoGIntBig(osJSon.c_str() + i+strlen("{ \"$date\" : { \"$numberLong\" : \"-")-1);
            osJSon = osJSon.substr(0, i+strlen("{ \"$date\" : ")) +
                     CPLSPrintf(CPL_FRMT_GIB, negNumber) +
                     osJSon.substr(j+strlen("\" }"));
        }

        BSONObj obj(OGRLocaleSafeFromJSON(osJSon));
        if( (m_bIgnoreSourceID || !obj.hasField("_id")) && !bUpdate )
        {
            const OID generated = OID::gen();
            b.append("_id", generated);
            poFeature->SetField(0, generated.toString().c_str());
        }
        b.appendElementsUnique(obj);
        //BSONObj obj2(b.obj());
        //printf("%s\n", obj2.jsonString(Strict).c_str());
        //return obj2;
        return b.obj();
    }
    
    if( poFeature->GetFID() >= 0 && m_osFID.size() )
    {
        b.append( m_osFID.c_str(), poFeature->GetFID() );
    }
    
    CPLAssert((int)m_aaosFieldPaths.size() == m_poFeatureDefn->GetFieldCount());
    
    if( !poFeature->IsFieldSet(0) || (!bUpdate && m_bIgnoreSourceID) )
    {
        const OID generated = OID::gen();
        b.append("_id", generated);
        poFeature->SetField(0, generated.toString().c_str());
    }
    else
        b.append("_id", OID(poFeature->GetFieldAsString(0)) );
        
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

    CPLAssert((int)m_aaosGeomFieldPaths.size() == m_poFeatureDefn->GetGeomFieldCount());
    CPLAssert((int)m_apoCT.size() == m_poFeatureDefn->GetGeomFieldCount());
    for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
    {
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == NULL )
            continue;
        if( !bUpdate && m_apoCT[i] != NULL )
            poGeom->transform( m_apoCT[i] );

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
    
    return b.obj();
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRMongoDBLayer::ICreateFeature( OGRFeature *poFeature )
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
                m_nNextFID = GetFeatureCount(FALSE);
            poFeature->SetFID(++m_nNextFID);
        }

        BSONObj bsonObj( BuildBSONObjFromFeature(poFeature, FALSE) );
        if( m_poDS->BulkInsert() )
        {
            if( m_nFeaturesInBulk == MAX_DOCS_IN_BULK )
                SyncToDisk();
            if( m_poBulkBuilder == NULL )
                m_poBulkBuilder = new BulkOperationBuilder(
                    m_poDS->GetConn(), m_osQualifiedCollection, false);
            m_poBulkBuilder->insert( bsonObj );
            m_nFeaturesInBulk ++;
        }
        else
        {
            m_poDS->GetConn()->insert( m_osQualifiedCollection, bsonObj );
        }
        
        return OGRERR_NONE;
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "CreateFeature()", e.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ISetFeature()                             */
/************************************************************************/

OGRErr OGRMongoDBLayer::ISetFeature( OGRFeature *poFeature )
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( !m_bHasEstablishedFeatureDefn )
        EstablishFeatureDefn();
    if( m_poBulkBuilder )
        SyncToDisk();

    if( !poFeature->IsFieldSet(0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "_id field not set");
        return OGRERR_FAILURE;
    }
    
    try
    {
        BSONObj obj( BuildBSONObjFromFeature(poFeature, TRUE) );
        // TODO? we should theoretically detect if the provided _id doesn't exist
        m_poDS->GetConn()->update( m_osQualifiedCollection,
                                 MONGO_QUERY("_id" << obj.getField("_id")),
                                 obj, false, false );
        return OGRERR_NONE;
    }
    catch( const DBException &e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                 "SetFeature()", e.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRMongoDBLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCRandomRead) )
    {
        EstablishFeatureDefn();
        return m_osFID.size() > 0;
    }
    else if( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        EstablishFeatureDefn();
        for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
        {
            if( m_aosGeomIndexes[i] == "none" )
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    else if( EQUAL(pszCap, OLCCreateField) ||
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
               m_osFID.size() > 0;
    }
    
    return FALSE;
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr OGRMongoDBLayer::SetAttributeFilter(const char* pszFilter)
{
    m_oQueryAttr = BSONObj();
    
    if( pszFilter != NULL && pszFilter[0] == '{' )
    {
        OGRLayer::SetAttributeFilter(NULL);
        try
        {
            m_oQueryAttr = OGRLocaleSafeFromJSON(pszFilter);
            return OGRERR_NONE;
        }
        catch( const DBException &e )
        {
            m_oQueryAttr = BSONObj();
            CPLError(CE_Failure, CPLE_AppDefined, "%s: %s",
                     "SetAttributeFilter()", e.what());
            return OGRERR_FAILURE;
        }
    }
    return OGRLayer::SetAttributeFilter(pszFilter);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRMongoDBLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

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

    m_oQuerySpat = BSONObj();
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
                m_oQuerySpat = OGRLocaleSafeFromJSON(CPLSPrintf("{ \"%s\" : { $geoIntersects : "
                "{ $geometry : { type : \"Polygon\" , coordinates : [["
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
                m_oQuerySpat = OGRLocaleSafeFromJSON(CPLSPrintf("{ \"%s\" : { $geoWithin : "
                "{ $box : [ [ %.16g , %.16g ] , [ %.16g , %.16g ] ] } } }",
                              m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef(),
                              sEnvelope.MinX, sEnvelope.MinY,
                              sEnvelope.MaxX, sEnvelope.MaxY));
            }
        }
        catch( const DBException &e )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: %s", "SetSpatialFilter()", e.what());
        }        
    }
}
/************************************************************************/
/*                        OGRMongoDBDataSource()                        */
/************************************************************************/

OGRMongoDBDataSource::OGRMongoDBDataSource()
{
    m_poConn = NULL;
    m_nBatchSize = 0;
    m_nFeatureCountToEstablishFeatureDefn = 0;
    m_bJSonField = FALSE;
    m_bUseOGRMetadata = TRUE;
    m_bBulkInsert = TRUE;
}

/************************************************************************/
/*                       ~OGRMongoDBDataSource()                        */
/************************************************************************/

OGRMongoDBDataSource::~OGRMongoDBDataSource()
{
    for(int i=0; i<(int)m_apoLayers.size(); i++ )
    {
        delete m_apoLayers[i];
    }
    delete m_poConn;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer* OGRMongoDBDataSource::GetLayer(int nIdx)
{
    if( nIdx < 0 || nIdx >= (int)m_apoLayers.size() )
        return NULL;
    return m_apoLayers[nIdx];
}

/************************************************************************/
/*                         GetLayerByName()                             */
/************************************************************************/

OGRLayer *OGRMongoDBDataSource::GetLayerByName(const char* pszLayerName)
{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszLayerName);
    if( poLayer != NULL )
        return poLayer;

    for(int i=0; i<(int)m_apoLayers.size(); i++ )
    {
        m_apoLayers[i]->SyncToDisk();
    }

    CPLString osDatabase;
    if( m_osDatabase.size() == 0 )
    {
        const char* pszDot = strchr(pszLayerName, '.');
        if( pszDot == NULL )
            return NULL;
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
            std::list<std::string> l = m_poConn->getCollectionNames( osDatabase );
            for ( std::list<std::string>::iterator i = l.begin(); i != l.end(); i++ )
            {
                const std::string& m_osCollection(*i);
                if( EQUAL(m_osCollection.c_str(),pszLayerName) )
                {
                    OGRMongoDBLayer* poLayer = new OGRMongoDBLayer(this,
                                                          osDatabase,
                                                          m_osCollection.c_str());
                    m_apoLayers.push_back(poLayer);
                    return poLayer;
                }
            }
        }
        catch( const DBException &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Command failed: %s", e.what());
        }
        if( i == 0 )
        {
            if( m_osDatabase.size() == 0 )
                break;
            const char* pszDot = strchr(pszLayerName, '.');
            if( pszDot == NULL )
                break;
            osDatabase = pszLayerName;
            osDatabase.resize(pszDot - pszLayerName);
            pszLayerName = pszDot + 1;
        }
    }
    
    return NULL;
}

/************************************************************************/
/*                            Initialize()                              */
/************************************************************************/

int OGRMongoDBDataSource::Initialize(char** papszOpenOptions)
{
    CPLString osPEMKeyFile = CSLFetchNameValueDef(papszOpenOptions, "SSL_PEM_KEY_FILE", "");
    CPLString osPEMKeyPassword = CSLFetchNameValueDef(papszOpenOptions, "SSL_PEM_KEY_PASSWORD", "");
    CPLString osCAFile = CSLFetchNameValueDef(papszOpenOptions, "SSL_CA_FILE", "");
    CPLString osCRLFile = CSLFetchNameValueDef(papszOpenOptions, "SSL_CRL_FILE", "");
    int bAllowInvalidCertificates = CSLFetchBoolean(papszOpenOptions, "SSL_ALLOW_INVALID_CERTIFICATES", FALSE);
    int bAllowInvalidHostnames = CSLFetchBoolean(papszOpenOptions, "SSL_ALLOW_INVALID_HOSTNAMES", FALSE);
    int bFIPSMode = CSLFetchBoolean(papszOpenOptions, "FIPS_MODE", FALSE);
    if( bMongoInitialized < 0 )
    {
        Options options;
        osStaticPEMKeyFile = osPEMKeyFile;
        osStaticPEMKeyPassword = osPEMKeyPassword;
        osStaticCAFile = osCAFile;
        osStaticCRLFile = osCRLFile;
        bStaticAllowInvalidCertificates = bAllowInvalidCertificates;
        bStaticAllowInvalidHostnames = bAllowInvalidHostnames;
        bStaticFIPSMode = bFIPSMode;
        
        if( osPEMKeyFile.size() || osPEMKeyPassword.size() ||
            osCAFile.size() || osCRLFile.size() )
        {
            options.setSSLMode(Options::kSSLRequired);
            if( osPEMKeyFile.size() )
                options.setSSLPEMKeyFile(osPEMKeyFile);
            if( osPEMKeyPassword )
                options.setSSLPEMKeyPassword(osPEMKeyPassword);
            if( osCAFile.size() )
                options.setSSLCAFile(osCAFile);
            if( osCRLFile.size() )
                options.setSSLCRLFile(osCRLFile);
            if( bAllowInvalidCertificates )
                options.setSSLAllowInvalidCertificates(true);
            if( bAllowInvalidHostnames )
                options.setSSLAllowInvalidHostnames(true);
        }
        if( bFIPSMode )
            options.setFIPSMode(true);
        Status status = client::initialize(options);
        bMongoInitialized = status.isOK();
        if( !status.isOK() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Mongo initialization failed: %s", status.toString().c_str());
            return FALSE;
        }
    }
    else if( !bMongoInitialized )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Previous initialization of MongoDB failed");
        return FALSE;
    }
    
    if( osPEMKeyFile != osStaticPEMKeyFile )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_PEM_KEY_FILE");
    }
    if( osPEMKeyPassword != osStaticPEMKeyPassword )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_PEM_KEY_PASSWORD");
    }
    if( osCAFile != osStaticCAFile )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_CA_FILE");
    }
    if( osCRLFile != osStaticCRLFile )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_CRL_FILE");
    }
    if( bAllowInvalidCertificates != bStaticAllowInvalidCertificates )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_ALLOW_INVALID_CERTIFICATES");
    }
    if( bAllowInvalidHostnames != bStaticAllowInvalidHostnames )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "SSL_ALLOW_INVALID_HOSTNAMES");
    }
    if( bFIPSMode != bStaticFIPSMode )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Value of %s different from first initialization. Using initial value",
                 "FIPS_MODE");
    }
    return TRUE;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

int OGRMongoDBDataSource::Open(const char* pszFilename,
                               GDALAccess eAccess,
                               char** papszOpenOptions)
{
    if( !Initialize(papszOpenOptions) )
        return FALSE;
    
    this->eAccess = eAccess;
    
    const char* pszHost = CSLFetchNameValueDef(papszOpenOptions, "HOST", "localhost");
    const char* pszPort = CSLFetchNameValueDef(papszOpenOptions, "PORT", "27017");
    const char* pszURI = CSLFetchNameValue(papszOpenOptions, "URI");
    if( EQUALN(pszFilename, "mongodb://", strlen("mongodb://")) )
        pszURI = pszFilename;

    std::string errmsg;
    if( pszURI != NULL )
    {
        try
        {
            /**
             * ConnectionString can parse MongoDB URIs with the following format:
             *
             *    mongodb://[usr:pwd@]host1[:port1]...[,hostN[:portN]]][/[db][?options]]
             *
             * For a complete list of URI string options, see
             * http://docs.mongodb.org/v2.6/reference/connection-string/
             *
             * Examples:
             *
             *    A replica set with three members (one running on default port 27017):
             *      string uri = mongodb://localhost,localhost:27018,localhost:27019
             *
             *    Authenticated connection to db 'bedrock' with user 'barney' and pwd 'rubble':
             *      string url = mongodb://barney:rubble@localhost/bedrock
             */
            
            ConnectionString cs = ConnectionString::parse( pszURI, errmsg );
            if( !cs.isValid() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Bad connection string: %s", errmsg.c_str());
                return FALSE;
            }
            m_osDatabase = cs.getDatabase();
            m_poConn = cs.connect( errmsg );
            if( m_poConn == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot connect: %s", errmsg.c_str());
                return FALSE;
            }
        }
        catch( const DBException &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot connect: %s", e.what());
            return FALSE;
        }
    }
    else
    {
        DBClientConnection* poConn = new DBClientConnection();
        m_poConn = poConn;
        try
        {
            if( !poConn->connect(CPLSPrintf("%s:%s", pszHost, pszPort), errmsg ) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot connect: %s", errmsg.c_str());
                return FALSE;
            }
        }
        catch( const DBException &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot connect: %s", e.what());
            return FALSE;
        }
    }

    if( m_osDatabase.size() == 0 )
    {
        m_osDatabase = CSLFetchNameValueDef(papszOpenOptions, "DBNAME", "");
        /*if( m_osDatabase.size() == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No database name specified");
            return FALSE;
        }*/
    }

    const char* pszAuthJSON = CSLFetchNameValue(papszOpenOptions, "AUTH_JSON");
    if( pszAuthJSON )
    {
        try
        {
             /* The "params" BSONObj should be initialized with some of the fields below.  Which fields
             * are required depends on the mechanism, which is mandatory.
             *
             *     "mechanism": The string name of the sasl mechanism to use.  Mandatory.
                    const char kAuthMechMongoCR[] = "MONGODB-CR";
                    const char kAuthMechScramSha1[] = "SCRAM-SHA-1";
                    const char kAuthMechDefault[] = "DEFAULT";
             *     "user": The string name of the user to authenticate.  Mandatory.
             *     "db": The database target of the auth command, which identifies the location
             *         of the credential information for the user.  May be "$external" if
             *         credential information is stored outside of the mongo cluster.  Mandatory.
             *     "pwd": The password data.
             *     "digestPassword": Boolean, set to true if the "pwd" is undigested (default).
             *     "serviceName": The GSSAPI service name to use.  Defaults to "mongodb".
             *     "serviceHostname": The GSSAPI hostname to use.  Defaults to the name of the remote
             *          host.
             */
            m_poConn->auth(OGRLocaleSafeFromJSON(pszAuthJSON));
        }
        catch( const DBException &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Authentication failed: %s", e.what());
            return FALSE;
        }
    }
    else
    {
        const char* pszUser = CSLFetchNameValue(papszOpenOptions, "USER");
        const char* pszPassword = CSLFetchNameValue(papszOpenOptions, "PASSWORD");
        const char* pszAuthDBName = CSLFetchNameValue(papszOpenOptions, "AUTH_DBNAME");
        if( (pszUser != NULL && pszPassword == NULL) ||
            (pszUser == NULL && pszPassword != NULL) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "USER and PASSWORD open options must be both specified.");
            return FALSE;
        }
        if( pszUser && pszPassword )
        {
            if( m_osDatabase.size() == 0 && pszAuthDBName == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "No database or authentication database name specified.");
                return FALSE;
            }
            try
            {
                if( !m_poConn->auth(pszAuthDBName ? pszAuthDBName : m_osDatabase.c_str(),
                                    pszUser, pszPassword, errmsg) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Authentication failed: %s", errmsg.c_str());
                    return FALSE;
                }
            }
            catch( const DBException &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Authentication failed: %s", e.what());
                return FALSE;
            }
        }
        else if( pszAuthDBName != NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "AUTH_DBNAME ignored when USER and PASSWORD open options are not specified.");
        }
    }
    
    m_nBatchSize = atoi(CSLFetchNameValueDef(papszOpenOptions, "BATCH_SIZE", "0"));
    m_nFeatureCountToEstablishFeatureDefn = atoi(CSLFetchNameValueDef(
        papszOpenOptions, "FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN", "100"));
    m_bJSonField = CSLFetchBoolean(papszOpenOptions, "JSON_FIELD", FALSE);
    m_bFlattenNestedAttributes = CSLFetchBoolean(
            papszOpenOptions, "FLATTEN_NESTED_ATTRIBUTES", TRUE);
    m_osFID = CSLFetchNameValueDef(papszOpenOptions, "FID", "ogc_fid");
    m_bUseOGRMetadata = CSLFetchBoolean(
            papszOpenOptions, "USE_OGR_METADATA", TRUE);
    m_bBulkInsert = CSLFetchBoolean(papszOpenOptions, "BULK_INSERT", TRUE);
    
    int bRet = TRUE;
    if( m_osDatabase.size() == 0 )
    {
        try
        {
            std::list<std::string> l = m_poConn->getDatabaseNames();
            for ( std::list<std::string>::iterator i = l.begin(); i != l.end(); i++ )
            {
                bRet &= ListLayers((*i).c_str());
            }
        }
        catch( const DBException &e)
        {
            if( e.getCode() == 10005 )
            {
                try
                {
                    BSONObj info;
                    m_poConn->runCommand("admin", BSON("listDatabases" << 1),
                                          info, QueryOption_SlaveOk);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Command failed: %s", info.jsonString().c_str());
                }
                catch( const DBException &e)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Command failed: %s", e.what());
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Command failed: %s", e.what());
            }
            return FALSE;
        }
    }
    else
    {
        bRet = ListLayers(m_osDatabase);
    }

    return bRet;
}
        
/************************************************************************/
/*                             ListLayers()                             */
/************************************************************************/

int OGRMongoDBDataSource::ListLayers(const char* pszDatabase)
{
    try
    {
        std::list<std::string> l = m_poConn->getCollectionNames( pszDatabase );
        for ( std::list<std::string>::iterator i = l.begin(); i != l.end(); i++ )
        {
            const std::string& m_osCollection(*i);
            if( strncmp(m_osCollection.c_str(), "system.", strlen("system.")) != 0 &&
                m_osCollection != "startup_log" &&
                m_osCollection != "_ogr_metadata" )
            {
                m_apoLayers.push_back(new OGRMongoDBLayer(this,
                                                      pszDatabase,
                                                      m_osCollection.c_str()));
            }
        }
        return TRUE;
    }
    catch( const DBException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Command failed: %s", e.what());
        return FALSE;
    }
}      
/************************************************************************/
/*                            ICreateLayer()                            */
/************************************************************************/

OGRLayer* OGRMongoDBDataSource::ICreateLayer( const char *pszName, 
                                              OGRSpatialReference *poSpatialRef,
                                              OGRwkbGeometryType eGType,
                                              char ** papszOptions )
{
    if( m_osDatabase.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create layer/collection when dataset opened without explicit database");
        return NULL;
    }
    
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return NULL;
    }
    
    for(int i=0; i<(int)m_apoLayers.size(); i++)
    {
        if( EQUAL(m_apoLayers[i]->GetName(), pszName) )
        {
            if( CSLFetchNameValue( papszOptions, "OVERWRITE" ) != NULL
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
                return NULL;
            }
        }
    }
    
    OGRMongoDBLayer* poLayer = new OGRMongoDBLayer(this, m_osDatabase, pszName);

    poLayer->SetFID(CSLFetchNameValueDef(papszOptions, "FID", "ogc_fid"));
    poLayer->SetCreateLayerMetadata(CSLFetchBoolean(papszOptions, "WRITE_OGR_METADATA", TRUE));
    poLayer->SetDotAsNestedField(CSLFetchBoolean(papszOptions, "DOT_AS_NESTED_FIELD", TRUE));
    poLayer->SetIgnoreSourceID(CSLFetchBoolean(papszOptions, "IGNORE_SOURCE_ID", FALSE));
    poLayer->SetCreateSpatialIndex(CSLFetchBoolean(papszOptions, "SPATIAL_INDEX", TRUE));

    if( eGType != wkbNone )
    {
        const char* pszGeometryName = CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "geometry");
        OGRGeomFieldDefn oFieldDefn(pszGeometryName, eGType);
        oFieldDefn.SetSpatialRef(poSpatialRef);
        poLayer->CreateGeomField(&oFieldDefn, FALSE);
    }

    m_apoLayers.push_back(poLayer);
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRMongoDBDataSource::DeleteLayer( int iLayer )

{
    if( eAccess != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= (int)m_apoLayers.size() )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Blow away our OGR structures related to the layer.  This is     */
/*      pretty dangerous if anything has a reference to this layer!     */
/* -------------------------------------------------------------------- */
    CPLString osLayerName = m_apoLayers[iLayer]->GetName();
    CPLString m_osDatabase = m_apoLayers[iLayer]->GetDatabase();
    CPLString m_osCollection = m_apoLayers[iLayer]->GetCollection();
    CPLString m_osQualifiedCollection = m_apoLayers[iLayer]->GetQualifiedCollection();

    CPLDebug( "MongoDB", "DeleteLayer(%s)", osLayerName.c_str() );

    delete m_apoLayers[iLayer];
    m_apoLayers.erase( m_apoLayers.begin() + iLayer );
    
    try
    {
        m_poConn->findAndRemove(
                CPLSPrintf("%s._ogr_metadata", m_osDatabase.c_str()),
                BSON("layer" << m_osCollection.c_str()), 
                BSONObj(),
                BSONObj());
        
        return m_poConn->dropCollection(m_osQualifiedCollection) ? OGRERR_NONE: OGRERR_FAILURE;
    }
    catch( const DBException &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Command failed: %s", e.what());
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMongoDBDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) 
        || EQUAL(pszCap,ODsCDeleteLayer)
        || EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return eAccess == GA_Update;
    else
        return FALSE;
}

/************************************************************************/
/*                    OGRMongoDBSingleFeatureLayer                      */
/************************************************************************/

class OGRMongoDBSingleFeatureLayer: public OGRLayer
{
    OGRFeatureDefn     *m_poFeatureDefn;
    CPLString           osVal;
    int                 iNextShapeId;
    public:
        OGRMongoDBSingleFeatureLayer( const char *pszVal );
       ~OGRMongoDBSingleFeatureLayer() { m_poFeatureDefn->Release(); }
       void             ResetReading() { iNextShapeId = 0; }
       OGRFeature      *GetNextFeature();
       OGRFeatureDefn  *GetLayerDefn() { return m_poFeatureDefn; }
       int              TestCapability( const char * ) { return FALSE; }
};

/************************************************************************/
/*                    OGRMongoDBSingleFeatureLayer()                     */
/************************************************************************/

OGRMongoDBSingleFeatureLayer::OGRMongoDBSingleFeatureLayer( const char *pszVal )
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

OGRFeature * OGRMongoDBSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetField(0, osVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer* OGRMongoDBDataSource::ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect )
{
    for(int i=0; i<(int)m_apoLayers.size(); i++ )
    {
        m_apoLayers[i]->SyncToDisk();
    }
    
/* -------------------------------------------------------------------- */
/*      Special case DELLAYER: command.                                 */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand,"DELLAYER:",9) )
    {
        const char *pszLayerName = pszSQLCommand + 9;

        while( *pszLayerName == ' ' )
            pszLayerName++;

        for( int iLayer = 0; iLayer < (int)m_apoLayers.size(); iLayer++ )
        {
            if( EQUAL(m_apoLayers[iLayer]->GetName(), 
                      pszLayerName ))
            {
                DeleteLayer( iLayer );
                break;
            }
        }
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Special case WRITE_OGR_METADATA command.                        */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand, "WRITE_OGR_METADATA ", strlen("WRITE_OGR_METADATA ")) )
    {
        if( eAccess != GA_Update )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
            return NULL;
        }
        const char* pszLayerName = pszSQLCommand + strlen("WRITE_OGR_METADATA ");
        OGRMongoDBLayer* poLayer = (OGRMongoDBLayer*) GetLayerByName(pszLayerName);
        if( poLayer == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Layer %s not found", pszLayerName);
            return NULL;
        }
        poLayer->GetLayerDefn(); // force schema discovery
        poLayer->SetCreateLayerMetadata(TRUE);
        poLayer->SyncToDisk();
        
        return NULL;
    }

    if( pszDialect != NULL && EQUAL(pszDialect, "MONGODB") )
    {
        BSONObj info;
        try
        {
            m_poConn->runCommand(m_osDatabase, OGRLocaleSafeFromJSON(pszSQLCommand), info);
            return new OGRMongoDBSingleFeatureLayer(info.jsonString().c_str());
        }
        catch( const DBException &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Command failed: %s", e.what());
            return NULL;
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

void OGRMongoDBDataSource::ReleaseResultSet( OGRLayer * poLayer )
{
    delete poLayer;
}
    
/************************************************************************/
/*                          OGRMongoDBDriverUnload()                    */
/************************************************************************/

extern "C" int GDALIsInGlobalDestructor();

static void OGRMongoDBDriverUnload( CPL_UNUSED GDALDriver* poDriver )
{
    if( bMongoInitialized != -1 && !GDALIsInGlobalDestructor() ) 
    {
        client::shutdown();
    }
}

/************************************************************************/
/*                     OGRMongoDBDriverIdentify()                       */
/************************************************************************/

static int OGRMongoDBDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return EQUALN(poOpenInfo->pszFilename, "MongoDB:", strlen("MongoDB:"));
}

/************************************************************************/
/*                     OGRMongoDBDriverOpen()                           */
/************************************************************************/

static GDALDataset* OGRMongoDBDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRMongoDBDriverIdentify(poOpenInfo) )
        return NULL;

    OGRMongoDBDataSource *m_poDS = new OGRMongoDBDataSource();

    if( !m_poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->eAccess,
                     poOpenInfo->papszOpenOptions ) )
    {
        delete m_poDS;
        m_poDS = NULL;
    }

    return m_poDS;
}

#if 0
/************************************************************************/
/*                     OGRMongoDBDriverCreate()                         */
/************************************************************************/

static GDALDataset* OGRMongoDBDriverCreate( const char * pszName,
                                            int nXSize, int nYSize, int nBands,
                                            GDALDataType eType, char ** papszOptions )
{
    OGRMongoDBDataSource *m_poDS = new OGRMongoDBDataSource();

    if( !m_poDS->Open( pszName, papszOptions) )
    {
        delete m_poDS;
        m_poDS = NULL;
    }

    return m_poDS;
}
#endif

/************************************************************************/
/*                         RegisterOGRMongoDB()                         */
/************************************************************************/

void RegisterOGRMongoDB()
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "MongoDB" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "MongoDB" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "MongoDB" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_mongodb.html" );

#if 0
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"</CreationOptionList>");
#endif

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
"  <Option name='AUTH_DBNAME' type='string' description='Authentication database name' />"
"  <Option name='USER' type='string' description='User name' />"
"  <Option name='PASSWORD' type='string' description='User password' />"
"  <Option name='AUTH_JSON' type='string' description='Authentication elements as JSon object' />"
"  <Option name='SSL_PEM_KEY_FILE' type='string' description='SSL PEM certificate/key filename' />"
"  <Option name='SSL_PEM_KEY_PASSWORD' type='string' description='SSL PEM key password' />"
"  <Option name='SSL_CA_FILE' type='string' description='SSL Certification Authority filename' />"
"  <Option name='SSL_CRL_FILE' type='string' description='SSL Certification Revocation List filename' />"
"  <Option name='SSL_ALLOW_INVALID_CERTIFICATES' type='boolean' description='Whether to allow connections to servers with invalid certificates' default='NO'/>"
"  <Option name='SSL_ALLOW_INVALID_HOSTNAMES' type='boolean' description='Whether to allow connections to servers with non-matching hostnames' default='NO'/>"
"  <Option name='FIPS_MODE' type='boolean' description='Whether to activate FIPS 140-2 mode at startup' default='NO'/>"
"  <Option name='BATCH_SIZE' type='integer' description='Number of features to retrieve per batch'/>"
"  <Option name='FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN' type='integer' description='Number of features to retrieve to establish feature definition. -1 = unlimited' default='100'/>"
"  <Option name='JSON_FIELD' type='boolean' description='Whether to include a field with the full document as JSON' default='NO'/>"
"  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' description='Whether to recursively explore nested objects and produce flatten OGR attributes' default='YES'/>"
"  <Option name='FID' type='string' description='Field name, with integer values, to use as FID' default='ogc_fid'/>"
"  <Option name='USE_OGR_METADATA' type='boolean' description='Whether to use the _ogr_metadata collection to read layer metadata' default='YES'/>"
"  <Option name='BULK_INSERT' type='boolean' description='Whether to use bulk insert for feature creation' default='YES'/>"
"</OpenOptionList>");

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time IntegerList Integer64List RealList StringList Binary" );

        poDriver->pfnOpen = OGRMongoDBDriverOpen;
        poDriver->pfnIdentify = OGRMongoDBDriverIdentify;
        poDriver->pfnUnloadDriver = OGRMongoDBDriverUnload;
        //poDriver->pfnCreate = OGRMongoDBDriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
