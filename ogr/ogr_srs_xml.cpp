/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGRSpatialReference interface to OGC XML (014r4).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
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
 * Revision 1.1  2001/12/06 18:16:17  warmerda
 * new
 *
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"
#include "cpl_minixml.h"

/************************************************************************/
/*                             addNameSet()                             */
/*                                                                      */
/*      Add a single name nameset.                                      */
/************************************************************************/

static void addNameSet( CPLXMLNode *psXMLParent, const char *pszName )

{
    CPLXMLNode *psNode;

    psNode = CPLCreateXMLNode( psXMLParent, CXT_Element, "NameSet" );
    CPLCreateXMLNode( CPLCreateXMLNode( psNode, CXT_Element, "name" ), 
                      CXT_Text, pszName );
}

/************************************************************************/
/*                            addAuthority()                            */
/************************************************************************/

static CPLXMLNode *addAuthority( CPLXMLNode *psParent, 
                                 const char *pszCode, 
                                 const char *pszCodeSpace, 
                                 const char *pszEdition )

{
    CPLXMLNode *psIdentifier;

    psIdentifier = CPLCreateXMLNode( psParent, CXT_Element, "Identifier" );

    CPLCreateXMLElementAndValue( psIdentifier, "code", pszCode );
    CPLCreateXMLElementAndValue( psIdentifier, "codeSpace", pszCodeSpace );
    if( pszEdition != NULL )
        CPLCreateXMLElementAndValue( psIdentifier, "edition", pszEdition );

    return psIdentifier;
}

/************************************************************************/
/*                        exportAuthorityToXML()                        */
/************************************************************************/

static CPLXMLNode *exportAuthorityToXML( OGR_SRSNode *poAuthParent,
                                         CPLXMLNode *psXMLParent )

{
    CPLXMLNode *psIdentification, *psNode;
    OGR_SRSNode *poAuthority;

/* -------------------------------------------------------------------- */
/*      Get authority node from parent.                                 */
/* -------------------------------------------------------------------- */
    if( poAuthParent->FindChild( "AUTHORITY" ) == -1 )
        return NULL;

    poAuthority = poAuthParent->GetChild( 
        poAuthParent->FindChild( "AUTHORITY" ));

/* -------------------------------------------------------------------- */
/*      Create identification.                                          */
/* -------------------------------------------------------------------- */
    psIdentification = CPLCreateXMLNode( psXMLParent, CXT_Element, 
                                         "Identifier" );

    psNode = CPLCreateXMLNode( psIdentification, CXT_Element, "code" );
    CPLCreateXMLNode( psNode, CXT_Text, 
                      poAuthority->GetChild(1)->GetValue() );

    psNode = CPLCreateXMLNode( psIdentification, CXT_Element, "codeSpace" );
    CPLCreateXMLNode( psNode, CXT_Text, 
                      poAuthority->GetChild(0)->GetValue() );

    /* ... note: no way to set edition, omitting for now ... */

    return psIdentification;
}

/************************************************************************/
/*                            addMeterUnit()                            */
/************************************************************************/

static void addMeterUnit( CPLXMLNode *psParent )

{
    CPLXMLNode *psUnitXML;

    psUnitXML = CPLCreateXMLNode( psParent, CXT_Element, "LinearUnit" );

    addNameSet( psUnitXML, "metre" );
    CPLCreateXMLElementAndValue( psParent, "metresPerUnit", "1" );
    addAuthority( psParent, "9001", "EPSG", "6.0" );
}

/************************************************************************/
/*                           addRadianUnit()                            */
/************************************************************************/

static void addRadianUnit( CPLXMLNode *psParent )

{
    CPLXMLNode *psUnitXML;

    psUnitXML = CPLCreateXMLNode( psParent, CXT_Element, "AngularUnit" );

    addNameSet( psUnitXML, "degree" );
    CPLCreateXMLElementAndValue( psParent, "radiansPerUnit", 
                                 "0.0174532925199433" );
    addAuthority( psParent, "9102", "EPSG", "6.0" );
}

/************************************************************************/
/*                          exportUnitToXML()                           */
/************************************************************************/

static CPLXMLNode *exportUnitToXML( OGR_SRSNode *poParent,
                                    CPLXMLNode *psXMLParent,
                                    int bLinearUnit )
    
{
    CPLXMLNode *psUnitXML, *psNode;
    OGR_SRSNode *poUNIT;

/* -------------------------------------------------------------------- */
/*      Get authority node from parent.                                 */
/* -------------------------------------------------------------------- */
    if( poParent->FindChild( "UNIT" ) == -1 )
        return NULL;

    poUNIT = poParent->GetChild( poParent->FindChild( "UNIT" ));

/* -------------------------------------------------------------------- */
/*      Create Linear/Angular units declaration.                        */
/* -------------------------------------------------------------------- */
    if( bLinearUnit )
        psUnitXML = CPLCreateXMLNode( psXMLParent, CXT_Element, 
                                      "LinearUnit" );
    else
        psUnitXML = CPLCreateXMLNode( psXMLParent, CXT_Element, 
                                      "AngularUnit" );

/* -------------------------------------------------------------------- */
/*      Add the name as a nameset.                                      */
/* -------------------------------------------------------------------- */
    addNameSet( psUnitXML, poUNIT->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Add the authority, if present.                                  */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poUNIT, psUnitXML );

/* -------------------------------------------------------------------- */
/*      Give definition.                                                */
/* -------------------------------------------------------------------- */
    if( bLinearUnit )
        psNode = CPLCreateXMLNode( psUnitXML, CXT_Element, "metersPerUnit" );
    else
        psNode = CPLCreateXMLNode( psUnitXML, CXT_Element, "radiansPerUnit" );

    CPLCreateXMLNode( psNode, CXT_Text, poUNIT->GetChild(1)->GetValue() );

    return psUnitXML;
}

/************************************************************************/
/*                             addProjArg()                             */
/************************************************************************/

static void addProjArg( OGRSpatialReference *poSRS, CPLXMLNode *psBase, 
                        const char *pszMeasureType, const char *pszValue, 
                        const char *pszXMLName, const char *pszWKTName )

{
    CPLXMLNode *psNode;

    psNode = CPLCreateXMLNode( psBase, CXT_Element, pszXMLName );
    
    if( poSRS->GetAttrNode( pszWKTName ) != NULL )
        pszValue = poSRS->GetAttrValue( pszWKTName );

    CPLCreateXMLElementAndValue( psNode, "value", pszValue );

    if( EQUAL(pszMeasureType,"Linear") )
        addMeterUnit( psNode );
    else if( EQUAL(pszMeasureType, "Angular") )
        addRadianUnit( psNode );
}

/************************************************************************/
/*                         exportGeogCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportGeogCSToXML( OGRSpatialReference *poSRS )

{
    CPLXMLNode  *psGCS_XML;
    OGR_SRSNode *poGeogCS = poSRS->GetAttrNode( "GEOGCS" );

    if( poGeogCS == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Establish initial infrastructure.                               */
/* -------------------------------------------------------------------- */
    psGCS_XML = CPLCreateXMLNode( NULL, CXT_Element, 
                                  "CoordinateReferenceSystem" );
    
/* -------------------------------------------------------------------- */
/*      Attach symbolic name (a name in a nameset).                     */
/* -------------------------------------------------------------------- */
    addNameSet( psGCS_XML, poGeogCS->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Does the overall coordinate system have an authority?  If so    */
/*      attach as an identification section.                            */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poGeogCS, psGCS_XML );

/* -------------------------------------------------------------------- */
/*      It is a 2D Geographic CRS.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *ps2DGeog;

    ps2DGeog = CPLCreateXMLNode( psGCS_XML, CXT_Element, "Geographic2dCRS" );

/* -------------------------------------------------------------------- */
/*      Start with the datum.                                           */
/* -------------------------------------------------------------------- */
    OGR_SRSNode    *poDatum = poGeogCS->GetNode( "DATUM" );
    CPLXMLNode     *psDatumXML;

    if( poDatum == NULL )
    {
        CPLDestroyXMLNode( psGCS_XML );
        return NULL;
    }

    psDatumXML = CPLCreateXMLNode( ps2DGeog, CXT_Element, "GeodeticDatum" );

/* -------------------------------------------------------------------- */
/*      add name, authority and units.                                  */
/* -------------------------------------------------------------------- */
    addNameSet( psDatumXML, poDatum->GetChild(0)->GetValue() );
    exportAuthorityToXML( poDatum, psDatumXML );

/* -------------------------------------------------------------------- */
/*      Translate the ellipsoid.                                        */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poEllipsoid = poDatum->GetNode( "SPHEROID" );

    if( poEllipsoid != NULL )
    {
        CPLXMLNode *psEllipseXML;

        psEllipseXML = CPLCreateXMLNode( psDatumXML, CXT_Element, 
                                         "Ellipsoid" );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psEllipseXML, CXT_Attribute, "flatteningDefinitive"),
            CXT_Text, "true" );
        
        addNameSet( psEllipseXML, poEllipsoid->GetChild(0)->GetValue() );

        exportAuthorityToXML( poEllipsoid, psEllipseXML );

        CPLCreateXMLNode( 
            CPLCreateXMLNode( psEllipseXML, CXT_Element, "semiMajorAxis" ),
            CXT_Text, poEllipsoid->GetChild(1)->GetValue() );
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psEllipseXML, CXT_Element, "inverseFlattening" ),
            CXT_Text, poEllipsoid->GetChild(2)->GetValue() );

        /* NOTE: we should really add a LinearUnit Meter declaration here */
    }

/* -------------------------------------------------------------------- */
/*      Add the prime meridian to the datum.                            */
/* -------------------------------------------------------------------- */
    OGR_SRSNode *poPRIMEM = poGeogCS->GetNode( "PRIMEM" );
        
    if( poPRIMEM != NULL )
    {
        CPLXMLNode *psPM_XML;

        psPM_XML = CPLCreateXMLNode( psDatumXML, CXT_Element, "PrimeMeridian");
        addNameSet( psPM_XML, poPRIMEM->GetChild(0)->GetValue() );

        exportAuthorityToXML( poPRIMEM, psPM_XML );

        CPLCreateXMLNode( 
            CPLCreateXMLNode( psPM_XML, CXT_Element, "greenwichLongitude"),
            CXT_Text, poPRIMEM->GetChild(1)->GetValue() );
    }

/* -------------------------------------------------------------------- */
/*      Create EllipsoidalCoordinateSystem definition.                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psECS;

    psECS = CPLCreateXMLNode( ps2DGeog, CXT_Element, 
                              "EllipsoidalCoordinateSystem" );
    
/* -------------------------------------------------------------------- */
/*      Setup the dimensions.                                           */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode( 
        CPLCreateXMLNode( psECS, CXT_Element, "dimensions" ), 
        CXT_Text, "2" );
    
/* -------------------------------------------------------------------- */
/*      Setup the latitude.                                             */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCA;

    psCA = CPLCreateXMLNode( psECS, CXT_Element, "CoordinateAxis" );
    
    addNameSet( psCA, "Geodetic latitude" );

    CPLCreateXMLElementAndValue( psCA, "axisAbbreviation", "Lat" );
    CPLCreateXMLElementAndValue( psCA, "axisDirection", "north" );
    
    exportUnitToXML( poGeogCS, psCA, FALSE );

/* -------------------------------------------------------------------- */
/*      Setup the longitude.                                            */
/* -------------------------------------------------------------------- */
    psCA = CPLCreateXMLNode( psECS, CXT_Element, "CoordinateAxis" );
    
    addNameSet( psCA, "Geodetic longitude" );

    CPLCreateXMLElementAndValue( psCA, "axisAbbreviation", "Lon" );
    CPLCreateXMLElementAndValue( psCA, "axisDirection", "east" );
    
    exportUnitToXML( poGeogCS, psCA, FALSE );

    return psGCS_XML;
}

/************************************************************************/
/*                         exportProjCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportProjCSToXML( OGRSpatialReference *poSRS )

{
    OGR_SRSNode *poProjCS = poSRS->GetAttrNode( "PROJCS" );

    if( poProjCS == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Establish initial infrastructure.                               */
/* -------------------------------------------------------------------- */
    CPLXMLNode   *psCRS_XML;

    psCRS_XML = CPLCreateXMLNode( NULL, CXT_Element, 
                                  "CoordinateReferenceSystem" );
    
/* -------------------------------------------------------------------- */
/*      Attach symbolic name (a name in a nameset).                     */
/* -------------------------------------------------------------------- */
    addNameSet( psCRS_XML, poProjCS->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      It is a ProjectedCRS.                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psPCRS;

    psPCRS = CPLCreateXMLNode( psCRS_XML, CXT_Element, "ProjectedCRS" );

/* -------------------------------------------------------------------- */
/*      Define the cartesian coordinate system.                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode  *psCCS;

    psCCS = CPLCreateXMLNode( psPCRS, CXT_Element, 
                              "CartesianCoordinateSystem" );

    addNameSet( psCCS, "Cartesian" );

/* -------------------------------------------------------------------- */
/*      Setup the dimensions.                                           */
/* -------------------------------------------------------------------- */
    CPLCreateXMLNode( 
        CPLCreateXMLNode( psCCS, CXT_Element, "dimensions" ), 
        CXT_Text, "2" );
    
/* -------------------------------------------------------------------- */
/*      Setup the Easting axis.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCA;

    psCA = CPLCreateXMLNode( psCCS, CXT_Element, "CoordinateAxis" );
    
    addNameSet( psCA, "Easting" );

    CPLCreateXMLElementAndValue( psCA, "axisAbbreviation", "E" );
    CPLCreateXMLElementAndValue( psCA, "axisDirection", "east" );
    
    exportUnitToXML( poProjCS, psCA, TRUE );

/* -------------------------------------------------------------------- */
/*      Setup the Northing axis.                                        */
/* -------------------------------------------------------------------- */
    psCA = CPLCreateXMLNode( psCCS, CXT_Element, "CoordinateAxis" );
    
    addNameSet( psCA, "Northing" );

    CPLCreateXMLElementAndValue( psCA, "axisAbbreviation", "N" );
    CPLCreateXMLElementAndValue( psCA, "axisDirection", "north" );
    
    exportUnitToXML( poProjCS, psCA, TRUE );

/* -------------------------------------------------------------------- */
/*      Emit the GEOGCS coordinate system associated with this          */
/*      PROJCS.                                                         */
/* -------------------------------------------------------------------- */
    CPLAddXMLChild( psPCRS, exportGeogCSToXML( poSRS ) );
    
/* -------------------------------------------------------------------- */
/*      Create the CoordinateTransformationDefinition                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psCTD;

    psCTD = CPLCreateXMLNode( psPCRS, CXT_Element, 
                              "CoordinateTransformationDefinition" );

    CPLCreateXMLElementAndValue( psCTD, "sourceDimensions", "2" );
    CPLCreateXMLElementAndValue( psCTD, "targetDimensions", "2" );

/* -------------------------------------------------------------------- */
/*      Projections are handled as ParameterizedTransformations.        */
/* -------------------------------------------------------------------- */
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");
    CPLXMLNode *psPT;

    psPT = CPLCreateXMLNode( psCTD, CXT_Element, 
                             "ParameterizedTransformation" );

/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        CPLXMLNode *psBase;

        psBase = CPLCreateXMLNode( psPT, CXT_Element, "TransverseMercator" );
        
        addProjArg( poSRS, psBase, "Angular", "0.0",
                    "LatitudeOfNaturalOrigin", SRS_PP_LATITUDE_OF_ORIGIN );
        addProjArg( poSRS, psBase, "Angular", "0.0",
                    "LongitudeOfNaturalOrigin", SRS_PP_CENTRAL_MERIDIAN );
        addProjArg( poSRS, psBase, "Unitless", "1.0",
                    "ScaleFactorAtNaturalOrigin", SRS_PP_SCALE_FACTOR );
        addProjArg( poSRS, psBase, "Linear", "0.0",
                    "FalseEasting", SRS_PP_FALSE_EASTING );
        addProjArg( poSRS, psBase, "Linear", "0.0",
                    "FalseNorthing", SRS_PP_FALSE_NORTHING );
    }

/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    else if( EQUAL(pszProjection,SRS_PT_POLAR_STEREOGRAPHIC) )
    {
        CPLXMLNode *psBase;

        psBase = CPLCreateXMLNode( psPT, CXT_Element, "TransverseMercator" );
        
        addProjArg( poSRS, psBase, "Angular", "0.0",
                    "LatitudeOfNaturalOrigin", SRS_PP_LATITUDE_OF_ORIGIN );
        addProjArg( poSRS, psBase, "Angular", "0.0",
                    "LongitudeOfNaturalOrigin", SRS_PP_CENTRAL_MERIDIAN );
        addProjArg( poSRS, psBase, "Unitless", "1.0",
                    "ScaleFactorAtNaturalOrigin", SRS_PP_SCALE_FACTOR );
        addProjArg( poSRS, psBase, "Linear", "0.0",
                    "FalseEasting", SRS_PP_FALSE_EASTING );
        addProjArg( poSRS, psBase, "Linear", "0.0",
                    "FalseNorthing", SRS_PP_FALSE_NORTHING );
    }

    return psCRS_XML;
}

/************************************************************************/
/*                            exportToXML()                             */
/************************************************************************/

OGRErr OGRSpatialReference::exportToXML( char **ppszRawXML, 
                                         const char * )

{
    CPLXMLNode *psXMLTree = NULL;

    if( IsGeographic() )
    {
        psXMLTree = exportGeogCSToXML( this );
    }
    else if( IsProjected() )
    {
        psXMLTree = exportProjCSToXML( this );
    }
    else
        return OGRERR_UNSUPPORTED_SRS;

    *ppszRawXML = CPLSerializeXMLTree( psXMLTree );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           importXMLUnits()                           */
/************************************************************************/

static void importXMLUnits( CPLXMLNode *psSrcXML, const char *pszClass,
                            OGRSpatialReference *poSRS, const char *pszTarget)

{
    const char *pszUnitName, *pszUnitsPer;
    OGR_SRSNode *poNode = poSRS->GetAttrNode( pszTarget );
    OGR_SRSNode *poUnits;

    CPLAssert( EQUAL(pszClass,"AngularUnit") 
               || EQUAL(pszClass,"LinearUnit") );
        
    psSrcXML = CPLGetXMLNode( psSrcXML, pszClass );
    if( psSrcXML == NULL )
        goto DefaultTarget;

    pszUnitName = CPLGetXMLValue( psSrcXML, "NameSet.name", "unnamed" );
    if( EQUAL(pszClass,"AngularUnit") )
        pszUnitsPer = CPLGetXMLValue( psSrcXML, "radiansPerUnit", NULL );
    else
        pszUnitsPer = CPLGetXMLValue( psSrcXML, "metresPerUnit", NULL );
    
    if( pszUnitsPer == NULL )
    {
        CPLDebug( "OGR_SRS_XML", 
                  "Missing PerUnit value for %s.", 
                  pszClass );
        goto DefaultTarget;
    }
    
    if( poNode == NULL )
    {
        CPLDebug( "OGR_SRS_XML", "Can't find %s in importXMLUnits.", 
                  pszTarget );
        goto DefaultTarget;
    }

    if( poNode->FindChild("UNIT") != -1 )
    {
        poUnits = poNode->GetChild( poNode->FindChild( "UNIT" ) );
        poUnits->GetChild(0)->SetValue( pszUnitName );
        poUnits->GetChild(1)->SetValue( pszUnitsPer );
    }
    else
    {
        poUnits = new OGR_SRSNode( "UNIT" );
        poUnits->AddChild( new OGR_SRSNode( pszUnitName ) );
        poUnits->AddChild( new OGR_SRSNode( pszUnitsPer ) );
        
        poNode->AddChild( poUnits );
    }
    return;

  DefaultTarget:
    poUnits = new OGR_SRSNode( "UNIT" );
    if( EQUAL(pszClass,"AngularUnit") )
    {
        poUnits->AddChild( new OGR_SRSNode( SRS_UA_DEGREE ) );
        poUnits->AddChild( new OGR_SRSNode( SRS_UA_DEGREE_CONV ) );
    }
    else
    {
        poUnits->AddChild( new OGR_SRSNode( SRS_UL_METER ) );
        poUnits->AddChild( new OGR_SRSNode( "1.0" ) );
    }

    poNode->AddChild( poUnits );
}


/************************************************************************/
/*                         importXMLAuthority()                         */
/************************************************************************/

static void importXMLAuthority( CPLXMLNode *psSrcXML, 
                                OGRSpatialReference *poSRS, 
                                const char *pszTargetKey )

{
    if( CPLGetXMLNode( psSrcXML, "Identifier" ) == NULL 
        || CPLGetXMLNode( psSrcXML, "Identifier.code" ) == NULL 
        || CPLGetXMLNode( psSrcXML, "Identifier.codeSpace" ) == NULL )
        return;

    poSRS->SetAuthority( pszTargetKey, 
                         CPLGetXMLValue(psSrcXML,"Identifier.codeSpace",""),
                         atoi(CPLGetXMLValue(psSrcXML,"Identifier.code","0")));
}

/************************************************************************/
/*                        importGeogCSFromXML()                         */
/************************************************************************/

static OGRErr importGeogCSFromXML( OGRSpatialReference *poSRS, 
                                   CPLXMLNode *psCRS )

{
    CPLXMLNode     *psGeo2DCRS;
    const char     *pszGeogName, *pszDatumName, *pszEllipsoidName, *pszPMName;
    double         dfSemiMajor, dfInvFlattening, dfPMOffset = 0.0;
    double         dfEllipsoidUnits;

    pszGeogName = 
        CPLGetXMLValue( psCRS, "NameSet.name", "Unnamed GeogCS" );

/* -------------------------------------------------------------------- */
/*      Get datum name.                                                 */
/* -------------------------------------------------------------------- */
    psGeo2DCRS = CPLGetXMLNode( psCRS, "Geographic2dCRS" );
    pszDatumName = 
        CPLGetXMLValue( psGeo2DCRS, "GeodeticDatum.NameSet.name", 
                        "Unnamed Datum" );

/* -------------------------------------------------------------------- */
/*      Get ellipsoid information.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psE;

    psE = CPLGetXMLNode( psGeo2DCRS, "GeodeticDatum.Ellipsoid" );
    pszEllipsoidName = 
        CPLGetXMLValue( psE, "NameSet.name", "Unnamed Ellipsoid" );

    dfEllipsoidUnits = atof(
        CPLGetXMLValue( psE, "LinearUnit.metresPerUnit", "1.0" ));
    if( dfEllipsoidUnits == 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Ellipsoid has corrupt linear units." );
        return OGRERR_CORRUPT_DATA; 
    }

    dfSemiMajor = 
        dfEllipsoidUnits * atof(CPLGetXMLValue( psE, "semiMajorAxis", "0.0" ));
    if( dfSemiMajor == 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Ellipsoid semiMajorAxis corrupt or missing." );
        return OGRERR_CORRUPT_DATA;
    }
            
    dfInvFlattening = atof(CPLGetXMLValue( psE, "inverseFlattening", "0.0" ));
    if( dfInvFlattening == 0.0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Ellipsoid inverseFlattening corrupt or missing." );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Get the prime meridian.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psPM;

    psPM = CPLGetXMLNode( psGeo2DCRS, "GeodeticDatum.PrimeMeridian" );
    if( psPM == NULL )
    {
        pszPMName = "Greenwich";
        dfPMOffset = 0.0;
    }
    else
    {
        pszPMName = CPLGetXMLValue( psPM, "NameSet.name", 
                                    "Unnamed Prime Meridian");
        dfPMOffset = 
            atof(CPLGetXMLValue( psPM, "greenwichLongitude", "0.0" ));

        /* There should likely be a check for units here, note that
           we want to have it in degrees not radians for SetGeogCS(). */
    }

/* -------------------------------------------------------------------- */
/*      Set the geographic definition.                                  */
/* -------------------------------------------------------------------- */
    poSRS->SetGeogCS( pszGeogName, pszDatumName, 
                      pszEllipsoidName, dfSemiMajor, dfInvFlattening, 
                      pszPMName, dfPMOffset );

/* -------------------------------------------------------------------- */
/*      Look for angular units.  We don't check that all axes match     */
/*      at this time.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psAxis;

    psAxis = CPLGetXMLNode( psGeo2DCRS, 
                            "EllipsoidalCoordinateSystem.CoordinateAxis" );
    importXMLUnits( psAxis, "AngularUnit", poSRS, "GEOGCS" );

/* -------------------------------------------------------------------- */
/*      Can we set authorities for any of the levels?                   */
/* -------------------------------------------------------------------- */
    importXMLAuthority( psCRS, poSRS, "GEOGCS" );
    importXMLAuthority( 
        CPLGetXMLNode( psCRS, "Geographic2dCRS.GeodeticDatum" ), 
        poSRS, "GEOGCS|DATUM" );
    importXMLAuthority( 
        CPLGetXMLNode( psCRS, "Geographic2dCRS.GeodeticDatum.Ellipsoid" ), 
        poSRS, "GEOGCS|DATUM|SPHEROID" );
    importXMLAuthority( 
        CPLGetXMLNode( psCRS, "Geographic2dCRS.GeodeticDatum.PrimeMeridian" ), 
        poSRS, "GEOGCS|PRIMEM" );
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           importFromXML()                            */
/************************************************************************/

OGRErr OGRSpatialReference::importFromXML( const char *pszXML )

{
    CPLXMLNode *psTree;

    this->Clear();
        
/* -------------------------------------------------------------------- */
/*      Parse the XML.                                                  */
/* -------------------------------------------------------------------- */
    psTree = CPLParseXMLString( pszXML );
    
    if( psTree == NULL )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Do we have a CoordinateSystemDefinition                         */
/* -------------------------------------------------------------------- */
    if( !EQUAL(psTree->pszValue,"CoordinateReferenceSystem") )
        return OGRERR_CORRUPT_DATA;

/* -------------------------------------------------------------------- */
/*      Is this a GEOGCS.                                               */
/* -------------------------------------------------------------------- */
    if( CPLGetXMLNode( psTree, "Geographic2dCRS" ) != NULL  )
    {
        return importGeogCSFromXML( this, psTree );
    }

    return OGRERR_UNSUPPORTED_SRS;
}

