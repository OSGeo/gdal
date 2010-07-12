/******************************************************************************
 * $Id: ogrgmldatasource.cpp 12743 2007-11-13 13:59:37Z dron $
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASDataSource class.
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

#include "ogr_nas.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrgmldatasource.cpp 12743 2007-11-13 13:59:37Z dron $");

/************************************************************************/
/*                         OGRNASDataSource()                         */
/************************************************************************/

OGRNASDataSource::OGRNASDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;

    poReader = NULL;
}

/************************************************************************/
/*                        ~OGRNASDataSource()                         */
/************************************************************************/

OGRNASDataSource::~OGRNASDataSource()

{
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

int OGRNASDataSource::Open( const char * pszNewName, int bTestOpen )

{
    FILE        *fp;
    char        szHeader[8192];

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszNewName, "r" );
    if( fp == NULL )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open NAS file `%s'.", 
                      pszNewName );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is NAS, load a header chunk and check      */
/*      for signs it is NAS                                             */
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
/*      Here, we expect the opening chevrons of NAS tree root element   */
/* -------------------------------------------------------------------- */
        if( szPtr[0] != '<' 
            || strstr(szPtr,"opengis.net/gml") == NULL 
            || strstr(szPtr,"NAS") == NULL )
        {
            VSIFClose( fp );
            return FALSE;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      We assume now that it is NAS.  Close and instantiate a          */
/*      NASReader on it.                                                */
/* -------------------------------------------------------------------- */
    VSIFClose( fp );
    
    poReader = CreateNASReader();
    if( poReader == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s appears to be NAS but the NAS reader can't\n"
                  "be instantiated, likely because Xerces support wasn't\n"
                  "configured in.", 
                  pszNewName );
        return FALSE;
    }

    poReader->SetSourceFile( pszNewName );
    
    pszName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Can we find a NAS Feature Schema (.gfs) for the input file?     */
/* -------------------------------------------------------------------- */
    const char *pszGFSFilename;
    VSIStatBuf sGFSStatBuf, sNASStatBuf;
    int        bHaveSchema = FALSE;

    pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
    if( CPLStat( pszGFSFilename, &sGFSStatBuf ) == 0 )
    {
        CPLStat( pszNewName, &sNASStatBuf );

        if( sNASStatBuf.st_mtime > sGFSStatBuf.st_mtime )
        {
            CPLDebug( "NAS", 
                      "Found %s but ignoring because it appears\n"
                      "be older than the associated NAS file.", 
                      pszGFSFilename );
        }
        else
        {
            bHaveSchema = poReader->LoadClasses( pszGFSFilename );
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
    if( !bHaveSchema )
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
            CPLDebug("NAS", 
                     "Not saving %s files already exists or can't be created.",
                     pszGFSFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the NASFeatureClasses into layers.                    */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRLayer **)
        CPLCalloc( sizeof(OGRNASLayer *), poReader->GetClassCount()+1 );
    nLayers = 0;

    while( nLayers < poReader->GetClassCount() )
    {
        papoLayers[nLayers] = TranslateNASSchema(poReader->GetClass(nLayers));
        nLayers++;
    }
    
    poRelationLayer = new OGRNASRelationLayer( this );
    papoLayers[nLayers++] = poRelationLayer;
    
    return TRUE;
}

/************************************************************************/
/*                         TranslateNASSchema()                         */
/************************************************************************/

OGRNASLayer *OGRNASDataSource::TranslateNASSchema( GMLFeatureClass *poClass )

{
    OGRNASLayer *poLayer;
    OGRwkbGeometryType eGType 
        = (OGRwkbGeometryType) poClass->GetGeometryType();

    if( poClass->GetFeatureCount() == 0 )
        eGType = wkbUnknown;

/* -------------------------------------------------------------------- */
/*      Create an empty layer.                                          */
/* -------------------------------------------------------------------- */
    poLayer = new OGRNASLayer( poClass->GetName(), NULL, eGType, this );

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

        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }

    return poLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRNASDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                         PopulateRelations()                          */
/************************************************************************/

void OGRNASDataSource::PopulateRelations()

{
    GMLFeature  *poFeature;

    poReader->ResetReading();
    while( (poFeature = poReader->NextFeature()) != NULL )
    {
        char **papszOBProperties = poFeature->GetOBProperties();
        int i;

        for( i = 0; papszOBProperties != NULL && papszOBProperties[i] != NULL;
             i++ )
        {
            const GMLProperty *psGMLId = poFeature->GetProperty( "gml_id" );
            char *pszName = NULL;
            const char *pszValue = CPLParseNameValue( papszOBProperties[i], 
                                                      &pszName );

            if( EQUALN(pszValue,"urn:adv:oid:",12) 
                && psGMLId != NULL && psGMLId->nSubProperties == 1 )
            {
                poRelationLayer->AddRelation( psGMLId->papszSubProperties[0],
                                              pszName, 
                                              pszValue + 12 );
            }
            CPLFree( pszName );
        }
        
        delete poFeature;
    }

    poRelationLayer->MarkRelationsPopulated();
}
