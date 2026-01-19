/******************************************************************************
 *
 * Project:  Interlis 1/2 Translator
 * Purpose:  IlisMeta model reader.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2014, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

// IlisMeta model: http://www.interlis.ch/models/core/IlisMeta07-20111222.ili

#include "cpl_minixml.h"
#include "imdreader.h"

#include <set>
#include <vector>
#include <algorithm>

typedef std::map<CPLString, CPLXMLNode *> StrNodeMap;
typedef std::vector<CPLXMLNode *> NodeVector;
typedef std::map<const CPLXMLNode *, int> NodeCountMap;
class IliClass;
// All classes with XML node for lookup.
typedef std::map<const CPLXMLNode *, IliClass *> ClassesMap;

/* Helper class for collection class infos */
class IliClass
{
  public:
    CPLXMLNode *node;
    int iliVersion;
    CPLString modelVersion;
    OGRFeatureDefn *poTableDefn;
    StrNodeMap &oTidLookup;
    ClassesMap &oClasses;
    NodeCountMap &oAxisCount;
    GeomFieldInfos poGeomFieldInfos;
    StructFieldInfos poStructFieldInfos;
    NodeVector oFields;
    bool isAssocClass;
    bool hasDerivedClasses;

    IliClass(CPLXMLNode *node_, int iliVersion_, const CPLString &modelVersion_,
             StrNodeMap &oTidLookup_, ClassesMap &oClasses_,
             NodeCountMap &oAxisCount_)
        : node(node_), iliVersion(iliVersion_), modelVersion(modelVersion_),
          oTidLookup(oTidLookup_), oClasses(oClasses_), oAxisCount(oAxisCount_),
          poGeomFieldInfos(), poStructFieldInfos(), oFields(),
          isAssocClass(false), hasDerivedClasses(false)
    {
        char *layerName = LayerName();
        poTableDefn = new OGRFeatureDefn(layerName);
        poTableDefn->Reference();
        CPLFree(layerName);
    }

    ~IliClass()
    {
        poTableDefn->Release();
    }

    const char *GetName() const
    {
        return poTableDefn->GetName();
    }

    const char *GetIliName()
    {
        return CPLGetXMLValue(node, "TID",
                              CPLGetXMLValue(node, "ili:tid", nullptr));
    }

    char *LayerName()
    {
        const char *psClassTID = GetIliName();
        if (iliVersion == 1)
        {
            // Skip topic and replace . with __
            char **papszTokens =
                CSLTokenizeString2(psClassTID, ".", CSLT_ALLOWEMPTYTOKENS);

            CPLString layername;
            for (int i = 1; papszTokens != nullptr && papszTokens[i] != nullptr;
                 i++)
            {
                if (i > 1)
                    layername += "__";
                layername += papszTokens[i];
            }
            CSLDestroy(papszTokens);
            return CPLStrdup(layername);
        }
        else if (EQUAL(modelVersion, "2.4"))
        {
            // Remove namespace
            const CPLStringList aosTokens(
                CSLTokenizeString2(psClassTID, ".", 0));
            return CPLStrdup(aosTokens[aosTokens.size() - 1]);
        }
        else
        {
            return CPLStrdup(psClassTID);
        }
    }

    void AddFieldNode(CPLXMLNode *nodeIn, int iOrderPos)
    {
        if (iOrderPos < 0 || iOrderPos > 100000)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid order pos = %d",
                     iOrderPos);
            return;
        }
        if (iOrderPos >= (int)oFields.size())
            oFields.resize(iOrderPos + 1);
#ifdef DEBUG_VERBOSE
        CPLDebug("OGR_ILI", "Register field with OrderPos %d to Class %s",
                 iOrderPos, GetName());
#endif
        oFields[iOrderPos] = nodeIn;
    }

    void AddRoleNode(CPLXMLNode *nodeIn, int iOrderPos)
    {
        isAssocClass = true;
        AddFieldNode(nodeIn, iOrderPos);
    }

    bool isEmbedded()
    {
        if (isAssocClass)
            for (NodeVector::const_iterator it = oFields.begin();
                 it != oFields.end(); ++it)
            {
                if (*it == nullptr)
                    continue;
                if (CPLTestBool(CPLGetXMLValue(
                        *it, "EmbeddedTransfer",
                        CPLGetXMLValue(*it, "IlisMeta16:EmbeddedTransfer",
                                       "FALSE"))))
                    return true;
            }
        return false;
    }

    // Add additional Geometry table for Interlis 1
    void AddGeomTable(const CPLString &layerName, const char *psFieldName,
                      OGRwkbGeometryType eType, bool bRefTIDField = false)
    {
        OGRFeatureDefn *poGeomTableDefn = new OGRFeatureDefn(layerName);
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
        CPLDebug("OGR_ILI", "Adding geometry table %s for field %s",
                 poGeomTableDefn->GetName(), psFieldName);
        poGeomFieldInfos[psFieldName].SetGeomTableDefn(poGeomTableDefn);
    }

    void AddField(const char *psName, OGRFieldType fieldType) const
    {
        OGRFieldDefn fieldDef(psName, fieldType);
        poTableDefn->AddFieldDefn(&fieldDef);
        CPLDebug("OGR_ILI", "Adding field '%s' to Class %s", psName, GetName());
    }

    void AddGeomField(const char *psName, OGRwkbGeometryType geomType) const
    {
        OGRGeomFieldDefn fieldDef(psName, geomType);
        // oGFld.SetSpatialRef(geomlayer->GetSpatialRef());
        poTableDefn->AddGeomFieldDefn(&fieldDef);
        CPLDebug("OGR_ILI", "Adding geometry field '%s' to Class %s", psName,
                 GetName());
    }

    void AddCoord(const char *psName, const CPLXMLNode *psTypeNode) const
    {
        auto oIter = oAxisCount.find(psTypeNode);
        int dim = (oIter == oAxisCount.end()) ? 0 : oIter->second;
        if (dim == 0)
            dim = 2;  // Area center points have no Axis spec
        if (iliVersion == 1)
        {
            for (int i = 0; i < dim; i++)
            {
                AddField(CPLSPrintf("%s_%d", psName, i), OFTReal);
            }
        }
        OGRwkbGeometryType geomType = (dim > 2) ? wkbPoint25D : wkbPoint;
        AddGeomField(psName, geomType);
    }

    OGRFieldType GetFormattedType(CPLXMLNode *nodeIn)
    {
        const char *psRefSuper = CPLGetXMLValue(
            nodeIn, "Super.REF",
            CPLGetXMLValue(nodeIn, "IlisMeta16:Super.ili:ref", nullptr));
        if (psRefSuper)
            return GetFormattedType(oTidLookup[psRefSuper]);

        return OFTString;  // TODO: Time, Date, etc. if possible
    }

    void InitFieldDefinitions()
    {
        // Delete default geometry field
        poTableDefn->DeleteGeomFieldDefn(0);

        const char *psKind = CPLGetXMLValue(
            node, "Kind", CPLGetXMLValue(node, "IlisMeta16:Kind", ""));
#ifdef DEBUG_VERBOSE
        CPLDebug("OGR_ILI", "InitFieldDefinitions of '%s' kind: %s", GetName(),
                 psKind);
#endif
        if (EQUAL(psKind, "Structure"))
        {
            // add foreign_key field
            OGRFieldDefn ofieldDefn1("REF_NAME", OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn1);
            OGRFieldDefn ofieldDefn2("REF_ID", OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn2);
        }
        else
        {  // Class
            // add TID field
            const char *psTidColName = (iliVersion == 1) ? "_TID" : "TID";
            OGRFieldDefn ofieldDefn(psTidColName, OFTString);
            poTableDefn->AddFieldDefn(&ofieldDefn);
        }
        if (CPLTestBool(CPLGetXMLValue(node, "Abstract", "FALSE")))
            hasDerivedClasses = true;
    }

    const CPLXMLNode *TidLookup(const char *pszKey) const
    {
        if (pszKey == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null key passed to TidLookup");
            return nullptr;
        }
        auto oIter = oTidLookup.find(pszKey);
        if (oIter == oTidLookup.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown key %s passed to TidLookup", pszKey);
            return nullptr;
        }
        return oIter->second;
    }

    void AddFieldDefinitions(NodeVector oArcLineTypes)
    {
        for (NodeVector::const_iterator it = oFields.begin();
             it != oFields.end(); ++it)
        {
            if (*it == nullptr)
                continue;
            const char *psName = CPLGetXMLValue(
                *it, "Name", CPLGetXMLValue(*it, "IlisMeta16:Name", nullptr));
            if (psName == nullptr)
                continue;
            const char *psTypeRef = CPLGetXMLValue(
                *it, "Type.REF",
                CPLGetXMLValue(*it, "IlisMeta16:Type.ili:ref", nullptr));
            if (psTypeRef == nullptr)         // Assoc Role
                AddField(psName, OFTString);  // TODO: numeric?
            else
            {
                const CPLXMLNode *psElementNode = TidLookup(psTypeRef);
                if (psElementNode == nullptr)
                    continue;
                const char *typeName = psElementNode->pszValue;
                CPLDebug("OGR_ILI", "AddFieldDefinitions typename '%s'",
                         typeName);
                if (EQUAL(typeName, "IlisMeta07.ModelData.TextType") ||
                    EQUAL(typeName, "IlisMeta16:TextType"))
                {  // Kind Text,MText
                    AddField(psName, OFTString);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.EnumType") ||
                         EQUAL(typeName, "IlisMeta16:EnumType"))
                {
                    AddField(psName,
                             (iliVersion == 1) ? OFTInteger : OFTString);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.BooleanType") ||
                         EQUAL(typeName, "IlisMeta16:BooleanType"))
                {
                    AddField(psName, OFTString);  //??
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.NumType") ||
                         EQUAL(typeName, "IlisMeta16:NumType"))
                {  //// Unit INTERLIS.ANYUNIT, INTERLIS.TIME, INTERLIS.h,
                    /// INTERLIS.min, INTERLIS.s, INTERLIS.M, INTERLIS.d
                    AddField(psName, OFTReal);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.BlackboxType") ||
                         EQUAL(typeName, "IlisMeta16:BlackboxType"))
                {
                    AddField(psName, OFTString);
                }
                else if (EQUAL(typeName,
                               "IlisMeta07.ModelData.FormattedType") ||
                         EQUAL(typeName, "IlisMeta16:FormattedType"))
                {
                    AddField(psName, GetFormattedType(*it));
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.MultiValue") ||
                         EQUAL(typeName, "IlisMeta16:MultiValue"))
                {
                    // min -> Multiplicity/IlisMeta07.ModelData.Multiplicity/Min
                    // max -> Multiplicity/IlisMeta07.ModelData.Multiplicity/Max
                    const char *psClassRef = CPLGetXMLValue(
                        psElementNode, "BaseType.REF",
                        CPLGetXMLValue(psElementNode,
                                       "IlisMeta16:BaseType.ili:ref", nullptr));
                    if (psClassRef)
                    {
                        IliClass *psParentClass =
                            oClasses[oTidLookup[psClassRef]];
                        poStructFieldInfos[psName] = psParentClass->GetName();
                        CPLDebug("OGR_ILI",
                                 "Register table %s for struct field '%s'",
                                 poStructFieldInfos[psName].c_str(), psName);
                        /* Option: Embed fields if max == 1
                        CPLDebug( "OGR_ILI", "Adding embedded struct members of
                        MultiValue field '%s' from Class %s", psName,
                        psClassRef);
                        AddFieldDefinitions(psParentClass->oFields);
                        */
                    }
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.CoordType") ||
                         EQUAL(typeName, "IlisMeta16:CoordType"))
                {
                    AddCoord(psName, psElementNode);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.LineType") ||
                         EQUAL(typeName, "IlisMeta16:LineType"))
                {
                    const char *psKind = CPLGetXMLValue(
                        psElementNode, "Kind",
                        CPLGetXMLValue(psElementNode, "IlisMeta16:Kind", ""));
                    poGeomFieldInfos[psName].iliGeomType = psKind;
                    bool isLinearType =
                        (std::find(oArcLineTypes.begin(), oArcLineTypes.end(),
                                   psElementNode) == oArcLineTypes.end());
                    bool linearGeom =
                        isLinearType || CPLTestBool(CPLGetConfigOption(
                                            "OGR_STROKE_CURVE", "FALSE"));
                    OGRwkbGeometryType multiLineType =
                        linearGeom ? wkbMultiLineString : wkbMultiCurve;
                    OGRwkbGeometryType polyType =
                        linearGeom ? wkbPolygon : wkbCurvePolygon;
                    if (iliVersion == 1)
                    {
                        if (EQUAL(psKind, "Area"))
                        {
                            CPLString lineLayerName =
                                GetName() + CPLString("_") + psName;
                            AddGeomTable(lineLayerName, psName, multiLineType);

                            // Add geometry field for polygonized areas
                            AddGeomField(psName, wkbPolygon);

                            // We add the area helper point geometry after
                            // polygon for better behavior of clients with
                            // limited multi geometry support
                            CPLString areaPointGeomName =
                                psName + CPLString("__Point");
                            AddCoord(areaPointGeomName, psElementNode);
                        }
                        else if (EQUAL(psKind, "Surface"))
                        {
                            CPLString geomLayerName =
                                GetName() + CPLString("_") + psName;
                            AddGeomTable(geomLayerName, psName, multiLineType,
                                         true);
                            AddGeomField(psName, polyType);
                        }
                        else
                        {  // Polyline, DirectedPolyline
                            AddGeomField(psName, multiLineType);
                        }
                    }
                    else
                    {
                        if (EQUAL(psKind, "Area") || EQUAL(psKind, "Surface"))
                        {
                            AddGeomField(psName, polyType);
                        }
                        else
                        {  // Polyline, DirectedPolyline
                            AddGeomField(psName, multiLineType);
                        }
                    }
                }
                else
                {
                    // ClassRefType
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Field '%s' of class %s has unsupported type %s",
                             psName, GetName(), typeName);
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

ImdReader::ImdReader(int iliVersionIn)
    : iliVersion(iliVersionIn), mainTopicName("OGR"), codeBlank('_'),
      codeUndefined('@'), codeContinue('\\')
{
}

ImdReader::~ImdReader()
{
}

void ImdReader::ReadModel(const char *pszFilename)
{
    CPLDebug("OGR_ILI", "Reading model '%s'", pszFilename);

    CPLXMLNode *psRootNode = CPLParseXMLFile(pszFilename);
    if (psRootNode == nullptr)
        return;
    CPLXMLNode *psSectionNode =
        CPLGetXMLNode(psRootNode, "=TRANSFER.DATASECTION");
    if (psSectionNode == nullptr)
        psSectionNode =
            CPLGetXMLNode(psRootNode, "=ili:transfer.ili:datasection");
    if (psSectionNode == nullptr)
    {
        CPLDestroyXMLNode(psRootNode);
        return;
    }

    StrNodeMap oTidLookup; /* for fast lookup of REF relations */
    ClassesMap oClasses;
    NodeCountMap oAxisCount;
    NodeVector oArcLineTypes;

    const auto TidLookup = [&oTidLookup](const char *pszKey)
    {
        if (pszKey == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Null key passed to TidLookup");
            return static_cast<CPLXMLNode *>(nullptr);
        }
        auto oIter = oTidLookup.find(pszKey);
        if (oIter == oTidLookup.end())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown key %s passed to TidLookup", pszKey);
            return static_cast<CPLXMLNode *>(nullptr);
        }
        return static_cast<CPLXMLNode *>(oIter->second);
    };

    /* Fill TID lookup map and IliClasses lookup map */
    CPLXMLNode *psModel = psSectionNode->psChild;
    while (psModel != nullptr)
    {
#ifdef DEBUG_VERBOSE
        const char *modelName = CPLGetXMLValue(
            psModel, "BID", CPLGetXMLValue(psModel, "ili:bid", nullptr));
        CPLDebug("OGR_ILI", "Model: '%s'", modelName);
#endif

        for (CPLXMLNode *psEntry = psModel->psChild; psEntry != nullptr;
             psEntry = psEntry->psNext)
        {
            if (psEntry->eType != CXT_Attribute)  // ignore BID
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
#endif
                const char *psTID =
                    CPLGetXMLValue(psEntry, "TID",
                                   CPLGetXMLValue(psEntry, "ili:tid", nullptr));
                if (psTID != nullptr)
                    oTidLookup[psTID] = psEntry;

                if (EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Model") ||
                    EQUAL(psEntry->pszValue, "IlisMeta16:Model"))
                {
                    IliModelInfo modelInfo;
                    modelInfo.name = CPLGetXMLValue(
                        psEntry, "Name",
                        CPLGetXMLValue(psEntry, "IlisMeta16:Name", "OGR"));
                    modelInfo.version = CPLGetXMLValue(
                        psEntry, "Version",
                        CPLGetXMLValue(psEntry, "IlisMeta16:iliVersion", ""));
                    // "1", "2.3", "2.4"
                    modelInfo.uri = CPLGetXMLValue(psEntry, "At", "");
                    modelInfos.push_back(std::move(modelInfo));

                    CPLXMLNode *psFormatNode =
                        CPLGetXMLNode(psEntry, "ili1Format");
                    if (psFormatNode != nullptr)
                    {
                        psFormatNode = psFormatNode->psChild;
                        codeBlank = static_cast<char>(atoi(
                            CPLGetXMLValue(psFormatNode, "blankCode", "95")));
                        codeUndefined = static_cast<char>(atoi(CPLGetXMLValue(
                            psFormatNode, "undefinedCode", "64")));
                        codeContinue = static_cast<char>(atoi(CPLGetXMLValue(
                            psFormatNode, "continueCode", "92")));
                    }
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.SubModel") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:SubModel"))
                {
                    mainBasketName = CPLGetXMLValue(
                        psEntry, "TID",
                        CPLGetXMLValue(psEntry, "ili:tid", "OGR"));
                    mainTopicName = CPLGetXMLValue(
                        psEntry, "Name",
                        CPLGetXMLValue(psEntry, "IlisMeta16:Name", "OGR"));
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.Class") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:Class"))
                {
                    CPLDebug("OGR_ILI", "Class name: '%s'", psTID);
                    const auto &modelVersion = modelInfos.back().version;
                    oClasses[psEntry] =
                        new IliClass(psEntry, iliVersion, modelVersion,
                                     oTidLookup, oClasses, oAxisCount);
                }
            }
        }

        // 2nd pass: add fields via TransferElement entries & role associations
        for (CPLXMLNode *psEntry = psModel->psChild; psEntry != nullptr;
             psEntry = psEntry->psNext)
        {
            if (psEntry->eType != CXT_Attribute)  // ignore BID
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
#endif
                if (iliVersion == 1 &&
                    EQUAL(psEntry->pszValue,
                          "IlisMeta07.ModelData.Ili1TransferElement"))
                {
                    const char *psClassRef = CPLGetXMLValue(
                        psEntry, "Ili1TransferClass.REF", nullptr);
                    const char *psElementRef =
                        CPLGetXMLValue(psEntry, "Ili1RefAttr.REF", nullptr);
                    if (psClassRef == nullptr || psElementRef == nullptr)
                        continue;
                    int iOrderPos =
                        atoi(CPLGetXMLValue(psEntry, "Ili1RefAttr.ORDER_POS",
                                            "0")) -
                        1;
                    auto tidClassRef = TidLookup(psClassRef);
                    if (tidClassRef == nullptr)
                        continue;
                    auto classesIter = oClasses.find(tidClassRef);
                    if (classesIter == oClasses.end())
                        continue;
                    IliClass *psParentClass = classesIter->second;
                    CPLXMLNode *psElementNode = TidLookup(psElementRef);
                    if (psElementNode == nullptr)
                        continue;
                    psParentClass->AddFieldNode(psElementNode, iOrderPos);
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.TransferElement") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:TransferElement"))
                {
                    const char *psClassRef = CPLGetXMLValue(
                        psEntry, "TransferClass.REF",
                        CPLGetXMLValue(psEntry,
                                       "IlisMeta16:TransferClass.ili:ref",
                                       nullptr));
                    const char *psElementRef = CPLGetXMLValue(
                        psEntry, "TransferElement.REF",
                        CPLGetXMLValue(psEntry,
                                       "IlisMeta16:TransferElement.ili:ref",
                                       nullptr));
                    if (psClassRef == nullptr || psElementRef == nullptr)
                        continue;
                    int iOrderPos =
                        atoi(CPLGetXMLValue(
                            psEntry, "TransferElement.ORDER_POS",
                            CPLGetXMLValue(
                                psEntry,
                                "IlisMeta16:TransferElement.ili:order_pos",
                                "0"))) -
                        1;
                    auto tidClassRef = TidLookup(psClassRef);
                    if (tidClassRef == nullptr)
                        continue;
                    auto classesIter = oClasses.find(tidClassRef);
                    if (classesIter == oClasses.end())
                        continue;
                    IliClass *psParentClass = classesIter->second;
                    CPLXMLNode *psElementNode = TidLookup(psElementRef);
                    if (psElementNode == nullptr)
                        continue;
                    psParentClass->AddFieldNode(psElementNode, iOrderPos);
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.Role") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:Role"))
                {
                    const char *psRefParent = CPLGetXMLValue(
                        psEntry, "Association.REF",
                        CPLGetXMLValue(psEntry,
                                       "IlisMeta16:Association.ili:ref",
                                       nullptr));
                    if (psRefParent == nullptr)
                        continue;
                    int iOrderPos =
                        atoi(CPLGetXMLValue(
                            psEntry, "Association.ORDER_POS",
                            CPLGetXMLValue(
                                psEntry, "IlisMeta16:Association.ili:order_pos",
                                "0"))) -
                        1;
                    auto tidClassRef = TidLookup(psRefParent);
                    if (tidClassRef == nullptr)
                        continue;
                    auto classesIter = oClasses.find(tidClassRef);
                    if (classesIter == oClasses.end())
                        continue;
                    IliClass *psParentClass = classesIter->second;
                    if (psParentClass)
                        psParentClass->AddRoleNode(psEntry, iOrderPos);
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.AxisSpec") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:AxisSpec"))
                {
                    const char *psClassRef = CPLGetXMLValue(
                        psEntry, "CoordType.REF",
                        CPLGetXMLValue(psEntry, "IlisMeta16:CoordType.ili:ref",
                                       nullptr));
                    if (psClassRef == nullptr)
                        continue;
                    // int iOrderPos = atoi(
                    //     CPLGetXMLValue( psEntry, "Axis.ORDER_POS", "0" ))-1;
                    CPLXMLNode *psCoordTypeNode = TidLookup(psClassRef);
                    if (psCoordTypeNode == nullptr)
                        continue;
                    oAxisCount[psCoordTypeNode] += 1;
                }
                else if (EQUAL(psEntry->pszValue,
                               "IlisMeta07.ModelData.LinesForm") ||
                         EQUAL(psEntry->pszValue, "IlisMeta16:LinesForm"))
                {
                    const char *psLineForm = CPLGetXMLValue(
                        psEntry, "LineForm.REF",
                        CPLGetXMLValue(psEntry, "IlisMeta16:LineForm.ili:ref",
                                       nullptr));
                    if (psLineForm != nullptr &&
                        EQUAL(psLineForm, "INTERLIS.ARCS"))
                    {
                        const char *psElementRef = CPLGetXMLValue(
                            psEntry, "LineType.REF",
                            CPLGetXMLValue(psEntry,
                                           "IlisMeta16:LineType.ili:ref",
                                           nullptr));
                        CPLXMLNode *psElementNode = TidLookup(psElementRef);
                        if (psElementNode == nullptr)
                            continue;
                        oArcLineTypes.push_back(psElementNode);
                    }
                }
            }
        }

        psModel = psModel->psNext;
    }

    // Last model is main model
    const CPLString mainModelName = modelInfos.back().name;
    const CPLString modelVersion = modelInfos.back().version;
    CPLDebug("OGR_ILI", "mainModelName: '%s' version: '%s'",
             mainModelName.c_str(), modelVersion.c_str());

    /* Analyze class inheritance & add fields to class table defn */
    for (ClassesMap::const_iterator it = oClasses.begin(); it != oClasses.end();
         ++it)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("OGR_ILI", "Class: '%s'", it->second->GetName());
#endif
        const char *psRefSuper = CPLGetXMLValue(
            it->first, "Super.REF",
            CPLGetXMLValue(it->first, "IlisMeta16:Super.ili:ref", nullptr));
        if (psRefSuper)
        {
            if (oTidLookup.find(psRefSuper) != oTidLookup.end() &&
                oClasses.find(oTidLookup[psRefSuper]) != oClasses.end())
            {
                oClasses[oTidLookup[psRefSuper]]->hasDerivedClasses = true;
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Couldn't reference super class '%s'", psRefSuper);
            }
        }
        it->second->InitFieldDefinitions();
        it->second->AddFieldDefinitions(oArcLineTypes);
    }

    /* Filter relevant classes */
    for (ClassesMap::const_iterator it = oClasses.begin(); it != oClasses.end();
         ++it)
    {
        const char *className = it->second->GetIliName();
        FeatureDefnInfo oClassInfo = it->second->tableDefs();
        bool include = EQUAL(modelVersion, "2.4")
                           ? STARTS_WITH(className, mainModelName.c_str())
                           : !STARTS_WITH_CI(className, "INTERLIS.");
        if (include && oClassInfo.GetTableDefnRef())
        {
            featureDefnInfos.push_back(oClassInfo);
        }
    }

    for (ClassesMap::iterator it = oClasses.begin(); it != oClasses.end(); ++it)
    {
        delete it->second;
    }

    CPLDestroyXMLNode(psRootNode);
}

FeatureDefnInfo ImdReader::GetFeatureDefnInfo(const char *pszLayerName)
{
    FeatureDefnInfo featureDefnInfo;
    for (FeatureDefnInfos::const_iterator it = featureDefnInfos.begin();
         it != featureDefnInfos.end(); ++it)
    {
        OGRFeatureDefn *fdefn = it->GetTableDefnRef();
        if (EQUAL(fdefn->GetName(), pszLayerName))
            featureDefnInfo = *it;
    }
    return featureDefnInfo;
}
