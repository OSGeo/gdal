/******************************************************************************
 * $Id: ogrwaspdatasource.cpp 25307 2012-12-15 09:04:40Z rouault $
 *
 * Project:  WAsP Translator
 * Purpose:  Implements OGRWAsPDataSource class
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
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

#include "ogrwasp.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include <cassert>
#include <sstream>

/************************************************************************/
/*                          OGRWAsPDataSource()                          */
/************************************************************************/

OGRWAsPDataSource::OGRWAsPDataSource( const char * pszName, 
                                      VSILFILE * hFileHandle )
    : sFilename( pszName )
    , hFile( hFileHandle )

{
}

/************************************************************************/
/*                         ~OGRWAsPDataSource()                          */
/************************************************************************/

OGRWAsPDataSource::~OGRWAsPDataSource()

{
    oLayer.reset(); /* we write to file int layer dtor */
    VSIFCloseL( hFile ); /* nothing smart can be done here in case of error */
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWAsPDataSource::TestCapability( const char * pszCap )

{
    return EQUAL(pszCap,ODsCCreateLayer) ;
}

/************************************************************************/
/*                              GetLayerByName()                        */
/************************************************************************/

OGRLayer *OGRWAsPDataSource::GetLayerByName( const char * pszName )

{
    return ( oLayer.get() && EQUAL( pszName, oLayer->GetName() ) ) 
        ? oLayer.get() 
        : NULL;
}

/************************************************************************/
/*                              Load()                                  */
/************************************************************************/

OGRErr OGRWAsPDataSource::Load(bool bSilent)

{
    /* if we don't have a layer, we read from file */
    if ( oLayer.get() )
    {
        if (!bSilent) CPLError( CE_Failure, CPLE_NotSupported, "layer already loaded");
        return OGRERR_FAILURE;
    }
    /* Parse the first line of the file in case it'a a spatial ref*/
    const char * pszLine = CPLReadLine2L( hFile, 1024, NULL );
    if ( !pszLine )
    {
        if (!bSilent) CPLError( CE_Failure, CPLE_FileIO, "empty file");
        return OGRERR_FAILURE;
    }
    CPLString sLine( pszLine );
    sLine = sLine.substr(0, sLine.find("|"));
    OGRSpatialReference * poSpatialRef = new OGRSpatialReference;
    if ( poSpatialRef->importFromProj4( sLine.c_str() ) != OGRERR_NONE )
    {
        if (!bSilent) CPLError( CE_Warning, CPLE_FileIO, "cannot find spatial reference");
        delete poSpatialRef;
        poSpatialRef = NULL;
    }

    /* TODO Parse those line since they define a coordinate transformation */
    CPLReadLineL( hFile );
    CPLReadLineL( hFile );
    CPLReadLineL( hFile );

    oLayer.reset( new OGRWAsPLayer( CPLGetBasename(sFilename.c_str()), 
                                    hFile, 
                                    poSpatialRef ) );
    if (poSpatialRef) poSpatialRef->Release();

    const vsi_l_offset iOffset = VSIFTellL( hFile );
    pszLine = CPLReadLineL( hFile );
    if ( !pszLine ) 
    {
        if (!bSilent) CPLError( CE_Failure, CPLE_FileIO, "no feature in file");
        oLayer.reset();
        return OGRERR_FAILURE;
    }
    
    double dfValues[4];
    int iNumValues = 0;
    {
        std::istringstream iss(pszLine);
        while ( iNumValues < 4 && (iss >> dfValues[iNumValues] ) ){ ++iNumValues; }

        if ( iNumValues < 2 )
        {
            if (!bSilent && iNumValues) 
                CPLError(CE_Failure, CPLE_FileIO, "no enough values" );
            else if (!bSilent) 
                CPLError(CE_Failure, CPLE_FileIO, "no feature in file" );

            oLayer.reset();
            return OGRERR_FAILURE;
        }
    }

    if ( iNumValues == 3 || iNumValues == 4 )
    {
        OGRFieldDefn left("z_left", OFTReal);
        OGRFieldDefn right("z_right", OFTReal);
        oLayer->CreateField( &left );
        oLayer->CreateField( &right );
    }
    if ( iNumValues == 2 || iNumValues == 4 )
    {
        OGRFieldDefn height("elevation", OFTReal);
        oLayer->CreateField( &height );
    }

    VSIFSeekL( hFile, iOffset, SEEK_SET );	
    return OGRERR_NONE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/


OGRLayer *OGRWAsPDataSource::GetLayer( int iLayer )

{
    return ( iLayer == 0 ) ? oLayer.get() : NULL;
}


/************************************************************************/
/*                             ICreateLayer()                           */
/************************************************************************/

OGRLayer *OGRWAsPDataSource::ICreateLayer(const char *pszName, 
                                     OGRSpatialReference *poSpatialRef,
                                     OGRwkbGeometryType eGType,
                                     char ** papszOptions)

{

    if ( eGType != wkbLineString
      && eGType != wkbLineString25D
      && eGType != wkbMultiLineString
      && eGType != wkbMultiLineString25D
      && eGType != wkbPolygon
      && eGType != wkbPolygon25D
      && eGType != wkbMultiPolygon
      && eGType != wkbMultiPolygon25D )
    {
        CPLError( CE_Failure, 
                CPLE_NotSupported, 
                "unsupported geometry type %s", OGRGeometryTypeToName( eGType ) );
        return NULL;
    }

    if ( !OGRGeometryFactory::haveGEOS() 
            && ( eGType == wkbPolygon
              || eGType == wkbPolygon25D
              || eGType == wkbMultiPolygon
              || eGType == wkbMultiPolygon25D ))
    {
        CPLError( CE_Failure, 
                CPLE_NotSupported, 
                "unsupported geometry type %s without GEOS support", OGRGeometryTypeToName( eGType ) );
        return NULL;
    }

    if ( oLayer.get() )
    {
        CPLError( CE_Failure, 
                CPLE_NotSupported, 
                "this data source does not support more than one layer" );
        return NULL;
    }

    CPLString sFirstField, sSecondField, sGeomField;

    const char *pszFields = CSLFetchNameValue( papszOptions, "WASP_FIELDS" );
    const CPLString sFields( pszFields ? pszFields : "" );
    if ( ! sFields.empty() )
    {
        /* parse the coma separated list of fields */
        const size_t iComa = sFields.find(',');
        if ( std::string::npos != iComa )
        {
            sFirstField = sFields.substr(0, iComa); 
            sSecondField = sFields.substr( iComa + 1 );
        }
        else
        {
            sFirstField = sFields;
        }
    }

    const char *pszGeomField = CSLFetchNameValue( papszOptions, "WASP_GEOM_FIELD" );
    sGeomField = CPLString( pszGeomField ? pszGeomField : "" );

    const bool bMerge = CSLTestBoolean(CSLFetchNameValueDef( papszOptions, "WASP_MERGE", "YES" ));

    std::auto_ptr<double> pdfTolerance;
    {
        const char *pszToler = CSLFetchNameValue( papszOptions, "WASP_TOLERANCE" );

        if (pszToler)
        {
            if ( !OGRGeometryFactory::haveGEOS() )
            {
                CPLError( CE_Warning, 
                        CPLE_IllegalArg, 
                        "GEOS support not enabled, ignoring option WASP_TOLERANCE" );
            }
            else
            {
                pdfTolerance.reset( new double );
                if (!(std::istringstream( pszToler ) >> *pdfTolerance ))
                {
                    CPLError( CE_Failure, 
                            CPLE_IllegalArg, 
                            "cannot set tolerance from %s", pszToler );
                    return NULL;
                }
            }
        }
    }

    std::auto_ptr<double> pdfAdjacentPointTolerance;
    {
        const char *pszAdjToler = CSLFetchNameValue( papszOptions, "WASP_ADJ_TOLER" );
        if ( pszAdjToler )
        {
            pdfAdjacentPointTolerance.reset( new double );
            if (!(std::istringstream( pszAdjToler ) >> *pdfAdjacentPointTolerance ))
            {
                CPLError( CE_Failure, 
                        CPLE_IllegalArg, 
                        "cannot set tolerance from %s", pszAdjToler );
                return NULL;
            }
        }
    }

    std::auto_ptr<double> pdfPointToCircleRadius;
    {
        const char *pszPtToCircRad = CSLFetchNameValue( papszOptions, "WASP_POINT_TO_CIRCLE_RADIUS" );
        if ( pszPtToCircRad )
        {
            pdfPointToCircleRadius.reset( new double );
            if (!(std::istringstream( pszPtToCircRad ) >> *pdfPointToCircleRadius ))
            {
                CPLError( CE_Failure, 
                        CPLE_IllegalArg, 
                        "cannot set tolerance from %s", pszPtToCircRad );
                return NULL;
            }
        }
    }

    oLayer.reset( new OGRWAsPLayer( CPLGetBasename(pszName), 
                                    hFile, 
                                    poSpatialRef,
                                    sFirstField, 
                                    sSecondField,
                                    sGeomField,
                                    bMerge,
                                    pdfTolerance.release(),
                                    pdfAdjacentPointTolerance.release(),
                                    pdfPointToCircleRadius.release() ) );

    char * ppszWktSpatialRef = NULL ;
    if ( poSpatialRef 
            && poSpatialRef->exportToProj4( &ppszWktSpatialRef ) == OGRERR_NONE )
    {
        VSIFPrintfL( hFile, "%s\n", ppszWktSpatialRef );
        OGRFree( ppszWktSpatialRef );
    }
    else
    {
        VSIFPrintfL( hFile, "no spatial ref sys\n" );
    }

    VSIFPrintfL( hFile, "  0.0 0.0 0.0 0.0\n" );
    VSIFPrintfL( hFile, "  1.0 0.0 1.0 0.0\n" );
    VSIFPrintfL( hFile, "  1.0 0.0\n" );
    return oLayer.get();
}


