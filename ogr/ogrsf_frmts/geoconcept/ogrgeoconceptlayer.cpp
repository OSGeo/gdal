/******************************************************************************
 * $Id: ogrgeoconceptlayer.cpp 
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptLayer class.
 * Author:   Didier Richard, didier.richard@ign.fr
 * Language: C++
 *
 ******************************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 * Copyright (c) 2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrgeoconceptlayer.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id: ogrgeoconceptlayer.cpp 00000 2007-11-03 16:08:14Z drichard $");

/************************************************************************/
/*                         OGRGeoconceptLayer()                         */
/************************************************************************/

OGRGeoconceptLayer::OGRGeoconceptLayer()
{
    _poFeatureDefn = NULL;
    _gcFeature = NULL;
}

/************************************************************************/
/*                          ~OGRGeoconceptLayer()                      */
/************************************************************************/

OGRGeoconceptLayer::~OGRGeoconceptLayer()

{
  CPLDebug( "GEOCONCEPT",
            "%ld features on layer %s.",
            GetSubTypeNbFeatures_GCIO(_gcFeature),
            _poFeatureDefn->GetName());

  if( _poFeatureDefn )
  {
    _poFeatureDefn->Release();
  }

  _gcFeature= NULL; /* deleted when OGCGeoconceptDatasource destroyed */
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

OGRErr OGRGeoconceptLayer::Open( GCSubType* Subclass )

{
    _gcFeature= Subclass;
    if( GetSubTypeFeatureDefn_GCIO(_gcFeature) )
    {
      _poFeatureDefn = (OGRFeatureDefn *)GetSubTypeFeatureDefn_GCIO(_gcFeature);
      _poFeatureDefn->Reference();
    }
    else
    {
      char pszln[512];
      int n, i;

      snprintf(pszln, 511, "%s.%s", GetSubTypeName_GCIO(_gcFeature),
                                    GetTypeName_GCIO(GetSubTypeType_GCIO(_gcFeature)));
      pszln[511]='\0';

      _poFeatureDefn = new OGRFeatureDefn(pszln);
      _poFeatureDefn->Reference();
      _poFeatureDefn->SetGeomType(wkbUnknown);

      if( (n= CountSubTypeFields_GCIO(_gcFeature))>0 )
      {
        GCField* aField= NULL;
        OGRFieldType oft;
        for( i= 0; i<n; i++ )
        {
          if( (aField= GetSubTypeField_GCIO(_gcFeature,i)) )
          {
            if( IsPrivateField_GCIO(aField) ) continue;
            switch(GetFieldKind_GCIO(aField)) {
            case vIntFld_GCIO      :
            case vPositionFld_GCIO :
              oft= OFTInteger;
              break;
            case vRealFld_GCIO     :
            case vLengthFld_GCIO   :
            case vAreaFld_GCIO     :
              oft= OFTReal;
              break;
            case vDateFld_GCIO     :
              oft= OFTDate;
              break;
            case vTimeFld_GCIO     :
              oft= OFTTime;
              break;
            case vMemoFld_GCIO     :
            case vChoiceFld_GCIO   :
            case vInterFld_GCIO    :
            default                :
              oft= OFTString;
              break;
            }
            OGRFieldDefn ofd(GetFieldName_GCIO(aField), oft);
            _poFeatureDefn->AddFieldDefn(&ofd);
          }
        }
      }
      SetSubTypeFeatureDefn_GCIO(_gcFeature, (OGRFeatureDefnH) _poFeatureDefn);
      _poFeatureDefn->Reference();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoconceptLayer::ResetReading()

{
    Rewind_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature),_gcFeature);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoconceptLayer::GetNextFeature()

{
    OGRFeature* poFeature = NULL;

    for( ;; )
    {
      if( !(poFeature= (OGRFeature*)ReadNextFeature_GCIO(_gcFeature)) )
      {
        /*
         * As several features are embed in the Geoconcept file,
         * when reaching the end of the feature type, resetting
         * the reader would allow reading other features :
         * ogrinfo -ro export.gxt FT1 FT2 ...
         * will be all features for all features types !
         */ 
        Rewind_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature),NULL);
        break;
      }
      if( (m_poFilterGeom == NULL || FilterGeometry( poFeature->GetGeometryRef() ) )
          &&
          (m_poAttrQuery == NULL  || m_poAttrQuery->Evaluate( poFeature )) )
      {
        break;
      }
      delete poFeature;
    }

    CPLDebug( "GEOCONCEPT",
              "FID : %ld\n"
              "%s  : %s",
              poFeature? poFeature->GetFID():-1L,
              poFeature && poFeature->GetFieldCount()>0? poFeature->GetFieldDefnRef(0)->GetNameRef():"-",
              poFeature && poFeature->GetFieldCount()>0? poFeature->GetFieldAsString(0):"");

    return poFeature;
}

/************************************************************************/
/*            OGRGeoconceptLayer_GetCompatibleFieldName()               */
/************************************************************************/

static char* OGRGeoconceptLayer_GetCompatibleFieldName(const char* pszName)
{
    char* pszCompatibleName = CPLStrdup(pszName);
    int i;
    for(i=0;pszCompatibleName[i] != 0;i++)
    {
        if (pszCompatibleName[i] == ' ')
            pszCompatibleName[i] = '_';
    }
    return pszCompatibleName;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoconceptLayer::CreateFeature( OGRFeature* poFeature )

{
    OGRwkbGeometryType eGt;
    OGRGeometry* poGeom;
    int nextField, iGeom, nbGeom, isSingle;

    poGeom= poFeature->GetGeometryRef();

    if (poGeom == NULL)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "NULL geometry not supported in Geoconcept, feature skipped.\n");
        return OGRERR_NONE;
    }

    eGt= poGeom->getGeometryType();
    switch( eGt ) {
    case wkbPoint                 :
    case wkbPoint25D              :
    case wkbMultiPoint            :
    case wkbMultiPoint25D         :
      if( GetSubTypeKind_GCIO(_gcFeature)==vUnknownItemType_GCIO )
      {
        SetSubTypeKind_GCIO(_gcFeature,vPoint_GCIO);
      }
      else if( GetSubTypeKind_GCIO(_gcFeature)!=vPoint_GCIO )
      {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't write non ponctual feature in a ponctual Geoconcept layer %s.\n",
                  _poFeatureDefn->GetName());
        return OGRERR_FAILURE;
      }
      break;
    case wkbLineString            :
    case wkbLineString25D         :
    case wkbMultiLineString       :
    case wkbMultiLineString25D    :
      if( GetSubTypeKind_GCIO(_gcFeature)==vUnknownItemType_GCIO )
      {
        SetSubTypeKind_GCIO(_gcFeature,vLine_GCIO);
      }
      else if( GetSubTypeKind_GCIO(_gcFeature)!=vLine_GCIO )
      {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't write non linear feature in a linear Geoconcept layer %s.\n",
                  _poFeatureDefn->GetName());
        return OGRERR_FAILURE;
      }
      break;
    case wkbPolygon               :
    case wkbPolygon25D            :
    case wkbMultiPolygon          :
    case wkbMultiPolygon25D       :
      if( GetSubTypeKind_GCIO(_gcFeature)==vUnknownItemType_GCIO )
      {
        SetSubTypeKind_GCIO(_gcFeature,vPoly_GCIO);
      }
      else if( GetSubTypeKind_GCIO(_gcFeature)!=vPoly_GCIO )
      {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't write non polygonal feature in a polygonal Geoconcept layer %s.\n",
                  _poFeatureDefn->GetName());
        return OGRERR_FAILURE;
      }
      break;
    case wkbUnknown               :
    case wkbGeometryCollection    :
    case wkbGeometryCollection25D :
    case wkbNone                  :
    case wkbLinearRing            :
    default                       :
      CPLError( CE_Warning, CPLE_AppDefined,
                "Geometry type %s not supported in Geoconcept, feature skipped.\n",
                OGRGeometryTypeToName(eGt) );
      return OGRERR_NONE;
    }
    if( GetSubTypeDim_GCIO(_gcFeature)==vUnknown3D_GCIO )
    {
      if( poGeom->getCoordinateDimension()==3 )
      {
        SetSubTypeDim_GCIO(_gcFeature,v3D_GCIO);
      }
      else
      {
        SetSubTypeDim_GCIO(_gcFeature,v2D_GCIO);
      }
    }

    switch( eGt ) {
    case wkbPoint                 :
    case wkbPoint25D              :
      nbGeom= 1;
      isSingle= TRUE;
      break;
    case wkbMultiPoint            :
    case wkbMultiPoint25D         :
      nbGeom= ((OGRGeometryCollection*)poGeom)->getNumGeometries();
      isSingle= FALSE;
      break;
    case wkbLineString            :
    case wkbLineString25D         :
      nbGeom= 1;
      isSingle= TRUE;
      break;
    case wkbMultiLineString       :
    case wkbMultiLineString25D    :
      nbGeom= ((OGRGeometryCollection*)poGeom)->getNumGeometries();
      isSingle= FALSE;
      break;
    case wkbPolygon               :
    case wkbPolygon25D            :
      nbGeom= 1;
      isSingle= TRUE;
      break;
    case wkbMultiPolygon          :
    case wkbMultiPolygon25D       :
      nbGeom= ((OGRGeometryCollection*)poGeom)->getNumGeometries();
      isSingle= FALSE;
      break;
    case wkbUnknown               :
    case wkbGeometryCollection    :
    case wkbGeometryCollection25D :
    case wkbNone                  :
    case wkbLinearRing            :
    default                       :
      nbGeom= 0;
      isSingle= FALSE;
      break;
    }

    /* 1st feature, let's write header : */
    if( GetGCMode_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature)) == vWriteAccess_GCIO &&
        GetFeatureCount(TRUE) == 0 )
      if( WriteHeader_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature))==NULL )
      {
        return OGRERR_FAILURE;
      }

    if( nbGeom>0 )
    {
      for( iGeom= 0; iGeom<nbGeom; iGeom++ )
      {
        nextField= StartWritingFeature_GCIO(_gcFeature,
                                            isSingle? poFeature->GetFID():OGRNullFID);
        while (nextField!=WRITECOMPLETED_GCIO)
        {
          if( nextField==WRITEERROR_GCIO )
          {
            return OGRERR_FAILURE;
          }
          if( nextField==GEOMETRYEXPECTED_GCIO )
          {
            OGRGeometry* poGeomPart=
                isSingle? poGeom:((OGRGeometryCollection*)poGeom)->getGeometryRef(iGeom);
            nextField= WriteFeatureGeometry_GCIO(_gcFeature,
                                                 (OGRGeometryH)poGeomPart);
          }
          else
          {
            int iF, nF;
            OGRFieldDefn *poField;
            GCField* theField= GetSubTypeField_GCIO(_gcFeature,nextField);
            /* for each field, find out its mapping ... */
            if( (nF= poFeature->GetFieldCount())>0 )
            {
              for( iF= 0; iF<nF; iF++ )
              {
                poField= poFeature->GetFieldDefnRef(iF);
                char* pszName = OGRGeoconceptLayer_GetCompatibleFieldName(poField->GetNameRef());
                if( EQUAL(pszName, GetFieldName_GCIO(theField)) )
                {
                  CPLFree(pszName);
                  nextField= WriteFeatureFieldAsString_GCIO(_gcFeature,
                                                            nextField,
                                                            poFeature->IsFieldSet(iF)?
                                                              poFeature->GetFieldAsString(iF)
                                                            :
                                                              NULL);
                  break;
                }
                CPLFree(pszName);
              }
              if( iF==nF )
              {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Can't find a field attached to %s on Geoconcept layer %s.\n",
                          GetFieldName_GCIO(theField), _poFeatureDefn->GetName());
                return OGRERR_FAILURE;
              }
            }
            else
            {
              nextField= WRITECOMPLETED_GCIO;
            }
          }
        }
        StopWritingFeature_GCIO(_gcFeature);
      }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGeoconceptLayer::GetSpatialRef()

{
    GCExportFileH* hGXT= GetSubTypeGCHandle_GCIO(_gcFeature);
    if( !hGXT ) return NULL;
    GCExportFileMetadata* Meta= GetGCMeta_GCIO(hGXT);
    if( !Meta ) return NULL;
    return (OGRSpatialReference*)GetMetaSRS_GCIO(Meta);
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/************************************************************************/

int OGRGeoconceptLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return GetSubTypeNbFeatures_GCIO(_gcFeature);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGeoconceptLayer::GetExtent( OGREnvelope* psExtent, int bForce )

{
    GCExtent* theExtent;

    theExtent= GetSubTypeExtent_GCIO( _gcFeature );
    psExtent->MinX= GetExtentULAbscissa_GCIO(theExtent);
    psExtent->MinY= GetExtentLROrdinate_GCIO(theExtent);
    psExtent->MaxX= GetExtentLRAbscissa_GCIO(theExtent);
    psExtent->MaxY= GetExtentULOrdinate_GCIO(theExtent);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoconceptLayer::TestCapability( const char* pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE; // the GetFeature() method does not work for this layer. TODO

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE; // the CreateFeature() method works for this layer.

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return FALSE; // the SetFeature() method is not operational on this layer.

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE; // this layer does not implement spatial filtering efficiently.

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE; // this layer can not return a feature count efficiently. FIXME

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return FALSE; // this layer can not return its data extent efficiently. FIXME

    else if( EQUAL(pszCap,OLCFastSetNextByIndex) )
        return FALSE; // this layer can not perform the SetNextByIndex() call efficiently.

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return FALSE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGeoconceptLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    if( GetGCMode_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature))==vReadAccess_GCIO )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields on a read-only Geoconcept layer.\n");
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Add field to layer                                              */
/* -------------------------------------------------------------------- */

    {
      /* check whether field exists ... */
      GCField* theField;
      char* pszName = OGRGeoconceptLayer_GetCompatibleFieldName(poField->GetNameRef());

      if( !(theField= FindFeatureField_GCIO(_gcFeature,pszName)) )
      {
        if( GetFeatureCount(TRUE) > 0 )
        {
          CPLError( CE_Failure, CPLE_NotSupported,
                    "Can't create field '%s' on existing Geoconcept layer '%s.%s'.\n",
                    pszName,
                    GetSubTypeName_GCIO(_gcFeature),
                    GetTypeName_GCIO(GetSubTypeType_GCIO(_gcFeature)) );
          CPLFree(pszName);
          return OGRERR_FAILURE;
        }
        if( GetSubTypeNbFields_GCIO(_gcFeature)==-1)
          SetSubTypeNbFields_GCIO(_gcFeature, 0L);
        if( !(theField= AddSubTypeField_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature),
                                             GetTypeName_GCIO(GetSubTypeType_GCIO(_gcFeature)),
                                             GetSubTypeName_GCIO(_gcFeature),
                                             FindFeatureFieldIndex_GCIO(_gcFeature,kNbFields_GCIO)
                                            +GetSubTypeNbFields_GCIO(_gcFeature)+1L,
                                             pszName,
                                             GetSubTypeNbFields_GCIO(_gcFeature)-999L,
                                             vUnknownItemType_GCIO, NULL, NULL)) )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Field '%s' could not be created for Feature %s.%s.\n",
                    pszName,
                    GetSubTypeName_GCIO(_gcFeature),
                    GetTypeName_GCIO(GetSubTypeType_GCIO(_gcFeature))
                  );
          CPLFree(pszName);
          return OGRERR_FAILURE;
        }
        SetSubTypeNbFields_GCIO(_gcFeature, GetSubTypeNbFields_GCIO(_gcFeature)+1L);
        _poFeatureDefn->AddFieldDefn(poField);
      }
      else
      {
        if( _poFeatureDefn->GetFieldIndex(GetFieldName_GCIO(theField))==-1 )
        {
          CPLError( CE_Failure, CPLE_AppDefined,
                    "Field %s not found for Feature %s.%s.\n",
                    GetFieldName_GCIO(theField),
                    GetSubTypeName_GCIO(_gcFeature),
                    GetTypeName_GCIO(GetSubTypeType_GCIO(_gcFeature))
                  );
          CPLFree(pszName);
          return OGRERR_FAILURE;
        }
      }

      CPLFree(pszName);
      pszName = NULL;

      /* check/update type ? */
      if( GetFieldKind_GCIO(theField)==vUnknownItemType_GCIO )
      {
        switch(poField->GetType()) {
        case OFTInteger        :
          SetFieldKind_GCIO(theField,vIntFld_GCIO);
          break;
        case OFTReal           :
          SetFieldKind_GCIO(theField,vRealFld_GCIO);
          break;
        case OFTDate           :
          SetFieldKind_GCIO(theField,vDateFld_GCIO);
          break;
        case OFTTime           :
        case OFTDateTime       :
          SetFieldKind_GCIO(theField,vTimeFld_GCIO);
          break;
        case OFTString         :
          SetFieldKind_GCIO(theField,vMemoFld_GCIO);
          break;
        case OFTIntegerList    :
        case OFTRealList       :
        case OFTStringList     :
        case OFTBinary         :
        default                :
          CPLError( CE_Failure, CPLE_NotSupported,
                    "Can't create fields of type %s on Geoconcept feature %s.\n",
                    OGRFieldDefn::GetFieldTypeName(poField->GetType()),
                    _poFeatureDefn->GetName() );
          return OGRERR_FAILURE;
        }
      }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRGeoconceptLayer::SyncToDisk()

{
    FFlush_GCIO(GetSubTypeGCHandle_GCIO(_gcFeature));
    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetSpatialRef()                          */
/************************************************************************/

void OGRGeoconceptLayer::SetSpatialRef( OGRSpatialReference *poSpatialRef )

{
    GCSysCoord* os, *ns;
    OGRSpatialReference* poSRS= GetSpatialRef();
    GCExportFileH* hGXT;
    GCExportFileMetadata* Meta;
    /*-----------------------------------------------------------------
     * Keep a copy of the OGRSpatialReference...
     * Note: we have to take the reference count into account...
     *----------------------------------------------------------------*/
    if( poSRS && poSRS->Dereference() == 0) delete poSRS;

    if( !poSpatialRef ) return;

    poSRS= poSpatialRef->Clone();
    if( !(hGXT= GetSubTypeGCHandle_GCIO(_gcFeature)) ) return;
    if( !(Meta= GetGCMeta_GCIO(hGXT)) ) return;
    os= GetMetaSysCoord_GCIO(Meta);
    ns= OGRSpatialReference2SysCoord_GCSRS((OGRSpatialReferenceH)poSRS);

    if( os && ns &&
        GetSysCoordSystemID_GCSRS(os)!=-1 &&
        (
          GetSysCoordSystemID_GCSRS(os)!=GetSysCoordSystemID_GCSRS(ns) ||
          GetSysCoordTimeZone_GCSRS(os)!=GetSysCoordTimeZone_GCSRS(ns)
        )
      )
    {
      CPLError( CE_Warning, CPLE_AppDefined,
                "Can't change SRS on Geoconcept layers.\n" );
      return;
    }

    if( os ) DestroySysCoord_GCSRS(&os);
    SetMetaSysCoord_GCIO(Meta, ns);
    SetMetaSRS_GCIO(Meta, (OGRSpatialReferenceH)poSRS);
    return;
}
