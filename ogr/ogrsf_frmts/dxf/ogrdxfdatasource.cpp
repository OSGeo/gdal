/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRDXFDataSource()                          */
/************************************************************************/

OGRDXFDataSource::OGRDXFDataSource()

{
    fp = NULL;
}

/************************************************************************/
/*                         ~OGRDXFDataSource()                          */
/************************************************************************/

OGRDXFDataSource::~OGRDXFDataSource()

{
/* -------------------------------------------------------------------- */
/*      Destroy layers.                                                 */
/* -------------------------------------------------------------------- */
    while( apoLayers.size() > 0 )
    {
        delete apoLayers.back();
        apoLayers.pop_back();
    }

/* -------------------------------------------------------------------- */
/*      Close file.                                                     */
/* -------------------------------------------------------------------- */
    if( fp != NULL )
    {
        VSIFCloseL( fp );
        fp = NULL;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/


OGRLayer *OGRDXFDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= (int) apoLayers.size() )
        return NULL;
    else
        return apoLayers[iLayer];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDXFDataSource::Open( const char * pszFilename, int bHeaderOnly )

{
    if( !EQUAL(CPLGetExtension(pszFilename),"dxf") )
        return FALSE;

    osEncoding = CPL_ENC_ISO8859_1;

    osName = pszFilename;

    bInlineBlocks = CSLTestBoolean(
        CPLGetConfigOption( "DXF_INLINE_BLOCKS", "TRUE" ) );

    if( CSLTestBoolean(
            CPLGetConfigOption( "DXF_HEADER_ONLY", "FALSE" ) ) )
        bHeaderOnly = TRUE;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "r" );
    if( fp == NULL )
        return FALSE;

    oReader.Initialize( fp );
    
/* -------------------------------------------------------------------- */
/*      Confirm we have a header section.                               */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];
    int  nCode;
    int  bEntitiesOnly = FALSE;

    if( ReadValue( szLineBuf ) != 0 || !EQUAL(szLineBuf,"SECTION") )
        return FALSE;

    if( ReadValue( szLineBuf ) != 2 
        || (!EQUAL(szLineBuf,"HEADER") && !EQUAL(szLineBuf,"ENTITIES")) )
        return FALSE;

    if( EQUAL(szLineBuf,"ENTITIES") )
        bEntitiesOnly = TRUE;

/* -------------------------------------------------------------------- */
/*      Process the header, picking up a few useful pieces of           */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !bEntitiesOnly )
    {
        ReadHeaderSection();
        ReadValue(szLineBuf);

/* -------------------------------------------------------------------- */
/*      Process the CLASSES section, if present.                        */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);

        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"CLASSES") )
        {
            while( (nCode = ReadValue( szLineBuf,sizeof(szLineBuf) )) > -1 
                   && !EQUAL(szLineBuf,"ENDSEC") )
            {
                //printf("C:%d/%s\n", nCode, szLineBuf );
            }
        }

/* -------------------------------------------------------------------- */
/*      Process the TABLES section, if present.                         */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"TABLES") )
        {
            ReadTablesSection();
            ReadValue(szLineBuf);
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a blocks layer if we are not in inlining mode.           */
/* -------------------------------------------------------------------- */
    if( !bInlineBlocks )
        apoLayers.push_back( new OGRDXFBlocksLayer( this ) );

/* -------------------------------------------------------------------- */
/*      Create out layer object - we will need it when interpreting     */
/*      blocks.                                                         */
/* -------------------------------------------------------------------- */
    apoLayers.push_back( new OGRDXFLayer( this ) );

/* -------------------------------------------------------------------- */
/*      Process the BLOCKS section if present.                          */
/* -------------------------------------------------------------------- */
    if( !bEntitiesOnly )
    {
        if( EQUAL(szLineBuf,"ENDSEC") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"SECTION") )
            ReadValue(szLineBuf);
        
        if( EQUAL(szLineBuf,"BLOCKS") )
        {
            ReadBlocksSection();
            ReadValue(szLineBuf);
        }
    }

    if( bHeaderOnly )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Now we are at the entities section, hopefully.  Confirm.        */
/* -------------------------------------------------------------------- */
    if( EQUAL(szLineBuf,"SECTION") )
        ReadValue(szLineBuf);

    if( !EQUAL(szLineBuf,"ENTITIES") )
        return FALSE;

    iEntitiesSectionOffset = oReader.iSrcBufferFileOffset + oReader.iSrcBufferOffset;
    apoLayers[0]->ResetReading();

    return TRUE;
}

/************************************************************************/
/*                         ReadTablesSection()                          */
/************************************************************************/

void OGRDXFDataSource::ReadTablesSection()

{
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

        //CPLDebug( "DXF", "Found table %s.", szLineBuf );

        while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1
               && !EQUAL(szLineBuf,"ENDTAB") )
        {
            if( nCode == 0 && EQUAL(szLineBuf,"LAYER") )
                ReadLayerDefinition();
            if( nCode == 0 && EQUAL(szLineBuf,"LTYPE") )
                ReadLineTypeDefinition();
        }
    }

    CPLDebug( "DXF", "Read %d layer definitions.", (int) oLayerTable.size() );
}

/************************************************************************/
/*                        ReadLayerDefinition()                         */
/************************************************************************/

void OGRDXFDataSource::ReadLayerDefinition()

{
    char szLineBuf[257];
    int  nCode;
    std::map<CPLString,CPLString> oLayerProperties;
    CPLString osLayerName = "";

    oLayerProperties["Hidden"] = "0";

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > 0 )
    {
        switch( nCode )
        {
          case 2:
            osLayerName = ACTextUnescape(szLineBuf,GetEncoding());
            oLayerProperties["Exists"] = "1";
            break;

          case 6:
            oLayerProperties["Linetype"] = ACTextUnescape(szLineBuf,
                                                          GetEncoding());
            break;
            
          case 62:
            oLayerProperties["Color"] = szLineBuf;

            if( atoi(szLineBuf) < 0 ) // Is layer off?
                oLayerProperties["Hidden"] = "1";
            break;
            
          case 70:
            oLayerProperties["Flags"] = szLineBuf;
            if( atoi(szLineBuf) & 0x01 ) // Is layer frozen?
                oLayerProperties["Hidden"] = "1";
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
    
    if( nCode == 0 )
        UnreadValue();
}

/************************************************************************/
/*                        LookupLayerProperty()                         */
/************************************************************************/

const char *OGRDXFDataSource::LookupLayerProperty( const char *pszLayer,
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

void OGRDXFDataSource::ReadLineTypeDefinition()

{
    char szLineBuf[257];
    int  nCode;
    CPLString osLineTypeName;
    CPLString osLineTypeDef;

    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > 0 )
    {
        switch( nCode )
        {
          case 2:
            osLineTypeName = ACTextUnescape(szLineBuf,GetEncoding());
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
    
    if( nCode == 0 )
        UnreadValue();
}

/************************************************************************/
/*                           LookupLineType()                           */
/************************************************************************/

const char *OGRDXFDataSource::LookupLineType( const char *pszName )

{
    if( oLineTypeTable.count(pszName) > 0 )
        return oLineTypeTable[pszName];
    else
        return NULL;
}

/************************************************************************/
/*                         ReadHeaderSection()                          */
/************************************************************************/

void OGRDXFDataSource::ReadHeaderSection()

{
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

    if (nCode != -1)
    {
        nCode = ReadValue( szLineBuf, sizeof(szLineBuf) );
        UnreadValue();
    }

    /* Unusual DXF files produced by dxflib */
    /* such as http://www.ribbonsoft.com/library/architecture/plants/decd5.dxf */
    /* where there is a spurious ENDSEC in the middle of the header variables */
    if (nCode == 9 && EQUALN(szLineBuf,"$", 1) )
    {
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
    }

    CPLDebug( "DXF", "Read %d header variables.", 
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
                                       
    if( CPLGetConfigOption( "DXF_ENCODING", NULL ) != NULL )
        osEncoding = CPLGetConfigOption( "DXF_ENCODING", NULL );

    if( osEncoding != CPL_ENC_ISO8859_1 )
        CPLDebug( "DXF", "Treating DXF as encoding '%s', $DWGCODEPAGE='%s'", 
                  osEncoding.c_str(), osCodepage.c_str() );
}

/************************************************************************/
/*                            GetVariable()                             */
/*                                                                      */
/*      Fetch a variable that came from the HEADER section.             */
/************************************************************************/

const char *OGRDXFDataSource::GetVariable( const char *pszName, 
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

void OGRDXFDataSource::AddStandardFields( OGRFeatureDefn *poFeatureDefn )

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
