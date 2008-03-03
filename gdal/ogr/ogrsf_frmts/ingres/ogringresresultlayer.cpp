/******************************************************************************
 * $Id: ogringresresultlayer.cpp 11522 2007-05-15 14:26:10Z mloskot $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresResultLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_ingres.h"

CPL_CVSID("$Id: ogringresresultlayer.cpp 11522 2007-05-15 14:26:10Z mloskot $");

/************************************************************************/
/*                        OGRIngresResultLayer()                         */
/************************************************************************/

OGRIngresResultLayer::OGRIngresResultLayer( OGRIngresDataSource *poDSIn, 
                                            const char * pszRawQueryIn,
                                            OGRIngresStatement *poResultSetIn )
{
    poDS = poDSIn;

    iNextShapeId = 0;

    pszRawStatement = CPLStrdup(pszRawQueryIn);

    poResultSet = poResultSetIn;

    BuildFullQueryStatement();

    poFeatureDefn = ReadResultDefinition();
}

/************************************************************************/
/*                        ~OGRIngresResultLayer()                        */
/************************************************************************/

OGRIngresResultLayer::~OGRIngresResultLayer()

{
    CPLFree( pszRawStatement );
}

/************************************************************************/
/*                        ReadResultDefinition()                        */
/*                                                                      */
/*      Build a schema from the current resultset.                      */
/************************************************************************/

OGRFeatureDefn *OGRIngresResultLayer::ReadResultDefinition()

{
/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( "sql_statement" );
    int            iRawField;

    poDefn->Reference();

    for( iRawField = 0; 
         iRawField < (int) poResultSet->getDescrParm.gd_descriptorCount; 
         iRawField++ )
    {
        IIAPI_DESCRIPTOR *psFDesc = 
            poResultSet->getDescrParm.gd_descriptor + iRawField;
        OGRFieldDefn    oField( psFDesc->ds_columnName, OFTString);

        switch( psFDesc->ds_dataType )
        {
          case IIAPI_CHR_TYPE:
          case IIAPI_CHA_TYPE:
            // string - fixed width.
            oField.SetWidth( psFDesc->ds_length );
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_LVCH_TYPE:
          case IIAPI_LTXT_TYPE:
          case IIAPI_VCH_TYPE:
          case IIAPI_TXT_TYPE:
            // default variable length string
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_INT_TYPE:
            oField.SetType( OFTInteger );
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_FLT_TYPE:
            oField.SetType( OFTReal );
            poDefn->AddFieldDefn( &oField );
            break;

          case IIAPI_DEC_TYPE:
            oField.SetWidth( psFDesc->ds_precision );
            if( psFDesc->ds_scale == 0 )
                oField.SetType( OFTInteger );
            else
            {
                oField.SetType( OFTReal );
                oField.SetPrecision( psFDesc->ds_scale );
            }
            poDefn->AddFieldDefn( &oField );
            break;

          default:
            // any other field we ignore. 
            break;
        }
    }

    poDefn->SetGeomType( wkbNone );

    return poDefn;
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRIngresResultLayer::BuildFullQueryStatement()

{
    osQueryStatement = pszRawStatement;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIngresResultLayer::ResetReading()

{
    OGRIngresLayer::ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRIngresResultLayer::GetFeatureCount( int bForce )

{
    // I wonder if we could do anything smart here...
    // ... not till Ingres grows up (HB)
    return OGRIngresLayer::GetFeatureCount( bForce );
}
