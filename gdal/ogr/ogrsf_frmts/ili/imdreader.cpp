/******************************************************************************
 * $Id$
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


#include "imdreader.h"
#include "cpl_minixml.h"
#include <map>
#include <set>
#include <vector>


CPL_CVSID("$Id$");


typedef std::map<CPLString,CPLXMLNode*> StrNodeMap;
typedef std::vector<CPLXMLNode*> NodeVector;
typedef std::map<CPLXMLNode*,int> NodeCountMap;

class IliClass
{
public:
    OGRFeatureDefn* poTableDefn;
    FeatureDefnList oAdditionalTableDefs;
    NodeVector oFields;
    bool isAssocClass;
    bool hasDerivedClasses;

    IliClass(OGRFeatureDefn* poTableDefnIn) : poTableDefn(poTableDefnIn), oFields(), isAssocClass(false), hasDerivedClasses(false)
    {
    };
    ~IliClass()
    {
        delete poTableDefn;
    };
    void AddFieldNode(CPLXMLNode* node, int iOrderPos)
    {
        if (iOrderPos >= (int)oFields.size())
            oFields.resize(iOrderPos+1);
        //CPLDebug( "OGR_ILI", "Register field with OrderPos %d to Class %s", iOrderPos, poTableDefn->GetName());
        oFields[iOrderPos] = node;
    }
    void AddRoleNode(CPLXMLNode* node, int iOrderPos)
    {
        isAssocClass = true;
        AddFieldNode(node, iOrderPos);
    }
    bool isEmbedded()
    {
        if (isAssocClass)
            for (NodeVector::const_iterator it = oFields.begin(); it != oFields.end(); ++it)
            {
                if (*it == NULL) continue;
                if (CSLTestBoolean(CPLGetXMLValue( *it, "EmbeddedTransfer", "FALSE" )))
                    return true;
            }
        return false;
    }
    // Add additional Geometry table for Interlis 1
    void AddGeomTable(CPLString layerName, const char* psFieldName, OGRwkbGeometryType eType)
    {
        OGRFeatureDefn* poGeomTableDefn = new OGRFeatureDefn(layerName);
        OGRFieldDefn fieldDef("_TID", OFTString);
        poGeomTableDefn->AddFieldDefn(&fieldDef);
        if (eType == wkbPolygon)
        {
            OGRFieldDefn fieldDefRef("_RefTID", OFTString);
            poGeomTableDefn->AddFieldDefn(&fieldDefRef);
        }
        poGeomTableDefn->DeleteGeomFieldDefn(0);
        OGRGeomFieldDefn fieldDefGeom(psFieldName, eType);
        poGeomTableDefn->AddGeomFieldDefn(&fieldDefGeom);
        CPLDebug( "OGR_ILI", "Adding geometry field %s to Class %s", psFieldName, poGeomTableDefn->GetName());
        oAdditionalTableDefs.push_back(poGeomTableDefn);
    }
    void AddField(const char* psName, OGRFieldType fieldType)
    {
        OGRFieldDefn fieldDef(psName, fieldType);
        poTableDefn->AddFieldDefn(&fieldDef);
        CPLDebug( "OGR_ILI", "Adding field '%s' to Class %s", psName, poTableDefn->GetName());                
    }
    void AddGeomField(const char* psName, OGRwkbGeometryType geomType)
    {
        OGRGeomFieldDefn fieldDef(psName, geomType);
        //oGFld.SetSpatialRef(geomlayer->GetSpatialRef());
        poTableDefn->AddGeomFieldDefn(&fieldDef);
        CPLDebug( "OGR_ILI", "Adding geometry field '%s' to Class %s", psName, poTableDefn->GetName());                
    }
    void AddCoord(int iliVersion, const char* psName, CPLXMLNode* psTypeNode, NodeCountMap& oAxisCount)
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
    void AddFieldDefinitions(int iliVersion, StrNodeMap& oTidLookup, NodeCountMap& oAxisCount)
    {
        // Delete default geometry field
        poTableDefn->DeleteGeomFieldDefn(0);
        // add TID field
        const char* psTidColName = (iliVersion == 1) ? "_TID" : "TID";
        OGRFieldDefn ofieldDefn(psTidColName, OFTString);
        poTableDefn->AddFieldDefn(&ofieldDefn);
        for (NodeVector::const_iterator it = oFields.begin(); it != oFields.end(); ++it)
        {
            if (*it == NULL) continue;
            const char* psName = CPLGetXMLValue( *it, "Name", NULL );
            const char* psTypeRef = CPLGetXMLValue( *it, "Type.REF", NULL );
            if (psTypeRef == NULL) //Assoc Role
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
                else if (EQUAL(typeName, "IlisMeta07.ModelData.CoordType"))
                {
                    AddCoord(iliVersion, psName, psElementNode, oAxisCount);
                }
                else if (EQUAL(typeName, "IlisMeta07.ModelData.LineType"))
                {
                    const char* psKind = CPLGetXMLValue( psElementNode, "Kind", NULL );
                    if (iliVersion == 1)
                    {
                        if (EQUAL(psKind, "Area"))  // Kind DirectedPolyline, Polyline(CoordType RoadsExdm2ben.Point2D), Area
                        {
                            CPLString areaPointGeomName = psName + CPLString("__Point");
                            AddCoord(iliVersion, areaPointGeomName, psElementNode, oAxisCount);

                            CPLString lineLayerName = poTableDefn->GetName() + CPLString("_") + psName;
                            AddGeomTable(lineLayerName, psName, wkbMultiLineString);

                            // OGR 1.10 had a seperate areay polygon table:
                            // CPLString areaLayerName = poTableDefn->GetName() + CPLString("__Areas");
                            // AddGeomTable(areaLayerName, psName, wkbPolygon);
                            // areaLayer->SetAreaLayers(layer, areaLineLayer);

                            //Add geometry field for polygonized areas
                            AddGeomField(psName, wkbPolygon);
                        } else if (EQUAL(psKind, "Surface"))
                        {
                            CPLString geomLayerName = poTableDefn->GetName() + CPLString("_") + psName;
                            AddGeomTable(geomLayerName, psName, wkbPolygon);
                            //layer->SetSurfacePolyLayer(polyLayer, layer->GetLayerDefn()->GetGeomFieldCount()-1);
                        } else {
                            CPLDebug( "OGR_ILI", "Adding geometry field of kind %s as wkbMultiLineString", psKind);
                            AddGeomField(psName, wkbMultiLineString);
                        }
                    } else {
                        if (EQUAL(psKind, "Area") || EQUAL(psKind, "Surface"))
                        {
                            AddGeomField(psName, wkbPolygon);
                        } else {
                            CPLDebug( "OGR_ILI", "Adding geometry field of kind %s as wkbMultiLineString", psKind);
                            AddGeomField(psName, wkbMultiLineString);
                        }
                    }
                }
                else
                {
                    //MultiValue // e.g. Axes, SurfaceEdge.LineAttrs, SurfaceBoundary.Lines, LineGeometry.Segments
                    //ClassRefType
                    CPLError(CE_Warning, CPLE_NotSupported,
                        "Field '%s' of class %s has unsupported type %s", psName, poTableDefn->GetName(), typeName);
                }
            }
        }
    }
    FeatureDefnList tableDefs()
    {
        FeatureDefnList poTableList;
        if (!hasDerivedClasses && !isEmbedded())
        {
            poTableList.push_back(poTableDefn);
            poTableList.insert(poTableList.end(), oAdditionalTableDefs.begin(), oAdditionalTableDefs.end());
        }
        return poTableList;
    }

};


ImdReader::ImdReader(int iliVersionIn) : iliVersion(iliVersionIn) {
  mainModelName = "OGR";
  mainTopicName = "OGR";
  codeBlank = '_';
  codeUndefined = '@';
  codeContinue = '\\';
}

ImdReader::~ImdReader() {
}

const char* ImdReader::LayerName(const char* psClassTID) {
    if (iliVersion == 1)
    {
        char **papszTokens =
            CSLTokenizeString2( psClassTID, ".", CSLT_ALLOWEMPTYTOKENS );

        CPLString layername;
        for(int i = 1; papszTokens != NULL && papszTokens[i] != NULL; i++)
        {
            if (i>1) layername += "__";
            layername += papszTokens[i];
        }
        CSLDestroy( papszTokens );
        return layername;
    } else {
        return psClassTID;
    }
}

FeatureDefnList ImdReader::ReadModel(const char *pszFilename) {
    CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Reading model '%s'", pszFilename);
    FeatureDefnList poTableList;

    CPLXMLNode* psRootNode = CPLParseXMLFile(pszFilename);
    if( psRootNode == NULL )
        return poTableList;
    CPLXMLNode *psSectionNode = CPLGetXMLNode( psRootNode, "=TRANSFER.DATASECTION" );
    if( psSectionNode == NULL )
        return poTableList;

    StrNodeMap oTidLookup; /* for fast lookup of REF relations */
    typedef std::map<CPLXMLNode*,IliClass*> ClassesMap; /* all classes with XML node for lookup */
    ClassesMap oClasses;
    NodeCountMap oAxisCount;
    const char *modelName;

    /* Fill TID lookup map and IliClasses lookup map */
    CPLXMLNode* psModel = psSectionNode->psChild;
    while( psModel != NULL )
    {
        modelName = CPLGetXMLValue( psModel, "BID", NULL );
        //CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Model: '%s'", modelName);

        CPLXMLNode* psEntry = psModel->psChild;
        while( psEntry != NULL )
        {
            if (psEntry->eType != CXT_Attribute) //ignore BID
            {
                //CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
                const char* psTID = CPLGetXMLValue( psEntry, "TID", NULL );
                if( psTID != NULL )
                    oTidLookup[psTID] = psEntry;


                if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Model") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    mainModelName = CPLGetXMLValue( psEntry, "Name", "OGR" ); //FIXME: check model inheritance
                    //version = CPLGetXMLValue(psEntry, "iliVersion", "0"); //1 or 2.3

                    CPLXMLNode *psFormatNode = CPLGetXMLNode( psEntry, "ili1Format" );
                    if (psFormatNode != NULL)
                    {
                        psFormatNode = psFormatNode->psChild;
                        codeBlank = atoi(CPLGetXMLValue(psFormatNode, "blankCode", "95"));
                        codeUndefined = atoi(CPLGetXMLValue(psFormatNode, "undefinedCode", "64"));
                        codeContinue = atoi(CPLGetXMLValue(psFormatNode, "continueCode", "92"));
                    }
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.SubModel") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    mainBasketName = CPLGetXMLValue(psEntry, "TID", "OGR");
                    mainTopicName = CPLGetXMLValue(psEntry, "Name", "OGR");
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Class") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    //CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Class Name: '%s'", psTID);
                    OGRFeatureDefn* poTableDefn = new OGRFeatureDefn(LayerName(psTID));
                    oClasses[psEntry] = new IliClass(poTableDefn);
                }
            }
            psEntry = psEntry->psNext;
        }

        // 2nd pass: add fields via TransferElement entries & role associations
        psEntry = psModel->psChild;
        while( psEntry != NULL )
        {
            if (psEntry->eType != CXT_Attribute) //ignore BID
            {
                //CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Node tag: '%s'", psEntry->pszValue);
                if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.TransferElement") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    const char* psClassRef = CPLGetXMLValue( psEntry, "TransferClass.REF", NULL );
                    const char* psElementRef = CPLGetXMLValue( psEntry, "TransferElement.REF", NULL );
                    int iOrderPos = atoi(CPLGetXMLValue( psEntry, "TransferElement.ORDER_POS", "0" ))-1;
                    IliClass* psParentClass = oClasses[oTidLookup[psClassRef]];
                    CPLXMLNode* psElementNode = oTidLookup[psElementRef];
                    psParentClass->AddFieldNode(psElementNode, iOrderPos);
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.Role") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    const char* psRefParent = CPLGetXMLValue( psEntry, "Association.REF", NULL );
                    int iOrderPos = atoi(CPLGetXMLValue( psEntry, "Association.ORDER_POS", "0" ))-1;
                    IliClass* psParentClass = oClasses[oTidLookup[psRefParent]];
                    if (psParentClass)
                        psParentClass->AddRoleNode(psEntry, iOrderPos);
                }
                else if( EQUAL(psEntry->pszValue, "IlisMeta07.ModelData.AxisSpec") && !EQUAL(modelName, "MODEL.INTERLIS"))
                {
                    const char* psClassRef = CPLGetXMLValue( psEntry, "CoordType.REF", NULL );
                    //int iOrderPos = atoi(CPLGetXMLValue( psEntry, "Axis.ORDER_POS", "0" ))-1;
                    CPLXMLNode* psCoordTypeNode = oTidLookup[psClassRef];
                    oAxisCount[psCoordTypeNode] += 1;
                }
            }
            psEntry = psEntry->psNext;

        }

        psModel = psModel->psNext;
    }

    /* Analyze class inheritance & add fields to class table defn */
    for (ClassesMap::const_iterator it = oClasses.begin(); it != oClasses.end(); ++it)
    {
        //CPLDebug( "ImdReader::ReadModel   OGR_ILI", "Class: '%s'", it->second->poTableDefn->GetName());
        const char* psRefSuper = CPLGetXMLValue( it->first, "Super.REF", NULL );
        if (psRefSuper)
            oClasses[oTidLookup[psRefSuper]]->hasDerivedClasses = true;
        it->second->AddFieldDefinitions(iliVersion, oTidLookup, oAxisCount);
    }

    /* Filter relevant classes */
    for (ClassesMap::const_iterator it = oClasses.begin(); it != oClasses.end(); ++it)
    {
        FeatureDefnList oClassTables = it->second->tableDefs();
        poTableList.insert(poTableList.end(), oClassTables.begin(), oClassTables.end());
    }

    CPLDestroyXMLNode(psRootNode);
    return poTableList;
}
