/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLDataSource class.
 * Author:   Christopher Condit, condit@sdsc.edu;
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 *               2007, Jens Oberender
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_port.h"
#include "ogr_kml.h"

#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "kml.h"
#include "kmlutility.h"
#include "kmlvector.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRKMLDataSource()                           */
/************************************************************************/

OGRKMLDataSource::OGRKMLDataSource() :
#ifdef HAVE_EXPAT
    poKMLFile_(nullptr),
#endif
    pszName_(nullptr),
    papoLayers_(nullptr),
    nLayers_(0),
    pszNameField_(nullptr),
    pszDescriptionField_(nullptr),
    pszAltitudeMode_(nullptr),
    papszCreateOptions_(nullptr),
    fpOutput_(nullptr),
    bIssuedCTError_(false)
{
}

/************************************************************************/
/*                        ~OGRKMLDataSource()                           */
/************************************************************************/

OGRKMLDataSource::~OGRKMLDataSource()
{
    if( fpOutput_ != nullptr )
    {
        if( nLayers_ > 0 )
        {
            if( nLayers_ == 1 && papoLayers_[0]->nWroteFeatureCount_ == 0 )
            {
                VSIFPrintfL( fpOutput_, "<Folder><name>%s</name>\n",
                             papoLayers_[0]->GetName() );
            }

            VSIFPrintfL( fpOutput_, "%s", "</Folder>\n");

            for( int i = 0; i < nLayers_; i++ )
            {
                if( !(papoLayers_[i]->bSchemaWritten_) &&
                    papoLayers_[i]->nWroteFeatureCount_ != 0 )
                {
                    CPLString osRet = papoLayers_[i]->WriteSchema();
                    if( !osRet.empty() )
                        VSIFPrintfL( fpOutput_, "%s", osRet.c_str() );
                }
            }
        }
        VSIFPrintfL( fpOutput_, "%s", "</Document></kml>\n" );

        VSIFCloseL( fpOutput_ );
    }

    CSLDestroy( papszCreateOptions_ );
    CPLFree( pszName_ );
    CPLFree( pszNameField_ );
    CPLFree( pszDescriptionField_ );
    CPLFree( pszAltitudeMode_ );

    for( int i = 0; i < nLayers_; i++ )
    {
        delete papoLayers_[i];
    }

    CPLFree( papoLayers_ );

#ifdef HAVE_EXPAT
    delete poKMLFile_;
#endif
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

#ifdef HAVE_EXPAT
int OGRKMLDataSource::Open( const char * pszNewName, int bTestOpen )
{
    CPLAssert( nullptr != pszNewName );

/* -------------------------------------------------------------------- */
/*      Create a KML object and open the source file.                   */
/* -------------------------------------------------------------------- */
    poKMLFile_ = new KMLVector();

    if( !poKMLFile_->open( pszNewName ) )
    {
        delete poKMLFile_;
        poKMLFile_ = nullptr;
        return FALSE;
    }

    pszName_ = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is KML, validate it by start parsing       */
/* -------------------------------------------------------------------- */
    if( bTestOpen && !poKMLFile_->isValid() )
    {
        delete poKMLFile_;
        poKMLFile_ = nullptr;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Prescan the KML file so we can later work with the structure    */
/* -------------------------------------------------------------------- */
    if( !poKMLFile_->parse() )
    {
        delete poKMLFile_;
        poKMLFile_ = nullptr;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Classify the nodes                                              */
/* -------------------------------------------------------------------- */
    if( !poKMLFile_->classifyNodes() )
    {
        delete poKMLFile_;
        poKMLFile_ = nullptr;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Eliminate the empty containers (if there is at least one        */
/*      valid container !)                                              */
/* -------------------------------------------------------------------- */
    const bool bHasOnlyEmpty = poKMLFile_->hasOnlyEmpty();
    if( bHasOnlyEmpty )
        CPLDebug("KML", "Has only empty containers");
    else
        poKMLFile_->eliminateEmpty();

/* -------------------------------------------------------------------- */
/*      Find layers to use in the KML structure                         */
/* -------------------------------------------------------------------- */
    poKMLFile_->findLayers(nullptr, bHasOnlyEmpty);

/* -------------------------------------------------------------------- */
/*      Print the structure                                             */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption("KML_DEBUG",nullptr) != nullptr )
        poKMLFile_->print(3);

    const int nLayers = poKMLFile_->getNumLayers();

/* -------------------------------------------------------------------- */
/*      Allocate memory for the Layers                                  */
/* -------------------------------------------------------------------- */
    papoLayers_ = static_cast<OGRKMLLayer **>(
        CPLMalloc( sizeof(OGRKMLLayer *) * nLayers ));

    OGRSpatialReference *poSRS =
        new OGRSpatialReference( SRS_WKT_WGS84_LAT_LONG );
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

/* -------------------------------------------------------------------- */
/*      Create the Layers and fill them                                 */
/* -------------------------------------------------------------------- */
    for( int nCount = 0; nCount < nLayers; nCount++ )
    {
        if( !poKMLFile_->selectLayer(nCount) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "There are no layers or a layer can not be found!");
            break;
        }

        OGRwkbGeometryType poGeotype = wkbUnknown;
        if( poKMLFile_->getCurrentType() == Point )
            poGeotype = wkbPoint;
        else if( poKMLFile_->getCurrentType() == LineString )
            poGeotype = wkbLineString;
        else if( poKMLFile_->getCurrentType() == Polygon )
            poGeotype = wkbPolygon;
        else if( poKMLFile_->getCurrentType() == MultiPoint )
            poGeotype = wkbMultiPoint;
        else if( poKMLFile_->getCurrentType() == MultiLineString )
            poGeotype = wkbMultiLineString;
        else if( poKMLFile_->getCurrentType() == MultiPolygon )
            poGeotype = wkbMultiPolygon;
        else if( poKMLFile_->getCurrentType() == MultiGeometry )
            poGeotype = wkbGeometryCollection;

        if (poGeotype != wkbUnknown && poKMLFile_->is25D())
            poGeotype = wkbSetZ(poGeotype);

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
        CPLString sName( poKMLFile_->getCurrentName() );

        if( sName.empty() )
        {
            sName.Printf( "Layer #%d", nCount );
        }
        else
        {
            // Build unique layer name
            int nIter = 2;
            while( true )
            {
                if( GetLayerByName(sName) == nullptr )
                    break;
                sName = CPLSPrintf("%s (#%d)",
                                   poKMLFile_->getCurrentName().c_str(),
                                   nIter);
                nIter ++;
            }
        }

        OGRKMLLayer *poLayer =
            new OGRKMLLayer( sName.c_str(), poSRS, false, poGeotype, this );

        poLayer->SetLayerNumber( nCount );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
        papoLayers_[nCount] = poLayer;

        nLayers_ = nCount + 1;
    }

    poSRS->Release();

    return TRUE;
}
#endif /* HAVE_EXPAT */

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRKMLDataSource::Create( const char* pszName, char** papszOptions )
{
    CPLAssert( nullptr != pszName );

    if( fpOutput_ != nullptr )
    {
        CPLAssert( false );
        return FALSE;
    }

    if( CSLFetchNameValue(papszOptions, "NameField") )
        pszNameField_ = CPLStrdup(CSLFetchNameValue(papszOptions, "NameField"));
    else
        pszNameField_ = CPLStrdup("Name");

    if( CSLFetchNameValue(papszOptions, "DescriptionField") )
        pszDescriptionField_ =
            CPLStrdup(CSLFetchNameValue(papszOptions, "DescriptionField"));
    else
        pszDescriptionField_ = CPLStrdup("Description");

    pszAltitudeMode_ = CPLStrdup(CSLFetchNameValue(papszOptions, "AltitudeMode"));
    if( (nullptr != pszAltitudeMode_) && strlen(pszAltitudeMode_) > 0 )
    {
        //Check to see that the specified AltitudeMode is valid
        if ( EQUAL(pszAltitudeMode_, "clampToGround")
             || EQUAL(pszAltitudeMode_, "relativeToGround")
             || EQUAL(pszAltitudeMode_, "absolute") )
        {
            CPLDebug("KML", "Using '%s' for AltitudeMode", pszAltitudeMode_);
        }
        else
        {
            CPLFree( pszAltitudeMode_ );
            pszAltitudeMode_ = nullptr;
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Invalid AltitudeMode specified, ignoring" );
        }
    }
    else
    {
        CPLFree( pszAltitudeMode_ );
        pszAltitudeMode_ = nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */

    if( strcmp(pszName, "/dev/stdout") == 0 )
        pszName = "/vsistdout/";

    pszName_ = CPLStrdup( pszName );

    fpOutput_ = VSIFOpenExL( pszName, "wb", true );
    if( fpOutput_ == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create KML file %s: %s", pszName,
                  VSIGetLastErrorMsg() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write out "standard" header.                                    */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fpOutput_, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" );

    VSIFPrintfL( fpOutput_,
                 "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
                 "<Document id=\"%s\">\n",
                 CSLFetchNameValueDef(papszOptions, "DOCUMENT_ID", "root_doc") );

    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRKMLDataSource::ICreateLayer( const char * pszLayerName,
                                OGRSpatialReference *poSRS,
                                OGRwkbGeometryType eType,
                                char ** /* papszOptions */ )
{
    CPLAssert( nullptr != pszLayerName);

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( fpOutput_ == nullptr )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened for read access.  "
                  "New layer %s cannot be created.",
                  pszName_, pszLayerName );

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Close the previous layer (if there is one open)                 */
/* -------------------------------------------------------------------- */
    if( GetLayerCount() > 0 )
    {
        if( nLayers_ == 1 && papoLayers_[0]->nWroteFeatureCount_ == 0 )
        {
            VSIFPrintfL( fpOutput_, "<Folder><name>%s</name>\n",
                         papoLayers_[0]->GetName() );
        }

        VSIFPrintfL( fpOutput_, "</Folder>\n" );
        papoLayers_[GetLayerCount()-1]->SetClosedForWriting();
    }

/* -------------------------------------------------------------------- */
/*      Ensure name is safe as an element name.                         */
/* -------------------------------------------------------------------- */
    char *pszCleanLayerName = CPLStrdup( pszLayerName );

    CPLCleanXMLElementName( pszCleanLayerName );
    if( strcmp(pszCleanLayerName, pszLayerName) != 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Layer name '%s' adjusted to '%s' for XML validity.",
                  pszLayerName, pszCleanLayerName );
    }

    if( GetLayerCount() > 0 )
    {
        VSIFPrintfL( fpOutput_, "<Folder><name>%s</name>\n",
                     pszCleanLayerName );
    }

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRKMLLayer *poLayer =
        new OGRKMLLayer( pszCleanLayerName, poSRS, true, eType, this );

    CPLFree( pszCleanLayerName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers_ = static_cast<OGRKMLLayer **>(
        CPLRealloc( papoLayers_,  sizeof(OGRKMLLayer *) * (nLayers_+1) ) );

    papoLayers_[nLayers_++] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRKMLDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRKMLDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers_ )
        return nullptr;

    return papoLayers_[iLayer];
}

/************************************************************************/
/*                            GrowExtents()                             */
/************************************************************************/

void OGRKMLDataSource::GrowExtents( OGREnvelope *psGeomBounds )
{
    CPLAssert( nullptr != psGeomBounds );

    oEnvelope_.Merge( *psGeomBounds );
}
