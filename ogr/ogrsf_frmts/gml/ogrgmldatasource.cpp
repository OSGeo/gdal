/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRGMLDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gml.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::OGRGMLDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    poReader = NULL;
    fpOutput = NULL;
    bFpOutputIsStdout = FALSE;

    papszCreateOptions = NULL;
    bOutIsTempFile = FALSE;
}

/************************************************************************/
/*                        ~OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::~OGRGMLDataSource()

{

    if( fpOutput != NULL )
    {
        PrintLine( fpOutput, "%s", 
                    "</ogr:FeatureCollection>" );

        InsertHeader();

        if( nBoundedByLocation != -1 
            && sBoundingRect.IsInit() 
            && VSIFSeekL( fpOutput, nBoundedByLocation, SEEK_SET ) == 0 )
        {
            PrintLine( fpOutput, "  <gml:boundedBy>" );
            PrintLine( fpOutput, "    <gml:Box>" );
            PrintLine( fpOutput, 
                        "      <gml:coord><gml:X>%.16g</gml:X>"
                        "<gml:Y>%.16g</gml:Y></gml:coord>",
                        sBoundingRect.MinX, sBoundingRect.MinY );
            PrintLine( fpOutput, 
                        "      <gml:coord><gml:X>%.16g</gml:X>"
                        "<gml:Y>%.16g</gml:Y></gml:coord>",
                        sBoundingRect.MaxX, sBoundingRect.MaxY );
            PrintLine( fpOutput, "    </gml:Box>" );
            PrintLine( fpOutput, "  </gml:boundedBy>" );
        }

        VSIFCloseL( fpOutput );
    }

    CSLDestroy( papszCreateOptions );
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    if( poReader )
    {
        if (bOutIsTempFile)
            VSIUnlink(poReader->GetSourceFileName());
        delete poReader;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGMLDataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[2048];

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszNewName, "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open GML file `%s'.", 
                      pszNewName );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is GML, load a header chunk and check      */
/*      for signs it is GML                                             */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
    {
        size_t nRead = VSIFReadL( szHeader, 1, sizeof(szHeader), fp );
        if (nRead <= 0)
        {
            VSIFCloseL( fp );
            return FALSE;
        }
        szHeader[MIN(nRead, sizeof(szHeader))-1] = '\0';

/* -------------------------------------------------------------------- */
/*      Check for a UTF-8 BOM and skip if found                         */
/*                                                                      */
/*      TODO: BOM is variable-lenght parameter and depends on encoding. */
/*            Add BOM detection for other encodings.                    */
/* -------------------------------------------------------------------- */

        // Used to skip to actual beginning of XML data
        char* szPtr = szHeader;

        if( ( (unsigned char)szHeader[0] == 0xEF )
            && ( (unsigned char)szHeader[1] == 0xBB )
            && ( (unsigned char)szHeader[2] == 0xBF) )
        {
            szPtr += 3;
        }

/* -------------------------------------------------------------------- */
/*      Here, we expect the opening chevrons of GML tree root element   */
/* -------------------------------------------------------------------- */
        if( szPtr[0] != '<' 
            || strstr(szPtr,"opengis.net/gml") == NULL )
        {
            VSIFCloseL( fp );
            return FALSE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      We assume now that it is GML.  Close and instantiate a          */
/*      GMLReader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFCloseL( fp );
    
    poReader = CreateGMLReader();
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s appears to be GML but the GML reader can't\n"
                  "be instantiated, likely because Xerces or Expat support wasn't\n"
                  "configured in.", 
                  pszNewName );
        return FALSE;
    }

    poReader->SetSourceFile( pszNewName );
    
/* -------------------------------------------------------------------- */
/*      Resolve the xlinks in the source file and save it with the      */
/*      extension ".resolved.gml". The source file will to set to that. */
/* -------------------------------------------------------------------- */

    char *pszXlinkResolvedFilename = NULL;
    const char *pszOption = CPLGetConfigOption("GML_SAVE_RESOLVED_TO", NULL);
    int bResolve = TRUE;
    if( pszOption != NULL && EQUALN( pszOption, "SAME", 4 ) )
    {
        // "SAME" will overwrite the existing gml file
        pszXlinkResolvedFilename = CPLStrdup( pszNewName );
    }
    else if( pszOption != NULL &&
             CPLStrnlen( pszOption, 5 ) >= 5 &&
             EQUALN( pszOption - 4 + strlen( pszOption ), ".gml", 4 ) )
    {
        // Any string ending with ".gml" will try and write to it
        pszXlinkResolvedFilename = CPLStrdup( pszOption );
    }
    else
    {
        // When no option is given or is not recognised,
        // use the same file name with the extension changed to .resolved.gml
        pszXlinkResolvedFilename = CPLStrdup(
                            CPLResetExtension( pszNewName, "resolved.gml" ) );

        // Check if the file already exists.
        VSIStatBufL sResStatBuf, sGMLStatBuf;
        if( VSIStatL( pszXlinkResolvedFilename, &sResStatBuf ) == 0 )
        {
            VSIStatL( pszNewName, &sGMLStatBuf );
            if( sGMLStatBuf.st_mtime > sResStatBuf.st_mtime )
            {
                CPLDebug( "GML", 
                          "Found %s but ignoring because it appears\n"
                          "be older than the associated GML file.", 
                          pszXlinkResolvedFilename );
            }
            else
            {
                poReader->SetSourceFile( pszXlinkResolvedFilename );
                bResolve = FALSE;
            }
        }
    }

    const char *pszSkipOption = CPLGetConfigOption( "GML_SKIP_RESOLVE_ELEMS",
                                                    "ALL");
    char **papszSkip = NULL;
    if( EQUAL( pszSkipOption, "ALL" ) )
        bResolve = FALSE;
    else if( !EQUAL( pszSkipOption, "NONE" ) )//use this to resolve everything
        papszSkip = CSLTokenizeString2( pszSkipOption, ",",
                                           CSLT_STRIPLEADSPACES |
                                           CSLT_STRIPENDSPACES );

    if( bResolve )
    {
        poReader->ResolveXlinks( pszXlinkResolvedFilename,
                                 &bOutIsTempFile,
                                 papszSkip );
    }

    CPLFree( pszXlinkResolvedFilename );
    CSLDestroy( papszSkip );

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Can we find a GML Feature Schema (.gfs) for the input file?     */
/* -------------------------------------------------------------------- */
    const char *pszGFSFilename;
    VSIStatBufL sGFSStatBuf, sGMLStatBuf;
    int        bHaveSchema = FALSE;

    pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
    if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) == 0 )
    {
        VSIStatL( pszNewName, &sGMLStatBuf );

        if( sGMLStatBuf.st_mtime > sGFSStatBuf.st_mtime )
        {
            CPLDebug( "GML", 
                      "Found %s but ignoring because it appears\n"
                      "be older than the associated GML file.", 
                      pszGFSFilename );
        }
        else
        {
            bHaveSchema = poReader->LoadClasses( pszGFSFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Can we find an xsd which might conform to tbe GML3 Level 0      */
/*      profile?  We really ought to look for it based on the rules     */
/*      schemaLocation in the GML feature collection but for now we     */
/*      just hopes it is in the same director with the same name.       */
/* -------------------------------------------------------------------- */
    const char *pszXSDFilename;

    if( !bHaveSchema )
    {
        pszXSDFilename = CPLResetExtension( pszNewName, "xsd" );
        if( VSIStatL( pszXSDFilename, &sGMLStatBuf ) == 0 )
        {
            bHaveSchema = poReader->ParseXSD( pszXSDFilename );
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Force a first pass to establish the schema.  Eventually we      */
/*      will have mechanisms for remembering the schema and related     */
/*      information.                                                    */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema && !poReader->PrescanForSchema( TRUE ) )
    {
        // we assume an errors have been reported.
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Save the schema file if possible.  Don't make a fuss if we      */
/*      can't ... could be read-only directory or something.            */
/* -------------------------------------------------------------------- */
    if( !bHaveSchema && !poReader->HasStoppedParsing() &&
        !EQUALN(pszNewName, "/vsitar/", strlen("/vsitar/")) &&
        !EQUALN(pszNewName, "/vsigzip/", strlen("/vsigzip/")) &&
        !EQUALN(pszNewName, "/vsizip/", strlen("/vsizip/")))
    {
        FILE    *fp = NULL;

        pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
        if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) != 0 
            && (fp = VSIFOpenL( pszGFSFilename, "wt" )) != NULL )
        {
            VSIFCloseL( fp );
            poReader->SaveClasses( pszGFSFilename );
        }
        else
        {
            CPLDebug("GML", 
                     "Not saving %s files already exists or can't be created.",
                     pszGFSFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the GMLFeatureClasses into layers.                    */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRGMLLayer **)
        CPLCalloc( sizeof(OGRGMLLayer *), poReader->GetClassCount());
    nLayers = 0;

    while( nLayers < poReader->GetClassCount() )
    {
        papoLayers[nLayers] = TranslateGMLSchema(poReader->GetClass(nLayers));
        nLayers++;
    }
    

    
    return TRUE;
}

/************************************************************************/
/*                         TranslateGMLSchema()                         */
/************************************************************************/

OGRGMLLayer *OGRGMLDataSource::TranslateGMLSchema( GMLFeatureClass *poClass )

{
    OGRGMLLayer *poLayer;
    OGRwkbGeometryType eGType 
        = (OGRwkbGeometryType) poClass->GetGeometryType();

    if( poClass->GetFeatureCount() == 0 )
        eGType = wkbUnknown;
    
/* -------------------------------------------------------------------- */
/*      Create an empty layer.                                          */
/* -------------------------------------------------------------------- */
    poLayer = new OGRGMLLayer( poClass->GetName(), NULL, FALSE, 
                               eGType, this );

/* -------------------------------------------------------------------- */
/*      Added attributes (properties).                                  */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < poClass->GetPropertyCount(); iField++ )
    {
        GMLPropertyDefn *poProperty = poClass->GetProperty( iField );
        OGRFieldType eFType;

        if( poProperty->GetType() == GMLPT_Untyped )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_String )
            eFType = OFTString;
        else if( poProperty->GetType() == GMLPT_Integer )
            eFType = OFTInteger;
        else if( poProperty->GetType() == GMLPT_Real )
            eFType = OFTReal;
        else if( poProperty->GetType() == GMLPT_StringList )
            eFType = OFTStringList;
        else if( poProperty->GetType() == GMLPT_IntegerList )
            eFType = OFTIntegerList;
        else if( poProperty->GetType() == GMLPT_RealList )
            eFType = OFTRealList;
        else
            eFType = OFTString;
        
        OGRFieldDefn oField( poProperty->GetName(), eFType );
        if ( EQUALN(oField.GetNameRef(), "ogr:", 4) )
          oField.SetName(poProperty->GetName()+4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth( poProperty->GetWidth() );
        if( poProperty->GetPrecision() > 0 )
            oField.SetPrecision( poProperty->GetPrecision() );

        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }

    return poLayer;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRGMLDataSource::Create( const char *pszFilename, 
                              char **papszOptions )

{
    if( fpOutput != NULL || poReader != NULL )
    {
        CPLAssert( FALSE );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    pszName = CPLStrdup( pszFilename );

    if( EQUAL(pszFilename,"stdout") || EQUAL(pszFilename,"/vsistdout/"))
    {
        fpOutput = VSIFOpenL("/vsistdout/", "wb");
        bFpOutputIsStdout = TRUE;
    }
    else
        fpOutput = VSIFOpenL( pszFilename, "wb+" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create GML file %s.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write out "standard" header.                                    */
/* -------------------------------------------------------------------- */
    PrintLine( fpOutput, "%s", 
                "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" );

    nSchemaInsertLocation = VSIFTellL( fpOutput );

    PrintLine( fpOutput, "%s", 
                "<ogr:FeatureCollection" );

/* -------------------------------------------------------------------- */
/*      Write out schema info if provided in creation options.          */
/* -------------------------------------------------------------------- */
    const char *pszSchemaURI = CSLFetchNameValue(papszOptions,"XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue( papszOptions, "XSISCHEMA" );

    if( pszSchemaURI != NULL )
    {
        PrintLine( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine( fpOutput, 
              "     xsi:schemaLocation=\"%s\"", 
                    CSLFetchNameValue( papszOptions, "XSISCHEMAURI" ) );
    }
    else if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        char *pszBasename = CPLStrdup(CPLGetBasename( pszName ));

        PrintLine( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"");
        PrintLine( fpOutput, 
              "     xsi:schemaLocation=\"http://ogr.maptools.org/ %s\"", 
                    CPLResetExtension( pszBasename, "xsd" ) );
        CPLFree( pszBasename );
    }

    PrintLine( fpOutput, "%s", 
                "     xmlns:ogr=\"http://ogr.maptools.org/\"" );
    PrintLine( fpOutput, "%s", 
                "     xmlns:gml=\"http://www.opengis.net/gml\">" );

/* -------------------------------------------------------------------- */
/*      Should we initialize an area to place the boundedBy element?    */
/*      We will need to seek back to fill it in.                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "BOUNDEDBY", TRUE ) )
    {
        nBoundedByLocation = VSIFTellL( fpOutput );

        if( nBoundedByLocation != -1 )
            PrintLine( fpOutput, "%280s", "" );
    }
    else
        nBoundedByLocation = -1;

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRGMLDataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened for read access.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Ensure name is safe as an element name.                         */
/* -------------------------------------------------------------------- */
    char *pszCleanLayerName = CPLStrdup( pszLayerName );

    CPLCleanXMLElementName( pszCleanLayerName );
    if( strcmp(pszCleanLayerName,pszLayerName) != 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer name '%s' adjusted to '%s' for XML validity.",
                  pszLayerName, pszCleanLayerName );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRGMLLayer *poLayer;

    poLayer = new OGRGMLLayer( pszCleanLayerName, poSRS, TRUE, eType, this );

    CPLFree( pszCleanLayerName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRGMLLayer **)
        CPLRealloc( papoLayers,  sizeof(OGRGMLLayer *) * (nLayers+1) );
    
    papoLayers[nLayers++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRGMLDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            GrowExtents()                             */
/************************************************************************/

void OGRGMLDataSource::GrowExtents( OGREnvelope *psGeomBounds )

{
    sBoundingRect.Merge( *psGeomBounds );
}

/************************************************************************/
/*                            InsertHeader()                            */
/*                                                                      */
/*      This method is used to update boundedby info for a              */
/*      dataset, and insert schema descriptions depending on            */
/*      selection options in effect.                                    */
/************************************************************************/

void OGRGMLDataSource::InsertHeader()

{
    FILE        *fpSchema;
    int         nSchemaStart = 0;

    if( fpOutput == NULL || bFpOutputIsStdout )
        return;

/* -------------------------------------------------------------------- */
/*      Do we want to write the schema within the GML instance doc      */
/*      or to a separate file?  For now we only support external.       */
/* -------------------------------------------------------------------- */
    const char *pszSchemaURI = CSLFetchNameValue(papszCreateOptions,
                                                 "XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue( papszCreateOptions, 
                                                  "XSISCHEMA" );

    if( pszSchemaURI != NULL )
        return;

    if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        const char *pszXSDFilename = CPLResetExtension( pszName, "xsd" );

        fpSchema = VSIFOpenL( pszXSDFilename, "wt" );
        if( fpSchema == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open file %.500s for schema output.", 
                      pszXSDFilename );
            return;
        }
        PrintLine( fpSchema, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" );
    }
    else if( EQUAL(pszSchemaOpt,"INTERNAL") )
    {
        nSchemaStart = VSIFTellL( fpOutput );
        fpSchema = fpOutput;
    }
    else                                                               
        return;

/* ==================================================================== */
/*      Write the schema section at the end of the file.  Once          */
/*      complete, we will read it back in, and then move the whole      */
/*      file "down" enough to insert the schema at the beginning.       */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Emit the start of the schema section.                           */
/* -------------------------------------------------------------------- */
    const char *pszTargetNameSpace = "http://ogr.maptools.org/";
    const char *pszPrefix = "ogr";

    PrintLine( fpSchema, 
                "<xs:schema targetNamespace=\"%s\" xmlns:%s=\"%s\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:gml=\"http://www.opengis.net/gml\" elementFormDefault=\"qualified\" version=\"1.0\">", 
                pszTargetNameSpace, pszPrefix, pszTargetNameSpace );
    
    PrintLine( fpSchema, 
                "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengis.net/gml/2.1.2/feature.xsd\"/>" );

/* -------------------------------------------------------------------- */
/*      Define the FeatureCollection                                    */
/* -------------------------------------------------------------------- */
    PrintLine( fpSchema, 
                "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>", 
                pszPrefix );

    PrintLine( fpSchema, "<xs:complexType name=\"FeatureCollectionType\">");
    PrintLine( fpSchema, "  <xs:complexContent>" );
    PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">" );
    PrintLine( fpSchema, "      <xs:attribute name=\"lockId\" type=\"xs:string\" use=\"optional\"/>" );
    PrintLine( fpSchema, "      <xs:attribute name=\"scope\" type=\"xs:string\" use=\"optional\"/>" );
    PrintLine( fpSchema, "    </xs:extension>" );
    PrintLine( fpSchema, "  </xs:complexContent>" );
    PrintLine( fpSchema, "</xs:complexType>" );

/* ==================================================================== */
/*      Define the schema for each layer.                               */
/* ==================================================================== */
    int iLayer;

    for( iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        OGRFeatureDefn *poFDefn = GetLayer(iLayer)->GetLayerDefn();
        
/* -------------------------------------------------------------------- */
/*      Emit initial stuff for a feature type.                          */
/* -------------------------------------------------------------------- */
        PrintLine(
            fpSchema,
            "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:_Feature\"/>",
            poFDefn->GetName(), pszPrefix, poFDefn->GetName() );

        PrintLine( fpSchema, "<xs:complexType name=\"%s_Type\">", poFDefn->GetName());
        PrintLine( fpSchema, "  <xs:complexContent>");
        PrintLine( fpSchema, "    <xs:extension base=\"gml:AbstractFeatureType\">");
        PrintLine( fpSchema, "      <xs:sequence>");

/* -------------------------------------------------------------------- */
/*      Define the geometry attribute.                                  */
/* -------------------------------------------------------------------- */
        const char* pszGeometryTypeName = "GeometryPropertyType";
        switch(wkbFlatten(poFDefn->GetGeomType()))
        {
            case wkbPoint:
                pszGeometryTypeName = "PointPropertyType";
                break;
            case wkbLineString:
                pszGeometryTypeName = "LineStringPropertyType";
                break;
            case wkbPolygon:
                pszGeometryTypeName = "PolygonPropertyType";
                break;
            case wkbMultiPoint:
                pszGeometryTypeName = "MultiPointPropertyType";
                break;
            case wkbMultiLineString:
                pszGeometryTypeName = "MultiLineStringPropertyType";
                break;
            case wkbMultiPolygon:
                pszGeometryTypeName = "MultiPolygonPropertyType";
                break;
            case wkbGeometryCollection:
                pszGeometryTypeName = "MultiGeometryPropertyType";
                break;
            default:
                break;
        }
        PrintLine( fpSchema,
            "        <xs:element name=\"geometryProperty\" type=\"gml:%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\"/>", pszGeometryTypeName );
            
/* -------------------------------------------------------------------- */
/*      Emit each of the attributes.                                    */
/* -------------------------------------------------------------------- */
        for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);

            if( poFieldDefn->GetType() == OFTInteger )
            {
                int nWidth;

                if( poFieldDefn->GetWidth() > 0 )
                    nWidth = poFieldDefn->GetWidth();
                else
                    nWidth = 16;

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">", poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:integer\">");
                PrintLine( fpSchema, "              <xs:totalDigits value=\"%d\"/>", nWidth);
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTReal )
            {
                int nWidth, nDecimals;

                nWidth = poFieldDefn->GetWidth();
                nDecimals = poFieldDefn->GetPrecision();

                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:decimal\">");
                if (nWidth > 0)
                {
                    PrintLine( fpSchema, "              <xs:totalDigits value=\"%d\"/>", nWidth);
                    PrintLine( fpSchema, "              <xs:fractionDigits value=\"%d\"/>", nDecimals);
                }
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTString )
            {
                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:string\">");
                if( poFieldDefn->GetWidth() != 0 )
                {
                    PrintLine( fpSchema, "              <xs:maxLength value=\"%d\"/>", poFieldDefn->GetWidth());
                }
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else if( poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime )
            {
                PrintLine( fpSchema, "        <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">",  poFieldDefn->GetNameRef());
                PrintLine( fpSchema, "          <xs:simpleType>");
                PrintLine( fpSchema, "            <xs:restriction base=\"xs:string\">");
                PrintLine( fpSchema, "            </xs:restriction>");
                PrintLine( fpSchema, "          </xs:simpleType>");
                PrintLine( fpSchema, "        </xs:element>");
            }
            else
            {
                /* TODO */
            }
        } /* next field */

/* -------------------------------------------------------------------- */
/*      Finish off feature type.                                        */
/* -------------------------------------------------------------------- */
        PrintLine( fpSchema, "      </xs:sequence>");
        PrintLine( fpSchema, "    </xs:extension>");
        PrintLine( fpSchema, "  </xs:complexContent>");
        PrintLine( fpSchema, "</xs:complexType>" );
    } /* next layer */

    PrintLine( fpSchema, "</xs:schema>" );

/* ==================================================================== */
/*      Move schema to the start of the file.                           */
/* ==================================================================== */
    if( bFpOutputIsStdout )
    {
/* -------------------------------------------------------------------- */
/*      Read the schema into memory.                                    */
/* -------------------------------------------------------------------- */
        int nSchemaSize = VSIFTellL( fpOutput ) - nSchemaStart;
        char *pszSchema = (char *) CPLMalloc(nSchemaSize+1);
    
        VSIFSeekL( fpOutput, nSchemaStart, SEEK_SET );

        VSIFReadL( pszSchema, 1, nSchemaSize, fpOutput );
        pszSchema[nSchemaSize] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Move file data down by "schema size" bytes from after <?xml>    */
/*      header so we have room insert the schema.  Move in pretty       */
/*      big chunks.                                                     */
/* -------------------------------------------------------------------- */
        int nChunkSize = MIN(nSchemaStart-nSchemaInsertLocation,250000);
        char *pszChunk = (char *) CPLMalloc(nChunkSize);
        int nEndOfUnmovedData = nSchemaStart;

        for( nEndOfUnmovedData = nSchemaStart;
             nEndOfUnmovedData > nSchemaInsertLocation; )
        {
            int nBytesToMove = 
                MIN(nChunkSize, nEndOfUnmovedData - nSchemaInsertLocation );

            VSIFSeekL( fpOutput, nEndOfUnmovedData - nBytesToMove, SEEK_SET );
            VSIFReadL( pszChunk, 1, nBytesToMove, fpOutput );
            VSIFSeekL( fpOutput, nEndOfUnmovedData - nBytesToMove + nSchemaSize, 
                      SEEK_SET );
            VSIFWriteL( pszChunk, 1, nBytesToMove, fpOutput );
        
            nEndOfUnmovedData -= nBytesToMove;
        }

        CPLFree( pszChunk );

/* -------------------------------------------------------------------- */
/*      Write the schema in the opened slot.                            */
/* -------------------------------------------------------------------- */
        VSIFSeekL( fpOutput, nSchemaInsertLocation, SEEK_SET );
        VSIFWriteL( pszSchema, 1, nSchemaSize, fpOutput );

        VSIFSeekL( fpOutput, 0, SEEK_END );

        nBoundedByLocation += nSchemaSize;
    }
/* -------------------------------------------------------------------- */
/*      Close external schema files.                                    */
/* -------------------------------------------------------------------- */
    else
        VSIFCloseL( fpSchema );
}


/************************************************************************/
/*                            PrintLine()                               */
/************************************************************************/

void OGRGMLDataSource::PrintLine(FILE* fp, const char *fmt, ...)
{
    CPLString osWork;
    va_list args;

    va_start( args, fmt );
    osWork.vPrintf( fmt, args );
    va_end( args );

#ifdef WIN32
    const char* pszEOL = "\r\n";
#else
    const char* pszEOL = "\n";
#endif

    VSIFPrintfL(fp, "%s%s", osWork.c_str(), pszEOL);
}
