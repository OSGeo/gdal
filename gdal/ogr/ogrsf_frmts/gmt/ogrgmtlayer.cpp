/******************************************************************************
 * $Id: ogrgmtlayer.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGmtLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gmt.h"
#include "cpl_conv.h"

CPL_CVSID("$Id: ogrgmtlayer.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/*                            OGRGmtLayer()                             */
/************************************************************************/

OGRGmtLayer::OGRGmtLayer( const char * pszFilename )

{
    poSRS = NULL;
    
    iNextFID = 0;
    bValidFile = FALSE;
    eWkbType = wkbUnknown;
    poFeatureDefn = NULL;

/* -------------------------------------------------------------------- */
/*      Open file.                                                      */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "r" );
    
    if( fp == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    CPLString osFieldNames, osFieldTypes, osGeometryType, osRegion;
    CPLString osWKT, osProj4, osEPSG;

    while( ReadLine() && osLine[0] == '#' )
    {
        int iKey;

        for( iKey = 0; 
             papszKeyedValues != NULL && papszKeyedValues[iKey] != NULL; 
             iKey++ )
        {
            if( papszKeyedValues[iKey][0] == 'N' )
                osFieldNames = papszKeyedValues[iKey] + 1;
            if( papszKeyedValues[iKey][0] == 'T' )
                osFieldTypes = papszKeyedValues[iKey] + 1;
            if( papszKeyedValues[iKey][0] == 'G' )
                osGeometryType = papszKeyedValues[iKey] + 1;
            if( papszKeyedValues[iKey][0] == 'R' )
                osRegion = papszKeyedValues[iKey] + 1;
            if( papszKeyedValues[iKey][0] == 'J' )
            {
                if( papszKeyedValues[iKey][1] == 'e' )
                    osEPSG = papszKeyedValues[iKey] + 2;
                if( papszKeyedValues[iKey][1] == 'p' )
                    osProj4 = papszKeyedValues[iKey] + 2;
                if( papszKeyedValues[iKey][1] == 'w' )
                    osWKT = papszKeyedValues[iKey] + 2;
            }
        }

    }

/* -------------------------------------------------------------------- */
/*      Handle coordinate system.                                       */
/* -------------------------------------------------------------------- */
    if( osWKT.length() )
    {
        char *pszWKT = (char *) osWKT.c_str();

        poSRS = new OGRSpatialReference();
        poSRS->importFromWkt(&pszWKT);
    }
    else if( osEPSG.length() )
    {
        poSRS = new OGRSpatialReference();
        poSRS->importFromEPSG( atoi(osEPSG) );
    }
    else if( osProj4.length() )
    {
        poSRS = new OGRSpatialReference();
        poSRS->importFromProj4( osProj4 );
    }

/* -------------------------------------------------------------------- */
/*      Create the feature definition, and set the geometry type, if    */
/*      known.                                                          */
/* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn( CPLGetBasename(pszFilename) );
    poFeatureDefn->Reference();

    if( osGeometryType == "POINT" )
        poFeatureDefn->SetGeomType( wkbPoint );
    else if( osGeometryType == "MULTIPOINT" )
        poFeatureDefn->SetGeomType( wkbMultiPoint );
    else if( osGeometryType == "LINESTRING" )
        poFeatureDefn->SetGeomType( wkbLineString );
    else if( osGeometryType == "MULTILINESTRING" )
        poFeatureDefn->SetGeomType( wkbMultiLineString );
    else if( osGeometryType == "POLYGON" )
        poFeatureDefn->SetGeomType( wkbPolygon );
    else if( osGeometryType == "MULTIPOLYGON" )
        poFeatureDefn->SetGeomType( wkbMultiPolygon );

/* -------------------------------------------------------------------- */
/*      Process fields.                                                 */
/* -------------------------------------------------------------------- */

    if( osFieldNames.length() || osFieldTypes.length() )
    {
        char **papszFN = CSLTokenizeStringComplex( osFieldNames, "|", 
                                                   FALSE, TRUE );
        char **papszFT = CSLTokenizeStringComplex( osFieldTypes, "|", 
                                                   FALSE, TRUE );
        int nFieldCount = MAX(CSLCount(papszFN),CSLCount(papszFT));
        int iField;

        for( iField = 0; iField < nFieldCount; iField++ )
        {
            OGRFieldDefn oField("", OFTString );

            if( iField < CSLCount(papszFN) )
                oField.SetName( papszFN[iField] );
            else
                oField.SetName( CPLString().Printf( "Field_%d", iField+1 ));

            if( iField < CSLCount(papszFT) )
            {
                if( EQUAL(papszFT[iField],"integer") )
                    oField.SetType( OFTInteger );
                else if( EQUAL(papszFT[iField],"double") )
                    oField.SetType( OFTReal );
                else if( EQUAL(papszFT[iField],"datetime") )
                    oField.SetType( OFTDateTime );
            }

            poFeatureDefn->AddFieldDefn( &oField );
        }        
    }

    bValidFile = TRUE;
}

/************************************************************************/
/*                           ~OGRGmtLayer()                           */
/************************************************************************/

OGRGmtLayer::~OGRGmtLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Gmt", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    CSLDestroy( papszKeyedValues );

    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS )
        poSRS->Release();

    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                              ReadLine()                              */
/*                                                                      */
/*      Read a line into osLine.  If it is a comment line with @        */
/*      keyed values, parse out the keyed values into                   */
/*      papszKeyedValues.                                               */
/************************************************************************/

int OGRGmtLayer::ReadLine()

{
/* -------------------------------------------------------------------- */
/*      Clear last line.                                                */
/* -------------------------------------------------------------------- */
    osLine.clear();
    if( papszKeyedValues )
    {
        CSLDestroy( papszKeyedValues );
        papszKeyedValues = NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Read newline.                                                   */
/* -------------------------------------------------------------------- */
    const char *pszLine = CPLReadLineL( fp );
    if( pszLine == NULL )
        return FALSE; // end of file.

    osLine = pszLine;

/* -------------------------------------------------------------------- */
/*      If this is a comment line with keyed values, parse them.        */
/* -------------------------------------------------------------------- */
    size_t i;

    if( osLine[0] != '#' || osLine.find_first_of('@') == std::string::npos )
        return TRUE;

    for( i = 0; i < osLine.length(); i++ )
    {
        if( osLine[i] == '@' )
        {
            size_t iValEnd;
            int bInQuotes = FALSE;

            for( iValEnd = i+2; iValEnd < osLine.length(); iValEnd++ )
            {
                if( !bInQuotes && isspace(osLine[iValEnd]) )
                    break;

                if( bInQuotes && osLine[iValEnd] == '\\' 
                    && iValEnd < osLine.length()-1 )
                {
                    iValEnd++;
                }
                else if( osLine[iValEnd] == '"' )
                    bInQuotes = !bInQuotes;
            }

            CPLString osValue = osLine.substr(i+2,iValEnd-i-2);
            char *pszUEValue = CPLUnescapeString( osValue, NULL, 
                                                  CPLES_BackslashQuotable );
            
            CPLString osKeyValue = osLine.substr(i+1,1);
            osKeyValue += pszUEValue;
            CPLFree( pszUEValue );
            papszKeyedValues = CSLAddString( papszKeyedValues, osKeyValue );

            i = iValEnd;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGmtLayer::ResetReading()

{
    if( iNextFID != 0 )
    {
        iNextFID = 0;
        VSIFSeekL( fp, 0, SEEK_SET );
        ReadLine();
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRGmtLayer::GetNextRawFeature()

{
    int  bMultiVertex = 
        poFeatureDefn->GetGeomType() != wkbPoint
        && poFeatureDefn->GetGeomType() != wkbUnknown;
    CPLString osFieldData;
    OGRGeometry *poGeom = NULL;

    for( ; TRUE; ReadLine() )
    {
        if( osLine.length() == 0 )
            break;

        if( osLine[0] == '>' )
        {
            if( poGeom != NULL )
                break;
            else if( poFeatureDefn->GetGeomType() == wkbUnknown )
            {
                poFeatureDefn->SetGeomType( wkbLineString );
                bMultiVertex = TRUE;
            }
        }
        else if( osLine[0] == '#' )
        {
            if( papszKeyedValues != NULL && papszKeyedValues[0][0] == 'D' )
                osFieldData = papszKeyedValues[0] + 1;
        }
        else
        {
            // Parse point line. 
            double dfX, dfY, dfZ = 0.0;
            int nDim = sscanf( osLine, "%lf %lf %lf", &dfX, &dfY, &dfZ );
                
            if( nDim >= 2 )
            {
                if( poGeom == NULL )
                {
                    switch( poFeatureDefn->GetGeomType() )
                    {
                      case wkbLineString:
                        poGeom = new OGRLineString();
                        break;
                    
                      case wkbPolygon:
                        poGeom = new OGRPolygon();
                        break;
                    
                      case wkbMultiPolygon:
                        poGeom = new OGRMultiPolygon();
                        break;
                    
                      case wkbMultiPoint:
                        poGeom = new OGRMultiPoint();
                        break;

                      case wkbPoint:
                      case wkbUnknown:
                      default:
                        poGeom = new OGRPoint();
                        break;
                    }

                }

                switch( wkbFlatten(poGeom->getGeometryType()) )
                {
                  case wkbPoint:
                    ((OGRPoint *) poGeom)->setX( dfX );
                    ((OGRPoint *) poGeom)->setY( dfY );
                    if( nDim == 3 )
                        ((OGRPoint *) poGeom)->setZ( dfZ );
                    break;

                  case wkbLineString:
                    if( nDim == 3 )
                        ((OGRLineString *)poGeom)->addPoint(dfX,dfY,dfZ);
                    else
                        ((OGRLineString *)poGeom)->addPoint(dfX,dfY);
                    break;

                  default:
                    CPLAssert( FALSE );
                }
            }
        }

        if( poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            ReadLine();
            break;
        }
    }

    if( poGeom == NULL )
        return NULL;

    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetGeometryDirectly( poGeom );
    poFeature->SetFID( iNextFID++ );

    // set field values...

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGmtLayer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature *poFeature = GetNextRawFeature();

        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }
        else
        {
            delete poFeature;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

#ifdef notdef
OGRErr OGRGmtLayer::CreateFeature( OGRFeature *poFeature )

{
    if( poFeature->GetFID() != OGRNullFID 
        && poFeature->GetFID() >= 0
        && poFeature->GetFID() < nMaxFeatureCount )
    {
        if( papoFeatures[poFeature->GetFID()] != NULL )
            poFeature->SetFID( OGRNullFID );
    }

    if( poFeature->GetFID() > 10000000 )
        poFeature->SetFID( OGRNullFID );

    return SetFeature( poFeature );
}
#endif

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHO files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRGmtLayer::GetExtent (OGREnvelope *psExtent, int bForce)

{
    return OGRLayer::GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGmtLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;  // TODO

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/
#ifdef notdef
OGRErr OGRGmtLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
/* -------------------------------------------------------------------- */
/*      simple case, no features exist yet.                             */
/* -------------------------------------------------------------------- */
    if( nFeatureCount == 0 )
    {
        poFeatureDefn->AddFieldDefn( poField );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Add field definition and setup remap definition.                */
/* -------------------------------------------------------------------- */
    int  *panRemap;
    int   i;

    poFeatureDefn->AddFieldDefn( poField );

    panRemap = (int *) CPLMalloc(sizeof(int) * poFeatureDefn->GetFieldCount());
    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i < poFeatureDefn->GetFieldCount() - 1 )
            panRemap[i] = i;
        else
            panRemap[i] = -1;
    }

/* -------------------------------------------------------------------- */
/*      Remap all the internal features.  Hopefully there aren't any    */
/*      external features referring to our OGRFeatureDefn!              */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nMaxFeatureCount; i++ )
    {
        if( papoFeatures[i] != NULL )
            papoFeatures[i]->RemapFields( NULL, panRemap );
    }

    return OGRERR_NONE;
}
#endif

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGmtLayer::GetSpatialRef()

{
    return poSRS;
}
