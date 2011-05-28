/******************************************************************************
 * $Id: ogrdxfdatasource.cpp 22009 2011-03-22 20:01:34Z warmerdam $
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

CPL_CVSID("$Id: ogrdxfdatasource.cpp 22009 2011-03-22 20:01:34Z warmerdam $");

/************************************************************************/
/*                          OGRDWGDataSource()                          */
/************************************************************************/

OGRDWGDataSource::OGRDWGDataSource()

{
    poDb = NULL;
}

/************************************************************************/
/*                         ~OGRDWGDataSource()                          */
/************************************************************************/

OGRDWGDataSource::~OGRDWGDataSource()

{
/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    while( apoLayers.size() > 0 )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/


OGRLayer *OGRDWGDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int) apoLayers.size() )
        return NULL;
    else
        return apoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDWGDataSource::Open( OGRDWGServices *poServices,
                            const char * pszFilename, int bHeaderOnly )

{
    if( !EQUAL(CPLGetExtension(pszFilename),"dwg") )
        return FALSE;

    this->poServices = poServices;

    osEncoding = CPL_ENC_ISO8859_1;

    osName = pszFilename;

    bInlineBlocks = CSLTestBoolean(
        CPLGetConfigOption( "DWG_INLINE_BLOCKS", "TRUE" ) );

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    OdString f(pszFilename);
    poDb = poServices->readFile(f.c_str(), true, false, Oda::kShareDenyNo); 

    if( poDb.isNull() )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Process the header, picking up a few useful pieces of           */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    ReadHeaderSection();
    ReadTablesSection();

/* -------------------------------------------------------------------- */
/*      Create a blocks layer if we are not in inlining mode.           */
/* -------------------------------------------------------------------- */
    if( !bInlineBlocks )
        apoLayers.push_back( new OGRDWGBlocksLayer( this ) );

/* -------------------------------------------------------------------- */
/*      Create out layer object - we will need it when interpreting     */
/*      blocks.                                                         */
/* -------------------------------------------------------------------- */
    apoLayers.push_back( new OGRDWGLayer( this ) );

    ReadBlocksSection();

    return TRUE;
}

/************************************************************************/
/*                         ReadTablesSection()                          */
/************************************************************************/

void OGRDWGDataSource::ReadTablesSection()

{
#ifdef notdef
    char szLineBuf[257];
    int  nCode;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1 
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        // We are only interested in extracting tables.
        if( nCode != 0 || !EQUAL(szLineBuf,"TABLE") )
            continue;

        // Currently we are only interested in the LAYER table.
        nCode = ReadValue( szLineBuf, sizeof(szLineBuf) );

        if( nCode != 2 )
            continue;

        //CPLDebug( "DWG", "Found table %s.", szLineBuf );

        while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1
               && !EQUAL(szLineBuf,"ENDTAB") )
        {
            if( nCode == 0 && EQUAL(szLineBuf,"LAYER") )
                ReadLayerDefinition();
            if( nCode == 0 && EQUAL(szLineBuf,"LTYPE") )
                ReadLineTypeDefinition();
        }
    }

    CPLDebug( "DWG", "Read %d layer definitions.", (int) oLayerTable.size() );
#endif
}

/************************************************************************/
/*                        ReadLayerDefinition()                         */
/************************************************************************/

void OGRDWGDataSource::ReadLayerDefinition()

{
#ifdef notdef
    char szLineBuf[257];
    int  nCode;
    std::map<CPLString,CPLString> oLayerProperties;
    CPLString osLayerName = "";

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > 0 )
    {
        switch( nCode )
        {
          case 2:
            osLayerName = szLineBuf;
            oLayerProperties["Exists"] = "1";
            break;

          case 6:
            oLayerProperties["Linetype"] = szLineBuf;
            break;
            
          case 62:
            oLayerProperties["Color"] = szLineBuf;
            break;
            
          case 70:
            oLayerProperties["Flags"] = szLineBuf;
            break;

          case 370:
          case 39:
            oLayerProperties["LineWeight"] = szLineBuf;
            break;

          default:
            break;
        }
    }

    if( oLayerProperties.size() > 0 )
        oLayerTable[osLayerName] = oLayerProperties;
    
    UnreadValue();
#endif
}

/************************************************************************/
/*                        LookupLayerProperty()                         */
/************************************************************************/

const char *OGRDWGDataSource::LookupLayerProperty( const char *pszLayer,
                                                   const char *pszProperty )

{
    if( pszLayer == NULL )
        return NULL;

    try {
        return (oLayerTable[pszLayer])[pszProperty];
    } catch( ... ) {
        return NULL;
    }
}

/************************************************************************/
/*                       ReadLineTypeDefinition()                       */
/************************************************************************/

void OGRDWGDataSource::ReadLineTypeDefinition()

{
#ifdef notdef
    char szLineBuf[257];
    int  nCode;
    CPLString osLineTypeName;
    CPLString osLineTypeDef;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > 0 )
    {
        switch( nCode )
        {
          case 2:
            osLineTypeName = szLineBuf;
            break;

          case 49:
          {
              if( osLineTypeDef != "" )
                  osLineTypeDef += " ";

              if( szLineBuf[0] == '-' )
                  osLineTypeDef += szLineBuf+1;
              else
                  osLineTypeDef += szLineBuf;

              osLineTypeDef += "g";
          }
          break;
            
          default:
            break;
        }
    }

    if( osLineTypeDef != "" )
        oLineTypeTable[osLineTypeName] = osLineTypeDef;
    
    UnreadValue();
#endif
}

/************************************************************************/
/*                           LookupLineType()                           */
/************************************************************************/

const char *OGRDWGDataSource::LookupLineType( const char *pszName )

{
    if( oLineTypeTable.count(pszName) > 0 )
        return oLineTypeTable[pszName];
    else
        return NULL;
}

/************************************************************************/
/*                         ReadHeaderSection()                          */
/************************************************************************/

void OGRDWGDataSource::ReadHeaderSection()

{
#ifdef notdef
    char szLineBuf[257];
    int  nCode;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1 
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        if( nCode != 9 )
            continue;

        CPLString osName = szLineBuf;

        ReadValue( szLineBuf, sizeof(szLineBuf) );

        CPLString osValue = szLineBuf;

        oHeaderVariables[osName] = osValue;
    }

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
    else if( EQUALN(osCodepage,"ANSI_",5) )
    {
        osEncoding = "CP";
        osEncoding += osCodepage + 5;
    }
    else
    {
        // fallback to the default 
        osEncoding = CPL_ENC_ISO8859_1;
    }
                                       
    if( CPLGetConfigOption( "DWG_ENCODING", NULL ) != NULL )
        osEncoding = CPLGetConfigOption( "DWG_ENCODING", NULL );

    if( osEncoding != CPL_ENC_ISO8859_1 )
        CPLDebug( "DWG", "Treating DWG as encoding '%s', $DWGCODEPAGE='%s'", 
                  osEncoding.c_str(), osCodepage.c_str() );
#endif
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
        OGRFieldDefn  oTextField( "BlockName", OFTString );
        poFeatureDefn->AddFieldDefn( &oTextField );
    }
}
