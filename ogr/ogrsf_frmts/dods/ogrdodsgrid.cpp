/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSGridLayer class, which implements the
 *           "Grid/Array" access strategy.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2004/02/17 16:22:19  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_dods.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRDODSGridLayer()                          */
/************************************************************************/

OGRDODSGridLayer::OGRDODSGridLayer( OGRDODSDataSource *poDSIn, 
                                            const char *pszTargetIn,
                                            AttrTable *poOGRLayerInfoIn )

        : OGRDODSLayer( poDSIn, pszTargetIn, poOGRLayerInfoIn )

{
    pRawData = NULL;

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
#ifdef notdef
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
#endif

/* -------------------------------------------------------------------- */
/*      Fetch the target variable.                                      */
/* -------------------------------------------------------------------- */
    BaseType *poTargVar = poDS->oDDS.var( pszTargetIn );
    if( poTargVar->type() == dods_grid_c )
    {
        poTargetGrid = dynamic_cast<Grid *>( poTargVar );
        poTargetArray = dynamic_cast<Array *>(poTargetGrid->array_var());
    }
    else if( poTargVar->type() == dods_array_c )
    {
        poTargetGrid = NULL;
        poTargetArray = dynamic_cast<Array *>( poTargVar );
    }
    else
    {
        CPLAssert( FALSE );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Collect dimension information.                                  */
/* -------------------------------------------------------------------- */
    int iDim;
    Array::Dim_iter iterDim;

    nDimCount = poTargetArray->dimensions();
    paoDimensions = new OGRDODSDim[nDimCount];
    nMaxRawIndex = 1;

    for( iterDim = poTargetArray->dim_begin(), iDim = 0;
         iterDim != poTargetArray->dim_end(); 
         iterDim++, iDim++ )
    {
        paoDimensions[iDim].pszDimName = 
            CPLStrdup(poTargetArray->dimension_name(iterDim).c_str());
        paoDimensions[iDim].nDimStart = 
            poTargetArray->dimension_start(iterDim);
        paoDimensions[iDim].nDimEnd = 
            poTargetArray->dimension_stop(iterDim);
        paoDimensions[iDim].nDimStride = 
            poTargetArray->dimension_stride(iterDim);
        paoDimensions[iDim].poMap = NULL;

        paoDimensions[iDim].nDimEntries = 
            (paoDimensions[iDim].nDimEnd + 1 - paoDimensions[iDim].nDimStart
             + paoDimensions[iDim].nDimStride - 1) 
            / paoDimensions[iDim].nDimStride;

        nMaxRawIndex *= paoDimensions[iDim].nDimEntries;
    }

/* -------------------------------------------------------------------- */
/*      If we are working with a grid, collect the maps.                */
/* -------------------------------------------------------------------- */
    if( poTargetGrid != NULL )
    {
        int iMap;
        Grid::Map_iter iterMap;

        for( iterMap = poTargetGrid->map_begin(), iMap = 0;
             iterMap != poTargetGrid->map_end(); 
             iterMap++, iMap++ )
        {
            paoDimensions[iMap].poMap = dynamic_cast<Array *>(*iterMap);
        }

        CPLAssert( iMap == nDimCount );
    }

/* -------------------------------------------------------------------- */
/*      Setup field definitions.  The first nDimCount will be the       */
/*      dimension attributes, and after that comes the actual target    */
/*      array.                                                          */
/* -------------------------------------------------------------------- */
    for( iDim = 0; iDim < nDimCount; iDim++ )
    {
        OGRFieldDefn oField( paoDimensions[iDim].pszDimName, OFTInteger );

        if( EQUAL(oField.GetNameRef(), poTargetArray->name().c_str()) )
            oField.SetName(CPLSPrintf("%s_i",paoDimensions[iDim].pszDimName));

        if( paoDimensions[iDim].poMap != NULL )
        {
            switch( paoDimensions[iDim].poMap->var()->type() )
            {
              case dods_byte_c:
              case dods_int16_c:
              case dods_uint16_c:
              case dods_int32_c:
              case dods_uint32_c:
                oField.SetType( OFTInteger );
                break;

              case dods_float32_c:
              case dods_float64_c:
                oField.SetType( OFTReal );
                break;

              case dods_str_c:
              case dods_url_c:
                oField.SetType( OFTString );
                break;

              default:
                // Ignore
                break;
            }
        }

        poFeatureDefn->AddFieldDefn( &oField );
    }

/* -------------------------------------------------------------------- */
/*      Setup the array attribute itself.                               */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oArrayField( poTargetArray->name().c_str(), OFTInteger );

    switch( poTargetArray->var()->type() )
    {
      case dods_byte_c:
      case dods_int16_c:
      case dods_uint16_c:
      case dods_int32_c:
      case dods_uint32_c:
        oArrayField.SetType( OFTInteger );
        break;

      case dods_float32_c:
      case dods_float64_c:
        oArrayField.SetType( OFTReal );
        break;

      case dods_str_c:
      case dods_url_c:
        oArrayField.SetType( OFTString );
        break;

      default:
        // Ignore
        break;
    }

    poFeatureDefn->AddFieldDefn( &oArrayField );
}

/************************************************************************/
/*                         ~OGRDODSGridLayer()                          */
/************************************************************************/

OGRDODSGridLayer::~OGRDODSGridLayer()

{
    delete[] paoDimensions;
}

/************************************************************************/
/*                         ArrayEntryToField()                          */
/************************************************************************/

int OGRDODSGridLayer::ArrayEntryToField( Array *poArray, void *pRawData, 
                                         int iArrayIndex,
                                         OGRFeature *poFeature, int iField)

{
    switch( poTargetArray->var()->type() )
    {
      case dods_int32_c:
      {
          GInt32 *panRawData = (GInt32 *) pRawData;
          poFeature->SetField( iField, panRawData[iArrayIndex] );
      }
      break;

      case dods_uint32_c:
      {
          GUInt32 *panRawData = (GUInt32 *) pRawData;
          poFeature->SetField( iField, (int) panRawData[iArrayIndex] );
      }
      break;

      case dods_float32_c:
      {
          float * pafRawData = (float *) pRawData;
          poFeature->SetField( iField, pafRawData[iArrayIndex] );
      }
      break;

      case dods_float64_c:
      {
          double * padfRawData = (double *) pRawData;
          poFeature->SetField( iField, padfRawData[iArrayIndex] );
      }
      break;

      default:
        return FALSE;
    }

    return TRUE;
}								       

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDODSGridLayer::GetFeature( long nFeatureId )

{
    if( nFeatureId < 0 || nFeatureId >= nMaxRawIndex )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Ensure we have the dataset.                                     */
/* -------------------------------------------------------------------- */
    if( !ProvideDataDDS() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the feature being read.                                  */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Establish the values for the various dimension indices.         */
/* -------------------------------------------------------------------- */
    int iDim;
    int nRemainder = nFeatureId;

    for( iDim = 0; iDim < nDimCount; iDim++ )
    {
        paoDimensions[iDim].iLastValue = 
            (nRemainder % paoDimensions[iDim].nDimEntries) 
            * paoDimensions[iDim].nDimStride 
            + paoDimensions[iDim].nDimStart;
        nRemainder = nRemainder / paoDimensions[iDim].nDimEntries;

        if( poTargetGrid == NULL )
            poFeature->SetField( iDim, paoDimensions[iDim].iLastValue );
    }
    CPLAssert( nRemainder == 0 );

/* -------------------------------------------------------------------- */
/*      For grids, we need to apply the values of the dimensions        */
/*      looked up in the corresponding map.                             */
/* -------------------------------------------------------------------- */
    if( poTargetGrid != NULL )
    {
        for( iDim = 0; iDim < nDimCount; iDim++ )
        {
            ArrayEntryToField( paoDimensions[iDim].poMap, 
                               paoDimensions[iDim].pRawData, 
                               paoDimensions[iDim].iLastValue, 
                               poFeature, iDim );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process all the regular data fields.                            */
/* -------------------------------------------------------------------- */
    ArrayEntryToField( poTargetArray, pRawData, nFeatureId, 
                       poFeature, nDimCount );

    return poFeature;
}

/************************************************************************/
/*                           ProvideDataDDS()                           */
/************************************************************************/

int OGRDODSGridLayer::ProvideDataDDS()

{
    int bResult = OGRDODSLayer::ProvideDataDDS();

    if( !bResult )
        return bResult;

    // We want to reset the grid and array points to point into the 
    // data instance, rather than the DDS instances that were available
    // at layer creation.
    if( poTargetVar->type() == dods_grid_c )
    {
        poTargetGrid = dynamic_cast<Grid *>( poTargetVar );
        poTargetArray = dynamic_cast<Array *>(poTargetGrid->array_var());
    }
    else if( poTargetVar->type() == dods_array_c )
    {
        poTargetGrid = NULL;
        poTargetArray = dynamic_cast<Array *>( poTargetVar );
    }
    else
    {
        CPLAssert( FALSE );
        return FALSE;
    }
        
        
    // Allocate appropriate raw data array, and pull out data into it.
    pRawData = CPLMalloc( poTargetArray->width() );
    poTargetArray->buf2val( &pRawData );

    // Setup pointers to each of the map objects.
    if( poTargetGrid != NULL )
    {
        int iMap;
        Grid::Map_iter iterMap;

        for( iterMap = poTargetGrid->map_begin(), iMap = 0;
             iterMap != poTargetGrid->map_end(); 
             iterMap++, iMap++ )
        {
            paoDimensions[iMap].poMap = dynamic_cast<Array *>(*iterMap);
            paoDimensions[iMap].pRawData = 
                CPLMalloc( paoDimensions[iMap].poMap->width() );
            paoDimensions[iMap].poMap->buf2val( &(paoDimensions[iMap].pRawData) );
        }
    }    

    return bResult;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRDODSGridLayer::GetFeatureCount( int bForce )

{
    return nMaxRawIndex;
}
