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

    papszCreateOptions = NULL;
}

/************************************************************************/
/*                        ~OGRGMLDataSource()                         */
/************************************************************************/

OGRGMLDataSource::~OGRGMLDataSource()

{

    if( fpOutput != NULL )
    {
        VSIFPrintf( fpOutput, "%s", 
                    "</ogr:FeatureCollection>\n" );

        InsertHeader();

        if( nBoundedByLocation != -1 
            && sBoundingRect.IsInit() 
            && VSIFSeek( fpOutput, nBoundedByLocation, SEEK_SET ) == 0 )
        {
            VSIFPrintf( fpOutput, "  <gml:boundedBy>\n" );
            VSIFPrintf( fpOutput, "    <gml:Box>\n" );
            VSIFPrintf( fpOutput, 
                        "      <gml:coord><gml:X>%.16g</gml:X>"
                        "<gml:Y>%.16g</gml:Y></gml:coord>\n",
                        sBoundingRect.MinX, sBoundingRect.MinY );
            VSIFPrintf( fpOutput, 
                        "      <gml:coord><gml:X>%.16g</gml:X>"
                        "<gml:Y>%.16g</gml:Y></gml:coord>\n",
                        sBoundingRect.MaxX, sBoundingRect.MaxY );
            VSIFPrintf( fpOutput, "    </gml:Box>\n" );
            VSIFPrintf( fpOutput, "  </gml:boundedBy>" );
        }

        if( fpOutput != stdout )
            VSIFClose( fpOutput );
    }

    CSLDestroy( papszCreateOptions );
    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    if( poReader )
        delete poReader;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRGMLDataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[1000];

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszNewName, "r" );
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
        size_t nRead = VSIFRead( szHeader, 1, sizeof(szHeader), fp );
        if (nRead <= 0)
        {
            VSIFClose( fp );
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
            VSIFClose( fp );
            return FALSE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      We assume now that it is GML.  Close and instantiate a          */
/*      GMLReader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );
    
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
    if( pszOption != NULL )
    {
        if( EQUALN( pszOption, "SAME", 4 ) )
        {
            // "SAME" will overwrite the existing gml file
            pszXlinkResolvedFilename = CPLStrdup( pszNewName );
        }
        else if( ( CPLStrnlen( pszOption, 5 ) >= 5 ) &&
                 EQUALN( pszOption - 4 + strlen( pszOption ), ".gml", 4 ) )
        {
            // Any string ending with ".gml" will try and write to it
            pszXlinkResolvedFilename = CPLStrdup( pszNewName );
        }
    }
    else
    {
        // Default action would be to use a file with the extension
        // changed to resolved.gml
        pszXlinkResolvedFilename = CPLStrdup(
                            CPLResetExtension( pszNewName, "resolved.gml" ) );

        // Check if the file already exists.
        VSIStatBuf sResStatBuf, sGMLStatBuf;
        if( CPLStat( pszXlinkResolvedFilename, &sResStatBuf ) == 0 )
        {
            CPLStat( pszNewName, &sGMLStatBuf );
            if( sGMLStatBuf.st_mtime > sResStatBuf.st_mtime )
            {
                CPLDebug( "GML", 
                          "Found %s but ignoring because it appears\n"
                          "be older than the associated GML file.", 
                          pszXlinkResolvedFilename );
                bResolve = FALSE;
            }
        }
    }

    const char *pszSkipOption = CPLGetConfigOption( "GML_SKIP_RESOLVE_ELEMS",
                                                    "");
    char **papszSkip = NULL;
    if( EQUAL( pszSkipOption, "ALL" ) )
        bResolve = FALSE;
    else
        papszSkip = CSLTokenizeString2( pszSkipOption, ",",
                                           CSLT_STRIPLEADSPACES |
                                           CSLT_STRIPENDSPACES );

    if( bResolve )
        poReader->ResolveXlinks( pszXlinkResolvedFilename, papszSkip );

    CPLFree( pszXlinkResolvedFilename );
    CSLDestroy( papszSkip );

    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Can we find a GML Feature Schema (.gfs) for the input file?     */
/* -------------------------------------------------------------------- */
    const char *pszGFSFilename;
    VSIStatBuf sGFSStatBuf, sGMLStatBuf;
    int        bHaveSchema = FALSE;

    pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
    if( CPLStat( pszGFSFilename, &sGFSStatBuf ) == 0 )
    {
        CPLStat( pszNewName, &sGMLStatBuf );

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
        if( CPLStat( pszXSDFilename, &sGMLStatBuf ) == 0 )
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
    if( !bHaveSchema && !poReader->HasStoppedParsing())
    {
        FILE    *fp = NULL;

        pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
        if( CPLStat( pszGFSFilename, &sGFSStatBuf ) != 0 
            && (fp = VSIFOpen( pszGFSFilename, "wt" )) != NULL )
        {
            VSIFClose( fp );
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

/* -------------------------------------------------------------------- */
/*      Create an empty layer.                                          */
/* -------------------------------------------------------------------- */
    poLayer = new OGRGMLLayer( poClass->GetName(), NULL, FALSE, 
                               wkbUnknown, this );

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

    if( EQUAL(pszFilename,"stdout") )
        fpOutput = stdout;
    else
        fpOutput = VSIFOpen( pszFilename, "wt+" );
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
    VSIFPrintf( fpOutput, "%s", 
                "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" );

    nSchemaInsertLocation = VSIFTell( fpOutput );

    VSIFPrintf( fpOutput, "%s", 
                "<ogr:FeatureCollection\n" );

/* -------------------------------------------------------------------- */
/*      Write out schema info if provided in creation options.          */
/* -------------------------------------------------------------------- */
    const char *pszSchemaURI = CSLFetchNameValue(papszOptions,"XSISCHEMAURI");
    const char *pszSchemaOpt = CSLFetchNameValue( papszOptions, "XSISCHEMA" );

    if( pszSchemaURI != NULL )
    {
        VSIFPrintf( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
              "     xsi:schemaLocation=\"%s\"\n", 
                    CSLFetchNameValue( papszOptions, "XSISCHEMAURI" ) );
    }
    else if( pszSchemaOpt == NULL || EQUAL(pszSchemaOpt,"EXTERNAL") )
    {
        char *pszBasename = CPLStrdup(CPLGetBasename( pszName ));

        VSIFPrintf( fpOutput, 
              "     xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
              "     xsi:schemaLocation=\"http://ogr.maptools.org/ %s\"\n", 
                    CPLResetExtension( pszBasename, "xsd" ) );
        CPLFree( pszBasename );
    }

    VSIFPrintf( fpOutput, "%s", 
                "     xmlns:ogr=\"http://ogr.maptools.org/\"\n" );
    VSIFPrintf( fpOutput, "%s", 
                "     xmlns:gml=\"http://www.opengis.net/gml\">\n" );

/* -------------------------------------------------------------------- */
/*      Should we initialize an area to place the boundedBy element?    */
/*      We will need to seek back to fill it in.                        */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszOptions, "BOUNDEDBY", TRUE ) )
    {
        nBoundedByLocation = VSIFTell( fpOutput );

        if( nBoundedByLocation != -1 )
            VSIFPrintf( fpOutput, "%280s\n", "" );
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

    if( fpOutput == NULL || fpOutput == stdout )
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

        fpSchema = VSIFOpen( pszXSDFilename, "wt" );
        if( fpSchema == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open file %.500s for schema output.", 
                      pszXSDFilename );
            return;
        }
        fprintf( fpSchema, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    }
    else if( EQUAL(pszSchemaOpt,"INTERNAL") )
    {
        nSchemaStart = VSIFTell( fpOutput );
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

    VSIFPrintf( fpSchema, 
                "<xs:schema targetNamespace=\"%s\" xmlns:%s=\"%s\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" xmlns:gml=\"http://www.opengis.net/gml\" elementFormDefault=\"qualified\" version=\"1.0\">\n", 
                pszTargetNameSpace, pszPrefix, pszTargetNameSpace );
    
    VSIFPrintf( fpSchema, 
                "<xs:import namespace=\"http://www.opengis.net/gml\" schemaLocation=\"http://schemas.opengeospatial.net/gml/2.1.2/feature.xsd\"/>" );

/* -------------------------------------------------------------------- */
/*      Define the FeatureCollection                                    */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpSchema, 
                "<xs:element name=\"FeatureCollection\" type=\"%s:FeatureCollectionType\" substitutionGroup=\"gml:_FeatureCollection\"/>\n", 
                pszPrefix );

    VSIFPrintf( 
        fpSchema, 
        "<xs:complexType name=\"FeatureCollectionType\">\n"
        "  <xs:complexContent>\n"
        "    <xs:extension base=\"gml:AbstractFeatureCollectionType\">\n"
        "      <xs:attribute name=\"lockId\" type=\"xs:string\" use=\"optional\"/>\n"
        "      <xs:attribute name=\"scope\" type=\"xs:string\" use=\"optional\"/>\n"
        "    </xs:extension>\n"
        "  </xs:complexContent>\n"
        "</xs:complexType>\n" );

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
        VSIFPrintf( 
            fpSchema,
            "<xs:element name=\"%s\" type=\"%s:%s_Type\" substitutionGroup=\"gml:_Feature\"/>\n",
            poFDefn->GetName(), pszPrefix, poFDefn->GetName() );

        VSIFPrintf( 
            fpSchema, 
            "<xs:complexType name=\"%s_Type\">\n"
            "  <xs:complexContent>\n"
            "    <xs:extension base=\"gml:AbstractFeatureType\">\n"
            "      <xs:sequence>\n",
            poFDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Define the geometry attribute.  For now I always use the        */
/*      generic geometry type, but eventually we should express         */
/*      particulars if available.                                       */
/* -------------------------------------------------------------------- */
        VSIFPrintf( 
            fpSchema,
            "<xs:element name=\"geometryProperty\" type=\"gml:GeometryPropertyType\" nillable=\"true\" minOccurs=\"1\" maxOccurs=\"1\"/>\n" );
            
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

                VSIFPrintf( fpSchema, 
                            "    <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">\n"
                            "      <xs:simpleType>\n"
                            "        <xs:restriction base=\"xs:integer\">\n"
                            "          <xs:totalDigits value=\"%d\"/>\n"
                            "        </xs:restriction>\n"
                            "      </xs:simpleType>\n"
                            "    </xs:element>\n",
                            poFieldDefn->GetNameRef(), nWidth );
            }
            else if( poFieldDefn->GetType() == OFTReal )
            {
                int nWidth, nDecimals;

                if( poFieldDefn->GetPrecision() == 0 )
                    nDecimals = 0;
                else
                    nDecimals = poFieldDefn->GetPrecision();

                if( poFieldDefn->GetWidth() > 0 )
                    nWidth = poFieldDefn->GetWidth();
                else
                    nWidth = 33;

                VSIFPrintf( fpSchema, 
                            "    <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">\n"
                            "      <xs:simpleType>\n"
                            "        <xs:restriction base=\"xs:decimal\">\n"
                            "          <xs:totalDigits value=\"%d\"/>\n"
                            "          <xs:fractionDigits value=\"%d\"/>\n"
                            "        </xs:restriction>\n"
                            "      </xs:simpleType>\n"
                            "    </xs:element>\n",
                            poFieldDefn->GetNameRef(), nWidth, nDecimals );
            }
            else if( poFieldDefn->GetType() == OFTString )
            {
                char szMaxLength[48];

                if( poFieldDefn->GetWidth() == 0 )
                    sprintf( szMaxLength, "unbounded" );
                else
                    sprintf( szMaxLength, "%d", poFieldDefn->GetWidth() );

                VSIFPrintf( fpSchema, 
                            "    <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">\n"
                            "      <xs:simpleType>\n"
                            "        <xs:restriction base=\"xs:string\">\n"
                            "          <xs:maxLength value=\"%s\"/>\n"
                            "        </xs:restriction>\n"
                            "      </xs:simpleType>\n"
                            "    </xs:element>\n",
                            poFieldDefn->GetNameRef(), szMaxLength );
            }
            else if( poFieldDefn->GetType() == OFTDate || poFieldDefn->GetType() == OFTDateTime )
            {
                VSIFPrintf( fpSchema, 
                            "    <xs:element name=\"%s\" nillable=\"true\" minOccurs=\"0\" maxOccurs=\"1\">\n"
                            "      <xs:simpleType>\n"
                            "        <xs:restriction base=\"xs:string\">\n"
                            "          <xs:maxLength value=\"unbounded\"/>\n"
                            "        </xs:restriction>\n"
                            "      </xs:simpleType>\n"
                            "    </xs:element>\n",
                            poFieldDefn->GetNameRef() );
            }
            else
            {
                /* TODO */
            }
        } /* next field */

/* -------------------------------------------------------------------- */
/*      Finish off feature type.                                        */
/* -------------------------------------------------------------------- */
        VSIFPrintf( fpSchema, 
                    "      </xs:sequence>\n"
                    "    </xs:extension>\n"
                    "  </xs:complexContent>\n"
                    "</xs:complexType>\n" );
    } /* next layer */

    VSIFPrintf( fpSchema, "</xs:schema>\n" );

/* ==================================================================== */
/*      Move schema to the start of the file.                           */
/* ==================================================================== */
    if( fpSchema == fpOutput )
    {
/* -------------------------------------------------------------------- */
/*      Read the schema into memory.                                    */
/* -------------------------------------------------------------------- */
        int nSchemaSize = VSIFTell( fpOutput ) - nSchemaStart;
        char *pszSchema = (char *) CPLMalloc(nSchemaSize+1);
    
        VSIFSeek( fpOutput, nSchemaStart, SEEK_SET );

        VSIFRead( pszSchema, 1, nSchemaSize, fpOutput );
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

            VSIFSeek( fpOutput, nEndOfUnmovedData - nBytesToMove, SEEK_SET );
            VSIFRead( pszChunk, 1, nBytesToMove, fpOutput );
            VSIFSeek( fpOutput, nEndOfUnmovedData - nBytesToMove + nSchemaSize, 
                      SEEK_SET );
            VSIFWrite( pszChunk, 1, nBytesToMove, fpOutput );
        
            nEndOfUnmovedData -= nBytesToMove;
        }

        CPLFree( pszChunk );

/* -------------------------------------------------------------------- */
/*      Write the schema in the opened slot.                            */
/* -------------------------------------------------------------------- */
        VSIFSeek( fpOutput, nSchemaInsertLocation, SEEK_SET );
        VSIFWrite( pszSchema, 1, nSchemaSize, fpOutput );

        VSIFSeek( fpOutput, 0, SEEK_END );

        nBoundedByLocation += nSchemaSize;
    }
/* -------------------------------------------------------------------- */
/*      Close external schema files.                                    */
/* -------------------------------------------------------------------- */
    else
        VSIFClose( fpSchema );
}
