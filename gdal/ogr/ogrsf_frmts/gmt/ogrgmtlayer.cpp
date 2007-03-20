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
#include "ogr_p.h"

CPL_CVSID("$Id: ogrgmtlayer.cpp 10645 2007-01-18 02:22:39Z warmerdam $");

/************************************************************************/
/*                            OGRGmtLayer()                             */
/************************************************************************/

OGRGmtLayer::OGRGmtLayer( const char * pszFilename, int bUpdate )

{
    poSRS = NULL;
    
    iNextFID = 0;
    bValidFile = FALSE;
    bHeaderComplete = !bUpdate; // assume header complete in readonly mode.
    eWkbType = wkbUnknown;
    poFeatureDefn = NULL;
    papszKeyedValues = NULL;

    this->bUpdate = bUpdate;

/* -------------------------------------------------------------------- */
/*      Open file.                                                      */
/* -------------------------------------------------------------------- */
    if( bUpdate )
        fp = VSIFOpenL( pszFilename, "r+" );
    else
        fp = VSIFOpenL( pszFilename, "r" );
    
    if( fp == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    CPLString osFieldNames, osFieldTypes, osGeometryType, osRegion;
    CPLString osWKT, osProj4, osEPSG;

    while( ReadLine() && osLine.c_str()[0] == '#' )
    {
        int iKey;

        if( strstr( osLine, "FEATURE_DATA" ) )
        {
            bHeaderComplete = TRUE;
            ReadLine();
            break;
        }

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
        if( poSRS->importFromWkt(&pszWKT) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }
    else if( osEPSG.length() )
    {
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromEPSG( atoi(osEPSG) ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }
    else if( osProj4.length() )
    {
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromProj4( osProj4 ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
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

    if( osLine.c_str()[0] != '#' || osLine.find_first_of('@') == std::string::npos )
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

            // Strip quotes
            if( osValue.c_str()[0] == '"' && osValue.c_str()[osValue.length()-1] == '"' )
                osValue = osValue.substr(1,osValue.length()-2);

            // special case for @Jp"..."
            if( osValue.c_str()[1] == '"' && osValue.c_str()[osValue.length()-1] == '"' )
                osValue = osValue.substr(0,1)
                    + osValue.substr(2,osValue.length()-2);

            // Unexcape contents
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
/*                          ScanAheadForHole()                          */
/*                                                                      */
/*      Scan ahead to see if the next geometry is a hole.  If so        */
/*      return TRUE, otherwise seek back to where we were and return    */
/*      FALSE.                                                          */
/************************************************************************/

int OGRGmtLayer::ScanAheadForHole()

{
    CPLString osSavedLine = osLine;
    vsi_l_offset nSavedLocation = VSIFTellL( fp );

    while( ReadLine() && osLine.c_str()[0] == '#' )
    {
        if( papszKeyedValues != NULL && papszKeyedValues[0][0] == 'H' )
            return TRUE;
    }

    VSIFSeekL( fp, nSavedLocation, SEEK_SET );
    osLine = osSavedLine;

    // We don't actually restore papszKeyedValues, but we 
    // assume it doesn't matter since this method is only called
    // when processing the '>' line.

    return FALSE;
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

/* -------------------------------------------------------------------- */
/*      Read lines associated with this feature.                        */
/* -------------------------------------------------------------------- */
    int iChild = 0;

    for( ; TRUE; ReadLine() )
    {
        if( osLine.length() == 0 )
            break;

        if( osLine.c_str()[0] == '>' )
        {
            if( poGeom != NULL 
                && wkbFlatten(poGeom->getGeometryType()) == wkbPolygon
                && ScanAheadForHole() )
            {
                iChild++;
            }
            else if( poGeom != NULL )
                break;
            else if( poFeatureDefn->GetGeomType() == wkbUnknown )
            {
                poFeatureDefn->SetGeomType( wkbLineString );
                bMultiVertex = TRUE;
            }
        }
        else if( osLine.c_str()[0] == '#' )
        {
            int i;
            for( i = 0; 
                 papszKeyedValues != NULL && papszKeyedValues[i] != NULL; 
                 i++ )
            {
                if( papszKeyedValues[i][0] == 'D' )
                    osFieldData = papszKeyedValues[i] + 1;
            }
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

                  case wkbPolygon:
                  {
                      OGRPolygon *poPoly = (OGRPolygon *) poGeom;
                      OGRLinearRing *poRing;

                      if( iChild == 0 )
                          poRing = poPoly->getExteriorRing();
                      else
                          poRing = poPoly->getInteriorRing(iChild-1);
                      if( poRing == NULL )
                      {
                          poRing = new OGRLinearRing();
                          poPoly->addRingDirectly( poRing );
                      }
                      
                      if( nDim == 3 )
                        poRing->addPoint(dfX,dfY,dfZ);
                      else
                        poRing->addPoint(dfX,dfY);
                  }
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

/* -------------------------------------------------------------------- */
/*      Create feature.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetGeometryDirectly( poGeom );
    poFeature->SetFID( iNextFID++ );

/* -------------------------------------------------------------------- */
/*      Process field values.                                           */
/* -------------------------------------------------------------------- */
    char **papszFD = CSLTokenizeStringComplex( osFieldData, "|", TRUE, TRUE );
    int iField; 

    for( iField = 0; papszFD != NULL && papszFD[iField] != NULL; iField++ )
    {
        if( iField >= poFeatureDefn->GetFieldCount() )
            break;

        poFeature->SetField( iField, papszFD[iField] );
    }

    CSLDestroy( papszFD );

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

OGRErr OGRGmtLayer::CreateFeature( OGRFeature *poFeature )

{
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, 
                  "Can't create features on read-only dataset." );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to write the header describing the fields?           */
/* -------------------------------------------------------------------- */
    if( !bHeaderComplete )
    {
        CPLString osFieldNames, osFieldTypes;
        
        int iField;

        for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            if( iField > 0 )
            {
                osFieldNames += "|";
                osFieldTypes += "|";
            }

            osFieldNames += poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
            switch( poFeatureDefn->GetFieldDefn(iField)->GetType() )
            {
              case OFTInteger:
                osFieldTypes += "integer";
                break;
                
              case OFTReal:
                osFieldTypes += "double";
                break;
                
              case OFTDateTime:
                osFieldTypes += "datetime";
                break;
                
              default:
                osFieldTypes += "string";
                break;
            }
        }

        if( poFeatureDefn->GetFieldCount() > 0 )
        {
            VSIFPrintfL( fp, "# @N\"%s\"\n", osFieldNames.c_str() );
            VSIFPrintfL( fp, "# @T%s\n", osFieldTypes.c_str() );
        }

        VSIFPrintfL( fp, "# FEATURE_DATA\n" );

        bHeaderComplete = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Write out the feature                                           */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    
    if( poGeom == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Features without geometry not supported by GMT writer." );
        return OGRERR_FAILURE;
    }

    if( poFeatureDefn->GetGeomType() == wkbUnknown )
        poFeatureDefn->SetGeomType(wkbFlatten(poGeom->getGeometryType()));

    // Do we need a vertex collection marker grouping vertices. 
    if( poFeatureDefn->GetGeomType() != wkbPoint )
        VSIFPrintfL( fp, ">\n" );

/* -------------------------------------------------------------------- */
/*      Write feature properties()                                      */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetFieldCount() > 0 )
    {
        int iField;
        CPLString osFieldData;

        for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
        {
            char *pszEscapedVal;

            pszEscapedVal = 
                CPLEscapeString( poFeature->GetFieldAsString(iField), 
                                 -1, CPLES_BackslashQuotable );

            if( iField > 0 )
                osFieldData += "|";
            osFieldData += pszEscapedVal;
            CPLFree( pszEscapedVal );
        }

        VSIFPrintfL( fp, "# @D\"%s\"\n", osFieldData.c_str() );
    }

/* -------------------------------------------------------------------- */
/*      Write Geometry                                                  */
/* -------------------------------------------------------------------- */
    return WriteGeometry( (OGRGeometryH) poGeom, TRUE );
}

/************************************************************************/
/*                           WriteGeometry()                            */
/*                                                                      */
/*      Write a geometry to the file.  If bHaveAngle is TRUE it         */
/*      means the angle bracket preceeding the point stream has         */
/*      already been written out.                                       */
/*                                                                      */
/*      We use the C API for geometry access because of it's            */
/*      simplified access to vertices and children geometries.          */
/************************************************************************/

OGRErr OGRGmtLayer::WriteGeometry( OGRGeometryH hGeom, int bHaveAngle )

{
/* -------------------------------------------------------------------- */
/*      This is a geometry with sub-geometries.                         */
/* -------------------------------------------------------------------- */
    if( OGR_G_GetGeometryCount( hGeom ) > 0 )
    {
        int iGeom;
        OGRErr eErr = OGRERR_NONE;
        
        for( iGeom = 0; 
             iGeom < OGR_G_GetGeometryCount(hGeom) && eErr == OGRERR_NONE;
             iGeom++ )
        {
            // We need to emit polygon @P and @H items while we still
            // know this is a polygon and which is the outer and inner
            // ring. 
            if( wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPolygon )
            {
                if( !bHaveAngle )
                {
                    VSIFPrintfL( fp, ">\n" );
                    bHaveAngle = TRUE;
                }
                if( iGeom == 0 )
                    VSIFPrintfL( fp, "# @P\n" );
                else
                    VSIFPrintfL( fp, "# @H\n" );
            }

            eErr = WriteGeometry( OGR_G_GetGeometryRef( hGeom, iGeom ), 
                                  bHaveAngle );
            bHaveAngle = FALSE;
        }
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      If this is not a point we need to have an angle bracket to      */
/*      mark the vertex list.                                           */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(OGR_G_GetGeometryType(hGeom)) != wkbPoint 
        && !bHaveAngle )
        VSIFPrintfL( fp, ">\n" );

/* -------------------------------------------------------------------- */
/*      Dump vertices.                                                  */
/* -------------------------------------------------------------------- */
    int iPoint, nPointCount = OGR_G_GetPointCount(hGeom);
    int nDim = OGR_G_GetCoordinateDimension(hGeom);

    for( iPoint = 0; iPoint < nPointCount; iPoint++ )
    {
        char szLine[128];

        OGRMakeWktCoordinate( szLine, 
                              OGR_G_GetX( hGeom, iPoint ),
                              OGR_G_GetY( hGeom, iPoint ),
                              OGR_G_GetZ( hGeom, iPoint ),
                              nDim );
        if( VSIFPrintfL( fp, "%s\n", szLine ) < 1 )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Gmt write failure: %s", 
                      VSIStrerror( errno ) );
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

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

OGRErr OGRGmtLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess, 
                  "Can't create fields on read-only dataset." );
        return OGRERR_FAILURE;
    }

    if( bHeaderComplete )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create fields after features have been created.");
        return OGRERR_FAILURE;
    }

    switch( poField->GetType() )
    {
      case OFTInteger:
      case OFTReal:
      case OFTString:
      case OFTDateTime:
        poFeatureDefn->AddFieldDefn( poField );
        return OGRERR_NONE;
        break;

        break;

      default:
        if( !bApproxOK )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Field %s is of unsupported type %s.", 
                      poField->GetNameRef(), 
                      poField->GetFieldTypeName( poField->GetType() ) );
            return OGRERR_FAILURE;
        } 
        else if( poField->GetType() == OFTDate 
                 || poField->GetType() == OFTTime )
        {
            OGRFieldDefn oModDef( poField );
            oModDef.SetType( OFTDateTime );
            poFeatureDefn->AddFieldDefn( poField );
            return OGRERR_NONE;
        }
        else 
        {
            OGRFieldDefn oModDef( poField );
            oModDef.SetType( OFTString );
            poFeatureDefn->AddFieldDefn( poField );
            return OGRERR_NONE;
        }
    }
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGmtLayer::GetSpatialRef()

{
    return poSRS;
}
