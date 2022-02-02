/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRDWGDataSource()                          */
/************************************************************************/

OGRDWGDataSource::OGRDWGDataSource() :
  fp(nullptr),
  iEntitiesSectionOffset(0),
  bInlineBlocks(FALSE),
  bAttributes(FALSE),
  bAllAttributes(TRUE),
  poServices(nullptr),
  poDb(static_cast<const OdRxObject*>(nullptr))

{
}

/************************************************************************/
/*                         ~OGRDWGDataSource()                          */
/************************************************************************/

OGRDWGDataSource::~OGRDWGDataSource()

{
/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    while( !apoLayers.empty() )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGDataSource::TestCapability( const char * /*pszCap*/ )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDWGDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int) apoLayers.size() )
        return nullptr;
    else
        return apoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDWGDataSource::Open( OGRDWGServices *poServicesIn,
                            const char * pszFilename, int /*bHeaderOnly*/ )

{
    poServices = poServicesIn;

    osEncoding = CPL_ENC_ISO8859_1;

    m_osName = pszFilename;

    bInlineBlocks = CPLTestBool(
        CPLGetConfigOption( "DWG_INLINE_BLOCKS", "TRUE" ) );
    bAttributes = CPLTestBool(
    CPLGetConfigOption( "DWG_ATTRIBUTES", "FALSE" ) );
    bAllAttributes = CPLTestBool(
        CPLGetConfigOption("DWG_ALL_ATTRIBUTES", "TRUE"));

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    try
    {
        OdString f(pszFilename);
        poDb = poServices->readFile(f.c_str(), true, false, Oda::kShareDenyNo);
    }
    catch( OdError& e )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s",
                  (const char *) poServices->getErrorDescription(e.code()) );
    }
    catch( ... )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DWG readFile(%s) failed with generic exception.",
                  pszFilename );
    }

    if( poDb.isNull() )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Process the header, picking up a few useful pieces of           */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    ReadHeaderSection();
    ReadLineTypeDefinitions();
    ReadLayerDefinitions();

/* -------------------------------------------------------------------- */
/*      Create a blocks layer if we are not in inlining mode.           */
/* -------------------------------------------------------------------- */
    if( !bInlineBlocks )
        apoLayers.push_back( new OGRDWGBlocksLayer( this ) );

/* -------------------------------------------------------------------- */
/*      Create out layer object - we will need it when interpreting     */
/*      blocks.                                                         */
/* -------------------------------------------------------------------- */
    if(bAttributes){
        ReadAttDefinitions();
    }
    apoLayers.push_back( new OGRDWGLayer( this ) );

    ReadBlocksSection();

    return TRUE;
}


/************************************************************************/
/*                        ReadAttDefinitions()                          */
/************************************************************************/

void OGRDWGDataSource::ReadAttDefinitions()

{
    OdDbBlockTablePtr pTable = poDb->getBlockTableId().safeOpenObject();
    OdDbSymbolTableIteratorPtr pBlkIter = pTable->newIterator();
    OdDbBlockTableRecordPtr  poModelSpace, poBlock;
  
    for (pBlkIter->start(); !pBlkIter->done(); pBlkIter->step())
    {
        poBlock = pBlkIter->getRecordId().safeOpenObject();
        OdDbObjectIteratorPtr poEntIter = poBlock->newIterator();

        CPLString* attName = nullptr;
        /* -------------------------------------------------------------------- */
        /*      Fetch the next attribute definition.                            */
        /* -------------------------------------------------------------------- */
        for (poEntIter->start(); !poEntIter->done(); poEntIter->step())
        {
            OdDbObjectId oId = poEntIter->objectId();
            OdDbEntityPtr poEntity = OdDbEntity::cast(oId.openObject());
            if (poEntity.isNull())
                continue;
            /* -------------------------------------------------------------------- */
            /*      Check AcDbAttributeDefinition then create and insert attribute  */
            /* -------------------------------------------------------------------- */
            OdRxClass* poClass = poEntity->isA();
            const OdString osName = poClass->name();
            const char* pszEntityClassName =  osName;
            if (EQUAL(pszEntityClassName, "AcDbAttributeDefinition"))
            {
                OdDbAttributeDefinitionPtr poAttributeDefinition = OdDbAttributeDefinition::cast(poEntity);
                if (bAllAttributes || !poAttributeDefinition->isInvisible()) {
                    const char* tagName = poAttributeDefinition->tag();
                    attName = new CPLString(tagName);
                    if (attributeFields.count(*attName) == 0) {
                        attributeFields.insert(*attName);
                    }
                }

            }
        }
    }
}
/************************************************************************/
/*                        ReadLayerDefinitions()                        */
/************************************************************************/

void OGRDWGDataSource::ReadLayerDefinitions()

{
    OdDbLayerTablePtr poLT = poDb->getLayerTableId().safeOpenObject();
    OdDbSymbolTableIteratorPtr poIter = poLT->newIterator();

    for (poIter->start(); !poIter->done(); poIter->step())
    {
        CPLString osValue;
        OdDbLayerTableRecordPtr poLD = poIter->getRecordId().safeOpenObject();
        std::map<CPLString,CPLString> oLayerProperties;

        CPLString osLayerName = ACTextUnescape(poLD->getName(),GetEncoding(), false);

        oLayerProperties["Exists"] = "1";

        OdDbLinetypeTableRecordPtr poLinetypeTableRecord = poLD->linetypeObjectId().safeOpenObject();
        oLayerProperties["Linetype"] =
            ACTextUnescape(poLinetypeTableRecord->getName(),GetEncoding(), false);

        osValue.Printf( "%d", poLD->colorIndex() );
        oLayerProperties["Color"] = osValue;

        osValue.Printf( "%d", (int) poLD->lineWeight() );
        oLayerProperties["LineWeight"] = osValue;

        if( poLD->isFrozen() || poLD->isHidden() || poLD->isOff() )
            oLayerProperties["Hidden"] = "1";
        else
            oLayerProperties["Hidden"] = "0";

        oLayerTable[osLayerName] = oLayerProperties;
    }

    CPLDebug( "DWG", "Read %d layer definitions.",
              (int) oLayerTable.size() );
}

/************************************************************************/
/*                        LookupLayerProperty()                         */
/************************************************************************/

const char *OGRDWGDataSource::LookupLayerProperty( const char *pszLayer,
                                                   const char *pszProperty )

{
    if( pszLayer == nullptr )
        return nullptr;

    try {
        return (oLayerTable[pszLayer])[pszProperty];
    } catch( ... ) {
        return nullptr;
    }
}

/************************************************************************/
/*                       ReadLineTypeDefinitions()                      */
/************************************************************************/

void OGRDWGDataSource::ReadLineTypeDefinitions()

{
    OdDbLinetypeTablePtr poTable = poDb->getLinetypeTableId().safeOpenObject();
    OdDbSymbolTableIteratorPtr poIter = poTable->newIterator();

    for (poIter->start(); !poIter->done(); poIter->step())
    {
        CPLString osLineTypeName;
        CPLString osLineTypeDef;
        OdDbLinetypeTableRecordPtr poLT = poIter->getRecordId().safeOpenObject();

        osLineTypeName = ACTextUnescape(poLT->getName(),GetEncoding(),false);

        if (poLT->numDashes())
        {
            for (int i=0; i < poLT->numDashes(); i++)
            {
                if( i > 0 )
                    osLineTypeDef += " ";

                CPLString osValue;
                osValue.Printf( "%g", fabs(poLT->dashLengthAt(i)) );
                osLineTypeDef += osValue;

                osLineTypeDef += "g";
            }

            oLineTypeTable[osLineTypeName] = osLineTypeDef;
            CPLDebug( "DWG", "LineType '%s' = '%s'",
                       osLineTypeName.c_str(),
                       osLineTypeDef.c_str() );
        }
    }
}

/************************************************************************/
/*                           LookupLineType()                           */
/************************************************************************/

const char *OGRDWGDataSource::LookupLineType( const char *pszName )

{
    if( oLineTypeTable.count(pszName) > 0 )
        return oLineTypeTable[pszName];
    else
        return nullptr;
}

/************************************************************************/
/*                         ReadHeaderSection()                          */
/************************************************************************/

void OGRDWGDataSource::ReadHeaderSection()

{
    // using: DWGCODEPAGE, DIMTXT, LUPREC

    CPLString osValue;

    osValue.Printf( "%d", poDb->getLUPREC() );
    oHeaderVariables["$LUPREC"] = osValue;

    osValue.Printf( "%g", poDb->dimtxt() );
    oHeaderVariables["$DIMTXT"] = osValue;

    CPLDebug( "DWG", "Read %d header variables.",
              (int) oHeaderVariables.size() );

/* -------------------------------------------------------------------- */
/*      Decide on what CPLRecode() name to use for the files            */
/*      encoding or allow the encoding to be overridden.                */
/* -------------------------------------------------------------------- */
    CPLString osCodepage = GetVariable( "$DWGCODEPAGE", "ANSI_1252" );

    // not strictly accurate but works even without iconv.
    if( osCodepage == "ANSI_1252" )
        osEncoding = CPL_ENC_ISO8859_1;
    else if( STARTS_WITH_CI(osCodepage, "ANSI_") )
    {
        osEncoding = "CP";
        osEncoding += osCodepage + 5;
    }
    else
    {
        // fallback to the default
        osEncoding = CPL_ENC_ISO8859_1;
    }

    if( CPLGetConfigOption( "DWG_ENCODING", nullptr ) != nullptr )
        osEncoding = CPLGetConfigOption( "DWG_ENCODING", nullptr );

    if( osEncoding != CPL_ENC_ISO8859_1 )
        CPLDebug( "DWG", "Treating DWG as encoding '%s', $DWGCODEPAGE='%s'",
                  osEncoding.c_str(), osCodepage.c_str() );
}

/************************************************************************/
/*                            GetVariable()                             */
/*                                                                      */
/*      Fetch a variable that came from the HEADER section.             */
/************************************************************************/

const char *OGRDWGDataSource::GetVariable( const char *pszName,
                                           const char *pszDefault )

{
    if( oHeaderVariables.count(pszName) == 0 )
        return pszDefault;
    else
        return oHeaderVariables[pszName];
}

/************************************************************************/
/*                         AddStandardFields()                          */
/************************************************************************/

void OGRDWGDataSource::AddStandardFields( OGRFeatureDefn *poFeatureDefn )

{
    OGRFieldDefn  oLayerField( "Layer", OFTString );
    poFeatureDefn->AddFieldDefn( &oLayerField );

    OGRFieldDefn  oClassField( "SubClasses", OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oExtendedField( "ExtendedEntity", OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oLinetypeField( "Linetype", OFTString );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );

    OGRFieldDefn  oEntityHandleField( "EntityHandle", OFTString );
    poFeatureDefn->AddFieldDefn( &oEntityHandleField );

    OGRFieldDefn  oTextField( "Text", OFTString );
    poFeatureDefn->AddFieldDefn( &oTextField );

    if( !bInlineBlocks )
    {
        OGRFieldDefn  oBlockNameField( "BlockName", OFTString );
        poFeatureDefn->AddFieldDefn( &oBlockNameField );

        OGRFieldDefn  oBlockAttrField( "BlockAttributes", OFTString );
        oBlockAttrField.SetSubType(OFSTJSON);
        poFeatureDefn->AddFieldDefn( &oBlockAttrField );
    }
}
