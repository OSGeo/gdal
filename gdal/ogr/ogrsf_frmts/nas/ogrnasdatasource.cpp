/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_nas.h"

CPL_CVSID("$Id$")

static const char * const apszURNNames[] =
{
    "DE_DHDN_3GK2_*", "EPSG:31466",
    "DE_DHDN_3GK3_*", "EPSG:31467",
    "ETRS89_UTM32", "EPSG:25832",
    "ETRS89_UTM33", "EPSG:25833",
    nullptr, nullptr
};

/************************************************************************/
/*                         OGRNASDataSource()                           */
/************************************************************************/

OGRNASDataSource::OGRNASDataSource() :
    papoLayers(nullptr),
    nLayers(0),
    poRelationLayer(nullptr),
    pszName(nullptr),
    poReader(nullptr)
{}

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

int OGRNASDataSource::Open( const char * pszNewName )

{
    poReader = CreateNASReader();
    if( poReader == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be NAS but the NAS reader cannot\n"
                 "be instantiated, likely because Xerces support was not\n"
                 "configured in.",
                 pszNewName );
        return FALSE;
    }

    poReader->SetSourceFile( pszNewName );

    pszName = CPLStrdup( pszNewName );

    bool bHaveSchema = false;
    bool bHaveTemplate = false;
    const char *pszGFSFilename;
    VSIStatBufL sGFSStatBuf;

    // Is some NAS Feature Schema (.gfs) TEMPLATE required?
    const char *pszNASTemplateName = CPLGetConfigOption("NAS_GFS_TEMPLATE", nullptr);
    if( pszNASTemplateName != nullptr )
    {
        // Load the TEMPLATE.
        if( !poReader->LoadClasses(pszNASTemplateName) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "NAS schema %s could not be loaded\n",
                     pszNASTemplateName );
            return FALSE;
        }

        bHaveTemplate = true;

        CPLDebug("NAS", "Schema loaded.");
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Can we find a NAS Feature Schema (.gfs) for the input file?     */
        /* -------------------------------------------------------------------- */
        pszGFSFilename  = CPLResetExtension( pszNewName, "gfs" );
        if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) == 0 )
        {
            VSIStatBufL sNASStatBuf;
            if( VSIStatL( pszNewName, &sNASStatBuf ) == 0 &&
                sNASStatBuf.st_mtime > sGFSStatBuf.st_mtime )
            {
                 CPLDebug( "NAS",
                           "Found %s but ignoring because it appears "
                           "be older than the associated NAS file.",
                           pszGFSFilename );
            }
            else
            {
                bHaveSchema = poReader->LoadClasses( pszGFSFilename );
            }
        }

        if( !bHaveSchema )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No schema information loaded");
        }
    }

/* -------------------------------------------------------------------- */
/*      Force a first pass to establish the schema.  The loaded schema  */
/*      if any will be cleaned from any unavailable classes.            */
/* -------------------------------------------------------------------- */
    CPLErrorReset();
    if( !bHaveSchema
        && !poReader->PrescanForSchema( TRUE )
        && CPLGetLastErrorType() == CE_Failure )
    {
        // Assume an error has been reported.
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Save the schema file if possible.  Do not make a fuss if we     */
/*      cannot.  It could be read-only directory or something.          */
/* -------------------------------------------------------------------- */
    if( !bHaveTemplate && !bHaveSchema &&
        poReader->GetClassCount() > 0 &&
        !STARTS_WITH_CI(pszNewName, "/vsitar/") &&
        !STARTS_WITH_CI(pszNewName, "/vsizip/") &&
        !STARTS_WITH_CI(pszNewName, "/vsigzip/vsi") &&
        !STARTS_WITH_CI(pszNewName, "/vsigzip//vsi") &&
        !STARTS_WITH_CI(pszNewName, "/vsicurl/") &&
        !STARTS_WITH_CI(pszNewName, "/vsicurl_streaming/") )
    {
        VSILFILE *fp = nullptr;

        pszGFSFilename = CPLResetExtension( pszNewName, "gfs" );
        if( VSIStatL( pszGFSFilename, &sGFSStatBuf ) != 0
            && (fp = VSIFOpenL( pszGFSFilename, "wt" )) != nullptr )
        {
            VSIFCloseL( fp );
            poReader->SaveClasses( pszGFSFilename );
        }
        else
        {
            CPLDebug( "NAS",
                      "Not saving %s. File already exists or can't be created.",
                      pszGFSFilename );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the GMLFeatureClasses into layers.                    */
/* -------------------------------------------------------------------- */
    papoLayers = (OGRLayer **)
        CPLCalloc( sizeof(OGRNASLayer *), poReader->GetClassCount()+1 );
    nLayers = 0;

    while( nLayers < poReader->GetClassCount() )
    {
        papoLayers[nLayers] = TranslateNASSchema(poReader->GetClass(nLayers));
        nLayers++;
    }

    if( EQUAL( CPLGetConfigOption("NAS_NO_RELATION_LAYER", "NO"), "NO") || poReader->GetClassCount() == 0 )
    {
        poRelationLayer = new OGRNASRelationLayer( this );

        // keep delete the last layer
        if( nLayers>0 && EQUAL( papoLayers[nLayers-1]->GetName(), "Delete" ) )
        {
          papoLayers[nLayers]   = papoLayers[nLayers-1];
          papoLayers[nLayers-1] = poRelationLayer;
        }
        else
        {
          papoLayers[nLayers] = poRelationLayer;
        }

        nLayers++;
    }

    return TRUE;
}

/************************************************************************/
/*                         TranslateNASSchema()                         */
/************************************************************************/

OGRNASLayer *OGRNASDataSource::TranslateNASSchema( GMLFeatureClass *poClass )

{
/* -------------------------------------------------------------------- */
/*      Translate SRS.                                                  */
/* -------------------------------------------------------------------- */
    const char* pszSRSName = poClass->GetSRSName();
    OGRSpatialReference* poSRS = nullptr;
    if (pszSRSName)
    {
        const char *pszHandle = strrchr( pszSRSName, ':' );
        if( pszHandle != nullptr )
        {
            pszHandle += 1;

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            for( int i = 0; apszURNNames[i*2+0] != nullptr; i++ )
            {
                const char *pszTarget = apszURNNames[i*2+0];
                const int nTLen = static_cast<int>(strlen(pszTarget));

                // Are we just looking for a prefix match?
                if( pszTarget[nTLen-1] == '*' )
                {
                    if( EQUALN(pszTarget,pszHandle,nTLen-1) )
                        pszSRSName = apszURNNames[i*2+1];
                }
                else
                {
                    if( EQUAL(pszTarget,pszHandle) )
                        pszSRSName = apszURNNames[i*2+1];
                }
            }

            if (poSRS->SetFromUserInput(pszSRSName) != OGRERR_NONE)
            {
                CPLDebug( "NAS", "Failed to translate srsName='%s'",
                        pszSRSName );
                delete poSRS;
                poSRS = nullptr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create an empty layer.                                          */
/* -------------------------------------------------------------------- */
    OGRNASLayer *poLayer =
        new OGRNASLayer( poClass->GetName(), this );

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
        if ( STARTS_WITH_CI(oField.GetNameRef(), "ogr:") )
          oField.SetName(poProperty->GetName()+4);
        if( poProperty->GetWidth() > 0 )
            oField.SetWidth( poProperty->GetWidth() );

        poLayer->GetLayerDefn()->AddFieldDefn( &oField );
    }

    for (int iField = 0;
      iField < poClass->GetGeometryPropertyCount();
      iField++)
    {
        GMLGeometryPropertyDefn *poProperty =
                poClass->GetGeometryProperty(iField);
        OGRGeomFieldDefn oField(poProperty->GetName(),
                        (OGRwkbGeometryType)poProperty->GetType());
        if (poClass->GetGeometryPropertyCount() == 1 &&
            poClass->GetFeatureCount() == 0)
        {
                    oField.SetType(wkbUnknown);
        }

        oField.SetSpatialRef(poSRS);
        oField.SetNullable(poProperty->IsNullable());
        poLayer->GetLayerDefn()->AddGeomFieldDefn(&oField);
    }

    if( poSRS )
        poSRS->Dereference();

    return poLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRNASDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNASDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                         PopulateRelations()                          */
/************************************************************************/

void OGRNASDataSource::PopulateRelations()

{
    poReader->ResetReading();

    GMLFeature  *poFeature = nullptr;
    while( (poFeature = poReader->NextFeature()) != nullptr )
    {
        char **papszOBProperties = poFeature->GetOBProperties();

        for( int i = 0;
             papszOBProperties != nullptr && papszOBProperties[i] != nullptr;
             i++ )
        {
            const int nGMLIdIndex =
                poFeature->GetClass()->GetPropertyIndex( "gml_id" );
            const GMLProperty *psGMLId =
              (nGMLIdIndex >= 0) ? poFeature->GetProperty(nGMLIdIndex ) : nullptr;
            char *l_pszName = nullptr;
            const char *pszValue = CPLParseNameValue( papszOBProperties[i],
                                                      &l_pszName );

            if( STARTS_WITH_CI(pszValue, "urn:adv:oid:")
                && psGMLId != nullptr && psGMLId->nSubProperties == 1 )
            {
                poRelationLayer->AddRelation( psGMLId->papszSubProperties[0],
                                              l_pszName,
                                              pszValue + 12 );
            }
            CPLFree( l_pszName );
        }

        delete poFeature;
    }

    poRelationLayer->MarkRelationsPopulated();
}
