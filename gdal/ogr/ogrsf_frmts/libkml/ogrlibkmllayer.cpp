/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "libkml_headers.h"

#include <string>

#include "ogr_libkml.h"
#include "cpl_error.h"
#include "ogrlibkmlfeature.h"
#include "ogrlibkmlfield.h"
#include "ogrlibkmlstyle.h"

#include <algorithm>
#include <set>

CPL_CVSID("$Id$")

using kmldom::CameraPtr;
using kmldom::ChangePtr;
using kmldom::CreatePtr;
using kmldom::ContainerPtr;
using kmldom::DataPtr;
using kmldom::DeletePtr;
using kmldom::DocumentPtr;
using kmldom::ElementPtr;
using kmldom::ExtendedDataPtr;
using kmldom::FeaturePtr;
using kmldom::GroundOverlayPtr;
using kmldom::IconPtr;
using kmldom::KmlFactory;
using kmldom::KmlPtr;
using kmldom::LatLonAltBoxPtr;
using kmldom::LodPtr;
using kmldom::LookAtPtr;
using kmldom::PlacemarkPtr;
using kmldom::RegionPtr;
using kmldom::SchemaDataPtr;
using kmldom::ScreenOverlayPtr;
using kmldom::SimpleFieldPtr;
using kmldom::UpdatePtr;
using kmlengine::Bbox;

/************************************************************************/
/*                    OGRLIBKMLGetSanitizedNCName()                     */
/************************************************************************/

CPLString OGRLIBKMLGetSanitizedNCName( const char* pszName )
{
    CPLString osName(pszName);
    // (Approximate) validation rules for a valid NCName.
    for( size_t i = 0; i < osName.size(); i++)
    {
        char ch = osName[i];
        if( (ch >= 'A' && ch <= 'Z') || ch == '_' || (ch >= 'a' && ch <= 'z') )
        {
            /* ok */
        }
        else if ( i > 0 && (ch == '-' || ch == '.' ||
                            (ch >= '0' && ch <= '9')) )
        {
            /* ok */
        }
        // Always false: ch > 127.
        else
        {
            osName[i] = '_';
        }
    }
    return osName;
}

/******************************************************************************
 OGRLIBKMLLayer constructor

 Args:          pszLayerName    the name of the layer
                eGType          the layers geometry type
                poOgrDS         pointer to the datasource the layer is in
                poKmlRoot       pointer to the root kml element of the layer
                poKmlContainer  pointer to the kml container of the layer
                pszFileName     the filename of the layer
                bNew            true if its a new layer
                bUpdate         true if the layer is writable

 Returns:       nothing

******************************************************************************/

OGRLIBKMLLayer::OGRLIBKMLLayer( const char *pszLayerName,
                                OGRwkbGeometryType eGType,
                                OGRLIBKMLDataSource * poOgrDS,
                                ElementPtr poKmlRoot,
                                ContainerPtr poKmlContainer,
                                UpdatePtr poKmlUpdate,
                                const char *pszFileName,
                                int bNew,
                                int bUpdateIn ) :
    bUpdate(CPL_TO_BOOL(bUpdateIn)),
    nFeatures(0),
    iFeature(0),
    nFID(1),
    m_pszName(CPLStrdup(pszLayerName)),
    m_pszFileName(CPLStrdup(pszFileName)),
    m_poKmlLayer(poKmlContainer),  // Store the layers container.
    m_poKmlLayerRoot(poKmlRoot),  // Store the root element pointer.
    m_poKmlUpdate(poKmlUpdate),
    m_poOgrDS(poOgrDS),
    m_poOgrFeatureDefn(new OGRFeatureDefn(pszLayerName)),
    m_poKmlSchema(nullptr),
    m_poOgrSRS(new OGRSpatialReference(nullptr)),
    m_bReadGroundOverlay(CPLTestBool(
        CPLGetConfigOption("LIBKML_READ_GROUND_OVERLAY", "YES"))),
    m_bUseSimpleField(CPLTestBool(
        CPLGetConfigOption("LIBKML_USE_SIMPLEFIELD", "YES"))),
    m_bWriteRegion(false),
    m_bRegionBoundsAuto(false),
    m_dfRegionMinLodPixels(0),
    m_dfRegionMaxLodPixels(-1),
    m_dfRegionMinFadeExtent(0),
    m_dfRegionMaxFadeExtent(0),
    m_dfRegionMinX(200),
    m_dfRegionMinY(200),
    m_dfRegionMaxX(-200),
    m_dfRegionMaxY(-200),
    m_bUpdateIsFolder(false)
{
    m_poStyleTable = nullptr;
    m_poOgrSRS->SetWellKnownGeogCS( "WGS84" );
    m_poOgrSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    SetDescription( m_poOgrFeatureDefn->GetName() );
    m_poOgrFeatureDefn->Reference();
    m_poOgrFeatureDefn->SetGeomType( eGType );
    if( m_poOgrFeatureDefn->GetGeomFieldCount() != 0 )
        m_poOgrFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poOgrSRS);

    /***** was the layer created from a DS::Open *****/
    if( !bNew )
    {
        /***** get the number of features on the layer *****/
        nFeatures = static_cast<int>(m_poKmlLayer->get_feature_array_size());

        /***** get the field config *****/
        struct fieldconfig oFC;
        get_fieldconfig( &oFC );

        /***** name field *****/
        OGRFieldDefn oOgrFieldName( oFC.namefield,OFTString );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldName );

        /***** description field *****/
        OGRFieldDefn oOgrFieldDesc( oFC.descfield, OFTString );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldDesc );

        /***** timestamp field *****/
        OGRFieldDefn oOgrFieldTs( oFC.tsfield, OFTDateTime );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldTs );

        /*****  timespan begin field *****/
        OGRFieldDefn oOgrFieldBegin( oFC.beginfield, OFTDateTime );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldBegin );

        /*****  timespan end field *****/
        OGRFieldDefn oOgrFieldEnd( oFC.endfield, OFTDateTime );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldEnd );

        /*****  altitudeMode field *****/
        OGRFieldDefn oOgrFieldAltitudeMode( oFC.altitudeModefield, OFTString );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldAltitudeMode );

        /***** tessellate field *****/
        OGRFieldDefn oOgrFieldTessellate( oFC.tessellatefield, OFTInteger );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldTessellate );

        /***** extrude field *****/
        OGRFieldDefn oOgrFieldExtrude( oFC.extrudefield, OFTInteger );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldExtrude );

        /***** visibility field *****/
        OGRFieldDefn oOgrFieldVisibility( oFC.visibilityfield, OFTInteger );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldVisibility );

        /***** draw order field *****/
        OGRFieldDefn oOgrFieldDrawOrder( oFC.drawOrderfield, OFTInteger );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldDrawOrder );

        /***** icon field *****/
        OGRFieldDefn oOgrFieldIcon( oFC.iconfield, OFTString );
        m_poOgrFeatureDefn->AddFieldDefn( &oOgrFieldIcon );

        /***** get the styles *****/
        if( m_poKmlLayer->IsA( kmldom::Type_Document ) )
            ParseStyles( AsDocument ( m_poKmlLayer ), &m_poStyleTable );

        bool bCanSetKmlSchema = true;

        /***** get the schema if the layer is a Document *****/
        if( m_poKmlLayer->IsA( kmldom::Type_Document ) )
        {
            DocumentPtr poKmlDocument = AsDocument( m_poKmlLayer );

            if( poKmlDocument->get_schema_array_size() )
            {
                for(size_t i = 0; i < poKmlDocument->get_schema_array_size(); i++ )
                {
                    auto schema = poKmlDocument->get_schema_array_at( i );
                    if( bCanSetKmlSchema && !m_poKmlSchema )
                    {
                        m_poKmlSchema = schema;
                        bCanSetKmlSchema = false;
                    }
                    else
                    {
                        m_poKmlSchema = nullptr;
                    }
                    kml2FeatureDef( schema, m_poOgrFeatureDefn );
                }
            }
        }

        /***** the schema is somewhere else *****/
        if( bCanSetKmlSchema )
        {
            /***** try to find the correct schema *****/
            bool bHasHeading = false;
            bool bHasTilt = false;
            bool bHasRoll = false;
            bool bHasSnippet = false;
            FeaturePtr poKmlFeature = nullptr;
            const bool bLaunderFieldNames =
                CPLTestBool(CPLGetConfigOption(
                    "LIBKML_LAUNDER_FIELD_NAMES", "YES"));
            std::set<std::string> oSetSchemaAlreadyVisited;

            /***** find the first placemark *****/
            for( iFeature = 0; iFeature < nFeatures; iFeature++ )
            {
                poKmlFeature =
                    m_poKmlLayer->get_feature_array_at( iFeature );

                if( poKmlFeature->Type() == kmldom::Type_Placemark )
                {
                    PlacemarkPtr poKmlPlacemark = AsPlacemark( poKmlFeature );
                    if( !poKmlPlacemark->has_geometry() &&
                        poKmlPlacemark->has_abstractview() &&
                        poKmlPlacemark->get_abstractview()->
                            IsA( kmldom::Type_Camera) )
                    {
                        const CameraPtr& camera =
                            AsCamera(poKmlPlacemark->get_abstractview());
                        if( camera->has_heading() && !bHasHeading )
                        {
                            bHasHeading = true;
                            OGRFieldDefn oOgrField( oFC.headingfield, OFTReal );
                            m_poOgrFeatureDefn->AddFieldDefn( &oOgrField );
                        }
                        if( camera->has_tilt() && !bHasTilt )
                        {
                            bHasTilt = true;
                            OGRFieldDefn oOgrField( oFC.tiltfield, OFTReal );
                            m_poOgrFeatureDefn->AddFieldDefn( &oOgrField );
                        }
                        if( camera->has_roll() && !bHasRoll )
                        {
                            bHasRoll = true;
                            OGRFieldDefn oOgrField( oFC.rollfield, OFTReal );
                            m_poOgrFeatureDefn->AddFieldDefn( &oOgrField );
                        }
                    }

                    if( poKmlFeature->has_extendeddata() )
                    {
                        const ExtendedDataPtr poKmlExtendedData =
                            poKmlFeature->get_extendeddata();

                        if( poKmlExtendedData->get_schemadata_array_size() > 0 )
                        {
                            const SchemaDataPtr poKmlSchemaData =
                                poKmlExtendedData->get_schemadata_array_at( 0 );

                            if( poKmlSchemaData->has_schemaurl() )
                            {
                                std::string oKmlSchemaUrl =
                                    poKmlSchemaData->get_schemaurl();
                                if( oSetSchemaAlreadyVisited.find(
                                        oKmlSchemaUrl) ==
                                            oSetSchemaAlreadyVisited.end() )
                                {
                                    oSetSchemaAlreadyVisited.insert(
                                        oKmlSchemaUrl);
                                    auto schema = m_poOgrDS->FindSchema(
                                            oKmlSchemaUrl.c_str() );
                                    if( schema )
                                    {
                                        if( bCanSetKmlSchema && !m_poKmlSchema )
                                        {
                                            m_poKmlSchema = schema;
                                            bCanSetKmlSchema = false;
                                        }
                                        else
                                        {
                                            m_poKmlSchema = nullptr;
                                        }
                                        kml2FeatureDef( schema, m_poOgrFeatureDefn );
                                    }
                                }
                            }
                        }
                        else if( poKmlExtendedData->get_data_array_size() > 0 )
                        {
                            const size_t nDataArraySize =
                                poKmlExtendedData->get_data_array_size();
                            for( size_t i = 0; i < nDataArraySize; i++ )
                            {
                                const DataPtr& data =
                                    poKmlExtendedData->get_data_array_at(i);
                                if( data->has_name() )
                                {
                                    CPLString osName = std::string(data->get_name());
                                    if( bLaunderFieldNames )
                                        osName = LaunderFieldNames(osName);
                                    if( m_poOgrFeatureDefn->GetFieldIndex(osName) < 0 )
                                    {
                                        OGRFieldDefn oOgrField( osName, OFTString );
                                        m_poOgrFeatureDefn->AddFieldDefn( &oOgrField );
                                    }
                                }
                            }
                        }
                    }
                }
                if( !bHasSnippet && poKmlFeature->has_snippet() )
                {
                    bHasSnippet = true;
                    OGRFieldDefn oOgrField( oFC.snippetfield, OFTString );
                    m_poOgrFeatureDefn->AddFieldDefn( &oOgrField );
                }
            }

            iFeature = 0;
        }
    }
}

/******************************************************************************
 OGRLIBKMLLayer Destructor

 Args:          none

 Returns:       nothing

******************************************************************************/

OGRLIBKMLLayer::~OGRLIBKMLLayer()
{
    CPLFree( const_cast<char *>(m_pszName) );
    CPLFree( const_cast<char *>(m_pszFileName) );
    m_poOgrSRS->Release();

    m_poOgrFeatureDefn->Release();
}

/******************************************************************************
 Method to get the next feature on the layer.

 Args:          none

 Returns:       The next feature, or NULL if there is no more

******************************************************************************/

OGRFeature *OGRLIBKMLLayer::GetNextRawFeature()
{
    OGRFeature *poOgrFeature = nullptr;

    if( !m_poKmlLayer )
        return nullptr;

    /***** loop over the kml features to find the next placemark *****/

    do {
        if( iFeature >= nFeatures )
            break;

        /***** get the next kml feature in the container *****/
        const FeaturePtr poKmlFeature =
            m_poKmlLayer->get_feature_array_at( iFeature++ );

        /***** what type of kml feature in the container? *****/
        switch( poKmlFeature->Type() )
        {
            case kmldom::Type_Placemark:
                poOgrFeature = kml2feat( AsPlacemark( poKmlFeature ),
                                         m_poOgrDS, this,
                                         m_poOgrFeatureDefn, m_poOgrSRS );
                break;

            case kmldom::Type_GroundOverlay:
                if( m_bReadGroundOverlay )
                {
                    poOgrFeature =
                        kmlgroundoverlay2feat( AsGroundOverlay( poKmlFeature ),
                                               m_poOgrDS, this,
                                               m_poOgrFeatureDefn,
                                               m_poOgrSRS );
                }
                break;

            default:
                break;
        }
    } while ( !poOgrFeature );

    /***** set the FID on the ogr feature *****/
    if( poOgrFeature )
        poOgrFeature->SetFID(nFID++);

    return poOgrFeature;
}

/******************************************************************************
 Method to add a feature to a layer.

 Args:          poOgrFeat   pointer to the feature to add

 Returns:       OGRERR_NONE, or OGRERR_UNSUPPORTED_OPERATION of the layer is
                not writable

******************************************************************************/

OGRErr OGRLIBKMLLayer::ICreateFeature( OGRFeature * poOgrFeat )
{
    if( !bUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;

    if( m_bRegionBoundsAuto && poOgrFeat->GetGeometryRef() != nullptr &&
        !(poOgrFeat->GetGeometryRef()->IsEmpty()) )
    {
        OGREnvelope sEnvelope;
        poOgrFeat->GetGeometryRef()->getEnvelope(&sEnvelope);
        m_dfRegionMinX = std::min(m_dfRegionMinX, sEnvelope.MinX);
        m_dfRegionMinY = std::min(m_dfRegionMinY, sEnvelope.MinY);
        m_dfRegionMaxX = std::max(m_dfRegionMaxX, sEnvelope.MaxX);
        m_dfRegionMaxY = std::max(m_dfRegionMaxY, sEnvelope.MaxY);
    }

    FeaturePtr poKmlFeature =
        feat2kml( m_poOgrDS, this, poOgrFeat, m_poOgrDS->GetKmlFactory(),
                  m_bUseSimpleField );

    if( m_poKmlLayer )
    {
        m_poKmlLayer->add_feature( poKmlFeature );
    }
    else
    {
        CPLAssert( m_poKmlUpdate != nullptr );
        KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
        CreatePtr poCreate = poKmlFactory->CreateCreate();
        ContainerPtr poContainer;
        if( m_bUpdateIsFolder )
            poContainer = poKmlFactory->CreateFolder();
        else
            poContainer = poKmlFactory->CreateDocument();
        poContainer->set_targetid(OGRLIBKMLGetSanitizedNCName(GetName()));
        poContainer->add_feature( poKmlFeature );
        poCreate->add_container(poContainer);
        m_poKmlUpdate->add_updateoperation(poCreate);
    }

    /***** update the layer class count of features  *****/
    if( m_poKmlLayer )
    {
        nFeatures++;

        const char* pszId =
            CPLSPrintf("%s.%d",
                       OGRLIBKMLGetSanitizedNCName(GetName()).c_str(),
                       nFeatures);
        poOgrFeat->SetFID(nFeatures);
        poKmlFeature->set_id(pszId);
    }
    else
    {
        if( poOgrFeat->GetFID() < 0 )
        {
            static bool bAlreadyWarned = false;
            if( !bAlreadyWarned )
            {
                bAlreadyWarned = true;
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "It is recommended to define a FID when calling "
                    "CreateFeature() in a update document");
            }
        }
        else
        {
            const char* pszId =
                CPLSPrintf(
                    "%s." CPL_FRMT_GIB,
                    OGRLIBKMLGetSanitizedNCName(GetName()).c_str(),
                    poOgrFeat->GetFID());
            poOgrFeat->SetFID(nFeatures);
            poKmlFeature->set_id(pszId);
        }
    }

    /***** mark as updated *****/
    m_poOgrDS->Updated();

    return OGRERR_NONE;
}

/******************************************************************************
 Method to update a feature to a layer.

 Only work on a NetworkLinkControl/Update.

 Args:          poOgrFeat   pointer to the feature to update

 Returns:       OGRERR_NONE, or OGRERR_UNSUPPORTED_OPERATION of the layer is
                not writable

******************************************************************************/

OGRErr OGRLIBKMLLayer::ISetFeature( OGRFeature * poOgrFeat )
{
    if( !bUpdate || !m_poKmlUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;
    if( poOgrFeat->GetFID() == OGRNullFID )
        return OGRERR_FAILURE;

    FeaturePtr poKmlFeature =
        feat2kml( m_poOgrDS, this, poOgrFeat, m_poOgrDS->GetKmlFactory(),
                  m_bUseSimpleField );

    const KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
    const ChangePtr poChange = poKmlFactory->CreateChange();
    poChange->add_object(poKmlFeature);
    m_poKmlUpdate->add_updateoperation(poChange);

    const char* pszId = CPLSPrintf("%s." CPL_FRMT_GIB,
                    OGRLIBKMLGetSanitizedNCName(GetName()).c_str(), poOgrFeat->GetFID());
    poKmlFeature->set_targetid(pszId);

    /***** mark as updated *****/
    m_poOgrDS->Updated();

    return OGRERR_NONE;
}

/******************************************************************************
 Method to delete a feature to a layer.

 Only work on a NetworkLinkControl/Update.

 Args:          nFID   id of the feature to delete

 Returns:       OGRERR_NONE, or OGRERR_UNSUPPORTED_OPERATION of the layer is
                not writable

******************************************************************************/

OGRErr OGRLIBKMLLayer::DeleteFeature( GIntBig nFIDIn )
{
    if( !bUpdate || !m_poKmlUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;

    KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
    DeletePtr poDelete = poKmlFactory->CreateDelete();
    m_poKmlUpdate->add_updateoperation(poDelete);
    PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark();
    poDelete->add_feature(poKmlPlacemark);

    const char* pszId = CPLSPrintf("%s." CPL_FRMT_GIB,
                    OGRLIBKMLGetSanitizedNCName(GetName()).c_str(), nFIDIn);
    poKmlPlacemark->set_targetid(pszId);

    /***** mark as updated *****/
    m_poOgrDS->Updated();

    return OGRERR_NONE;
}

/******************************************************************************
 Method to get the number of features on the layer.

 Args:          bForce      no effect as of now

 Returns:       the number of features on the layer

 Note:          the result can include links, folders and other items that are
                not supported by OGR

******************************************************************************/

GIntBig OGRLIBKMLLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
    {
        return static_cast<int>(OGRLayer::GetFeatureCount( bForce ));
    }

    if( !m_poKmlLayer )
      return 0;

    int count = 0;

    const size_t nKmlFeatures = m_poKmlLayer->get_feature_array_size();

    /***** loop over the kml features in the container *****/
    for( size_t iKmlFeature = 0; iKmlFeature < nKmlFeatures; iKmlFeature++ )
    {
        FeaturePtr poKmlFeature =
            m_poKmlLayer->get_feature_array_at( iKmlFeature );

        /***** what type of kml feature? *****/
        switch( poKmlFeature->Type() )
        {
            case kmldom::Type_Placemark:
                count++;
                break;

            case kmldom::Type_GroundOverlay:
                if( m_bReadGroundOverlay )
                    count++;
                break;

            default:
                break;
        }
    }

    return count;
}

/******************************************************************************
 GetExtent()

 Args:          psExtent    pointer to the Envelope to store the result in
                bForce      no effect as of now

 Returns:       nothing

******************************************************************************/

OGRErr OGRLIBKMLLayer::GetExtent( OGREnvelope * psExtent, int bForce )
{
    Bbox oKmlBbox;

    if( m_poKmlLayer &&
        kmlengine::GetFeatureBounds( AsFeature( m_poKmlLayer ), &oKmlBbox ) )
    {
        psExtent->MinX = oKmlBbox.get_west();
        psExtent->MinY = oKmlBbox.get_south();
        psExtent->MaxX = oKmlBbox.get_east();
        psExtent->MaxY = oKmlBbox.get_north();

        return OGRERR_NONE;
    }

    return OGRLayer::GetExtent(psExtent, bForce);
}

/******************************************************************************
 Method to create a field on a layer.

 Args:          poField     pointer to the Field Definition to add
                bApproxOK   no effect as of now

 Returns:       OGRERR_NONE on success or OGRERR_UNSUPPORTED_OPERATION if the
                layer is not writable

******************************************************************************/

OGRErr OGRLIBKMLLayer::CreateField(
    OGRFieldDefn * poField,
    int /* bApproxOK */ )
{
    if( !bUpdate )
        return OGRERR_UNSUPPORTED_OPERATION;

    if( m_bUseSimpleField )
    {
        SimpleFieldPtr poKmlSimpleField = nullptr;

        if( (poKmlSimpleField =
                 FieldDef2kml( poField, m_poOgrDS->GetKmlFactory() )) )
        {
            if( !m_poKmlSchema )
            {
                /***** Create a new schema *****/
                KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();

                m_poKmlSchema = poKmlFactory->CreateSchema();

                /***** Set the id on the new schema *****/
                std::string oKmlSchemaID =
                    OGRLIBKMLGetSanitizedNCName(m_pszName);
                oKmlSchemaID.append( ".schema" );
                m_poKmlSchema->set_id( oKmlSchemaID );
            }

            m_poKmlSchema->add_simplefield( poKmlSimpleField );
        }
    }

    m_poOgrFeatureDefn->AddFieldDefn( poField );

    /***** mark as updated *****/
    m_poOgrDS->Updated();

    return OGRERR_NONE;
}

/******************************************************************************
 Method to write the datasource to disk.

 Args:      none

 Returns    nothing

******************************************************************************/

OGRErr OGRLIBKMLLayer::SyncToDisk()
{
    m_poOgrDS->FlushCache(false);
    return OGRERR_NONE;
}

/******************************************************************************
 Method to get a layers style table.

 Args:          none

 Returns:       pointer to the layers style table, or NULL if it does
                not have one

******************************************************************************/

OGRStyleTable *OGRLIBKMLLayer::GetStyleTable()
{
    return m_poStyleTable;
}

/******************************************************************************
 Method to write a style table to a layer.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

 Note: This method assumes ownership of the style table.
******************************************************************************/

void OGRLIBKMLLayer::SetStyleTableDirectly( OGRStyleTable * poStyleTable )
{
    if( !bUpdate || !m_poKmlLayer )
        return;

    KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();

    if( m_poStyleTable )
        delete m_poStyleTable;

    m_poStyleTable = poStyleTable;

    if( m_poKmlLayer->IsA( kmldom::Type_Document ) )
    {
        /***** delete all the styles *****/
        DocumentPtr poKmlDocument = AsDocument( m_poKmlLayer );
        const int nKmlStyles =
            static_cast<int>(poKmlDocument->get_schema_array_size());

        for( int iKmlStyle = nKmlStyles - 1; iKmlStyle >= 0; iKmlStyle-- )
        {
            poKmlDocument->DeleteStyleSelectorAt( iKmlStyle );
        }

        /***** add the new style table to the document *****/
        styletable2kml( poStyleTable, poKmlFactory,
                        AsContainer( poKmlDocument ) );
    }

    /***** mark as updated *****/
    m_poOgrDS->Updated();
}

/******************************************************************************
 Method to write a style table to a layer.

 Args:          poStyleTable    pointer to the style table to add

 Returns:       nothing

 Note:  This method copies the style table, and the user will still be
        responsible for its destruction.
******************************************************************************/

void OGRLIBKMLLayer::SetStyleTable( OGRStyleTable * poStyleTable )
{
    if( !bUpdate || !m_poKmlLayer )
        return;

    if( poStyleTable )
        SetStyleTableDirectly( poStyleTable->Clone() );
    else
        SetStyleTableDirectly( nullptr );
}

/******************************************************************************
 Test if capability is available.

 Args:          pszCap  layer capability name to test

 Returns:       True if the layer supports the capability, otherwise false

******************************************************************************/

int OGRLIBKMLLayer::TestCapability( const char *pszCap )
{
    int result = FALSE;

    // TODO(schwehr): The false statements are weird.
    if( EQUAL( pszCap, OLCRandomRead ) )
        result = FALSE;
    else if( EQUAL( pszCap, OLCSequentialWrite ) )
        result = bUpdate;
    else if( EQUAL( pszCap, OLCRandomWrite ) )
        result = bUpdate && m_poKmlUpdate;
    else if( EQUAL( pszCap, OLCFastFeatureCount ) )
        result = FALSE;
    else if( EQUAL( pszCap, OLCFastSetNextByIndex ) )
        result = FALSE;
    else if( EQUAL( pszCap, OLCCreateField ) )
        result = bUpdate;
    else if( EQUAL( pszCap, OLCDeleteFeature ) )
        result = bUpdate && m_poKmlUpdate;
    else if( EQUAL( pszCap, OLCStringsAsUTF8 ) )
        result = TRUE;

    return result;
}

/************************************************************************/
/*                        LaunderFieldNames()                           */
/************************************************************************/

CPLString OGRLIBKMLLayer::LaunderFieldNames( CPLString osName )
{
    CPLString osLaunderedName;
    for( int i = 0; i < static_cast<int>(osName.size()); i++ )
    {
        const char ch = osName[i];
        if( (ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch == '_') )
            osLaunderedName += ch;
        else
            osLaunderedName += "_";
    }
    return osLaunderedName;
}

/************************************************************************/
/*                            SetLookAt()                               */
/************************************************************************/

void OGRLIBKMLLayer::SetLookAt( const char* pszLookatLongitude,
                                const char* pszLookatLatitude,
                                const char* pszLookatAltitude,
                                const char* pszLookatHeading,
                                const char* pszLookatTilt,
                                const char* pszLookatRange,
                                const char* pszLookatAltitudeMode )
{
    KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
    LookAtPtr lookAt = poKmlFactory->CreateLookAt();
    lookAt->set_latitude(CPLAtof(pszLookatLatitude));
    lookAt->set_longitude(CPLAtof(pszLookatLongitude));
    if( pszLookatAltitude != nullptr )
        lookAt->set_altitude(CPLAtof(pszLookatAltitude));
    if( pszLookatHeading != nullptr )
        lookAt->set_heading(CPLAtof(pszLookatHeading));
    if( pszLookatTilt != nullptr )
    {
        double dfTilt = CPLAtof(pszLookatTilt);
        if( dfTilt >= 0 && dfTilt <= 90 )
            lookAt->set_tilt(dfTilt);
        else
            CPLError(CE_Warning, CPLE_AppDefined, "Invalid value for tilt: %s",
                     pszLookatTilt);
    }
    lookAt->set_range(CPLAtof(pszLookatRange));
    if( pszLookatAltitudeMode != nullptr )
    {
        int isGX = FALSE;
        const int iAltitudeMode =
            kmlAltitudeModeFromString(pszLookatAltitudeMode, isGX);
        if( iAltitudeMode != kmldom::ALTITUDEMODE_CLAMPTOGROUND &&
            pszLookatAltitude == nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Lookat altitude should be present for altitudeMode = %s",
                     pszLookatAltitudeMode);
        }
        else if( isGX )
        {
            lookAt->set_gx_altitudemode(iAltitudeMode);
        }
        else
        {
            lookAt->set_altitudemode(iAltitudeMode);
        }
    }

    m_poKmlLayer->set_abstractview(lookAt);
}

/************************************************************************/
/*                            SetCamera()                               */
/************************************************************************/

void OGRLIBKMLLayer::SetCamera( const char* pszCameraLongitude,
                                const char* pszCameraLatitude,
                                const char* pszCameraAltitude,
                                const char* pszCameraHeading,
                                const char* pszCameraTilt,
                                const char* pszCameraRoll,
                                const char* pszCameraAltitudeMode )
{
    int isGX = FALSE;
    int iAltitudeMode = kmlAltitudeModeFromString(pszCameraAltitudeMode, isGX);
    if( isGX == FALSE && iAltitudeMode == kmldom::ALTITUDEMODE_CLAMPTOGROUND )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Camera altitudeMode should be different from %s",
                 pszCameraAltitudeMode);
        return;
    }
    KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
    CameraPtr camera = poKmlFactory->CreateCamera();
    camera->set_latitude(CPLAtof(pszCameraLatitude));
    camera->set_longitude(CPLAtof(pszCameraLongitude));
    camera->set_altitude(CPLAtof(pszCameraAltitude));
    if( pszCameraHeading != nullptr )
        camera->set_heading(CPLAtof(pszCameraHeading));

    if( pszCameraTilt != nullptr )
    {
        double dfTilt = CPLAtof(pszCameraTilt);
        if( dfTilt >= 0 && dfTilt <= 90 )
            camera->set_tilt(dfTilt);
        else
            CPLError(CE_Warning, CPLE_AppDefined, "Invalid value for tilt: %s",
                     pszCameraTilt);
    }

    if( pszCameraRoll != nullptr )
        camera->set_roll(CPLAtof(pszCameraRoll));
    if( isGX )
        camera->set_gx_altitudemode(iAltitudeMode);
    else
        camera->set_altitudemode(iAltitudeMode);

    m_poKmlLayer->set_abstractview(camera);
}

/************************************************************************/
/*                         SetWriteRegion()                             */
/************************************************************************/

void OGRLIBKMLLayer::SetWriteRegion( double dfMinLodPixels,
                                     double dfMaxLodPixels,
                                     double dfMinFadeExtent,
                                     double dfMaxFadeExtent )
{
    m_bWriteRegion = true;
    m_bRegionBoundsAuto = true;
    m_dfRegionMinLodPixels = dfMinLodPixels;
    m_dfRegionMaxLodPixels = dfMaxLodPixels;
    m_dfRegionMinFadeExtent = dfMinFadeExtent;
    m_dfRegionMaxFadeExtent = dfMaxFadeExtent;
}

/************************************************************************/
/*                          SetRegionBounds()                           */
/************************************************************************/

void OGRLIBKMLLayer::SetRegionBounds( double dfMinX, double dfMinY,
                                      double dfMaxX, double dfMaxY )
{
    m_bRegionBoundsAuto = false;
    m_dfRegionMinX = dfMinX;
    m_dfRegionMinY = dfMinY;
    m_dfRegionMaxX = dfMaxX;
    m_dfRegionMaxY = dfMaxY;
}

/************************************************************************/
/*                            Finalize()                                */
/************************************************************************/

void OGRLIBKMLLayer::Finalize( DocumentPtr poKmlDocument )
{
    KmlFactory * const poKmlFactory = m_poOgrDS->GetKmlFactory();

    if( m_bWriteRegion && m_dfRegionMinX < m_dfRegionMaxX )
    {
        RegionPtr region = poKmlFactory->CreateRegion();

        LatLonAltBoxPtr box = poKmlFactory->CreateLatLonAltBox();
        box->set_west(m_dfRegionMinX);
        box->set_east(m_dfRegionMaxX);
        box->set_south(m_dfRegionMinY);
        box->set_north(m_dfRegionMaxY);
        region->set_latlonaltbox(box);

        LodPtr lod = poKmlFactory->CreateLod();
        lod->set_minlodpixels(m_dfRegionMinLodPixels);
        lod->set_maxlodpixels(m_dfRegionMaxLodPixels);
        if( (m_dfRegionMinFadeExtent != 0 || m_dfRegionMaxFadeExtent != 0) &&
            m_dfRegionMinFadeExtent + m_dfRegionMaxFadeExtent <
                m_dfRegionMaxLodPixels - m_dfRegionMinLodPixels )
        {
            lod->set_minfadeextent(m_dfRegionMinFadeExtent);
            lod->set_maxfadeextent(m_dfRegionMaxFadeExtent);
        }

        region->set_lod(lod);
        m_poKmlLayer->set_region(region);
    }

    createkmlliststyle(poKmlFactory,
                       GetName(),
                       m_poKmlLayer,
                       poKmlDocument,
                       osListStyleType,
                       osListStyleIconHref);
}

/************************************************************************/
/*                             LIBKMLGetUnits()                         */
/************************************************************************/

static int LIBKMLGetUnits( const char* pszUnits )
{
    if( EQUAL(pszUnits, "fraction") )
        return kmldom::UNITS_FRACTION;
    if( EQUAL(pszUnits, "pixels") )
        return  kmldom::UNITS_PIXELS;
    if( EQUAL(pszUnits, "insetPixels") )
        return  kmldom::UNITS_INSETPIXELS;
    return kmldom::UNITS_FRACTION;
}

/************************************************************************/
/*                         LIBKMLSetVec2()                              */
/************************************************************************/

static void LIBKMLSetVec2(
    kmldom::Vec2Ptr vec2, const char* pszX, const char* pszY,
    const char* pszXUnits, const char* pszYUnits )
{
    const double dfX = CPLAtof(pszX);
    const double dfY = CPLAtof(pszY);
    vec2->set_x(dfX);
    vec2->set_y(dfY);
    if( dfX <= 1 && dfY <= 1 )
    {
        if( pszXUnits == nullptr ) pszXUnits = "fraction";
        if( pszYUnits == nullptr ) pszYUnits = "fraction";
    }
    else
    {
        if( pszXUnits == nullptr ) pszXUnits = "pixels";
        if( pszYUnits == nullptr ) pszYUnits = "pixels";
    }
    vec2->set_xunits(LIBKMLGetUnits(pszXUnits));
    vec2->set_yunits(LIBKMLGetUnits(pszYUnits));
}

/************************************************************************/
/*                         SetScreenOverlay()                           */
/************************************************************************/

void OGRLIBKMLLayer::SetScreenOverlay( const char* pszSOHref,
                                       const char* pszSOName,
                                       const char* pszSODescription,
                                       const char* pszSOOverlayX,
                                       const char* pszSOOverlayY,
                                       const char* pszSOOverlayXUnits,
                                       const char* pszSOOverlayYUnits,
                                       const char* pszSOScreenX,
                                       const char* pszSOScreenY,
                                       const char* pszSOScreenXUnits,
                                       const char* pszSOScreenYUnits,
                                       const char* pszSOSizeX,
                                       const char* pszSOSizeY,
                                       const char* pszSOSizeXUnits,
                                       const char* pszSOSizeYUnits )
{
    KmlFactory *poKmlFactory = m_poOgrDS->GetKmlFactory();
    ScreenOverlayPtr so = poKmlFactory->CreateScreenOverlay();

    if( pszSOName != nullptr )
        so->set_name(pszSOName);
    if( pszSODescription != nullptr )
        so->set_description(pszSODescription);

    IconPtr icon = poKmlFactory->CreateIcon();
    icon->set_href(pszSOHref);
    so->set_icon(icon);

    if( pszSOOverlayX != nullptr && pszSOOverlayY != nullptr )
    {
        kmldom::OverlayXYPtr overlayxy = poKmlFactory->CreateOverlayXY();
        LIBKMLSetVec2(overlayxy, pszSOOverlayX, pszSOOverlayY,
                      pszSOOverlayXUnits, pszSOOverlayYUnits);
        so->set_overlayxy(overlayxy);
    }

    if( pszSOScreenX != nullptr && pszSOScreenY != nullptr )
    {
        kmldom::ScreenXYPtr screenxy = poKmlFactory->CreateScreenXY();
        LIBKMLSetVec2(screenxy, pszSOScreenX, pszSOScreenY,
                      pszSOScreenXUnits, pszSOScreenYUnits);
        so->set_screenxy(screenxy);
    }
    else
    {
        kmldom::ScreenXYPtr screenxy = poKmlFactory->CreateScreenXY();
        LIBKMLSetVec2(screenxy, "0.05", "0.05", nullptr, nullptr);
        so->set_screenxy(screenxy);
    }

    if( pszSOSizeX != nullptr && pszSOSizeY != nullptr )
    {
        kmldom::SizePtr sizexy = poKmlFactory->CreateSize();
        LIBKMLSetVec2(sizexy, pszSOSizeX, pszSOSizeY,
                      pszSOSizeXUnits, pszSOSizeYUnits);
        so->set_size(sizexy);
    }

    m_poKmlLayer->add_feature(so);
}

/************************************************************************/
/*                           SetListStyle()                              */
/************************************************************************/

void OGRLIBKMLLayer::SetListStyle( const char* pszListStyleType,
                                   const char* pszListStyleIconHref )
{
    osListStyleType = pszListStyleType ? pszListStyleType : "";
    osListStyleIconHref = pszListStyleIconHref ? pszListStyleIconHref : "";
}
