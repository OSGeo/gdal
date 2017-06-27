/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGRKMLLayer class.
 * Author:   Christopher Condit, condit@sdsc.edu
 *           Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/* Function utility to dump OGRGeometry to KML text. */
char *OGR_G_ExportToKML( OGRGeometryH hGeometry, const char* pszAltitudeMode );

/************************************************************************/
/*                           OGRKMLLayer()                              */
/************************************************************************/

OGRKMLLayer::OGRKMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, bool bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRKMLDataSource *poDSIn ) :
    poDS_(poDSIn),
    poSRS_(poSRSIn ? new OGRSpatialReference(NULL) : NULL),
    poCT_(NULL),
    poFeatureDefn_(new OGRFeatureDefn( pszName )),
    iNextKMLId_(0),
    nTotalKMLCount_(-1),
    bWriter_(bWriterIn),
    nLayerNumber_(0),
    nWroteFeatureCount_(0),
    bSchemaWritten_(false),
    pszName_(CPLStrdup(pszName)),
    nLastAsked(-1),
    nLastCount(-1)
{
    // KML should be created as WGS84.
    if( poSRSIn != NULL )
    {
        poSRS_->SetWellKnownGeogCS( "WGS84" );
        if( !poSRS_->IsSame(poSRSIn) )
        {
            poCT_ = OGRCreateCoordinateTransformation( poSRSIn, poSRS_ );
            if( poCT_ == NULL && poDSIn->IsFirstCTError() )
            {
                // If we can't create a transformation, issue a warning - but
                // continue the transformation.
                char *pszWKT = NULL;

                poSRSIn->exportToPrettyWkt( &pszWKT, FALSE );

                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Failed to create coordinate transformation between the "
                    "input coordinate system and WGS84.  This may be because "
                    "they are not transformable, or because projection "
                    "services (PROJ.4 DLL/.so) could not be loaded.  "
                    "KML geometries may not render correctly.  "
                    "This message will not be issued any more."
                    "\nSource:\n%s\n",
                    pszWKT );

                CPLFree( pszWKT );
                poDSIn->IssuedFirstCTError();
            }
        }
    }

    SetDescription( poFeatureDefn_->GetName() );
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eReqType );
    if( poFeatureDefn_->GetGeomFieldCount() != 0 )
        poFeatureDefn_->GetGeomFieldDefn(0)->SetSpatialRef(poSRS_);

    OGRFieldDefn oFieldName( "Name", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldName );

    OGRFieldDefn oFieldDesc( "Description", OFTString );
    poFeatureDefn_->AddFieldDefn( &oFieldDesc );

    bClosedForWriting = !bWriterIn;
}

/************************************************************************/
/*                           ~OGRKMLLayer()                             */
/************************************************************************/

OGRKMLLayer::~OGRKMLLayer()
{
    if( NULL != poFeatureDefn_ )
        poFeatureDefn_->Release();

    if( NULL != poSRS_ )
        poSRS_->Release();

    if( NULL != poCT_ )
        delete poCT_;

    CPLFree( pszName_ );
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRKMLLayer::GetLayerDefn()
{
    return poFeatureDefn_;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRKMLLayer::ResetReading()
{
    iNextKMLId_ = 0;
    nLastAsked = -1;
    nLastCount = -1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRKMLLayer::GetNextFeature()
{
#ifndef HAVE_EXPAT
    return NULL;
#else
    /* -------------------------------------------------------------------- */
    /*      Loop till we find a feature matching our criteria.              */
    /* -------------------------------------------------------------------- */
    KML *poKMLFile = poDS_->GetKMLFile();
    if( poKMLFile == NULL )
        return NULL;

    poKMLFile->selectLayer(nLayerNumber_);

    while( true )
    {
        Feature *poFeatureKML =
            poKMLFile->getFeature(iNextKMLId_++, nLastAsked, nLastCount);

        if( poFeatureKML == NULL )
            return NULL;

        OGRFeature *poFeature = new OGRFeature( poFeatureDefn_ );

        if( poFeatureKML->poGeom )
        {
            poFeature->SetGeometryDirectly(poFeatureKML->poGeom);
            poFeatureKML->poGeom = NULL;
        }

        // Add fields.
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Name"),
                             poFeatureKML->sName.c_str() );
        poFeature->SetField( poFeatureDefn_->GetFieldIndex("Description"),
                             poFeatureKML->sDescription.c_str() );
        poFeature->SetFID( iNextKMLId_ - 1 );

        // Clean up.
        delete poFeatureKML;

        if( poFeature->GetGeometryRef() != NULL && poSRS_ != NULL )
        {
            poFeature->GetGeometryRef()->assignSpatialReference( poSRS_ );
        }

        // Check spatial/attribute filters.
        if( (m_poFilterGeom == NULL ||
             FilterGeometry( poFeature->GetGeometryRef() ) ) &&
            (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }

#endif /* HAVE_EXPAT */
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

#ifndef HAVE_EXPAT
GIntBig OGRKMLLayer::GetFeatureCount( int /* bForce */ ) { return 0; }
#else

GIntBig OGRKMLLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount(bForce);

    KML *poKMLFile = poDS_->GetKMLFile();
    if( NULL == poKMLFile )
        return 0;

    poKMLFile->selectLayer(nLayerNumber_);

    return poKMLFile->getNumFeatures();
}
#endif

/************************************************************************/
/*                           WriteSchema()                              */
/************************************************************************/

CPLString OGRKMLLayer::WriteSchema()
{
    if( bSchemaWritten_ )
        return "";

    CPLString osRet;

    OGRFeatureDefn *featureDefinition = GetLayerDefn();
    for( int j = 0; j < featureDefinition->GetFieldCount(); j++ )
    {
        OGRFieldDefn *fieldDefinition = featureDefinition->GetFieldDefn(j);

        if (NULL != poDS_->GetNameField() &&
            EQUAL(fieldDefinition->GetNameRef(), poDS_->GetNameField()) )
            continue;

        if (NULL != poDS_->GetDescriptionField() &&
            EQUAL(fieldDefinition->GetNameRef(), poDS_->GetDescriptionField()) )
            continue;

        if( osRet.empty() )
        {
            osRet += CPLSPrintf( "<Schema name=\"%s\" id=\"%s\">\n",
                                 pszName_, pszName_ );
        }

        const char* pszKMLType = NULL;
        const char* pszKMLEltName = NULL;
        // Match the OGR type to the GDAL type.
        switch (fieldDefinition->GetType())
        {
          case OFTInteger:
            pszKMLType = "int";
            pszKMLEltName = "SimpleField";
            break;
          case OFTIntegerList:
            pszKMLType = "int";
            pszKMLEltName = "SimpleArrayField";
            break;
          case OFTReal:
            pszKMLType = "float";
            pszKMLEltName = "SimpleField";
            break;
          case OFTRealList:
            pszKMLType = "float";
            pszKMLEltName = "SimpleArrayField";
            break;
          case OFTString:
            pszKMLType = "string";
            pszKMLEltName = "SimpleField";
            break;
          case OFTStringList:
            pszKMLType = "string";
            pszKMLEltName = "SimpleArrayField";
            break;
            //TODO: KML doesn't handle these data types yet...
          case OFTDate:
          case OFTTime:
          case OFTDateTime:
            pszKMLType = "string";
            pszKMLEltName = "SimpleField";
            break;

          default:
            pszKMLType = "string";
            pszKMLEltName = "SimpleField";
            break;
        }
        osRet += CPLSPrintf( "\t<%s name=\"%s\" type=\"%s\"></%s>\n",
                    pszKMLEltName, fieldDefinition->GetNameRef() ,pszKMLType, pszKMLEltName );
    }

    if( !osRet.empty() )
        osRet += CPLSPrintf( "%s", "</Schema>\n" );

    return osRet;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRKMLLayer::ICreateFeature( OGRFeature* poFeature )
{
    CPLAssert( NULL != poFeature );
    CPLAssert( NULL != poDS_ );

    if( !bWriter_ )
        return OGRERR_FAILURE;

    if( bClosedForWriting )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Interleaved feature adding to different layers is not supported");
        return OGRERR_FAILURE;
    }

    VSILFILE *fp = poDS_->GetOutputFP();
    CPLAssert( NULL != fp );

    if( poDS_->GetLayerCount() == 1 && nWroteFeatureCount_ == 0 )
    {
        CPLString osRet = WriteSchema();
        if( !osRet.empty() )
            VSIFPrintfL( fp, "%s", osRet.c_str() );
        bSchemaWritten_ = true;

        VSIFPrintfL( fp, "<Folder><name>%s</name>\n", pszName_);
    }

    VSIFPrintfL( fp, "  <Placemark>\n" );

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextKMLId_++ );

    // Find and write the name element
    if( NULL != poDS_->GetNameField() )
    {
        for( int iField = 0;
             iField < poFeatureDefn_->GetFieldCount();
             iField++ )
        {
            OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

            if( poFeature->IsFieldSetAndNotNull( iField )
                && EQUAL(poField->GetNameRef(), poDS_->GetNameField()) )
            {
                const char *pszRaw = poFeature->GetFieldAsString( iField );
                while( *pszRaw == ' ' )
                    pszRaw++;

                char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

                VSIFPrintfL( fp, "\t<name>%s</name>\n", pszEscaped);
                CPLFree( pszEscaped );
            }
        }
    }

    if( NULL != poDS_->GetDescriptionField() )
    {
        for( int iField = 0;
             iField < poFeatureDefn_->GetFieldCount();
             iField++ )
        {
            OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

            if( poFeature->IsFieldSetAndNotNull( iField )
                && EQUAL(poField->GetNameRef(), poDS_->GetDescriptionField()) )
            {
                const char *pszRaw = poFeature->GetFieldAsString( iField );
                while( *pszRaw == ' ' )
                    pszRaw++;

                char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

                VSIFPrintfL( fp, "\t<description>%s</description>\n",
                             pszEscaped);
                CPLFree( pszEscaped );
            }
        }
    }

    OGRwkbGeometryType eGeomType = wkbNone;
    if( poFeature->GetGeometryRef() != NULL )
        eGeomType = wkbFlatten(poFeature->GetGeometryRef()->getGeometryType());

    if( wkbPolygon == eGeomType
        || wkbMultiPolygon == eGeomType
        || wkbLineString == eGeomType
        || wkbMultiLineString == eGeomType )
    {
        OGRStylePen *poPen = NULL;
        OGRStyleMgr oSM;

        if( poFeature->GetStyleString() != NULL )
        {
            oSM.InitFromFeature( poFeature );

            for( int i = 0; i < oSM.GetPartCount(); i++ )
            {
                OGRStyleTool *poTool = oSM.GetPart(i);
                if (poTool && poTool->GetType() == OGRSTCPen )
                {
                    poPen = (OGRStylePen*) poTool;
                    break;
                }
                delete poTool;
            }
        }

        VSIFPrintfL( fp, "\t<Style>");
        if( poPen != NULL )
        {
            bool bHasWidth = false;
            GBool bDefault = FALSE;

            /* Require width to be returned in pixel */
            poPen->SetUnit(OGRSTUPixel);
            double fW = poPen->Width(bDefault);
            if( bDefault )
                fW = 1;
            else
                bHasWidth = true;
            const char* pszColor = poPen->Color(bDefault);
            const int nColorLen = static_cast<int>(CPLStrnlen(pszColor, 10));
            if( pszColor != NULL &&
                pszColor[0] == '#' &&
                !bDefault && nColorLen >= 7)
            {
                char acColor[9] = {0};
                /* Order of KML color is aabbggrr, whereas OGR color is #rrggbb[aa] ! */
                if(nColorLen == 9)
                {
                    acColor[0] = pszColor[7]; /* A */
                    acColor[1] = pszColor[8];
                }
                else
                {
                    acColor[0] = 'F';
                    acColor[1] = 'F';
                }
                acColor[2] = pszColor[5]; /* B */
                acColor[3] = pszColor[6];
                acColor[4] = pszColor[3]; /* G */
                acColor[5] = pszColor[4];
                acColor[6] = pszColor[1]; /* R */
                acColor[7] = pszColor[2];
                VSIFPrintfL( fp, "<LineStyle><color>%s</color>", acColor);
                if (bHasWidth)
                    VSIFPrintfL( fp, "<width>%g</width>", fW);
                VSIFPrintfL( fp, "</LineStyle>");
            }
            else
            {
                VSIFPrintfL(
                    fp, "<LineStyle><color>ff0000ff</color></LineStyle>");
            }
        }
        else
        {
            VSIFPrintfL( fp, "<LineStyle><color>ff0000ff</color></LineStyle>");
        }
        delete poPen;
        // If we're dealing with a polygon, add a line style that will stand out
        // a bit.
        VSIFPrintfL( fp, "<PolyStyle><fill>0</fill></PolyStyle></Style>\n" );
    }

    bool bHasFoundOtherField = false;

    // Write all fields as SchemaData
    for( int iField = 0; iField < poFeatureDefn_->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poField = poFeatureDefn_->GetFieldDefn( iField );

        if( poFeature->IsFieldSetAndNotNull( iField ))
        {
            if (NULL != poDS_->GetNameField() &&
                EQUAL(poField->GetNameRef(), poDS_->GetNameField()) )
                continue;

            if (NULL != poDS_->GetDescriptionField() &&
                EQUAL(poField->GetNameRef(), poDS_->GetDescriptionField()) )
                continue;

            if( !bHasFoundOtherField )
            {
                VSIFPrintfL( fp, "\t<ExtendedData><SchemaData schemaUrl=\"#%s\">\n", pszName_ );
                bHasFoundOtherField = true;
            }
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            while( *pszRaw == ' ' )
                pszRaw++;

            char *pszEscaped = NULL;
            if (poFeatureDefn_->GetFieldDefn(iField)->GetType() == OFTReal)
            {
                pszEscaped = CPLStrdup( pszRaw );
            }
            else
            {
                pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );
            }

            VSIFPrintfL( fp, "\t\t<SimpleData name=\"%s\">%s</SimpleData>\n",
                        poField->GetNameRef(), pszEscaped);

            CPLFree( pszEscaped );
        }
    }

    if( bHasFoundOtherField )
    {
        VSIFPrintfL( fp, "\t</SchemaData></ExtendedData>\n" );
    }

    // Write out Geometry - for now it isn't indented properly.
    if( poFeature->GetGeometryRef() != NULL )
    {
        char* pszGeometry = NULL;
        OGREnvelope sGeomBounds;
        OGRGeometry *poWGS84Geom = NULL;

        if (NULL != poCT_)
        {
            poWGS84Geom = poFeature->GetGeometryRef()->clone();
            poWGS84Geom->transform( poCT_ );
        }
        else
        {
            poWGS84Geom = poFeature->GetGeometryRef();
        }

        // TODO - porting
        // pszGeometry = poFeature->GetGeometryRef()->exportToKML();
        pszGeometry =
            OGR_G_ExportToKML( (OGRGeometryH)poWGS84Geom,
                               poDS_->GetAltitudeMode());
        if( pszGeometry != NULL )
        {
            VSIFPrintfL( fp, "      %s\n", pszGeometry );
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Export of geometry to KML failed");
        }
        CPLFree( pszGeometry );

        poWGS84Geom->getEnvelope( &sGeomBounds );
        poDS_->GrowExtents( &sGeomBounds );

        if (NULL != poCT_)
        {
            delete poWGS84Geom;
        }
    }

    VSIFPrintfL( fp, "  </Placemark>\n" );
    nWroteFeatureCount_++;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRKMLLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap, OLCSequentialWrite) )
    {
        return bWriter_;
    }
    else if( EQUAL(pszCap, OLCCreateField) )
    {
        return bWriter_ && iNextKMLId_ == 0;
    }
    else if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
//        if( poFClass == NULL
//            || m_poFilterGeom != NULL
//            || m_poAttrQuery != NULL )
            return FALSE;

//        return poFClass->GetFeatureCount() != -1;
    }

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRKMLLayer::CreateField( OGRFieldDefn *poField,
                                 CPL_UNUSED int bApproxOK )
{
    if( !bWriter_ || iNextKMLId_ != 0 )
        return OGRERR_FAILURE;

    OGRFieldDefn oCleanCopy( poField );
    poFeatureDefn_->AddFieldDefn( &oCleanCopy );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetLayerNumber()                           */
/************************************************************************/

void OGRKMLLayer::SetLayerNumber( int nLayer )
{
    nLayerNumber_ = nLayer;
}
