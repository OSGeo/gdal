/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLDataSource class.
 * Author:   Christopher Condit, condit@sdsc.edu;
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 *               2007, Jens Oberender
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
#include "ogr_kml.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_error.h"
#include "cpl_minixml.h"

/************************************************************************/
/*                         OGRKMLDataSource()                           */
/************************************************************************/

OGRKMLDataSource::OGRKMLDataSource()
{
    pszName_ = NULL;
    pszNameField_ = NULL;
    pszDescriptionField_ = NULL;
    pszAltitudeMode_ = NULL;
    papoLayers_ = NULL;
    nLayers_ = 0;
    
    fpOutput_ = NULL;

    papszCreateOptions_ = NULL;
	
	bIssuedCTError_ = false;

#ifdef HAVE_EXPAT
    poKMLFile_ = NULL;
#endif
}

/************************************************************************/
/*                        ~OGRKMLDataSource()                           */
/************************************************************************/

OGRKMLDataSource::~OGRKMLDataSource()
{
    if( fpOutput_ != NULL )
    {
        VSIFPrintfL( fpOutput_, "%s", "</Folder>\n");
        for( int i = 0; i < nLayers_; i++ )
        {
            papoLayers_[i]->WriteSchema();
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
    CPLAssert( NULL != pszNewName );

    int nCount = 0;
    OGRKMLLayer *poLayer = NULL;
    OGRwkbGeometryType poGeotype;

/* -------------------------------------------------------------------- */
/*      Create a KML object and open the source file.                   */
/* -------------------------------------------------------------------- */
    poKMLFile_ = new KMLVector();

    if( !poKMLFile_->open( pszNewName ) )
    {
        delete poKMLFile_;
        poKMLFile_ = NULL;
        return FALSE;
    }

    pszName_ = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is KML, validate it by start parsing       */
/* -------------------------------------------------------------------- */
    if( bTestOpen && !poKMLFile_->isValid() )
    {
        delete poKMLFile_;
        poKMLFile_ = NULL;
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Prescan the KML file so we can later work with the structure    */
/* -------------------------------------------------------------------- */
    poKMLFile_->parse();

/* -------------------------------------------------------------------- */
/*      Classify the nodes                                              */
/* -------------------------------------------------------------------- */
    if( !poKMLFile_->classifyNodes() )
    {
        delete poKMLFile_;
        poKMLFile_ = NULL;
        return FALSE;
    }


/* -------------------------------------------------------------------- */
/*      Eliminate the empty containers (if there is at least one        */
/*      valid container !)                                              */
/* -------------------------------------------------------------------- */
    int bHasOnlyEmpty = poKMLFile_->hasOnlyEmpty();
    if (bHasOnlyEmpty)
        CPLDebug("KML", "Has only empty containers");
    else
        poKMLFile_->eliminateEmpty();

/* -------------------------------------------------------------------- */
/*      Find layers to use in the KML structure                         */
/* -------------------------------------------------------------------- */
    poKMLFile_->findLayers(NULL, bHasOnlyEmpty);

/* -------------------------------------------------------------------- */
/*      Print the structure                                             */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption("KML_DEBUG",NULL) != NULL )
        poKMLFile_->print(3);

    nLayers_ = poKMLFile_->getNumLayers();

/* -------------------------------------------------------------------- */
/*      Allocate memory for the Layers                                  */
/* -------------------------------------------------------------------- */
    papoLayers_ = (OGRKMLLayer **)
        CPLMalloc( sizeof(OGRKMLLayer *) * nLayers_ );

    OGRSpatialReference *poSRS = new OGRSpatialReference("GEOGCS[\"WGS 84\", "
        "   DATUM[\"WGS_1984\","
        "       SPHEROID[\"WGS 84\",6378137,298.257223563,"
        "           AUTHORITY[\"EPSG\",\"7030\"]],"
        "           AUTHORITY[\"EPSG\",\"6326\"]],"
        "       PRIMEM[\"Greenwich\",0,"
        "           AUTHORITY[\"EPSG\",\"8901\"]],"
        "       UNIT[\"degree\",0.01745329251994328,"
        "           AUTHORITY[\"EPSG\",\"9122\"]],"
        "           AUTHORITY[\"EPSG\",\"4326\"]]");

/* -------------------------------------------------------------------- */
/*      Create the Layers and fill them                                 */
/* -------------------------------------------------------------------- */
    for( nCount = 0; nCount < nLayers_; nCount++ )
    {
        if( !poKMLFile_->selectLayer(nCount) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "There are no layers or a layer can not be found!");
            break;
        }

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
        else
            poGeotype = wkbUnknown;
        
        if (poGeotype != wkbUnknown && poKMLFile_->is25D())
            poGeotype = (OGRwkbGeometryType) (poGeotype | wkb25DBit);

/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
        CPLString sName( poKMLFile_->getCurrentName() );

        if( sName.empty() )
        {
            sName.Printf( "Layer #%d", nCount );
        }

        poLayer = new OGRKMLLayer( sName.c_str(), poSRS, FALSE, poGeotype, this );

        poLayer->SetLayerNumber( nCount );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
        papoLayers_[nCount] = poLayer;
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
    CPLAssert( NULL != pszName );

    if( fpOutput_ != NULL )
    {
        CPLAssert( FALSE );
        return FALSE;
    }

    if (CSLFetchNameValue(papszOptions, "NameField"))
        pszNameField_ = CPLStrdup(CSLFetchNameValue(papszOptions, "NameField"));
    else
        pszNameField_ = CPLStrdup("Name");
    
    if (CSLFetchNameValue(papszOptions, "DescriptionField"))
        pszDescriptionField_ = CPLStrdup(CSLFetchNameValue(papszOptions, "DescriptionField"));
    else
        pszDescriptionField_ = CPLStrdup("Description");

    pszAltitudeMode_ = CPLStrdup(CSLFetchNameValue(papszOptions, "AltitudeMode")); 
    if( (NULL != pszAltitudeMode_) && strlen(pszAltitudeMode_) > 0) 
    {
        //Check to see that the specified AltitudeMode is valid 
        if ( EQUAL(pszAltitudeMode_, "clampToGround")
             || EQUAL(pszAltitudeMode_, "relativeToGround")
             || EQUAL(pszAltitudeMode_, "absolute")) 
        { 
            CPLDebug("KML", "Using '%s' for AltitudeMode", pszAltitudeMode_); 
        } 
        else 
        { 
            CPLFree( pszAltitudeMode_ ); 
            pszAltitudeMode_ = NULL; 
            CPLError( CE_Warning, CPLE_AppDefined, "Invalide AltitideMode specified, ignoring" );  
        } 
    } 
    else 
    { 
        CPLFree( pszAltitudeMode_ ); 
        pszAltitudeMode_ = NULL; 
    } 
    
/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */

    if (strcmp(pszName, "/dev/stdout") == 0)
        pszName = "/vsistdout/";

    pszName_ = CPLStrdup( pszName );

    fpOutput_ = VSIFOpenL( pszName, "wb" );
    if( fpOutput_ == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create KML file %s.", pszName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Write out "standard" header.                                    */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fpOutput_, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" );	
    
    VSIFPrintfL( fpOutput_, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n<Document>" );

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRKMLDataSource::CreateLayer( const char * pszLayerName,
                               OGRSpatialReference *poSRS,
                               OGRwkbGeometryType eType,
                               char ** papszOptions )
{
    CPLAssert( NULL != pszLayerName);

/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( fpOutput_ == NULL )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened for read access.\n"
                  "New layer %s cannot be created.\n",
                  pszName_, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Close the previous layer (if there is one open)                 */
/* -------------------------------------------------------------------- */
    if (GetLayerCount() > 0)
    {
        VSIFPrintfL( fpOutput_, "</Folder>\n");
        ((OGRKMLLayer*)GetLayer(GetLayerCount()-1))->SetClosedForWriting();
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
    VSIFPrintfL( fpOutput_, "<Folder><name>%s</name>\n", pszCleanLayerName);
    
/* -------------------------------------------------------------------- */
/*      Create the layer object.                                        */
/* -------------------------------------------------------------------- */
    OGRKMLLayer *poLayer;
    poLayer = new OGRKMLLayer( pszCleanLayerName, poSRS, TRUE, eType, this );
    
    CPLFree( pszCleanLayerName );

/* -------------------------------------------------------------------- */
/*      Add layer to data source layer list.                            */
/* -------------------------------------------------------------------- */
    papoLayers_ = (OGRKMLLayer **)
        CPLRealloc( papoLayers_,  sizeof(OGRKMLLayer *) * (nLayers_+1) );
    
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
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRKMLDataSource::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers_ )
        return NULL;
    else
        return papoLayers_[iLayer];
}

/************************************************************************/
/*                            GrowExtents()                             */
/************************************************************************/

void OGRKMLDataSource::GrowExtents( OGREnvelope *psGeomBounds )
{
    CPLAssert( NULL != psGeomBounds );

    oEnvelope_.Merge( *psGeomBounds );
}
