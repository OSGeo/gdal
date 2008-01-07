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
    _poSRS = NULL;
    _poFeatureDefn = NULL;
    _nTotalFeatures = 0;
    _pszFullName = NULL;
    _gcFeature = NULL;
    _hGCT = NULL;
}

/************************************************************************/
/*                          ~OGRGeoconceptLayer()                      */
/************************************************************************/

OGRGeoconceptLayer::~OGRGeoconceptLayer()

{
  CPLDebug( "GEOCONCEPT",
            "%ld features on layer %s.\n",
            _nTotalFeatures, _poFeatureDefn? _poFeatureDefn->GetName():"");
  _nTotalFeatures= 0;

  if( _poFeatureDefn )
  {
    _poFeatureDefn->Release();
  }
  if( _poSRS )
  {
    _poSRS->Release();
  }

  if( _pszFullName )
  {
    CPLFree( _pszFullName );
  }

  _gcFeature= NULL; /* deleted when _hGCT destroyed */

  if( _hGCT )
  {
    Close_GCIO(&_hGCT);
  }
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

OGRErr OGRGeoconceptLayer::Open( const char* pszName,
                                 const char* pszExt,
                                 const char* pszMode,
                                 const char* pszGCTName,
                                 const char* pszLayerName )

{
    GCField* aField;
    int n, i;

    if( (_hGCT= Open_GCIO(pszName,pszExt,pszMode,pszGCTName))==NULL )
    {
      return OGRERR_FAILURE;
    }
    _nTotalFeatures= GetGCNbObjects_GCIO(_hGCT);

    /* pszLayerName is -lco FEATURETYPE=Class.Subclass or NULL */
    if( !pszLayerName)
    {
       _poFeatureDefn = new OGRFeatureDefn("");
       _poFeatureDefn->Reference();
       return OGRERR_NONE;
    }
    if( !(_gcFeature= FindFeature_GCIO(_hGCT,pszLayerName)) )
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "Can't find feature %s as a Geoconcept layer within %s.\n",
                pszLayerName,
                pszGCTName? pszGCTName:"'not given schema'");
      return OGRERR_FAILURE;
    }
    _poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    _poFeatureDefn->Reference();
    /* can't make difference between single and multi-geometry for some types */
    switch(GetSubTypeKind_GCIO(_gcFeature)) {
    case vPoint_GCIO :
      switch(GetSubTypeDim_GCIO(_gcFeature)) {
      case v2D_GCIO  :
        _poFeatureDefn->SetGeomType(wkbPoint);
        break;
      case v3D_GCIO  :
      case v3DM_GCIO :
        _poFeatureDefn->SetGeomType(wkbPoint25D);
        break;
      default        :
        break;
      }
      break;
    case vLine_GCIO  :
      switch(GetSubTypeDim_GCIO(_gcFeature)) {
      case v2D_GCIO  :
        _poFeatureDefn->SetGeomType(wkbLineString);
        break;
      case v3D_GCIO  :
      case v3DM_GCIO :
        _poFeatureDefn->SetGeomType(wkbLineString25D);
        break;
      default        :
        break;
      }
      break;
    case vPoly_GCIO  :
      switch(GetSubTypeDim_GCIO(_gcFeature)) {
      case v2D_GCIO  :
        _poFeatureDefn->SetGeomType(wkbMultiPolygon);
        break;
      case v3D_GCIO  :
      case v3DM_GCIO :
        _poFeatureDefn->SetGeomType(wkbMultiPolygon25D);
        break;
      default        :
        break;
      }
      break;
    default          :
      _poFeatureDefn->SetGeomType(wkbUnknown);
      break;
    }
    if( (n= CountSubTypeFields_GCIO(_gcFeature))>0 )
    {
      OGRFieldType oft;
      for( i= 0; i<n; i++ )
      {
        if( (aField= GetSubTypeField_GCIO(_gcFeature,i)) )
        {
          /*
           * Keep the same order in Geoconcept and OGR : 
           */
#if 0
          if( IsPrivateField_GCIO(aField) ) continue;
#endif /* 0 */
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

    return OGRERR_NONE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoconceptLayer::ResetReading()

{
    Rewind_GCIO(_hGCT);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoconceptLayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;
    return poFeature;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoconceptLayer::CreateFeature( OGRFeature* poFeature )

{
    OGRwkbGeometryType eGt;
    OGRGeometry* poGeom;
    int nextField, iGeom, nbGeom, isSingle;

    if( !_gcFeature )
    {
      CPLError( CE_Failure, CPLE_NotSupported,
                "Can't write anonymous feature in a Geoconcept layer.\n");
      return OGRERR_FAILURE;
    }
    poGeom= poFeature->GetGeometryRef();
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

    if( nbGeom>0 )
    {
      for( iGeom= 0; iGeom<nbGeom; iGeom++ )
      {
        nextField= StartWritingFeature_GCIO(_hGCT,_gcFeature,
                                                  isSingle? poFeature->GetFID():OGRNullFID);
        while (nextField!=WRITECOMPLETED_GCIO)
        {
          if( nextField==WRITEERROR_GCIO )
          {
            return OGRERR_FAILURE;
          }
          if( nextField==GEOMETRYEXPECTED_GCIO )
          {
            OGRGeometry* poGeomPart;
            poGeomPart= isSingle? poGeom:((OGRGeometryCollection*)poGeom)->getGeometryRef(iGeom);
            nextField= WriteFeatureGeometry_GCIO(_hGCT,_gcFeature,poGeomPart);
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
                if( EQUAL(poField->GetNameRef(), GetFieldName_GCIO(theField)) )
                {
                  nextField= WriteFeatureFieldAsString_GCIO(_hGCT,_gcFeature,
                                                                  nextField,
                                                                  poFeature->IsFieldSet(iF)?
                                                                    poFeature->GetFieldAsString(iF)
                                                                  :
                                                                    NULL);
                  break;
                }
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
        StopWritingFeature_GCIO(_hGCT,_gcFeature);
        _nTotalFeatures= GetGCNbObjects_GCIO(_hGCT);
      }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGeoconceptLayer::GetSpatialRef()

{
    return _poSRS;
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
        return _nTotalFeatures;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGeoconceptLayer::GetExtent( OGREnvelope* psExtent, int bForce )

{
    GCExtent* theExtent;

    if( _hGCT == NULL )
        return OGRERR_FAILURE;

    theExtent= GetMetaExtent_GCIO( GetGCMeta_GCIO( _hGCT ) );
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
    if( GetGCMode_GCIO(_hGCT)==vReadAccess_GCIO )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields on a read-only Geoconcept layer.\n");
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Add field to layer                                              */
/* -------------------------------------------------------------------- */

    if( _gcFeature==NULL ) /* FIXME */
    {
      if( poField->GetType() == OFTInteger ||
          poField->GetType() == OFTReal    ||
          poField->GetType() == OFTString  ||
          poField->GetType() == OFTDate    ||
          poField->GetType() == OFTDateTime )
      {
        _poFeatureDefn->AddFieldDefn( poField );
      }
      else
      {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields of type %s on Geoconcept layers.\n",
                  OGRFieldDefn::GetFieldTypeName(poField->GetType()) );
        return OGRERR_FAILURE;
      }
    }
    else
    {
      /* check whether field exists ... */
      GCField* theField;
      int i;

      if( !(theField= FindFeatureField_GCIO(_gcFeature,poField->GetNameRef())) )
      {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Field %s not found for Feature %s.\n",
                  poField->GetNameRef(), _poFeatureDefn->GetName()
                );
        return OGRERR_FAILURE;
      }
      if( (i= _poFeatureDefn->GetFieldIndex(GetFieldName_GCIO(theField)))==-1 )
      {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Field %s not found for Feature %s.\n",
                  GetFieldName_GCIO(theField), _poFeatureDefn->GetName()
                );
        return OGRERR_FAILURE;
      }
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
          SetFieldKind_GCIO(theField,vTimeFld_GCIO);
          break;
        case OFTString         :
          SetFieldKind_GCIO(theField,vMemoFld_GCIO);
          break;
        case OFTIntegerList    :
        case OFTRealList       :
        case OFTStringList     :
        case OFTWideString     :
        case OFTWideStringList :
        case OFTBinary         :
        case OFTDateTime       :
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
    FFlush_GCIO(_hGCT);
    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetSpatialRef()                          */
/************************************************************************/

void OGRGeoconceptLayer::SetSpatialRef( OGRSpatialReference *poSpatialRef )

{
    int os, ns;
    /*-----------------------------------------------------------------
     * Keep a copy of the OGRSpatialReference...
     * Note: we have to take the reference count into account...
     *----------------------------------------------------------------*/
    if( _poSRS && _poSRS->Dereference() == 0) delete _poSRS;

    if( !poSpatialRef ) return;

    _poSRS= poSpatialRef->Clone();
    os= GetMetaSysCoord_GCIO(GetGCMeta_GCIO(_hGCT));
    ns= OGRSpatialReference2SysCoord_GCSRS((OGRSpatialReferenceH)_poSRS);

    if( os!=-1 && os!=ns )
    {
      CPLError( CE_Warning, CPLE_AppDefined,
                "Can't change SRS on Geoconcept layers.\n" );
      return;
    }

    SetMetaSysCoord_GCIO(GetGCMeta_GCIO(_hGCT), ns);
    if( GetGCMode_GCIO(_hGCT) == vWriteAccess_GCIO &&
        GetFeatureCount(TRUE) == 0 )
      if( WriteHeader_GCIO(_hGCT)==NULL )
      {
        return;
      }
}
