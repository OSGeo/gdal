/******************************************************************************
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  IlisMeta model reader.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2014, Pirmin Kalberer, Sourcepole AG
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

// IlisMeta model: http://www.interlis.ch/models/core/IlisMeta07-20111222.ili

#include "cpl_minixml.h"
#include "imdreader.h"

#include <set>
#include <vector>
#include <algorithm>

CPL_CVSID("$Id$")

typedef std::map<CPLString,CPLXMLNode*> StrNodeMap;
typedef std::vector<CPLXMLNode*> NodeVector;
typedef std::map<CPLXMLNode*,int> NodeCountMap;
class IliClass;
// All classes with XML node for lookup.
typedef std::map<CPLXMLNode*,IliClass*> ClassesMap;

/* Helper class for collection class infos */
class IliClass
{
public:
    CPLXMLNode* node;
    int iliVersion;
    OGRFeatureDefn* poTableDefn;
    StrNodeMap& oTidLookup;
    ClassesMap& oClasses;
    NodeCountMap& oAxisCount;
    GeomFieldInfos poGeomFieldInfos;
    StructFieldInfos poStructFieldInfos;
    NodeVector oFields;
    bool isAssocClass;
    bool hasDerivedClasses;

    IliClass( CPLXMLNode* node_, int iliVersion_, StrNodeMap& oTidLookup_,
              ClassesMap& oClasses_, NodeCountMap& oAxisCount_) :
        node(node_), iliVersion(iliVersion_), oTidLookup(oTidLookup_),
        oClasses(oClasses_), oAxisCount(oAxisCount_),
        poGeomFieldInfos(), poStructFieldInfos(), oFields(),
        isAssocClass(false), hasDerivedClasses(false)
    {
        char* layerName = LayerName();
        poTableDefn = new OGRFeatureDefn(layerName);
        poTableDefn->Reference();
        CPLFree(layerName);
    }

    ~IliClass()
    {
        poTableDefn->Release();
    }

    const char* GetName() {
        return poTableDefn->GetName();
    }
    const char* GetIliName() {
        return CPLGetXMLValue( node, "TID", nullptr );
    }
    char* LayerName() {
        const char* psClassTID = GetIliName();
        if (iliVersion == 1)
        {
            //Skip topic and replace . with __
            char **papszTokens =
                CSLTokenizeString2( psClassTID, ".", CSLT_ALLOWEMPTYTOKENS );

            CPLString layername;
            for(int i = 1; papszTokens != nullptr && papszTokens[i] != nullptr; i++)
            {
                if (i>1) layername += "__";
                layername += papszTokens[i];
            }
            CSLDestroy( papszTokens );
            return CPLStrdup(layername);
        }

        return CPLStrdup(psClassTID);
    }
    void AddFieldNode(CPLXMLNode* nodeIn, int iOrderPos)
    {
        if (iOrderPos >= (int)oFields.size())
            oFields.resize(iOrderPos+1);
#ifdef DEBUG_VERBOSE
        CPLDebug( "OGR_ILI", "Register field with OrderPos %d to Class %s",
                  iOrderPos, GetName() );
#endif
        oFields[iOrderPos] = nodeIn;
    }
    void AddRoleNode(CPLXMLNode* nodeIn, int iOrderPos)
    {
        isAssocClass = true;
        AddFieldNode(nodeIn, iOrderPos);
    }
    bool isEmbedded()
    {
        if (isAssocClass)
            for( NodeVector::const_iterator it = oFields.begin();
                 it != oFields.end();
                 ++it )
            {
                if (*it == nullptr) continue;
                if( CPLTestBool(
                        CPLGetXMLValue( *it, "EmbeddedTransfer", "FALSE" ) ) )
                    return true;
            }
        return false;
    }
    // Add additional Geometry table for Interlis 1
    void AddGeomTable( CPLString layerName, const char* psFieldName,
                       OGRwkbGeometryType eType, bool bRefTIDField = false )
    {
        OGRFeatureDefn* poGeomTableDefn = new OGRFeatureDefn(layerName);
        OGRFieldDefn fieldDef("_TID", OFTString);
        poGeomTableDefn->AddFieldDefn(&fieldDef);
        if (bRefTIDField)
        {
            OGRFieldDefn fieldDefRef("_RefTID", OFTString);
            poGeomTableDefn->AddFieldDefn(&fieldDefRef);
        }
        poGeomTableDefn->DeleteGeomFieldDefn(0);
        OGRGeomFieldDefn fieldDefGeom(psFieldName, eType);
        poGeomTableDefn->AddGeomFieldDefn(&fieldDefGeom);
        CPLDebug( "OGR_ILI", "Adding geometry table %s for field %s",
                  poGeomTableDefn->GetName(), psFieldName);
        poGeomFieldInfos[psFieldName].SetGeomTableDefn(poGeomTableDefn);
    }
    void AddField(const char* psName, OGRFieldType fieldType)
    {
        OGRFieldDefn fieldDef(psName, fieldType);
        poTableDefn->AddFieldDefn(&fieldDef);
        CPLDebug( "OGR_ILI", "Adding field '%s' to Class %s", psName, GetName());
    }
    void AddGeomField(const char* psName, OGRwkbGeometryType geomType)
    {
        OGRGeomFieldDefn fieldDef(psName, geomType);
        //oGFld.SetSpatialRef(geomlayer->GetSpatialRef());
        poTableDefn->AddGeomFieldDefn(&fieldDef);
        CPLDebug( "OGR_ILI", "Adding geometry field '%s' to Class %s",
                  psName, GetName());
    }
    void AddCoord(const char* psName, CPLXMLNode* psTypeNode)
    {
        int dim = oAxisCount[psTypeNode];
        if (dim == 0) dim = 2; //Area center points have no Axis spec
        if (iliVersion == 1)
        {
            for (int i=0; i<dim; i++)
            {
                AddField(CPLSPrintf("%s_%d", psName, i), OFTReal);
            }
        }
        OGRwkbGeometryType geomType = (dim > 2) ? wkbPoint25D : wkbPoint;
        AddGeomField(psName, geomType);
    }
    OGRFieldType GetFormattedType(CPLXMLNode* nodeIn)
    {
        const char* psRefSuper = CPLGetXMLValue( nodeIn, "Super.REF", nullptr );
        if (psRefSuper)
            return GetFormattedType(oTidLookup[psRefSuper]);

        return OFTString; //TODO: Time, Date, etc. if possible
    }
    void InitFieldDefinitions()
    {
        // Delete default geometry field
        poTableDefn->DeleteGeomFieldDefn(0);

        const char* psKind = CPLGetXMLValue( node, "Kind", "" );
#ifdef DEBUG_VERBOSE
        CPLDebug( "OGR_ILI", "InitFieldDefinitions of '%s' kind: %s",
                  GetName(), psKind );
#endif
        if (EQUAL(psKind, "Structure"))
        {
            // add foreign_key field
            OGRFieldDefn ofieldDefn1("REF_NAME", OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn1);
            OGRFieldDefn ofieldDefn2("REF_ID", OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn2);
        } else { // Class
            // add TID field
            const char* psTidColName = (iliVersion == 1) ? "_TID" : "TID";
            OGRFieldDefn ofieldDefn(psTidColName, OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn);
        }
        if (CPLTestBool(CPLGetXMLValue( node, "Abstract", "FALSE" )))
            hasDerivedClasses = true;
    }
    void AddFieldDefinitions(NodeVector oArcLineTypes)
    {
        for( NodeVector::const_iterator it = oFields.begin();
             it != oFields.end();
             ++it)
        {
            if (*it == nullptr) continue;
            const char* psName = CPLGetXMLValue( *it, "Name", nullptr );
            if( psName == nullptr )
                continue;
            const char* psTypeRef = CPLGetXMLValue( *it, "Type.REF", nullptr );
            if (psTypeRef == nullptr) //Assoc Role
                AddField(psName, OFTString); //FIXME: numeric?
            else
            {
                CPLXMLNode* psElementNode = oTidLookup[psTypeRef];
                const char* typeName = psElementNode->pszValue;
                if (EQUAL(typeName, "IlisMeta07.ModelData.TextType"))
                { //Kind Text,MText
                    AddField(psName, OFTString);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.EnumType"))
                {
                    AddField(psName, (iliVersion == 1) ? OFTInteger : OFTString);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.BooleanType"))
                {
                    AddField(psName, OFTString); //??
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.NumType"))
                { //// Unit INTERLIS.ANYUNIT, INTERLIS.TIME, INTERLIS.h, INTERLIS.min, INTERLIS.s, INTERLIS.M, INTERLIS.d
                    AddField(psName, OFTReal);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.BlackboxType"))
                {
                    AddField(psName, OFTString);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.FormattedType"))
                {
                    AddField(psName, GetFormattedType(*it));
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.MultiValue"))
                {
                    //min -> Multiplicity/IlisMeta07.ModelData.Multiplicity/Min
                    //max -> Multiplicity/IlisMeta07.ModelData.Multiplicity/Max
                    const char* psClassRef = CPLGetXMLValue( psElementNode, "BaseType.REF", nullptr );
                    if (psClassRef)
                    {
                        IliClass* psParentClass = oClasses[oTidLookup[psClassRef]];
                        poStructFieldInfos[psName] = psParentClass->GetName();
                        CPLDebug( "OGR_ILI", "Register table %s for struct field '%s'", poStructFieldInfos[psName].c_str(), psName);
                        /* Option: Embed fields if max == 1
                        CPLDebug( "OGR_ILI", "Adding embedded struct members of MultiValue field '%s' from Class %s", psName, psClassRef);
                        AddFieldDefinitions(psParentClass->oFields);
                        */
                    }
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.CoordType"))
                {
                    AddCoord(psName, psElementNode);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.LineType"))
                {
                    const char* psKind = CPLGetXMLValue( psElementNode, "Kind", "" );
                    poGeomFieldInfos[psName].iliGeomType = psKind;
                    bool isLinearType = (std::find(oArcLineTypes.begin(), oArcLineTypes.end(), psElementNode) == oArcLineTypes.end());
                    bool linearGeom = isLinearType || CPLTestBool(CPLGetConfigOption("OGR_STROKE_CURVE", "FALSE"));
                    OGRwkbGeometryType multiLineType = linearGeom ? wkbMultiLineString : wkbMultiCurve;
                    OGRwkbGeometryType polyType = linearGeom ? wkbPolygon : wkbCurvePolygon;
                    if (iliVersion == 1)
                    {
                        if (EQUAL(psKind, "Area"))
                        {
                            CPLString lineLayerName = GetName() + CPLString("_") + psName;
                            AddGeomTable(lineLayerName, psName, multiLineType);

                            //Add geometry field for polygonized areas
                            AddGeomField(psName, wkbPolygon);

                            //We add the area helper point geometry after polygon
                            //for better behavior of clients with limited multi geometry support
                            CPLString areaPointGeomName = psName + CPLString("__Point");
                            AddCoord(areaPointGeomName, psElementNode);
                        } else if (EQUAL(psKind, "Surface"))
                        {
                            CPLString geomLayerName = GetName() + CPLString("_") + psName;
                            AddGeomTable(geomLayerName, psName, multiLineType, true);
                            AddGeomField(psName, polyType);
                        } else { // Polyline, DirectedPolyline
                            AddGeomField(psName, multiLineType);
                        }
                    } else {
                        if (EQUAL(psKind, "Area") || EQUAL(psKind, "Surface"))
                        {
                            AddGeomField(psName, polyType);
                        } else { // Polyline, DirectedPolyline
                            AddGeomField(psName, multiLineType);
                        }
                    }
                }
                else
                {
                    //ClassRefType
                    CPLError(CE_Warning, CPLE_NotSupported,
                        "Field '%s' of class %s has unsupported type %s", psName, GetName(), typeName);
                }
            }
        }
    }
    FeatureDefnInfo tableDefs()
    {
        FeatureDefnInfo poLayerInfo;
        if (!hasDerivedClasses && !isEmbedded())
        {
            poLayerInfo.SetTableDefn(poTableDefn);
            poLayerInfo.poGeomFieldInfos = poGeomFieldInfos;
        }
        return poLayerInfo;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(IliClass)
};

ImdReader::ImdReader(int iliVersionIn) :
    iliVersion(iliVersionIn),
    modelInfos(),  // TODO(schwehr): Remove.  No need for default ctor, correct?
    mainModelName("OGR"),
    mainTopicName("OGR"),
    codeBlank('_'),
    codeUndefined('@'),
    codeContinue('\\')
{}

ImdReader::~ImdReader() {}

void ImdReader::ReadModel(const char *pszFilename) {
    CPLDebug( "OGR_ILI", "Reading model '%s'", pszFilename);

    CPLXMLNode* psRootNode = CPLParseXMLFile(pszFilename);
    if( psRootNode == nullptr )
        return;
    CPLXMLNode *psSectionNode
        = CPLGetXMLNode( psRootNode, "=TRANSFER.DATASECTION" );
    if( psSectionNode == nullptr )
    {
        CPLDestroyXMLNode(psRootNode);
        return;
    }

    StrNodeMap oTidLookup; /* for fast lookup of REF relations */
    ClassesMap oClasses;
    NodeCountMap oAxisCount;
    NodeVector oArcLineTypes;

    /* Fill TID lookup map and IliClasses lookup map */
    CPLXMLNode* psModel = psSectionNode->psChild;
    while( psModel != nullptr )
    {
        const char *modelName = CPLGetXMLValue( psModel, "BID", nullptr );
#ifdef DEBUG_VERBOSE
        CPLDebug( "OGR_ILI", "Model: '%s'", modelName);
#endif

        CPLXMLNode* psEntry = psModel->psChild;
        while( psEntry != nullptr )
                {
            if (psEntry->eType != CXT_Attribute) //ignore BID
            {
#ifdef DEBUG_VERBOSE
                CPLDebug( "OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
#endif
                const char* psTID = CPLGetXMLValue( psEntry, "TID", nullptr );
                if( psTID != nullptr )
                    oTidLookup[psTID] = psEntry;

                if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Model") &&
                    !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    IliModelInfo modelInfo;
                    modelInfo.name = CPLGetXMLValue( psEntry, "Name", "OGR" );
                    modelInfo.version = CPLGetXMLValue( psEntry, "Version", "" );
                    modelInfo.uri = CPLGetXMLValue( psEntry, "At", "" );
                    modelInfos.push_back(modelInfo);
                    mainModelName = modelInfo.name; // FIXME: check model inheritance
                    //version = CPLGetXMLValue(psEntry, "iliVersion", "0"); //1 or 2.3

                    CPLXMLNode *psFormatNode
                        = CPLGetXMLNode( psEntry, "ili1Format" );
                    if (psFormatNode != nullptr)
                    {
                        psFormatNode = psFormatNode->psChild;
                        codeBlank = static_cast<char>(
                            atoi( CPLGetXMLValue(
                                psFormatNode, "blankCode", "95")));
                        codeUndefined = static_cast<char>(
                            atoi( CPLGetXMLValue(
                                psFormatNode, "undefinedCode", "64")));
                        codeContinue = static_cast<char>(
                            atoi( CPLGetXMLValue(
                                psFormatNode, "continueCode", "92")));
                    }
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.SubModel"))
                {
                    mainBasketName = CPLGetXMLValue(psEntry, "TID", "OGR");
                    mainTopicName = CPLGetXMLValue(psEntry, "Name", "OGR");
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Class") )
                {
                    CPLDebug( "OGR_ILI", "Class name: '%s'", psTID);
                    oClasses[psEntry] = new IliClass(
                      psEntry, iliVersion, oTidLookup, oClasses, oAxisCount);
                }
            }
            psEntry = psEntry->psNext;
        }

        // 2nd pass: add fields via TransferElement entries & role associations
        psEntry = psModel->psChild;
        while( psEntry != nullptr )
        {
            if (psEntry->eType != CXT_Attribute) //ignore BID
            {
#ifdef DEBUG_VERBOSE
                CPLDebug( "OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
#endif
                if( iliVersion == 1 &&
                    EQUAL( psEntry->pszValue,
                           "IlisMeta07.ModelData.Ili1TransferElement"))
                {
                    const char* psClassRef = CPLGetXMLValue(
                      psEntry, "Ili1TransferClass.REF", nullptr );
                    const char* psElementRef
                        = CPLGetXMLValue( psEntry, "Ili1RefAttr.REF", nullptr );
                    int iOrderPos = atoi(CPLGetXMLValue(
                        psEntry, "Ili1RefAttr.ORDER_POS", "0" ))-1;
                    IliClass* psParentClass = oClasses[oTidLookup[psClassRef]];
                    CPLXMLNode* psElementNode = oTidLookup[psElementRef];
                    psParentClass->AddFieldNode(psElementNode, iOrderPos);
                }
                else if( EQUAL( psEntry->pszValue,
                                "IlisMeta07.ModelData.TransferElement"))
                {
                    const char* psClassRef
                        = CPLGetXMLValue( psEntry, "TransferClass.REF", nullptr );
                    const char* psElementRef = CPLGetXMLValue(
                        psEntry, "TransferElement.REF", nullptr );
                    int iOrderPos = atoi(CPLGetXMLValue(
                        psEntry, "TransferElement.ORDER_POS", "0" ))-1;
                    IliClass* psParentClass = oClasses[oTidLookup[psClassRef]];
                    CPLXMLNode* psElementNode = oTidLookup[psElementRef];
                    psParentClass->AddFieldNode(psElementNode, iOrderPos);
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Role"))
                {
                    const char* psRefParent
                        = CPLGetXMLValue( psEntry, "Association.REF", nullptr );
                    int iOrderPos = atoi(
                        CPLGetXMLValue( psEntry,
                                        "Association.ORDER_POS", "0" ))-1;
                    IliClass* psParentClass = oClasses[oTidLookup[psRefParent]];
                    if (psParentClass)
                        psParentClass->AddRoleNode(psEntry, iOrderPos);
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.AxisSpec"))
                {
                    const char* psClassRef
                        = CPLGetXMLValue( psEntry, "CoordType.REF", nullptr );
                    // int iOrderPos = atoi(
                    //     CPLGetXMLValue( psEntry, "Axis.ORDER_POS", "0" ))-1;
                    CPLXMLNode* psCoordTypeNode = oTidLookup[psClassRef];
                    oAxisCount[psCoordTypeNode] += 1;
                }
                else if( EQUAL( psEntry->pszValue,
                                "IlisMeta07.ModelData.LinesForm"))
                {
                    const char* psLineForm
                        = CPLGetXMLValue( psEntry, "LineForm.REF", nullptr );
                    if (EQUAL(psLineForm, "INTERLIS.ARCS")) {
                        const char* psElementRef
                            = CPLGetXMLValue( psEntry, "LineType.REF", nullptr );
                        CPLXMLNode* psElementNode = oTidLookup[psElementRef];
                        oArcLineTypes.push_back(psElementNode);
                    }
                }
            }
            psEntry = psEntry->psNext;
        }

        psModel = psModel->psNext;
    }

    /* Analyze class inheritance & add fields to class table defn */
    for( ClassesMap::const_iterator it = oClasses.begin();
         it != oClasses.end();
         ++it )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug( "OGR_ILI", "Class: '%s'", it->second->GetName());
#endif
        const char* psRefSuper = CPLGetXMLValue( it->first, "Super.REF", nullptr );
        if (psRefSuper) {
            if (oTidLookup.find(psRefSuper) != oTidLookup.end() &&
                oClasses.find(oTidLookup[psRefSuper]) != oClasses.end()) {
                oClasses[oTidLookup[psRefSuper]]->hasDerivedClasses = true;
            } else {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "Couldn't reference super class '%s'", psRefSuper);
            }
        }
        it->second->InitFieldDefinitions();
        it->second->AddFieldDefinitions(oArcLineTypes);
    }

    /* Filter relevant classes */
    for( ClassesMap::const_iterator it = oClasses.begin();
         it != oClasses.end();
         ++it)
    {
        const char* className = it->second->GetIliName();
        FeatureDefnInfo oClassInfo = it->second->tableDefs();
        if (!STARTS_WITH_CI(className, "INTERLIS.") &&
            oClassInfo.GetTableDefnRef())
            featureDefnInfos.push_back(oClassInfo);
    }

    for (ClassesMap::iterator it = oClasses.begin(); it != oClasses.end(); ++it)
    {
        delete it->second;
    }

    CPLDestroyXMLNode(psRootNode);
}

FeatureDefnInfo ImdReader::GetFeatureDefnInfo(const char *pszLayerName) {
    FeatureDefnInfo featureDefnInfo;
    for( FeatureDefnInfos::const_iterator it = featureDefnInfos.begin();
         it != featureDefnInfos.end();
         ++it )
    {
        OGRFeatureDefn* fdefn = it->GetTableDefnRef();
        if (EQUAL(fdefn->GetName(), pszLayerName)) featureDefnInfo = *it;
    }
    return featureDefnInfo;
}
