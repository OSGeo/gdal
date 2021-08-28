/******************************************************************************
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
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_dods.h"
#include "cpl_string.h"

#include <cmath>

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRDODSSequenceLayer()                        */
/************************************************************************/

OGRDODSSequenceLayer::OGRDODSSequenceLayer( OGRDODSDataSource *poDSIn,
                                            const char *pszTargetIn,
                                            AttrTable *poOGRLayerInfoIn ) :
    OGRDODSLayer( poDSIn, pszTargetIn, poOGRLayerInfoIn ),
    pszSubSeqPath("profile"), // hardcode for now.
    poSuperSeq(nullptr),
    iLastSuperSeq(-1),
    nRecordCount(-1),
    nSuperSeqCount(0),
    panSubSeqSize(nullptr)
{
/* -------------------------------------------------------------------- */
/*      What is the layer name?                                         */
/* -------------------------------------------------------------------- */
    string oLayerName;
    const char *pszLayerName = pszTargetIn;

    if( poOGRLayerInfo != nullptr )
    {
        oLayerName = poOGRLayerInfo->get_attr( "layer_name" );
        if( strlen(oLayerName.c_str()) > 0 )
            pszLayerName = oLayerName.c_str();
    }

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Fetch the target variable.                                      */
/* -------------------------------------------------------------------- */
    Sequence *seq = dynamic_cast<Sequence *>(poDS->poDDS->var( pszTargetIn ));

    poTargetVar = seq;
    poSuperSeq = FindSuperSequence( seq );

/* -------------------------------------------------------------------- */
/*      X/Y/Z fields.                                                   */
/* -------------------------------------------------------------------- */
    if( poOGRLayerInfo != nullptr )
    {
        AttrTable *poField = poOGRLayerInfo->find_container("x_field");
        if( poField != nullptr )
            oXField.Initialize( poField, poTargetVar, poSuperSeq );

        poField = poOGRLayerInfo->find_container("y_field");
        if( poField != nullptr )
            oYField.Initialize( poField, poTargetVar, poSuperSeq );

        poField = poOGRLayerInfo->find_container("z_field");
        if( poField != nullptr )
            oZField.Initialize( poField, poTargetVar, poSuperSeq );
    }

/* -------------------------------------------------------------------- */
/*      If we have no layerinfo, then check if there are obvious x/y    */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    else
    {
        string oTargName = pszTargetIn;
        string oSSTargName;
        string x, y;

        if( poSuperSeq != nullptr )
            oSSTargName = OGRDODSGetVarPath( poSuperSeq );
        else
            oSSTargName = "impossiblexxx";

        if( poDS->poDDS->var( oTargName + ".lon" ) != nullptr
            && poDS->poDDS->var( oTargName + ".lat" ) != nullptr )
        {
            oXField.Initialize( (oTargName + ".lon").c_str(), "dds",
                                poTargetVar, poSuperSeq );
            oYField.Initialize( (oTargName + ".lat").c_str(), "dds",
                                poTargetVar, poSuperSeq );
        }
        else if( poDS->poDDS->var( oSSTargName + ".lon" ) != nullptr
                 && poDS->poDDS->var( oSSTargName + ".lat" ) != nullptr )
        {
            oXField.Initialize( (oSSTargName + ".lon").c_str(), "dds",
                                poTargetVar, poSuperSeq );
            oYField.Initialize( (oSSTargName + ".lat").c_str(), "dds",
                                poTargetVar, poSuperSeq );
        }
    }

/* -------------------------------------------------------------------- */
/*      Add fields for the contents of the sequence.                    */
/* -------------------------------------------------------------------- */
    Sequence::Vars_iter v_i;

    for( v_i = seq->var_begin(); v_i != seq->var_end(); v_i++ )
        BuildFields( *v_i, nullptr, nullptr );

/* -------------------------------------------------------------------- */
/*      Add fields for the contents of the super-sequence if we have    */
/*      one.                                                            */
/* -------------------------------------------------------------------- */
    if( poSuperSeq != nullptr )
    {
        for( v_i = poSuperSeq->var_begin();
             v_i != poSuperSeq->var_end();
             v_i++ )
            BuildFields( *v_i, nullptr, nullptr );
    }
}

/************************************************************************/
/*                       ~OGRDODSSequenceLayer()                        */
/************************************************************************/

OGRDODSSequenceLayer::~OGRDODSSequenceLayer()

{
}

/************************************************************************/
/*                         FindSuperSequence()                          */
/*                                                                      */
/*      Are we a subsequence of a sequence?                             */
/************************************************************************/

Sequence *OGRDODSSequenceLayer::FindSuperSequence( BaseType *poChild )

{
    for( BaseType *poParent = poChild->get_parent();
         poParent != nullptr;
         poParent = poParent->get_parent() )
    {
        if( poParent->type() == dods_sequence_c )
        {
            return dynamic_cast<Sequence *>( poParent );
        }
    }

    return nullptr;
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build the field definition or definitions corresponding to      */
/*      the passed variable and its children (if it has them).         */
/************************************************************************/

bool OGRDODSSequenceLayer::BuildFields( BaseType *poFieldVar,
                                        const char *pszPathToVar,
                                        const char *pszPathToSequence )

{
    OGRFieldDefn oField( "", OFTInteger );

/* -------------------------------------------------------------------- */
/*      Setup field name, including path if non-local.                  */
/* -------------------------------------------------------------------- */
    if( pszPathToVar == nullptr )
        oField.SetName( poFieldVar->name().c_str() );
    else
        oField.SetName( CPLSPrintf( "%s.%s", pszPathToVar,
                                    poFieldVar->name().c_str() ) );

/* -------------------------------------------------------------------- */
/*      Capture this field definition.                                  */
/* -------------------------------------------------------------------- */
    switch( poFieldVar->type() )
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
          Sequence *seq = dynamic_cast<Sequence *>( poFieldVar );
          Sequence::Vars_iter v_i;

          // We don't support a 3rd level of sequence nesting.
          if( pszPathToSequence != nullptr )
              return false;

          // We don't explore down into the target sequence if we
          // are recursing from a supersequence.
          if( poFieldVar == poTargetVar )
              return false;

          for( v_i = seq->var_begin(); v_i != seq->var_end(); v_i++ )
          {
              BuildFields( *v_i, oField.GetNameRef(), oField.GetNameRef() );
          }
      }
      return false;

      default:
        return false;
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
        OGRDODSGetVarPath(poFieldVar).c_str(), "dds",
        poTargetVar, poSuperSeq );

    if( pszPathToSequence )
        papoFields[poFeatureDefn->GetFieldCount()-1]->pszPathToSequence
            = CPLStrdup( pszPathToSequence );

    return true;
}

/************************************************************************/
/*                           GetFieldValue()                            */
/************************************************************************/

BaseType *OGRDODSSequenceLayer::GetFieldValue( OGRDODSFieldDefn *poFDefn,
                                               int nFeatureId,
                                               Sequence *seq )

{
    if( seq == nullptr )
        seq = dynamic_cast<Sequence *>(poTargetVar);

    if( seq == nullptr || !poFDefn->bValid )
        return nullptr;

/* ==================================================================== */
/*      Fetch the actual value.                                         */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Simple case of a direct field within the sequence object.       */
/* -------------------------------------------------------------------- */
    if( poFDefn->iFieldIndex >= 0 && poFDefn->bRelativeToSequence )
    {
        return seq->var_value( nFeatureId, poFDefn->iFieldIndex );
    }
    else if( poSuperSeq != nullptr &&
             poFDefn->iFieldIndex >= 0 && poFDefn->bRelativeToSuperSequence )
    {
        return poSuperSeq->var_value( iLastSuperSeq, poFDefn->iFieldIndex );
    }

/* -------------------------------------------------------------------- */
/*      More complex case where we need to drill down by name.          */
/* -------------------------------------------------------------------- */
    if( poFDefn->bRelativeToSequence )
        return seq->var_value( nFeatureId, poFDefn->pszFieldName );
    else if( poSuperSeq != nullptr && poFDefn->bRelativeToSuperSequence )
        return poSuperSeq->var_value( iLastSuperSeq, poFDefn->pszFieldName );
    else
        return poDataDDS->var( poFDefn->pszFieldName );
}

/************************************************************************/
/*                          BaseTypeToDouble()                          */
/************************************************************************/

double OGRDODSSequenceLayer::BaseTypeToDouble( BaseType *poBT )

{
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
        return cpl::down_cast<Float32 *>(poBT)->value();

      case dods_float64_c:
        return cpl::down_cast<Float64 *>(poBT)->value();

      case dods_str_c:
      case dods_url_c:
      {
          string *poStrVal = nullptr;
          double dfResult;

          poBT->buf2val( (void **) &poStrVal );
          dfResult = CPLAtof(poStrVal->c_str());
          delete poStrVal;
          return dfResult;
      }
      break;

      default:
        CPLAssert( false );
        break;
    }

    return 0.0;
}

/************************************************************************/
/*                       GetFieldValueAsDouble()                        */
/************************************************************************/

double OGRDODSSequenceLayer::GetFieldValueAsDouble( OGRDODSFieldDefn *poFDefn,
                                                    int nFeatureId )

{
    BaseType *poBT = GetFieldValue( poFDefn, nFeatureId, nullptr );
    if( poBT == nullptr )
        return 0.0;

    return BaseTypeToDouble( poBT );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDODSSequenceLayer::GetFeature( GIntBig nFeatureId )

{
/* -------------------------------------------------------------------- */
/*      Ensure we have the dataset.                                     */
/* -------------------------------------------------------------------- */
    if( !ProvideDataDDS() )
        return nullptr;

    Sequence *seq = dynamic_cast<Sequence *>(poTargetVar);

/* -------------------------------------------------------------------- */
/*      Figure out what the super and subsequence number this           */
/*      feature will be, and validate it.  If there is not super        */
/*      sequence the feature id is the subsequence number.              */
/* -------------------------------------------------------------------- */
    int iSubSeq = -1;

    if( nFeatureId < 0 || nFeatureId >= nRecordCount )
        return nullptr;

    if( poSuperSeq == nullptr )
        iSubSeq = static_cast<int>(nFeatureId);
    else
    {
        int nSeqOffset = 0;

        // For now we just scan through till find find out what
        // super sequence this in.  In the long term we need a better (cached)
        // approach that doesn't involve this quadratic cost.
        int iSuperSeq = 0;  // Used after for.
        for( ;
             iSuperSeq < nSuperSeqCount;
             iSuperSeq++ )
        {
            if( nSeqOffset + panSubSeqSize[iSuperSeq] > nFeatureId )
            {
                iSubSeq = static_cast<int>(nFeatureId) - nSeqOffset;
                break;
            }
            nSeqOffset += panSubSeqSize[iSuperSeq];
        }

        CPLAssert( iSubSeq != -1 );

        // Make sure we have the right target var ... the one
        // corresponding to our current super sequence.
        if( iSuperSeq != iLastSuperSeq )
        {
            iLastSuperSeq = iSuperSeq;
            poTargetVar = poSuperSeq->var_value( iSuperSeq, pszSubSeqPath );
            seq = dynamic_cast<Sequence *>(poTargetVar);
        }
    }
    if( !seq )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create the feature being read.                                  */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( nFeatureId );
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Process all the regular data fields.                            */
/* -------------------------------------------------------------------- */
    int      iField;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if( papoFields[iField]->pszPathToSequence )
            continue;

        BaseType *poFieldVar = GetFieldValue( papoFields[iField], iSubSeq,
                                              nullptr );

        if( poFieldVar == nullptr )
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
                                 cpl::down_cast<Float32 *>(poFieldVar)->value());
            break;

          case dods_float64_c:
            poFeature->SetField( iField,
                                 cpl::down_cast<Float64 *>(poFieldVar)->value());
            break;

          case dods_str_c:
          case dods_url_c:
          {
              string *poStrVal = nullptr;
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

        if( poFD->pszPathToSequence == nullptr )
            continue;

        CPLAssert( strlen(poFD->pszPathToSequence)
                   < strlen(poFD->pszFieldName)-1 );

        const char *pszPathFromSubSeq = nullptr;

        if( strstr(poFD->pszFieldName,poFD->pszPathToSequence) != nullptr )
            pszPathFromSubSeq =
                strstr(poFD->pszFieldName,poFD->pszPathToSequence)
                + strlen(poFD->pszPathToSequence) + 1;
        else
            continue;

/* -------------------------------------------------------------------- */
/*      Get the sequence out of which this variable will be collected.  */
/* -------------------------------------------------------------------- */
        BaseType *poFieldVar = seq->var_value( iSubSeq,
                                               poFD->pszPathToSequence );
        int nSubSeqCount;

        if( poFieldVar == nullptr )
            continue;

        Sequence *poSubSeq = dynamic_cast<Sequence *>( poFieldVar );
        if( poSubSeq == nullptr )
            continue;

        nSubSeqCount = poSubSeq->number_of_rows();

/* -------------------------------------------------------------------- */
/*      Allocate array to put values into.                              */
/* -------------------------------------------------------------------- */
        OGRFieldDefn *poOFD = poFeature->GetFieldDefnRef( iField );
        int *panIntList = nullptr;
        double *padfDblList = nullptr;
        char **papszStrList = nullptr;

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
            poFieldVar = poSubSeq->var_value( iSubIndex, pszPathFromSubSeq );

            if( poFieldVar == nullptr )
                continue;

            switch( poFieldVar->type() )
            {
              case dods_byte_c:
              {
                  signed char byVal;
                  void *pValPtr = &byVal;

                  poFieldVar->buf2val( &pValPtr );
                  if( panIntList )
                    panIntList[iSubIndex] = byVal;
              }
              break;

              case dods_int16_c:
              {
                  GInt16 nIntVal;
                  void *pValPtr = &nIntVal;

                  poFieldVar->buf2val( &pValPtr );
                  if( panIntList )
                    panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_uint16_c:
              {
                  GUInt16 nIntVal;
                  void *pValPtr = &nIntVal;

                  poFieldVar->buf2val( &pValPtr );
                  if( panIntList )
                    panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_int32_c:
              {
                  GInt32 nIntVal;
                  void *pValPtr = &nIntVal;

                  poFieldVar->buf2val( &pValPtr );
                  if( panIntList )
                    panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_uint32_c:
              {
                  GUInt32 nIntVal;
                  void *pValPtr = &nIntVal;

                  poFieldVar->buf2val( &pValPtr );
                  if( panIntList )
                    panIntList[iSubIndex] = nIntVal;
              }
              break;

              case dods_float32_c:
                if( padfDblList )
                    padfDblList[iSubIndex] =
                        cpl::down_cast<Float32 *>(poFieldVar)->value();
                break;

              case dods_float64_c:
                if( padfDblList )
                    padfDblList[iSubIndex] =
                        cpl::down_cast<Float64 *>(poFieldVar)->value();
                break;

              case dods_str_c:
              case dods_url_c:
              {
                  string *poStrVal = nullptr;
                  poFieldVar->buf2val( (void **) &poStrVal );
                  if( papszStrList )
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
        if( poOFD->GetType() == OFTIntegerList && panIntList )
        {
            poFeature->SetField( iField, nSubSeqCount, panIntList );
        }
        else if( poOFD->GetType() == OFTRealList && padfDblList )
        {
            poFeature->SetField( iField, nSubSeqCount, padfDblList );
        }
        else if( poOFD->GetType() == OFTStringList && papszStrList )
        {
            poFeature->SetField( iField, papszStrList );
        }
        CPLFree(panIntList);
        CPLFree(padfDblList);
        CSLDestroy( papszStrList );
        }

/* ==================================================================== */
/*      Fetch the geometry.                                             */
/* ==================================================================== */
    if( oXField.bValid && oYField.bValid )
    {
        int iXField = poFeature->GetFieldIndex( oXField.pszFieldName );
        int iYField = poFeature->GetFieldIndex( oYField.pszFieldName );
        int iZField = -1;

        if( oZField.bValid )
            iZField = poFeature->GetFieldIndex(oZField.pszFieldName);

/* -------------------------------------------------------------------- */
/*      If we can't find the values in attributes then use the more     */
/*      general mechanism to fetch the value.                           */
/* -------------------------------------------------------------------- */

        if( iXField == -1 || iYField == -1
            || (oZField.bValid && iZField == -1) )
        {
            poFeature->SetGeometryDirectly(
                new OGRPoint( GetFieldValueAsDouble( &oXField, iSubSeq ),
                              GetFieldValueAsDouble( &oYField, iSubSeq ),
                              GetFieldValueAsDouble( &oZField, iSubSeq ) ) );
        }
/* -------------------------------------------------------------------- */
/*      If the fields are list values, then build a linestring.         */
/* -------------------------------------------------------------------- */
        else if( poFeature->GetFieldDefnRef(iXField)->GetType() == OFTRealList
            && poFeature->GetFieldDefnRef(iYField)->GetType() == OFTRealList )
        {
            int nPointCount, i;
            OGRLineString *poLS = new OGRLineString();

            const double *padfX =
                poFeature->GetFieldAsDoubleList(iXField, &nPointCount);
            const double *padfY =
                poFeature->GetFieldAsDoubleList(iYField, &nPointCount);
            const double *padfZ =
                iZField != -1
                ? poFeature->GetFieldAsDoubleList(iZField, &nPointCount)
                : nullptr;

            poLS->setPoints( nPointCount, (double *) padfX, (double *) padfY,
                             (double *) padfZ );

            // Make a pass clearing out NaN or Inf values.
            for( i = 0; i < nPointCount; i++ )
            {
                double dfX = poLS->getX(i);
                double dfY = poLS->getY(i);
                double dfZ = poLS->getZ(i);
                bool bReset = false;

                if( OGRDODSIsDoubleInvalid( &dfX ) )
                {
                    dfX = 0.0;
                    bReset = true;
                }
                if( OGRDODSIsDoubleInvalid( &dfY ) )
                {
                    dfY = 0.0;
                    bReset = true;
                }
                if( OGRDODSIsDoubleInvalid( &dfZ ) )
                {
                    dfZ = 0.0;
                    bReset = true;
                }

                if( bReset )
                    poLS->setPoint( i, dfX, dfY, dfZ );
            }

            poFeature->SetGeometryDirectly( poLS );
        }

/* -------------------------------------------------------------------- */
/*      Otherwise build a point.                                        */
/* -------------------------------------------------------------------- */
        else
        {
            poFeature->SetGeometryDirectly(
                iZField >= 0 ?
                    new OGRPoint(
                        poFeature->GetFieldAsDouble( iXField ),
                        poFeature->GetFieldAsDouble( iYField ),
                        poFeature->GetFieldAsDouble( iZField ) ) :
                    new OGRPoint(
                        poFeature->GetFieldAsDouble( iXField ),
                        poFeature->GetFieldAsDouble( iYField ) ) );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRDODSSequenceLayer::GetFeatureCount( int bForce )

{
    if( !bDataLoaded && !bForce )
        return -1;

    ProvideDataDDS();

    return nRecordCount;
}

/************************************************************************/
/*                           ProvideDataDDS()                           */
/************************************************************************/

bool OGRDODSSequenceLayer::ProvideDataDDS()

{
    if( bDataLoaded )
        return poTargetVar != nullptr;

    int bResult = OGRDODSLayer::ProvideDataDDS();

    if( !bResult )
        return bResult;

    // If we are in nested sequence mode, we now need to properly set
    // the poTargetVar based on the current step in the supersequence.
    poSuperSeq = FindSuperSequence( poTargetVar );

/* ==================================================================== */
/*      Figure out the record count.                                    */
/* ==================================================================== */
/* -------------------------------------------------------------------- */
/*      For simple sequences without a supersequence just return the    */
/*      count of elements.                                              */
/* -------------------------------------------------------------------- */
    if( poSuperSeq == nullptr )
    {
        Sequence* poSeq = dynamic_cast<Sequence *>(poTargetVar);
        if( poSeq == nullptr )
            return false;
        nRecordCount = poSeq->number_of_rows();
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we have to count up all the target sequence           */
/*      instances for each of the super sequence items.                 */
/* -------------------------------------------------------------------- */
    else
    {
        int iSuper;

        nSuperSeqCount = poSuperSeq->number_of_rows();
        panSubSeqSize = (int *) calloc(sizeof(int),nSuperSeqCount);
        nRecordCount = 0;
        for( iSuper = 0; iSuper < nSuperSeqCount; iSuper++ )
        {
            Sequence *poSubSeq = dynamic_cast<Sequence *>(
                poSuperSeq->var_value( iSuper, pszSubSeqPath ) );

            panSubSeqSize[iSuper] = poSubSeq->number_of_rows();
            nRecordCount += poSubSeq->number_of_rows();
        }
    }

    return true;
}

/* IEEE Constants:

  http://www.psc.edu/general/software/packages/ieee/ieee.html

Single Precision:

  S EEEEEEEE FFFFFFFFFFFFFFFFFFFFFFF
  0 1      8 9                    31

The value V represented by the word may be determined as follows:

    * If E=255 and F is nonzero, then V=NaN ("Not a number")
    * If E=255 and F is zero and S is 1, then V=-Infinity
    * If E=255 and F is zero and S is 0, then V=Infinity
    * If 0<E<255 then V=(-1)**S * 2 ** (E-127) * (1.F) where "1.F" is intended to represent the binary number created by prefixing F with an implicit leading 1 and a binary point.
    * If E=0 and F is nonzero, then V=(-1)**S * 2 ** (-126) * (0.F) These are "unnormalized" values.
    * If E=0 and F is zero and S is 1, then V=-0
    * If E=0 and F is zero and S is 0, then V=0

In particular,

  0 00000000 00000000000000000000000 = 0
  1 00000000 00000000000000000000000 = -0

  0 11111111 00000000000000000000000 = Infinity
  1 11111111 00000000000000000000000 = -Infinity

  0 11111111 00000100000000000000000 = NaN
  1 11111111 00100010001001010101010 = NaN

  0 10000000 00000000000000000000000 = +1 * 2**(128-127) * 1.0 = 2
  0 10000001 10100000000000000000000 = +1 * 2**(129-127) * 1.101 = 6.5
  1 10000001 10100000000000000000000 = -1 * 2**(129-127) * 1.101 = -6.5

  0 00000001 00000000000000000000000 = +1 * 2**(1-127) * 1.0 = 2**(-126)
  0 00000000 10000000000000000000000 = +1 * 2**(-126) * 0.1 = 2**(-127)
  0 00000000 00000000000000000000001 = +1 * 2**(-126) *
                                       0.00000000000000000000001 =
                                       2**(-149)  (Smallest positive value)

Double Precision:

The IEEE double precision floating point standard representation requires a 64 bit word, which may be represented as numbered from 0 to 63, left to right. The first bit is the sign bit, S, the next eleven bits are the exponent bits, 'E', and the final 52 bits are the fraction 'F':

  S EEEEEEEEEEE FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
  0 1        11 12                                                63

The value V represented by the word may be determined as follows:

    * If E=2047 and F is nonzero, then V=NaN ("Not a number")
    * If E=2047 and F is zero and S is 1, then V=-Infinity
    * If E=2047 and F is zero and S is 0, then V=Infinity
    * If 0<E<2047 then V=(-1)**S * 2 ** (E-1023) * (1.F) where "1.F" is intended to represent the binary number created by prefixing F with an implicit leading 1 and a binary point.
    * If E=0 and F is nonzero, then V=(-1)**S * 2 ** (-1022) * (0.F) These are "unnormalized" values.
    * If E=0 and F is zero and S is 1, then V=-0
    * If E=0 and F is zero and S is 0, then V=0

*/

/************************************************************************/
/*                       OGRDODSIsFloatInvalid()                        */
/*                                                                      */
/*      For now we are really just checking if the value is NaN, Inf    */
/*      or -Inf.                                                        */
/************************************************************************/

#if 0  // Unused.
bool OGRDODSIsFloatInvalid( const float * pfValToCheck )

{
    const unsigned char *pabyValToCheck = (unsigned char *) pfValToCheck;

#if CPL_IS_LSB == 0
    if( (pabyValToCheck[0] & 0x7f) == 0x7f
        && (pabyValToCheck[1] & 0x80) == 0x80 )
        return true;
    else
        return false;
#else
    if( (pabyValToCheck[3] & 0x7f) == 0x7f
        && (pabyValToCheck[2] & 0x80) == 0x80 )
        return true;
    else
        return false;
#endif
}
#endif

/************************************************************************/
/*                       OGRDODSIsDoubleInvalid()                       */
/*                                                                      */
/*      For now we are really just checking if the value is NaN, Inf    */
/*      or -Inf.                                                        */
/************************************************************************/

bool OGRDODSIsDoubleInvalid( const double * pdfValToCheck )

{
    return !std::isfinite(*pdfValToCheck);
}
