/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSSequenceLayer class, which implements the
 *           "Simple Sequence" access strategy.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/01/21 20:08:29  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_dods.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRDODSSequenceLayer()                        */
/************************************************************************/

OGRDODSSequenceLayer::OGRDODSSequenceLayer( OGRDODSDataSource *poDSIn, 
                                            const char *pszTargetIn,
                                            AttrTable *poOGRLayerInfoIn )

        : OGRDODSLayer( poDSIn, pszTargetIn, poOGRLayerInfoIn )

{
/* -------------------------------------------------------------------- */
/*      What is the layer name?                                         */
/* -------------------------------------------------------------------- */
    string oLayerName;
    const char *pszLayerName = pszTargetIn;

    if( poOGRLayerInfo != NULL )
    {
        oLayerName = poOGRLayerInfo->get_attr( "layer_name" );
        if( strlen(oLayerName.c_str()) > 0 )
            pszLayerName = oLayerName.c_str();
    }
        
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );

/* -------------------------------------------------------------------- */
/*      X/Y/Z fields.                                                   */
/* -------------------------------------------------------------------- */
    if( poOGRLayerInfo != NULL )
    {
        AttrTable *poField = poOGRLayerInfo->find_container("x_field");
        if( poField != NULL )
            oXField.Initialize( poField );

        poField = poOGRLayerInfo->find_container("y_field");
        if( poField != NULL )
            oYField.Initialize( poField );

        poField = poOGRLayerInfo->find_container("z_field");
        if( poField != NULL )
            oZField.Initialize( poField );
    }

/* -------------------------------------------------------------------- */
/*      If we have no layerinfo, then check if there are obvious x/y    */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    else
    {
        string x, y;

        x = pszTargetIn;
        x += ".lon";
        y = pszTargetIn;
        y += ".lat";
        
        if( poDS->oDDS.var( x ) != NULL && poDS->oDDS.var( y ) != NULL )
        {
            oXField.Initialize( x.c_str(), "dds" );
            oYField.Initialize( y.c_str(), "dds" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch the target variable.                                      */
/* -------------------------------------------------------------------- */
    Sequence *seq = dynamic_cast<Sequence *>(poDS->oDDS.var( pszTargetIn ));
    
/* -------------------------------------------------------------------- */
/*      Add fields for the contents of the sequence.                    */
/* -------------------------------------------------------------------- */
    Sequence::Vars_iter v_i;
    int iField = 0;

    for( v_i = seq->var_begin(); v_i != seq->var_end(); v_i++ )
    {
        if( BuildFields( *v_i, NULL ) )
            panFieldMapping[poFeatureDefn->GetFieldCount()-1] = iField;

        iField++;
    }
}

/************************************************************************/
/*                       ~OGRDODSSequenceLayer()                        */
/************************************************************************/

OGRDODSSequenceLayer::~OGRDODSSequenceLayer()

{
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/

BaseType *OGRDODSSequenceLayer::GetFieldValue( OGRDODSFieldDefn *poFDefn,
                                               int nFeatureId )

{
    Sequence *seq = dynamic_cast<Sequence *>(poTargetVar);

    if( !poFDefn->bValid )
        return NULL;

/* ==================================================================== */
/*      If we haven't tried to identify the field within the DataDDS    */
/*      yet, do so now.                                                 */
/* ==================================================================== */
    if( poFDefn->iFieldIndex == -1 )
    {
        if( EQUAL(poFDefn->pszFieldScope,"dds") 
            && strstr(poFDefn->pszFieldName,".") == NULL )
        {
            Sequence::Vars_iter v_i;

            for( v_i = seq->var_begin(), poFDefn->iFieldIndex = 0; 
                 v_i != seq->var_end(); 
                 v_i++, poFDefn->iFieldIndex++ )
            {
                if( EQUAL((*v_i)->name().c_str(),poFDefn->pszFieldName) )
                    break;
            }

            if( v_i == seq->var_end() )
                poFDefn->iFieldIndex = -2;
        }
    }

/* ==================================================================== */
/*      Fetch the actual value.                                         */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Simple case of a direct field within the sequence object.       */
/* -------------------------------------------------------------------- */
    if( poFDefn->iFieldIndex >= 0 )
    {
        return seq->var_value( nFeatureId, poFDefn->iFieldIndex );
    }

    return NULL;
}

/************************************************************************/
/*                       GetFieldValueAsDouble()                        */
/************************************************************************/

double OGRDODSSequenceLayer::GetFieldValueAsDouble( OGRDODSFieldDefn *poFDefn,
                                                    int nFeatureId )

{
    BaseType *poBT;

    poBT = GetFieldValue( poFDefn, nFeatureId );
    if( poBT == NULL )
        return 0.0;

    switch( poBT->type() )
    {
      case dods_byte_c:
      {
          signed char byVal;
          void *pValPtr = &byVal;
              
          poBT->buf2val( &pValPtr );
          return (double) byVal;
      }
      break;

      case dods_int16_c:
      {
          GInt16 nIntVal;
          void *pValPtr = &nIntVal;
              
          poBT->buf2val( &pValPtr );
          return (double) nIntVal;
      }
      break;

      case dods_uint16_c:
      {
          GUInt16 nIntVal;
          void *pValPtr = &nIntVal;
              
          poBT->buf2val( &pValPtr );
          return (double) nIntVal;
      }
      break;

      case dods_int32_c:
      {
          GInt32 nIntVal;
          void *pValPtr = &nIntVal;
              
          poBT->buf2val( &pValPtr );
          return (double) nIntVal;
      }
      break;

      case dods_uint32_c:
      {
          GUInt32 nIntVal;
          void *pValPtr = &nIntVal;
              
          poBT->buf2val( &pValPtr );
          return (double) nIntVal;
      }
      break;

      case dods_float32_c:
        return dynamic_cast<Float32 *>(poBT)->value();

      case dods_float64_c:
        return dynamic_cast<Float64 *>(poBT)->value();

      case dods_str_c:
      case dods_url_c:
      {
          string *poStrVal = NULL;
          double dfResult;

          poBT->buf2val( (void **) &poStrVal );
          dfResult = atof(poStrVal->c_str());
          delete poStrVal;
          return dfResult;
      }
      break;

      default:
        CPLAssert( FALSE );
        break;
    }

    return 0.0;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDODSSequenceLayer::GetFeature( long nFeatureId )

{
/* -------------------------------------------------------------------- */
/*      Ensure we have the dataset.                                     */
/* -------------------------------------------------------------------- */
    if( !ProvideDataDDS() )
        return NULL;

    Sequence *seq = dynamic_cast<Sequence *>(poTargetVar);

/* -------------------------------------------------------------------- */
/*      Create the feature being read.                                  */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    if( nFeatureId < 0 || nFeatureId >= seq->number_of_rows() )
        return NULL;

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Fetch the point information                                     */
/* -------------------------------------------------------------------- */
    if( oXField.bValid && oYField.bValid )
    {
        poFeature->SetGeometryDirectly( 
            new OGRPoint( GetFieldValueAsDouble( &oXField, nFeatureId ),
                          GetFieldValueAsDouble( &oYField, nFeatureId ),
                          GetFieldValueAsDouble( &oZField, nFeatureId ) ) );
    }

/* -------------------------------------------------------------------- */
/*      Process all the regular data fields.                            */
/* -------------------------------------------------------------------- */
    int      iField;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if( panFieldMapping[iField] == -1 )
            continue;

        BaseType *poFieldVar = seq->var_value( nFeatureId, 
                                               panFieldMapping[iField] );

        if( poFieldVar == NULL )
            continue;

        switch( poFieldVar->type() )
        {
          case dods_byte_c:
          {
              signed char byVal;
              void *pValPtr = &byVal;
              
              poFieldVar->buf2val( &pValPtr );
              poFeature->SetField( iField, byVal );
          }
          break;

          case dods_int16_c:
          {
              GInt16 nIntVal;
              void *pValPtr = &nIntVal;
              
              poFieldVar->buf2val( &pValPtr );
              poFeature->SetField( iField, nIntVal );
          }
          break;

          case dods_uint16_c:
          {
              GUInt16 nIntVal;
              void *pValPtr = &nIntVal;
              
              poFieldVar->buf2val( &pValPtr );
              poFeature->SetField( iField, nIntVal );
          }
          break;

          case dods_int32_c:
          {
              GInt32 nIntVal;
              void *pValPtr = &nIntVal;
              
              poFieldVar->buf2val( &pValPtr );
              poFeature->SetField( iField, nIntVal );
          }
          break;

          case dods_uint32_c:
          {
              GUInt32 nIntVal;
              void *pValPtr = &nIntVal;
              
              poFieldVar->buf2val( &pValPtr );
              poFeature->SetField( iField, (int) nIntVal );
          }
          break;

          case dods_float32_c:
            poFeature->SetField( iField, 
                                 dynamic_cast<Float32 *>(poFieldVar)->value());
            break;

          case dods_float64_c:
            poFeature->SetField( iField, 
                                 dynamic_cast<Float64 *>(poFieldVar)->value());
            break;

          case dods_str_c:
          case dods_url_c:
          {
              string *poStrVal = NULL;
              poFieldVar->buf2val( (void **) &poStrVal );
              poFeature->SetField( iField, poStrVal->c_str() );
              delete poStrVal;
          }
          break;

          default:
            break;
        }
    }
    
    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRDODSSequenceLayer::GetFeatureCount( int bForce )

{
    if( !bDataLoaded && !bForce )
        return -1;

    if( !ProvideDataDDS() )
        return -1;
    
    Sequence *seq = dynamic_cast<Sequence *>(poTargetVar);

    return seq->number_of_rows();
}
