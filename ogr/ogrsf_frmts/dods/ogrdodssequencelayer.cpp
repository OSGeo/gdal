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
 * Revision 1.3  2004/01/29 21:01:03  warmerda
 * added sequences within sequences support
 *
 * Revision 1.2  2004/01/22 21:16:06  warmerda
 * fixed up auto-lat/lon support
 *
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
            oXField.Initialize( "lon", "dds" );
            oYField.Initialize( "lat", "dds" );
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
        if( BuildFields( *v_i, NULL, NULL ) )
            papoFields[poFeatureDefn->GetFieldCount()-1]->iFieldIndex = iField;

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
/*                            BuildFields()                             */
/*                                                                      */
/*      Build the field definition or definitions corresponding to      */
/*      the passed variable and it's children (if it has them).         */
/************************************************************************/

int OGRDODSSequenceLayer::BuildFields( BaseType *poTargetVar, 
                                       const char *pszPathToVar,
                                       const char *pszPathToSequence )
    
{
    OGRFieldDefn oField( "", OFTInteger );

/* -------------------------------------------------------------------- */
/*      Setup field name, including path if non-local.                  */
/* -------------------------------------------------------------------- */
    if( pszPathToVar == NULL )
        oField.SetName( poTargetVar->name().c_str() );
    else
        oField.SetName( CPLSPrintf( "%s.%s", pszPathToVar, 
                                    poTargetVar->name().c_str() ) );
                                    
/* -------------------------------------------------------------------- */
/*      Capture this field definition.                                  */
/* -------------------------------------------------------------------- */
    switch( poTargetVar->type() )
    {
      case dods_byte_c:
      case dods_int16_c:
      case dods_uint16_c:
      case dods_int32_c:
      case dods_uint32_c:
        if( pszPathToSequence )
            oField.SetType( OFTIntegerList );
        else
            oField.SetType( OFTInteger );
        break;

      case dods_float32_c:
      case dods_float64_c:
        if( pszPathToSequence )
            oField.SetType( OFTRealList );
        else
            oField.SetType( OFTReal );
        break;

      case dods_str_c:
      case dods_url_c:
        if( pszPathToSequence )
            oField.SetType( OFTStringList );
        else
            oField.SetType( OFTString );
        break;

      case dods_sequence_c:
      {
          Sequence *seq = dynamic_cast<Sequence *>( poTargetVar );
          Sequence::Vars_iter v_i;

          // We don't support a 3rd level of sequence nesting.
          if( pszPathToSequence != NULL )
              return FALSE;

          for( v_i = seq->var_begin(); v_i != seq->var_end(); v_i++ )
          {
              BuildFields( *v_i, oField.GetNameRef(), oField.GetNameRef() );
          }
      }
      return FALSE;

      default:
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add field to feature defn, and capture mapping.                 */
/* -------------------------------------------------------------------- */
    poFeatureDefn->AddFieldDefn( &oField );

    papoFields = (OGRDODSFieldDefn **) 
        CPLRealloc( papoFields, sizeof(void*) * poFeatureDefn->GetFieldCount());

    papoFields[poFeatureDefn->GetFieldCount()-1] = 
        new OGRDODSFieldDefn();

    papoFields[poFeatureDefn->GetFieldCount()-1]->Initialize(
        oField.GetNameRef(), "dds" );

    
    if( pszPathToSequence )
        papoFields[poFeatureDefn->GetFieldCount()-1]->pszPathToSequence 
            = CPLStrdup( pszPathToSequence );

    return TRUE;
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/

BaseType *OGRDODSSequenceLayer::GetFieldValue( OGRDODSFieldDefn *poFDefn,
                                               int nFeatureId,
                                               Sequence *seq )

{
    if( seq == NULL )
        seq = dynamic_cast<Sequence *>(poTargetVar);

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

/* -------------------------------------------------------------------- */
/*      More complex case where we need to drill down by name.          */
/* -------------------------------------------------------------------- */
    const char *pszNameRemain;

    if( poFDefn->pszPathToSequence != NULL )
    {
        CPLAssert( strlen(poFDefn->pszFieldName) 
                   > strlen(poFDefn->pszPathToSequence)+1 );

        pszNameRemain = 
            poFDefn->pszFieldName + strlen(poFDefn->pszPathToSequence)+1;
    }
    else
        pszNameRemain = poFDefn->pszFieldName;

    return seq->var_value( nFeatureId, pszNameRemain );
}

/************************************************************************/
/*                       GetFieldValueAsDouble()                        */
/************************************************************************/

double OGRDODSSequenceLayer::GetFieldValueAsDouble( OGRDODSFieldDefn *poFDefn,
                                                    int nFeatureId )

{
    BaseType *poBT;

    poBT = GetFieldValue( poFDefn, nFeatureId, NULL );
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
        if( papoFields[iField]->pszPathToSequence )
            continue;

        BaseType *poFieldVar = GetFieldValue( papoFields[iField], nFeatureId,
                                              NULL );

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
    
/* -------------------------------------------------------------------- */
/*      Handle data nested in sequences.                                */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRDODSFieldDefn *poFD = papoFields[iField];

        if( poFD->pszPathToSequence == NULL )
            continue;

        CPLAssert( strlen(poFD->pszPathToSequence) 
                   < strlen(poFD->pszFieldName)-1 );

/* -------------------------------------------------------------------- */
/*      Get the sequence out of which this variable will be collected.  */
/* -------------------------------------------------------------------- */
        BaseType *poFieldVar = seq->var_value( nFeatureId, 
                                               poFD->pszPathToSequence );
        Sequence *poSubSeq;
        int nSubSeqCount;

        if( poFieldVar == NULL )
            continue;

        poSubSeq = dynamic_cast<Sequence *>( poFieldVar );
        if( poSubSeq == NULL )
            continue;

        nSubSeqCount = poSubSeq->number_of_rows();
            
/* -------------------------------------------------------------------- */
/*      Allocate array to put values into.                              */
/* -------------------------------------------------------------------- */
        OGRFieldDefn *poOFD = poFeature->GetFieldDefnRef( iField );
        int *panIntList = NULL;
        double *padfDblList = NULL;
        char **papszStrList = NULL;

        if( poOFD->GetType() == OFTIntegerList )
        {
            panIntList = (int *) CPLCalloc(sizeof(int),nSubSeqCount);
        }
        else if( poOFD->GetType() == OFTRealList )
        {
            padfDblList = (double *) CPLCalloc(sizeof(double),nSubSeqCount);
        }
        else if( poOFD->GetType() == OFTStringList )
        {
            papszStrList = (char **) CPLCalloc(sizeof(char*),nSubSeqCount+1);
        }
        else
            continue;

/* -------------------------------------------------------------------- */
/*      Loop, fetching subsequence values.                              */
/* -------------------------------------------------------------------- */
        int iSubIndex;
        for( iSubIndex = 0; iSubIndex < nSubSeqCount; iSubIndex++ )
        {
            poFieldVar = GetFieldValue( poFD, iSubIndex, poSubSeq );

            if( poFieldVar == NULL )
                continue;

            switch( poFieldVar->type() )
            {
              case dods_byte_c:
              {
                  signed char byVal;
                  void *pValPtr = &byVal;
                  
                  poFieldVar->buf2val( &pValPtr );
                  panIntList[iSubIndex] = byVal;
              }
              break;
              
              case dods_int16_c:
              {
                  GInt16 nIntVal;
                  void *pValPtr = &nIntVal;
                  
                  poFieldVar->buf2val( &pValPtr );
                  panIntList[iSubIndex] = nIntVal;
              }
              break;
              
              case dods_uint16_c:
              {
                  GUInt16 nIntVal;
                  void *pValPtr = &nIntVal;
                  
                  poFieldVar->buf2val( &pValPtr );
                  panIntList[iSubIndex] = nIntVal;
              }
              break;
              
              case dods_int32_c:
              {
                  GInt32 nIntVal;
                  void *pValPtr = &nIntVal;
                  
                  poFieldVar->buf2val( &pValPtr );
                  panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_uint32_c:
              {
                  GUInt32 nIntVal;
                  void *pValPtr = &nIntVal;
              
                  poFieldVar->buf2val( &pValPtr );
                  panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_float32_c:
                padfDblList[iSubIndex] = 
                    dynamic_cast<Float32 *>(poFieldVar)->value();
                break;

              case dods_float64_c:
                padfDblList[iSubIndex] = 
                    dynamic_cast<Float64 *>(poFieldVar)->value();
                break;

              case dods_str_c:
              case dods_url_c:
              {
                  string *poStrVal = NULL;
                  poFieldVar->buf2val( (void **) &poStrVal );
                  papszStrList[iSubIndex] = CPLStrdup( poStrVal->c_str() );
                  delete poStrVal;
              }
              break;

              default:
                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Apply back to feature.                                          */
/* -------------------------------------------------------------------- */
        if( poOFD->GetType() == OFTIntegerList )
        {
            poFeature->SetField( iField, nSubSeqCount, panIntList );
            CPLFree(panIntList);
        }
        else if( poOFD->GetType() == OFTRealList )
        {
            poFeature->SetField( iField, nSubSeqCount, padfDblList );
            CPLFree(padfDblList);
        }
        else if( poOFD->GetType() == OFTStringList )
        {
            poFeature->SetField( iField, papszStrList );
            CSLDestroy( papszStrList );
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
