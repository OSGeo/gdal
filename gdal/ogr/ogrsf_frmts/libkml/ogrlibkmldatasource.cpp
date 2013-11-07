/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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
 *****************************************************************************/

//#include "cpl_conv.h"
//#include "cpl_string.h"
//#include "cpl_error.h"
#include <iostream>
//#include <sstream>
#include <kml/dom.h>
#include <kml/base/file.h>

using kmldom::KmlFactory;
using kmldom::DocumentPtr;
using kmldom::FolderPtr;
using kmldom::FeaturePtr;
using kmldom::NetworkLinkPtr;
using kmldom::StyleSelectorPtr;
using kmldom::LinkPtr;
using kmldom::SchemaPtr;
using kmlbase::File;
using kmldom::KmlPtr;
using kmlbase::Attributes;

#include "ogr_libkml.h"
#include "ogrlibkmlstyle.h"

/***** this was shamelessly swiped from the kml driver *****/

#define OGRLIBKMLSRSWKT "GEOGCS[\"WGS 84\", "\
                        "   DATUM[\"WGS_1984\","\
                        "     SPHEROID[\"WGS 84\",6378137,298.257223563,"\
                        "           AUTHORITY[\"EPSG\",\"7030\"]],"\
                        "           AUTHORITY[\"EPSG\",\"6326\"]],"\
                        "       PRIMEM[\"Greenwich\",0,"\
                        "           AUTHORITY[\"EPSG\",\"8901\"]],"\
                        "       UNIT[\"degree\",0.01745329251994328,"\
                        "           AUTHORITY[\"EPSG\",\"9122\"]],"\
                        "           AUTHORITY[\"EPSG\",\"4326\"]]"

/******************************************************************************
 OGRLIBKMLDataSource Constructor

 Args:          none
 
 Returns:       nothing
                
******************************************************************************/

OGRLIBKMLDataSource::OGRLIBKMLDataSource ( KmlFactory * poKmlFactory )
{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    nAlloced = 0;

    bUpdated = FALSE;

    m_isKml = FALSE;
    m_poKmlDSKml = NULL;
    m_poKmlDSContainer = NULL;

    m_isKmz = FALSE;
    m_poKmlDocKml = NULL;
    pszStylePath = (char *) "";

    m_isDir = FALSE;
    
    m_poKmlFactory = poKmlFactory;

    //m_poStyleTable = NULL;

}

/******************************************************************************
 method to write a single file ds .kml at ds destroy

 Args:          none
 
 Returns:       nothing
                
******************************************************************************/

void OGRLIBKMLDataSource::WriteKml (
     )
{
    std::string oKmlFilename = pszName;

    if ( m_poKmlDSContainer
         && m_poKmlDSContainer->IsA ( kmldom::Type_Document ) ) {
        DocumentPtr poKmlDocument = AsDocument ( m_poKmlDSContainer );
        int iLayer;

        for ( iLayer = 0; iLayer < nLayers; iLayer++ ) {
            SchemaPtr poKmlSchema;
            SchemaPtr poKmlSchema2;

            if ( ( poKmlSchema = papoLayers[iLayer]->GetKmlSchema (  ) ) ) {
                size_t nKmlSchemas = poKmlDocument->get_schema_array_size (  );
                size_t iKmlSchema;

                for ( iKmlSchema = 0; iKmlSchema < nKmlSchemas; iKmlSchema++ ) {
                    poKmlSchema2 =
                        poKmlDocument->get_schema_array_at ( iKmlSchema );
                    if ( poKmlSchema2 == poKmlSchema )
                        break;
                }

                if ( poKmlSchema2 != poKmlSchema )
                    poKmlDocument->add_schema ( poKmlSchema );
            }
        }
    }

    std::string oKmlOut;
    if ( m_poKmlDSKml ) {
        oKmlOut = kmldom::SerializePretty ( m_poKmlDSKml );
    }
    else if ( m_poKmlDSContainer ) {
        oKmlOut = kmldom::SerializePretty ( m_poKmlDSContainer );
    }

    if (oKmlOut.size() != 0)
    {
        VSILFILE* fp = VSIFOpenL( oKmlFilename.c_str(), "wb" );
        if (fp == NULL)
        {
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR writing %s", oKmlFilename.c_str (  ) );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

    return;
}

/******************************************************************************/
/*                      OGRLIBKMLCreateOGCKml22()                             */
/******************************************************************************/

static KmlPtr OGRLIBKMLCreateOGCKml22(KmlFactory* poFactory)
{
    KmlPtr kml = poFactory->CreateKml (  );
    const char* kAttrs[] = { "xmlns", "http://www.opengis.net/kml/2.2", NULL };
    kml->AddUnknownAttributes(Attributes::Create(kAttrs));
    return kml;
}

/******************************************************************************
 method to write a ds .kmz at ds destroy

 Args:          none
 
 Returns:       nothing
                
******************************************************************************/

void OGRLIBKMLDataSource::WriteKmz (
     )
{

    void* hZIP = CPLCreateZip( pszName, NULL );

    if ( !hZIP ) {
        CPLError ( CE_Failure, CPLE_NoWriteAccess, "ERROR creating %s",
                   pszName );
        return;
    }

    /***** write out the doc.kml ****/

    const char *pszUseDocKml =
        CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( EQUAL ( pszUseDocKml, "yes" ) && m_poKmlDocKml ) {

        /***** if we dont have the doc.kml root *****/
        /***** make it and add the container    *****/
        
        if ( !m_poKmlDocKmlRoot ) {
            m_poKmlDocKmlRoot = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            AsKml( m_poKmlDocKmlRoot )->set_feature ( m_poKmlDocKml );
        }
        
        std::string oKmlOut = kmldom::SerializePretty ( m_poKmlDocKmlRoot );


        if ( CPLCreateFileInZip( hZIP, "doc.kml", NULL ) != CE_None ||
             CPLWriteFileInZip( hZIP, oKmlOut.data(), oKmlOut.size() ) != CE_None )
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR adding %s to %s", "doc.kml", pszName );
        CPLCloseFileInZip(hZIP);

    }

    /***** loop though the layers and write them *****/

    int iLayer;

    for ( iLayer = 0; iLayer < nLayers; iLayer++ ) {
        ContainerPtr poKlmContainer = papoLayers[iLayer]->GetKmlLayer (  );

        if ( poKlmContainer->IsA ( kmldom::Type_Document ) ) {

            DocumentPtr poKmlDocument = AsDocument ( poKlmContainer );
            SchemaPtr poKmlSchema = papoLayers[iLayer]->GetKmlSchema (  );

            if ( !poKmlDocument->get_schema_array_size (  ) &&
                 poKmlSchema &&
                 poKmlSchema->get_simplefield_array_size (  ) ) {
                poKmlDocument->add_schema ( poKmlSchema );
            }
        }

        /***** if we dont have the layers root *****/
        /***** make it and add the container    *****/

        KmlPtr poKmlKml = NULL;

        if ( !( poKmlKml = AsKml( papoLayers[iLayer]->GetKmlLayerRoot (  ) ) ) ) {

            poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            poKmlKml->set_feature ( poKlmContainer );
        }

        std::string oKmlOut = kmldom::SerializePretty ( poKmlKml );

        if ( CPLCreateFileInZip( hZIP, papoLayers[iLayer]->GetFileName (  ), NULL ) != CE_None ||
             CPLWriteFileInZip( hZIP, oKmlOut.data(), oKmlOut.size() ) != CE_None )
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR adding %s to %s", papoLayers[iLayer]->GetFileName (  ), pszName );
        CPLCloseFileInZip(hZIP);

    }

   /***** write the style table *****/

    if ( m_poKmlStyleKml ) {

        KmlPtr poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

        poKmlKml->set_feature ( m_poKmlStyleKml );
        std::string oKmlOut = kmldom::SerializePretty ( poKmlKml );

        if ( CPLCreateFileInZip( hZIP, "style/style.kml", NULL ) != CE_None ||
             CPLWriteFileInZip( hZIP, oKmlOut.data(), oKmlOut.size() ) != CE_None )
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR adding %s to %s", "style/style.kml", pszName );
        CPLCloseFileInZip(hZIP);
    }

    CPLCloseZip(hZIP);

    return;
}

/******************************************************************************
 method to write a dir ds at ds destroy

 Args:          none
 
 Returns:       nothing
                
******************************************************************************/

void OGRLIBKMLDataSource::WriteDir (
     )
{

    /***** write out the doc.kml ****/

    const char *pszUseDocKml =
        CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( EQUAL ( pszUseDocKml, "yes" ) && m_poKmlDocKml ) {

        /***** if we dont have the doc.kml root *****/
        /***** make it and add the container    *****/
        
        if ( !m_poKmlDocKmlRoot ) {
            m_poKmlDocKmlRoot = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            AsKml( m_poKmlDocKmlRoot )->set_feature ( m_poKmlDocKml );
        }
        
        std::string oKmlOut = kmldom::SerializePretty ( m_poKmlDocKmlRoot );

        const char *pszOutfile = CPLFormFilename ( pszName, "doc.kml", NULL );

        VSILFILE* fp = VSIFOpenL( pszOutfile, "wb" );
        if (fp == NULL)
        {
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR Writing %s to %s", "doc.kml", pszName );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

    /***** loop though the layers and write them *****/

    int iLayer;

    for ( iLayer = 0; iLayer < nLayers; iLayer++ ) {
        ContainerPtr poKmlContainer = papoLayers[iLayer]->GetKmlLayer (  );

        if ( poKmlContainer->IsA ( kmldom::Type_Document ) ) {

            DocumentPtr poKmlDocument = AsDocument ( poKmlContainer );
            SchemaPtr poKmlSchema = papoLayers[iLayer]->GetKmlSchema (  );

            if ( !poKmlDocument->get_schema_array_size (  ) &&
                 poKmlSchema &&
                 poKmlSchema->get_simplefield_array_size (  ) ) {
                poKmlDocument->add_schema ( poKmlSchema );
            };
        }

        /***** if we dont have the layers root *****/
        /***** make it and add the container    *****/

        KmlPtr poKmlKml = NULL;

        if ( !( poKmlKml = AsKml( papoLayers[iLayer]->GetKmlLayerRoot (  ) ) ) ) {

            poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

            poKmlKml->set_feature ( poKmlContainer );
        }

        std::string oKmlOut = kmldom::SerializePretty ( poKmlKml );

        const char *pszOutfile = CPLFormFilename ( pszName,
                                                   papoLayers[iLayer]->
                                                   GetFileName (  ),
                                                   NULL );

        VSILFILE* fp = VSIFOpenL( pszOutfile, "wb" );
        if (fp == NULL)
        {
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR Writing %s to %s",
                       papoLayers[iLayer]->GetFileName (  ), pszName );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

   /***** write the style table *****/

    if ( m_poKmlStyleKml ) {

        KmlPtr poKmlKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);

        poKmlKml->set_feature ( m_poKmlStyleKml );
        std::string oKmlOut = kmldom::SerializePretty ( poKmlKml );

        const char *pszOutfile = CPLFormFilename ( pszName,
                                                   "style.kml",
                                                   NULL );

        VSILFILE* fp = VSIFOpenL( pszOutfile, "wb" );
        if (fp == NULL)
        {
            CPLError ( CE_Failure, CPLE_FileIO,
                       "ERROR Writing %s to %s", "style.kml", pszName );
            return;
        }

        VSIFWriteL(oKmlOut.data(), 1, oKmlOut.size(), fp);
        VSIFCloseL(fp);
    }

    return;
}

/******************************************************************************
 method to write the datasource to disk

 Args:      none

 Returns    nothing

******************************************************************************/

OGRErr OGRLIBKMLDataSource::SyncToDisk (
     )
{

    if ( bUpdated ) {

        /***** kml *****/

        if ( bUpdate && IsKml (  ) )
            WriteKml (  );

        /***** kmz *****/

        else if ( bUpdate && IsKmz (  ) ) {
            WriteKmz (  );
        }

        else if ( bUpdate && IsDir (  ) ) {
            WriteDir (  );
        }

        bUpdated = FALSE;
    }

    return OGRERR_NONE;
}

/******************************************************************************
 OGRLIBKMLDataSource Destructor
 
 Args:          none
 
 Returns:       nothing
                
******************************************************************************/

OGRLIBKMLDataSource::~OGRLIBKMLDataSource (  )
{


    /***** sync the DS to disk *****/

    SyncToDisk (  );

    CPLFree ( pszName );

    if (! EQUAL(pszStylePath, ""))
        CPLFree ( pszStylePath );
    
    for ( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    CPLFree ( papoLayers );

    //delete m_poStyleTable;

}


/******************************************************************************
 method to parse a schemas out of a document

 Args:          poKmlDocument   pointer to the document to parse
 
 Returns:       nothing
                
******************************************************************************/

SchemaPtr OGRLIBKMLDataSource::FindSchema (
    const char *pszSchemaUrl )
{
    char *pszID = NULL;
    char *pszFile = NULL;
    char *pszSchemaName = NULL;
    char *pszPound;
    DocumentPtr poKmlDocument = NULL;
    SchemaPtr poKmlSchemaResult = NULL;

    if ( !pszSchemaUrl || !*pszSchemaUrl )
        return NULL;

    if ( *pszSchemaUrl == '#' ) {
        pszID = CPLStrdup ( pszSchemaUrl + 1 );

        /***** kml *****/

        if ( IsKml (  ) && m_poKmlDSContainer->IsA ( kmldom::Type_Document ) )
            poKmlDocument = AsDocument ( m_poKmlDSContainer );

        /***** kmz *****/

        else if ( ( IsKmz (  ) || IsDir (  ) ) && m_poKmlDocKml
                  && m_poKmlDocKml->IsA ( kmldom::Type_Document ) )
            poKmlDocument = AsDocument ( m_poKmlDocKml );

    }


    else if ( ( pszPound = strchr ( (char *)pszSchemaUrl, '#' ) ) ) {
        pszFile = CPLStrdup ( pszSchemaUrl );
        pszID = CPLStrdup ( pszPound + 1 );
        pszPound = strchr ( pszFile, '#' );
        *pszPound = '\0';
    }

    else {
        pszSchemaName = CPLStrdup ( pszSchemaUrl );

        /***** kml *****/

        if ( IsKml (  ) && m_poKmlDSContainer->IsA ( kmldom::Type_Document ) )
            poKmlDocument = AsDocument ( m_poKmlDSContainer );

        /***** kmz *****/

        else if ( ( IsKmz (  ) || IsDir (  ) ) && m_poKmlDocKml
                  && m_poKmlDocKml->IsA ( kmldom::Type_Document ) )
            poKmlDocument = AsDocument ( m_poKmlDocKml );

    }
    

    if ( poKmlDocument) {

        size_t nKmlSchemas = poKmlDocument->get_schema_array_size (  );
        size_t iKmlSchema;

        for ( iKmlSchema = 0; iKmlSchema < nKmlSchemas; iKmlSchema++ ) {
            SchemaPtr poKmlSchema =
                poKmlDocument->get_schema_array_at ( iKmlSchema );
            if ( poKmlSchema->has_id (  ) && pszID) {
                if ( EQUAL ( pszID, poKmlSchema->get_id (  ).c_str (  ) ) ) {
                    poKmlSchemaResult = poKmlSchema;
                    break;
                }
            }

            else if ( poKmlSchema->has_name (  ) && pszSchemaName) {
                if ( EQUAL ( pszSchemaName, poKmlSchema->get_name (  ).c_str (  ) ) ) {
                    poKmlSchemaResult = poKmlSchema;
                    break;
                }
            }

        }
    }

    if ( pszFile )
        CPLFree ( pszFile );
    if ( pszID )
        CPLFree ( pszID );
    if ( pszSchemaName )
        CPLFree ( pszSchemaName );

    return poKmlSchemaResult;

}

/******************************************************************************
Method to allocate memory for the layer array, create the layer,
 and add it to the layer array

 Args:          pszLayerName    the name of the layer
                poSpatialRef    the spacial Refrance for the layer
                eGType          the layers geometry type
                poOgrDS         pointer to the datasource the layer is in
                poKmlRoot       pointer to the root kml element of the layer
                pszFileName     the filename of the layer
                bNew            true if its a new layer
                bUpdate         true if the layer is writeable
                nGuess          a guess at the number of additional layers
                                we are going to need
 
 Returns:       Pointer to the new layer
******************************************************************************/

OGRLIBKMLLayer *OGRLIBKMLDataSource::AddLayer (
    const char *pszLayerName,
    OGRSpatialReference * poSpatialRef,
    OGRwkbGeometryType eGType,
    OGRLIBKMLDataSource * poOgrDS,
    ElementPtr poKmlRoot,
    ContainerPtr poKmlContainer,
    const char *pszFileName,
    int bNew,
    int bUpdate,
    int nGuess )
{

    /***** check to see if we have enough space to store the layer *****/

    if ( nLayers == nAlloced ) {
        nAlloced += nGuess;
        void *tmp = CPLRealloc ( papoLayers,
                                 sizeof ( OGRLIBKMLLayer * ) * nAlloced );

        papoLayers = ( OGRLIBKMLLayer ** ) tmp;
    }

    /***** create the layer *****/

    int iLayer = nLayers++;

    OGRLIBKMLLayer *poOgrLayer = new OGRLIBKMLLayer ( pszLayerName,
                                                      poSpatialRef,
                                                      eGType,
                                                      poOgrDS,
                                                      poKmlRoot,
                                                      poKmlContainer,
                                                      pszFileName,
                                                      bNew,
                                                      bUpdate );

    /***** add the layer to the array *****/

    papoLayers[iLayer] = poOgrLayer;

    return poOgrLayer;
}

/******************************************************************************
 method to parse multiple layers out of a container

 Args:          poKmlContainer  pointer to the container to parse
                poOgrSRS        SRS to use when creating the layer
 
 Returns:       number of features in the container that are not another
                container
                
******************************************************************************/

int OGRLIBKMLDataSource::ParseLayers (
    ContainerPtr poKmlContainer,
    OGRSpatialReference * poOgrSRS )
{
    int nResult = 0;

    /***** if container is null just bail now *****/

    if ( !poKmlContainer )
        return nResult;

    size_t nKmlFeatures = poKmlContainer->get_feature_array_size (  );

    /***** loop over the container to seperate the style, layers, etc *****/

    size_t iKmlFeature;

    for ( iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ ) {
        FeaturePtr poKmlFeat =
            poKmlContainer->get_feature_array_at ( iKmlFeature );

        /***** container *****/

        if ( poKmlFeat->IsA ( kmldom::Type_Container ) ) {

            /***** see if the container has a name *****/

            std::string oKmlFeatName;
            if ( poKmlFeat->has_name (  ) ) {
                /* Strip leading and trailing spaces */
                const char* pszName = poKmlFeat->get_name (  ).c_str();
                while(*pszName == ' ' || *pszName == '\n' || *pszName == '\r' || *pszName == '\t' )
                    pszName ++;
                oKmlFeatName = pszName;
                int nSize = (int)oKmlFeatName.size();
                while (nSize > 0 &&
                       (oKmlFeatName[nSize-1] == ' ' || oKmlFeatName[nSize-1] == '\n' ||
                        oKmlFeatName[nSize-1] == '\r' || oKmlFeatName[nSize-1] == '\t'))
                {
                    nSize --;
                    oKmlFeatName.resize(nSize);
                }
            }

            /***** use the feature index number as the name *****/
            /***** not sure i like this c++ ich *****/

            else {
                std::stringstream oOut;
                oOut << iKmlFeature;
                oKmlFeatName = "Layer";
                oKmlFeatName.append(oOut.str (  ));
            }

            /***** create the layer *****/

            AddLayer ( oKmlFeatName.c_str (  ),
                       poOgrSRS, wkbUnknown, this,
                       NULL, AsContainer( poKmlFeat ), "", FALSE, bUpdate, nKmlFeatures );

        }

        else
            nResult++;
    }

    return nResult;
}

/******************************************************************************
 function to get the container from the kmlroot
 
 Args:          poKmlRoot   the root element
 
 Returns:       root if its a container, if its a kml the container it
                contains, or NULL

******************************************************************************/

static ContainerPtr GetContainerFromRoot (
    KmlFactory *m_poKmlFactory, ElementPtr poKmlRoot )
{
    ContainerPtr poKmlContainer = NULL;

    int bReadGroundOverlay = CSLTestBoolean(CPLGetConfigOption("LIBKML_READ_GROUND_OVERLAY", "YES"));

    if ( poKmlRoot ) {

        /***** skip over the <kml> we want the container *****/

        if ( poKmlRoot->IsA ( kmldom::Type_kml ) ) {

            KmlPtr poKmlKml = AsKml ( poKmlRoot );

            if ( poKmlKml->has_feature (  ) ) {
                FeaturePtr poKmlFeat = poKmlKml->get_feature (  );

                if ( poKmlFeat->IsA ( kmldom::Type_Container ) )
                    poKmlContainer = AsContainer ( poKmlFeat );
                else if ( poKmlFeat->IsA ( kmldom::Type_Placemark ) ||
                          (bReadGroundOverlay && poKmlFeat->IsA ( kmldom::Type_GroundOverlay )) ) 
                {
                    poKmlContainer = m_poKmlFactory->CreateDocument (  );
                    poKmlContainer->add_feature ( kmldom::AsFeature(kmlengine::Clone(poKmlFeat)) );
                }
            }

        }

        else if ( poKmlRoot->IsA ( kmldom::Type_Container ) )
            poKmlContainer = AsContainer ( poKmlRoot );
    }

    return poKmlContainer;
}

/******************************************************************************
 method to parse a kml string into the style table
******************************************************************************/

int OGRLIBKMLDataSource::ParseIntoStyleTable (
    std::string *poKmlStyleKml,
    const char *pszMyStylePath)
{
    
    /***** parse the kml into the dom *****/

    std::string oKmlErrors;
    ElementPtr poKmlRoot = kmldom::Parse ( *poKmlStyleKml, &oKmlErrors );

    if ( !poKmlRoot ) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "ERROR Parseing style kml %s :%s",
                   pszStylePath, oKmlErrors.c_str (  ) );
        return false;
    }
    
    ContainerPtr poKmlContainer;

    if ( !( poKmlContainer = GetContainerFromRoot ( m_poKmlFactory, poKmlRoot ) ) ) {
        return false;
    }
    
    ParseStyles ( AsDocument ( poKmlContainer ), &m_poStyleTable );
    pszStylePath = CPLStrdup(pszMyStylePath);
    
        
    return true;
}

/******************************************************************************
 method to open a kml file
 
 Args:          pszFilename file to open
                bUpdate     update mode
 
 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::OpenKml (
    const char *pszFilename,
    int bUpdate )
{
    std::string oKmlKml;
    char szBuffer[1024+1];

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
    {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "Cannot open %s", pszFilename );
        return FALSE;
    }
    int nRead;
    while ((nRead = VSIFReadL(szBuffer, 1, 1024, fp)) != 0)
    {
        try
        {
            oKmlKml.append(szBuffer, nRead);
        }
        catch(std::bad_alloc& e)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }
    VSIFCloseL(fp);

    CPLLocaleC  oLocaleForcer;

    /***** create a SRS *****/

    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference ( OGRLIBKMLSRSWKT );

    /***** parse the kml into the DOM *****/

    std::string oKmlErrors;

    ElementPtr poKmlRoot = kmldom::Parse ( oKmlKml, &oKmlErrors );

    if ( !poKmlRoot ) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "ERROR Parseing kml %s :%s",
                   pszFilename, oKmlErrors.c_str (  ) );
        delete poOgrSRS;

        return FALSE;
    }

    /***** get the container from root  *****/

    if ( !( m_poKmlDSContainer = GetContainerFromRoot ( m_poKmlFactory, poKmlRoot ) ) ) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "ERROR Parseing kml %s :%s %s",
                   pszFilename, "This file does not fit the OGR model,",
                   "there is no container element at the root." );
        delete poOgrSRS;

        return FALSE;
    }

    m_isKml = TRUE;

    /***** get the styles *****/

    ParseStyles ( AsDocument ( m_poKmlDSContainer ), &m_poStyleTable );

    /***** parse for layers *****/

    int nPlacemarks = ParseLayers ( m_poKmlDSContainer, poOgrSRS );

    /***** if there is placemarks in the root its a layer *****/

    if ( nPlacemarks && !nLayers ) {
        AddLayer ( CPLGetBasename ( pszFilename ),
                   poOgrSRS, wkbUnknown,
                   this, poKmlRoot, m_poKmlDSContainer, pszFilename, FALSE, bUpdate, 1 );
    }

    delete poOgrSRS;

    return TRUE;
}

/******************************************************************************
 method to open a kmz file
 
 Args:          pszFilename file to open
                bUpdate     update mode
 
 Returns:       True on success, false on failure

******************************************************************************/


int OGRLIBKMLDataSource::OpenKmz (
    const char *pszFilename,
    int bUpdate )
{
    std::string oKmlKmz;
    char szBuffer[1024+1];

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
    {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "Cannot open %s", pszFilename );
        return FALSE;
    }
    int nRead;
    while ((nRead = VSIFReadL(szBuffer, 1, 1024, fp)) != 0)
    {
        try
        {
            oKmlKmz.append(szBuffer, nRead);
        }
        catch(std::bad_alloc& e)
        {
            VSIFCloseL(fp);
            return FALSE;
        }
    }
    VSIFCloseL(fp);

    KmzFile *poKmlKmzfile = KmzFile::OpenFromString ( oKmlKmz );

    if ( !poKmlKmzfile ) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "%s is not a valid kmz file", pszFilename );
        return FALSE;
    }

    CPLLocaleC  oLocaleForcer;

    /***** read the doc.kml *****/

    std::string oKmlKml;
    std::string oKmlKmlPath;
    if ( !poKmlKmzfile->ReadKmlAndGetPath ( &oKmlKml, &oKmlKmlPath ) ) {

        return FALSE;
    }

    /***** create a SRS *****/

    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference ( OGRLIBKMLSRSWKT );

    /***** parse the kml into the DOM *****/

    std::string oKmlErrors;
    ElementPtr poKmlDocKmlRoot = kmldom::Parse ( oKmlKml, &oKmlErrors );

    if ( !poKmlDocKmlRoot ) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "ERROR Parseing kml layer %s from %s :%s",
                   oKmlKmlPath.c_str (  ),
                   pszFilename, oKmlErrors.c_str (  ) );
        delete poOgrSRS;

        return FALSE;
    }

    /***** get the child contianer from root *****/

    ContainerPtr poKmlContainer;

    if (!(poKmlContainer = GetContainerFromRoot ( m_poKmlFactory, poKmlDocKmlRoot ))) {
        CPLError ( CE_Failure, CPLE_OpenFailed,
                   "ERROR Parseing %s from %s :%s",
                   oKmlKmlPath.c_str (  ),
                   pszFilename, "kml contains no Containers" );
        delete poOgrSRS;

        return FALSE;
    }

    /***** loop over the container looking for network links *****/

    size_t nKmlFeatures = poKmlContainer->get_feature_array_size (  );
    size_t iKmlFeature;
    int nLinks = 0;

    for ( iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ ) {
        FeaturePtr poKmlFeat =
            poKmlContainer->get_feature_array_at ( iKmlFeature );

        /***** is it a network link? *****/

        if ( !poKmlFeat->IsA ( kmldom::Type_NetworkLink ) )
            continue;

        NetworkLinkPtr poKmlNetworkLink = AsNetworkLink ( poKmlFeat );

        /***** does it have a link? *****/

        if ( !poKmlNetworkLink->has_link (  ) )
            continue;

        LinkPtr poKmlLink = poKmlNetworkLink->get_link (  );

        /***** does the link have a href? *****/

        if ( !poKmlLink->has_href (  ) )
            continue;

        kmlengine::Href * poKmlHref =
            new kmlengine::Href ( poKmlLink->get_href (  ) );

        /***** is the link relative? *****/

        if ( poKmlHref->IsRelativePath (  ) ) {

            nLinks++;

            std::string oKml;
            if ( poKmlKmzfile->
                 ReadFile ( poKmlHref->get_path (  ).c_str (  ), &oKml ) ) {

                /***** parse the kml into the DOM *****/

                std::string oKmlErrors;
                ElementPtr poKmlLyrRoot = kmldom::Parse ( oKml, &oKmlErrors );

                if ( !poKmlLyrRoot ) {
                    CPLError ( CE_Failure, CPLE_OpenFailed,
                               "ERROR Parseing kml layer %s from %s :%s",
                               poKmlHref->get_path (  ).c_str (  ),
                               pszFilename, oKmlErrors.c_str (  ) );
                    delete poKmlHref;

                    continue;
                }

                /***** get the container from root  *****/

                ContainerPtr poKmlLyrContainer =
                    GetContainerFromRoot ( m_poKmlFactory, poKmlLyrRoot );

                if ( !poKmlLyrContainer )
                {
                    CPLError ( CE_Failure, CPLE_OpenFailed,
                               "ERROR Parseing kml layer %s from %s :%s",
                               poKmlHref->get_path (  ).c_str (  ),
                               pszFilename, oKmlErrors.c_str (  ) );
                    delete poKmlHref;

                    continue;
                }

                /***** create the layer *****/

                AddLayer ( CPLGetBasename
                           ( poKmlHref->get_path (  ).c_str (  ) ), poOgrSRS,
                           wkbUnknown, this, poKmlLyrRoot, poKmlLyrContainer,
                           poKmlHref->get_path (  ).c_str (  ), FALSE, bUpdate,
                           nKmlFeatures );

            }
        }

        /***** cleanup *****/

        delete poKmlHref;
    }

    /***** if the doc.kml has links store it so if were in update mode we can write it *****/
    
    if ( nLinks ) {
        m_poKmlDocKml = poKmlContainer;
        m_poKmlDocKmlRoot = poKmlDocKmlRoot;
    }
        
    /***** if the doc.kml has no links treat it as a normal kml file *****/

    else {

        /* todo there could still be a seperate styles file in the kmz
           if there is this would be a layer style table IF its only a single
           layer
         */

        /***** get the styles *****/

        ParseStyles ( AsDocument ( poKmlContainer ), &m_poStyleTable );

        /***** parse for layers *****/

        int nPlacemarks = ParseLayers ( poKmlContainer, poOgrSRS );

        /***** if there is placemarks in the root its a layer *****/

        if ( nPlacemarks && !nLayers ) {
            AddLayer ( CPLGetBasename ( pszFilename ),
                       poOgrSRS, wkbUnknown,
                       this, poKmlDocKmlRoot, poKmlContainer, pszFilename, FALSE, bUpdate, 1 );
        }
    }

    /***** read the style table if it has one *****/

    std::string oKmlStyleKml;
    if ( poKmlKmzfile->ReadFile ( "style/style.kml", &oKmlStyleKml ) )
        ParseIntoStyleTable ( &oKmlStyleKml, "style/style.kml");

    /***** cleanup *****/

    delete poOgrSRS;

    delete poKmlKmzfile;
    m_isKmz = TRUE;
    
    return TRUE;
}

/******************************************************************************
 method to open a dir
 
 Args:          pszFilename Dir to open
                bUpdate     update mode
 
 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::OpenDir (
    const char *pszFilename,
    int bUpdate )
{

    char **papszDirList = NULL;

    if ( !( papszDirList = VSIReadDir ( pszFilename ) ) )
        return FALSE;

    /***** create a SRS *****/

    OGRSpatialReference *poOgrSRS =
        new OGRSpatialReference ( OGRLIBKMLSRSWKT );

    int nFiles = CSLCount ( papszDirList );
    int iFile;

    for ( iFile = 0; iFile < nFiles; iFile++ ) {

        /***** make sure its a .kml file *****/

        if ( !EQUAL ( CPLGetExtension ( papszDirList[iFile] ), "kml" ) )
            continue;

        /***** read the file *****/
        std::string oKmlKml;
        char szBuffer[1024+1];

        CPLString osFilePath =
            CPLFormFilename ( pszFilename, papszDirList[iFile], NULL );

        VSILFILE* fp = VSIFOpenL(osFilePath, "rb");
        if (fp == NULL)
        {
             CPLError ( CE_Failure, CPLE_OpenFailed,
                       "Cannot open %s", osFilePath.c_str() );
             continue;
        }

        int nRead;
        while ((nRead = VSIFReadL(szBuffer, 1, 1024, fp)) != 0)
        {
            try
            {
                oKmlKml.append(szBuffer, nRead);
            }
            catch(std::bad_alloc& e)
            {
                VSIFCloseL(fp);
                CSLDestroy ( papszDirList );
                return FALSE;
            }
        }
        VSIFCloseL(fp);

        CPLLocaleC  oLocaleForcer;

        /***** parse the kml into the DOM *****/

        std::string oKmlErrors;
        ElementPtr poKmlRoot = kmldom::Parse ( oKmlKml, &oKmlErrors );

        if ( !poKmlRoot ) {
            CPLError ( CE_Failure, CPLE_OpenFailed,
                       "ERROR Parseing kml layer %s from %s :%s",
                       osFilePath.c_str(), pszFilename, oKmlErrors.c_str (  ) );

            continue;
        }

        /***** get the cintainer from the root *****/

        ContainerPtr poKmlContainer;

        if ( !( poKmlContainer = GetContainerFromRoot ( m_poKmlFactory, poKmlRoot ) ) ) {
            CPLError ( CE_Failure, CPLE_OpenFailed,
                       "ERROR Parseing kml %s :%s %s",
                       pszFilename,
                       "This file does not fit the OGR model,",
                       "there is no container element at the root." );
            continue;
        }

        /***** is it a style table? *****/

        if ( EQUAL ( papszDirList[iFile], "style.kml" ) ) {
            ParseStyles ( AsDocument ( poKmlContainer ), &m_poStyleTable );
            pszStylePath = CPLStrdup((char *) "style.kml");
            continue;
        }


        /***** create the layer *****/

        AddLayer ( CPLGetBasename ( osFilePath.c_str() ),
                   poOgrSRS, wkbUnknown,
                   this, poKmlRoot, poKmlContainer, osFilePath.c_str(), FALSE, bUpdate, nFiles );

    }

    delete poOgrSRS;

    CSLDestroy ( papszDirList );

    if ( nLayers > 0 ) {
        m_isDir = TRUE;
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 Method to open a datasource
 
 Args:          pszFilename Darasource to open
                bUpdate     update mode
 
 Returns:       True on success, false on failure

******************************************************************************/

static int CheckIsKMZ(const char *pszFilename)
{
    char** papszFiles = VSIReadDir(pszFilename);
    char** papszIter = papszFiles;
    int bFoundKML = FALSE;
    while(papszIter && *papszIter)
    {
        if (EQUAL(CPLGetExtension(*papszIter), "kml"))
        {
            bFoundKML = TRUE;
            break;
        }
        else
        {
            CPLString osFilename(pszFilename);
            osFilename += "/";
            osFilename += *papszIter;
            if (CheckIsKMZ(osFilename))
            {
                bFoundKML = TRUE;
                break;
            }
        }
        papszIter ++;
    }
    CSLDestroy(papszFiles);
    return bFoundKML;
}

int OGRLIBKMLDataSource::Open (
    const char *pszFilename,
    int bUpdate )
{

    this->bUpdate = bUpdate;
    pszName = CPLStrdup ( pszFilename );

    /***** dir *****/

    VSIStatBufL sStatBuf = { };
    if ( !VSIStatExL ( pszFilename, &sStatBuf, VSI_STAT_NATURE_FLAG ) &&
         VSI_ISDIR ( sStatBuf.st_mode ) )
        return OpenDir ( pszFilename, bUpdate );

   /***** kml *****/

    else if ( EQUAL ( CPLGetExtension ( pszFilename ), "kml" ) )
        return OpenKml ( pszFilename, bUpdate );

    /***** kmz *****/

    else if ( EQUAL ( CPLGetExtension ( pszFilename ), "kmz" ) )
        return OpenKmz ( pszFilename, bUpdate );

    else
    {
        char szBuffer[1024+1];
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return FALSE;

        int nRead = VSIFReadL(szBuffer, 1, 1024, fp);
        szBuffer[nRead] = 0;

        VSIFCloseL(fp);

        /* Does it look like a zip file ? */
        if (nRead == 1024 &&
            szBuffer[0] == 0x50 && szBuffer[1] == 0x4B &&
            szBuffer[2] == 0x03  && szBuffer[3] == 0x04)
        {
            CPLString osFilename("/vsizip/");
            osFilename += pszFilename;
            if (!CheckIsKMZ(osFilename))
                return FALSE;

            return OpenKmz ( pszFilename, bUpdate );
        }

        if (strstr(szBuffer, "<kml>") || strstr(szBuffer, "<kml xmlns="))
            return OpenKml ( pszFilename, bUpdate );
        
        return FALSE;
    }
}

/******************************************************************************
 method to create a single file .kml ds
 
 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options
 
 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateKml (
    const char *pszFilename,
    char **papszOptions )
{

    m_poKmlDSKml = OGRLIBKMLCreateOGCKml22(m_poKmlFactory);
    DocumentPtr poKmlDocument = m_poKmlFactory->CreateDocument (  );

    m_poKmlDSKml->set_feature ( poKmlDocument );
    m_poKmlDSContainer = poKmlDocument;
    m_isKml = TRUE;
    bUpdated = TRUE;

    return true;
}

/******************************************************************************
 method to create a .kmz ds
 
 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options
 
 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateKmz (
    const char *pszFilename,
    char **papszOptions )
{


    /***** create the doc.kml  *****/

    const char *namefield = CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( !strcmp ( namefield, "yes" ) ) {
        m_poKmlDocKml = m_poKmlFactory->CreateDocument (  );
    }

    pszStylePath = CPLStrdup((char *) "style/style.kml");

    m_isKmz = TRUE;
    bUpdated = TRUE;

    return TRUE;
}

/******************************************************************************
 Method to create a dir datasource
 
 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options
 
 Returns:       True on success, false on failure

******************************************************************************/

int OGRLIBKMLDataSource::CreateDir (
    const char *pszFilename,
    char **papszOptions )
{

    if ( VSIMkdir ( pszFilename, 0755 ) ) {
        CPLError ( CE_Failure, CPLE_AppDefined,
                   "ERROR Creating dir: %s for KML datasource", pszFilename );
        return FALSE;
    }

    m_isDir = TRUE;
    bUpdated = TRUE;


    const char *namefield = CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( !strcmp ( namefield, "yes" ) ) {
        m_poKmlDocKml = m_poKmlFactory->CreateDocument (  );
    }

    pszStylePath = CPLStrdup((char *) "style.kml");

    return TRUE;
}


/******************************************************************************
 method to create a datasource
 
 Args:          pszFilename     the datasource to create
                papszOptions    datasource creation options
 
 Returns:       True on success, false on failure
 
 env vars:
  LIBKML_USE_DOC.KML         default: yes
 
******************************************************************************/

int OGRLIBKMLDataSource::Create (
    const char *pszFilename,
    char **papszOptions )
{

    int bResult = FALSE;

    if (strcmp(pszFilename, "/dev/stdout") == 0)
        pszFilename = "/vsistdout/";

    pszName = CPLStrdup ( pszFilename );
    bUpdate = TRUE;

    /***** kml *****/

    if ( strcmp(pszFilename, "/vsistdout/") == 0 ||
         strncmp(pszFilename, "/vsigzip/", 9) == 0 ||
         EQUAL ( CPLGetExtension ( pszFilename ), "kml" ) )
        bResult = CreateKml ( pszFilename, papszOptions );

    /***** kmz *****/

    else if ( EQUAL ( CPLGetExtension ( pszFilename ), "kmz" ) )
        bResult = CreateKmz ( pszFilename, papszOptions );

    /***** dir *****/

    else
        bResult = CreateDir ( pszFilename, papszOptions );

    return bResult;
}

/******************************************************************************
 method to get a layer by index
 
 Args:          iLayer      the index of the layer to get
 
 Returns:       pointer to the layer, or NULL if the layer does not exist

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::GetLayer (
    int iLayer )
{
    if ( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];

}

/******************************************************************************
 method to get a layer by name
 
 Args:          pszname     name of the layer to get
 
 Returns:       pointer to the layer, or NULL if the layer does not exist

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::GetLayerByName (
    const char *pszname )
{
    int iLayer = 0;

    for ( iLayer = 0; iLayer < nLayers; iLayer++ ) {
        if ( EQUAL ( pszname, papoLayers[iLayer]->GetName (  ) ) )
            return papoLayers[iLayer];
    }

    return NULL;
}


/******************************************************************************
 method to DeleteLayers in a .kml datasource
 
 Args:          iLayer  index of the layer to delete
 
 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayerKml (
    int iLayer )
{
    OGRLIBKMLLayer *poOgrLayer = ( OGRLIBKMLLayer * ) papoLayers[iLayer];

    /***** loop over the features *****/

    size_t nKmlFeatures = m_poKmlDSContainer->get_feature_array_size (  );
    size_t iKmlFeature;

    for ( iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ ) {
        FeaturePtr poKmlFeat =
            m_poKmlDSContainer->get_feature_array_at ( iKmlFeature );

        if ( poKmlFeat == poOgrLayer->GetKmlLayer (  ) ) {
            m_poKmlDSContainer->DeleteFeatureAt ( iKmlFeature );
            break;
        }

    }


    return OGRERR_NONE;
}

/******************************************************************************
 method to DeleteLayers in a .kmz datasource
 
 Args:          iLayer  index of the layer to delete
 
 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayerKmz (
    int iLayer )
{
    OGRLIBKMLLayer *poOgrLayer = ( OGRLIBKMLLayer * ) papoLayers[iLayer];

    const char *pszUseDocKml =
        CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( EQUAL ( pszUseDocKml, "yes" ) && m_poKmlDocKml ) {

        /***** loop over the features *****/

        size_t nKmlFeatures = m_poKmlDocKml->get_feature_array_size (  );
        size_t iKmlFeature;

        for ( iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ ) {
            FeaturePtr poKmlFeat =
                m_poKmlDocKml->get_feature_array_at ( iKmlFeature );

            if ( poKmlFeat->IsA ( kmldom::Type_NetworkLink ) ) {
                NetworkLinkPtr poKmlNetworkLink = AsNetworkLink ( poKmlFeat );

                /***** does it have a link? *****/

                if ( poKmlNetworkLink->has_link (  ) ) {
                    LinkPtr poKmlLink = poKmlNetworkLink->get_link (  );

                    /***** does the link have a href? *****/

                    if ( poKmlLink->has_href (  ) ) {
                        kmlengine::Href * poKmlHref =
                            new kmlengine::Href ( poKmlLink->get_href (  ) );

                        /***** is the link relative? *****/

                        if ( poKmlHref->IsRelativePath (  ) ) {

                            const char *pszLink =
                                poKmlHref->get_path (  ).c_str (  );

                            if ( EQUAL
                                 ( pszLink, poOgrLayer->GetFileName (  ) ) ) {
                                m_poKmlDocKml->DeleteFeatureAt ( iKmlFeature );
                                break;
                            }


                        }
                    }
                }
            }
        }

    }

    return OGRERR_NONE;
}

/******************************************************************************
 method to delete a layer in a datasource
 
 Args:          iLayer  index of the layer to delete
 
 Returns:       OGRERR_NONE on success, OGRERR_FAILURE on failure

******************************************************************************/

OGRErr OGRLIBKMLDataSource::DeleteLayer (
    int iLayer )
{

    if ( !bUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;

    if ( iLayer >= nLayers )
        return OGRERR_FAILURE;

    if ( IsKml (  ) )
        DeleteLayerKml ( iLayer );

    else if ( IsKmz (  ) )
        DeleteLayerKmz ( iLayer );

    else if ( IsDir (  ) ) {
        DeleteLayerKmz ( iLayer );

        /***** delete the file the layer corisponds to *****/

        const char *pszFilePath =
            CPLFormFilename ( pszName, papoLayers[iLayer]->GetFileName (  ),
                              NULL );
        VSIStatBufL oStatBufL = { };
        if ( !VSIStatL ( pszFilePath, &oStatBufL ) ) {
            if ( VSIUnlink ( pszFilePath ) ) {
                CPLError ( CE_Failure, CPLE_AppDefined,
                           "ERROR Deleteing Layer %s from filesystem as %s",
                           papoLayers[iLayer]->GetName (  ), pszFilePath );
            }
        }
    }


    delete papoLayers[iLayer];
    memmove ( papoLayers + iLayer, papoLayers + iLayer + 1,
              sizeof ( void * ) * ( nLayers - iLayer - 1 ) );
    nLayers--;
    bUpdated = TRUE;

    return OGRERR_NONE;
}

/******************************************************************************
 method to create a layer in a single file .kml
 
 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options
 
 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::CreateLayerKml (
    const char *pszLayerName,
    OGRSpatialReference * poOgrSRS,
    OGRwkbGeometryType eGType,
    char **papszOptions )
{

    OGRLIBKMLLayer *poOgrLayer = NULL;

    DocumentPtr poKmlDocument = m_poKmlFactory->CreateDocument (  );

    m_poKmlDSContainer->add_feature ( poKmlDocument );

    /***** create the layer *****/

    poOgrLayer = AddLayer ( pszLayerName, poOgrSRS, eGType, this,
                            NULL, poKmlDocument, "", TRUE, bUpdate, 1 );

    /***** add the layer name as a <Name> *****/

    poKmlDocument->set_name ( pszLayerName );

    return ( OGRLayer * ) poOgrLayer;
}

/******************************************************************************
 method to create a layer in a .kmz or dir
 
 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options
 
 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::CreateLayerKmz (
    const char *pszLayerName,
    OGRSpatialReference * poOgrSRS,
    OGRwkbGeometryType eGType,
    char **papszOptions )
{

    /***** add a network link to doc.kml *****/

    const char *pszUseDocKml =
        CPLGetConfigOption ( "LIBKML_USE_DOC.KML", "yes" );

    if ( EQUAL ( pszUseDocKml, "yes" ) && m_poKmlDocKml ) {

        DocumentPtr poKmlDocument = AsDocument ( m_poKmlDocKml );

        NetworkLinkPtr poKmlNetLink = m_poKmlFactory->CreateNetworkLink (  );
        LinkPtr poKmlLink = m_poKmlFactory->CreateLink (  );

        std::string oHref;
        oHref.append ( pszLayerName );
        oHref.append ( ".kml" );
        poKmlLink->set_href ( oHref );

        poKmlNetLink->set_link ( poKmlLink );
        poKmlDocument->add_feature ( poKmlNetLink );

    }

    /***** create the layer *****/

    OGRLIBKMLLayer *poOgrLayer = NULL;

    DocumentPtr poKmlDocument = m_poKmlFactory->CreateDocument (  );

    poOgrLayer = AddLayer ( pszLayerName, poOgrSRS, eGType, this,
                            NULL, poKmlDocument,
                            CPLFormFilename ( NULL, pszLayerName, ".kml" ),
                            TRUE, bUpdate, 1 );

    /***** add the layer name as a <Name> *****/

    poKmlDocument->set_name ( pszLayerName );

    return ( OGRLayer * ) poOgrLayer;
}

/******************************************************************************
 CreateLayer()
 
 Args:          pszLayerName    name of the layer to create
                poOgrSRS        the SRS of the layer
                eGType          the layers geometry type
                papszOptions    layer creation options
 
 Returns:       return a pointer to the new layer or NULL on failure

******************************************************************************/

OGRLayer *OGRLIBKMLDataSource::CreateLayer (
    const char *pszLayerName,
    OGRSpatialReference * poOgrSRS,
    OGRwkbGeometryType eGType,
    char **papszOptions )
{

    if ( !bUpdate )
        return NULL;
    
    if( (IsKmz () || IsDir ()) && EQUAL(pszLayerName, "doc") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "'doc' is an invalid layer name in a KMZ file");
        return NULL;
    }

    OGRLayer *poOgrLayer = NULL;

    /***** kml DS *****/

    if ( IsKml (  ) ) {
        poOgrLayer = CreateLayerKml ( pszLayerName, poOgrSRS,
                                      eGType, papszOptions );

    }

    else if ( IsKmz (  ) || IsDir (  ) ) {
        poOgrLayer = CreateLayerKmz ( pszLayerName, poOgrSRS,
                                      eGType, papszOptions );

    }




    /***** mark the dataset as updated *****/

    if ( poOgrLayer )
        bUpdated = TRUE;

    return poOgrLayer;
}

/******************************************************************************
 method to get a datasources style table
 
 Args:          none
 
 Returns:       pointer to the datasources style table, or NULL if it does
                not have one

******************************************************************************/

OGRStyleTable *OGRLIBKMLDataSource::GetStyleTable (
     )
{

    return m_poStyleTable;
}

/******************************************************************************
  method to write a style table to a single file .kml ds
 
 Args:          poStyleTable    pointer to the style table to add
 
 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable2Kml (
    OGRStyleTable * poStyleTable )
{

    /***** delete all the styles *****/

    DocumentPtr poKmlDocument = AsDocument ( m_poKmlDSContainer );
    size_t nKmlStyles = poKmlDocument->get_styleselector_array_size (  );
    int iKmlStyle;

    for ( iKmlStyle = nKmlStyles - 1; iKmlStyle >= 0; iKmlStyle-- ) {
        poKmlDocument->DeleteStyleSelectorAt ( iKmlStyle );
    }

    /***** add the new style table to the document *****/

    styletable2kml ( poStyleTable, m_poKmlFactory,
                     AsContainer ( poKmlDocument ) );

    return;
}

/******************************************************************************
 method to write a style table to a kmz ds
 
 Args:          poStyleTable    pointer to the style table to add
 
 Returns:       nothing

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable2Kmz (
    OGRStyleTable * poStyleTable )
{

    /***** replace the style document with a new one *****/

    m_poKmlStyleKml = m_poKmlFactory->CreateDocument (  );

    styletable2kml ( poStyleTable, m_poKmlFactory, m_poKmlStyleKml );

    return;
}

/******************************************************************************
 method to write a style table to a datasource
 
 Args:          poStyleTable    pointer to the style table to add
 
 Returns:       nothing

 note: this method assumes ownership of the style table
 
******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTableDirectly (
    OGRStyleTable * poStyleTable )
{

    if ( !bUpdate )
        return;

    if ( m_poStyleTable )
        delete m_poStyleTable;

    m_poStyleTable = poStyleTable;

    /***** a kml datasource? *****/

    if ( IsKml (  ) )
        SetStyleTable2Kml ( m_poStyleTable );

    else if ( IsKmz (  ) || IsDir (  ) )
        SetStyleTable2Kmz ( m_poStyleTable );

    bUpdated = TRUE;




    return;
}

/******************************************************************************
 method to write a style table to a datasource
 
 Args:          poStyleTable    pointer to the style table to add
 
 Returns:       nothing

 note:  this method copys the style table, and the user will still be
        responsible for its destruction

******************************************************************************/

void OGRLIBKMLDataSource::SetStyleTable (
    OGRStyleTable * poStyleTable )
{

    if ( !bUpdate )
        return;

    if ( poStyleTable )
        SetStyleTableDirectly ( poStyleTable->Clone (  ) );
    else
        SetStyleTableDirectly ( NULL );
    return;
}


/******************************************************************************
 Test if capability is available.

 Args:          pszCap  datasource capability name to test
 
 Returns:       nothing

 ODsCCreateLayer: True if this datasource can create new layers.
 ODsCDeleteLayer: True if this datasource can delete existing layers.
 
******************************************************************************/

int OGRLIBKMLDataSource::TestCapability (
    const char *pszCap )
{

    if ( EQUAL ( pszCap, ODsCCreateLayer ) )
        return bUpdate;
    else if ( EQUAL ( pszCap, ODsCDeleteLayer ) )
        return bUpdate;
    else
        return FALSE;

}
