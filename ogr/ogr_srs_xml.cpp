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
 * Revision 1.3  2003/03/21 22:14:43  warmerda
 * first pass re-implementation using GML 3 schemas
 *
 * Revision 1.2  2002/04/18 14:22:45  warmerda
 * made OGRSpatialReference and co 'const correct'
 *
 * Revision 1.1  2001/12/06 18:16:17  warmerda
 * new
 *
 */

#include "ogr_spatialref.h"
#include "ogr_p.h"
#include "cpl_minixml.h"

/************************************************************************/
/*                               addID()                                */
/************************************************************************/

static CPLXMLNode *addID( CPLXMLNode *psParent, 
                          const char *pszTagName,
                          const char *pszCode, 
                          const char *pszCodeSpace, 
                          const char *pszEdition = NULL )

{
    CPLXMLNode *psIdentifier;

    psIdentifier = CPLCreateXMLNode( psParent, CXT_Element, pszTagName );

    CPLCreateXMLElementAndValue( psIdentifier, "code", pszCode );
    CPLCreateXMLElementAndValue( psIdentifier, "codeSpace", pszCodeSpace );
    if( pszEdition != NULL )
        CPLCreateXMLElementAndValue( psIdentifier, "version", pszEdition );

    return psIdentifier;
}

/************************************************************************/
/*                        exportAuthorityToXML()                        */
/************************************************************************/

static CPLXMLNode *exportAuthorityToXML( const OGR_SRSNode *poAuthParent,
                                         const char *pszTagName,
                                         CPLXMLNode *psXMLParent )

{
    const OGR_SRSNode *poAuthority;

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
    const char *pszCode, *pszCodeSpace, *pszEdition;

    pszCode = poAuthority->GetChild(1)->GetValue();
    pszCodeSpace = poAuthority->GetChild(0)->GetValue();
    pszEdition = NULL;

    return addID( psXMLParent, pszTagName, pszCode, pszCodeSpace, pszEdition );
}

/************************************************************************/
/*                            addMeterUnit()                            */
/************************************************************************/

static void addMeterUnit( CPLXMLNode *psParent )

{
    CPLXMLNode *psUnitXML;

    psUnitXML = CPLCreateXMLNode( psParent, CXT_Element, "LinearUnit" );

    CPLCreateXMLElementAndValue( psParent, "metresPerUnit", "1" );
    addID( psParent, "id", "9001", "EPSG", "6.0" );
}

/************************************************************************/
/*                           addRadianUnit()                            */
/************************************************************************/

static void addRadianUnit( CPLXMLNode *psParent )

{
    CPLXMLNode *psUnitXML;

    psUnitXML = CPLCreateXMLNode( psParent, CXT_Element, "AngularUnit" );

    CPLCreateXMLElementAndValue( psParent, "radiansPerUnit", 
                                 "0.0174532925199433" );
    addID( psParent, "id", "9102", "EPSG", "6.0" );
}

/************************************************************************/
/*                          exportUnitToXML()                           */
/************************************************************************/

static CPLXMLNode *exportUnitToXML( const OGR_SRSNode *poParent,
                                    CPLXMLNode *psXMLParent,
                                    int bLinearUnit )
    
{
    CPLXMLNode *psUnitXML, *psNode;
    const OGR_SRSNode *poUNIT;

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
    //addNameSet( psUnitXML, poUNIT->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Add the authority, if present.                                  */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poUNIT, "id", psUnitXML );

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

static void addProjArg( const OGRSpatialReference *poSRS, CPLXMLNode *psBase, 
                        const char *pszMeasureType, double dfDefault,
                        const char *pszXMLName, const char *pszWKTName )

{
    CPLXMLNode *psNode, *psValue;
    char szFullName[128];

/* -------------------------------------------------------------------- */
/*      Create the uses*Value node.                                     */
/* -------------------------------------------------------------------- */
    sprintf( szFullName, "uses%sValue", pszXMLName );
    psNode = CPLCreateXMLNode( psBase, CXT_Element, szFullName );
    
/* -------------------------------------------------------------------- */
/*      Handle the UOM.                                                 */
/* -------------------------------------------------------------------- */
    const char *pszUOMValue;

    if( EQUAL(pszMeasureType,"Angular") )
        pszUOMValue = "#degree";
    else
        pszUOMValue = "#metre";

    psValue = CPLCreateXMLNode( psNode, CXT_Element, "value" );
    CPLCreateXMLNode( 
        CPLCreateXMLNode( psValue, CXT_Attribute, "uom" ),
        CXT_Text, pszUOMValue );
    
/* -------------------------------------------------------------------- */
/*      Add the parameter value itself.                                 */
/* -------------------------------------------------------------------- */
    double dfParmValue
        = poSRS->GetNormProjParm( pszWKTName, dfDefault, NULL );
        
    CPLCreateXMLNode( psValue, CXT_Text, 
                      CPLSPrintf( "%.16g", dfParmValue ) );

    // what about this valueOfParameter business?
}

/************************************************************************/
/*                              addAxis()                               */
/*                                                                      */
/*      Added the <usesAxis> element and down.                          */
/************************************************************************/

static CPLXMLNode *addAxis( CPLXMLNode *psXMLParent, 
                            const char *pszAxis, // "Lat", "Long", "E" or "N"
                            const OGR_SRSNode *poUnitsSrc )

{
    CPLXMLNode *psAxisXML;

    psAxisXML = 
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psXMLParent, CXT_Element, "usesAxis" ),
            CXT_Element, "CoordinateSystemAxis" );

    if( EQUAL(pszAxis,"Lat") )
    {
        CPLCreateXMLElementAndValue( psAxisXML, "axisName",
                                     "Geodetic latitude" );
        addID( psAxisXML, "axisID", "9901", "EPSG", "6.0" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisAbbrev", "Lat" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisDirection", "north" );
    }
    else if( EQUAL(pszAxis,"Long") )
    {
        CPLCreateXMLElementAndValue( psAxisXML, "axisName",
                                     "Geodetic longitude" );
        addID( psAxisXML, "axisID", "9902", "EPSG", "6.0" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisAbbrev", "Lon" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisDirection", "east" );
    }
    else if( EQUAL(pszAxis,"E") )
    {
        CPLCreateXMLElementAndValue( psAxisXML, "axisName", "Easting" );
        addID( psAxisXML, "axisID", "9906", "EPSG", "6.0" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisAbbrev", "E" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisDirection", "east" );
    }
    else if( EQUAL(pszAxis,"N") )
    {
        CPLCreateXMLElementAndValue( psAxisXML, "axisName", "Northing" );
        addID( psAxisXML, "axisID", "9907", "EPSG", "6.0" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisAbbrev", "N" );
        CPLCreateXMLElementAndValue( psAxisXML, "axisDirection", "north" );
    }
    else
    {
        CPLAssert( FALSE );
    }

    return psAxisXML;
}

/************************************************************************/
/*                         exportGeogCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportGeogCSToXML( const OGRSpatialReference *poSRS )

{
    CPLXMLNode  *psGCS_XML;
    const OGR_SRSNode *poGeogCS = poSRS->GetAttrNode( "GEOGCS" );

    if( poGeogCS == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Establish initial infrastructure.                               */
/* -------------------------------------------------------------------- */
    psGCS_XML = CPLCreateXMLNode( NULL, CXT_Element, "GeographicCRS" );
    
/* -------------------------------------------------------------------- */
/*      Attach symbolic name (srsName).                                 */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psGCS_XML, "srsName", 
                                 poGeogCS->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Does the overall coordinate system have an authority?  If so    */
/*      attach as an identification section.                            */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poGeogCS, "srsID", psGCS_XML );

/* -------------------------------------------------------------------- */
/*      Insert a big whack of fixed stuff defining the                  */
/*      ellipsoidalCS.  Basically this defines the axes and their       */
/*      units.                                                          */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psECS;

    psECS = CPLCreateXMLNode( 
        CPLCreateXMLNode( psGCS_XML, CXT_Element, "usesEllipsoidalCS" ),
        CXT_Element, "EllipsoidalCS" );

    CPLCreateXMLElementAndValue( psECS, "csName", "ellipsoidal" );
    addID( psECS, "csID", "6402", "EPSG", "6.0" );

    addAxis( psECS, "Lat", NULL );
    addAxis( psECS, "Long", NULL );

/* -------------------------------------------------------------------- */
/*      Start with the datum.                                           */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode    *poDatum = poGeogCS->GetNode( "DATUM" );
    CPLXMLNode     *psDatumXML;

    if( poDatum == NULL )
    {
        CPLDestroyXMLNode( psGCS_XML );
        return NULL;
    }

    psDatumXML = CPLCreateXMLNode( 
        CPLCreateXMLNode( psGCS_XML, CXT_Element, "usesGeodeticDatum" ),
        CXT_Element, "GeodeticDatum" );

/* -------------------------------------------------------------------- */
/*      Set the datumName.                                              */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psDatumXML, "datumName", 
                                 poDatum->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Set authority id info if available.                             */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poDatum, "datumID", psDatumXML );

/* -------------------------------------------------------------------- */
/*      Setup prime meridian information.                               */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poPMNode = poGeogCS->GetNode( "PRIMEM" );
    CPLXMLNode *psPM;
    char *pszPMName = "Greenwich";
    double dfPMOffset = poSRS->GetPrimeMeridian( &pszPMName );

    psPM = CPLCreateXMLNode( 
        CPLCreateXMLNode( psDatumXML, CXT_Element, "usesPrimeMeridian" ),
        CXT_Element, "PrimeMeridian" );

    CPLCreateXMLElementAndValue( psPM, "meridianName", pszPMName );

    if( poPMNode )
        exportAuthorityToXML( poPMNode, "meridianID", psPM );

    CPLXMLNode *psAngle;

    psAngle = 
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psPM, CXT_Element, "greenwichLongitude" ),
            CXT_Element, "angle" );
    
    CPLCreateXMLNode( CPLCreateXMLNode( psAngle, CXT_Attribute, "uom" ),
                      CXT_Text, "#degree" );

    CPLCreateXMLNode( psAngle, CXT_Text, CPLSPrintf( "%.16g", dfPMOffset ) );

/* -------------------------------------------------------------------- */
/*      Translate the ellipsoid.                                        */
/* -------------------------------------------------------------------- */
    const OGR_SRSNode *poEllipsoid = poDatum->GetNode( "SPHEROID" );

    if( poEllipsoid != NULL )
    {
        CPLXMLNode *psEllipseXML;

        psEllipseXML = 
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psDatumXML, CXT_Element, "usesEllipsoid" ),
                CXT_Element, "Ellipsoid" );

        CPLCreateXMLElementAndValue( psEllipseXML, "ellipsoidName", 
                                     poEllipsoid->GetChild(0)->GetValue() );

        exportAuthorityToXML( poEllipsoid, "ellipsoidID", psEllipseXML );
        
        CPLXMLNode *psParmXML;

        psParmXML = CPLCreateXMLNode( psEllipseXML, CXT_Element, 
                                      "semiMajorAxis" );

        CPLCreateXMLNode( CPLCreateXMLNode( psParmXML, CXT_Attribute, "uom" ),
                          CXT_Text, "#metre" );

        CPLCreateXMLNode( psParmXML, CXT_Text, 
                          poEllipsoid->GetChild(1)->GetValue() );

        psParmXML = 
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psEllipseXML, CXT_Element, "secondDefiningParameter" ),
                CXT_Element, "inverseFlattening" );
        
        CPLCreateXMLNode( CPLCreateXMLNode( psParmXML, CXT_Attribute, "uom" ),
                          CXT_Text, "#one" );

        CPLCreateXMLNode( psParmXML, CXT_Text, 
                          poEllipsoid->GetChild(2)->GetValue() );
    }

    return psGCS_XML;
}

/************************************************************************/
/*                         exportProjCSToXML()                          */
/************************************************************************/

static CPLXMLNode *exportProjCSToXML( const OGRSpatialReference *poSRS )

{
    const OGR_SRSNode *poProjCS = poSRS->GetAttrNode( "PROJCS" );

    if( poProjCS == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Establish initial infrastructure.                               */
/* -------------------------------------------------------------------- */
    CPLXMLNode   *psCRS_XML;

    psCRS_XML = CPLCreateXMLNode( NULL, CXT_Element, "ProjectedCRS" );
    
/* -------------------------------------------------------------------- */
/*      Attach symbolic name (a name in a nameset).                     */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( psCRS_XML, "srsName", 
                                 poProjCS->GetChild(0)->GetValue() );

/* -------------------------------------------------------------------- */
/*      Add authority info if we have it.                               */
/* -------------------------------------------------------------------- */
    exportAuthorityToXML( poProjCS, "srsID", psCRS_XML );

/* -------------------------------------------------------------------- */
/*      Use the GEOGCS as a <baseCRS>                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBaseCRSXML =
        CPLCreateXMLNode( psCRS_XML, CXT_Element, "baseCRS" );

    CPLAddXMLChild( psBaseCRSXML, exportGeogCSToXML( poSRS ) );

/* -------------------------------------------------------------------- */
/*      Our projected coordinate system is "defined by Conversion".     */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDefinedBy;

    psDefinedBy = CPLCreateXMLNode( psCRS_XML, CXT_Element, 
                                    "definedByConversion" );
    
/* -------------------------------------------------------------------- */
/*      Projections are handled as ParameterizedTransformations.        */
/* -------------------------------------------------------------------- */
    const char *pszProjection = poSRS->GetAttrValue("PROJECTION");
    CPLXMLNode *psConv;

/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        psConv = CPLCreateXMLNode( psDefinedBy, CXT_Element, 
                                   "TransverseMercatorConversion" );
        
        addProjArg( poSRS, psConv, "Angular", 0.0,
                    "LatitudeOfNaturalOrigin", SRS_PP_LATITUDE_OF_ORIGIN );
        addProjArg( poSRS, psConv, "Angular", 0.0,
                    "LongitudeOfNaturalOrigin", SRS_PP_CENTRAL_MERIDIAN );
        addProjArg( poSRS, psConv, "Unitless", 1.0,
                    "ScaleFactorAtNaturalOrigin", SRS_PP_SCALE_FACTOR );
        addProjArg( poSRS, psConv, "Linear", 0.0,
                    "FalseEasting", SRS_PP_FALSE_EASTING );
        addProjArg( poSRS, psConv, "Linear", 0.0,
                    "FalseNorthing", SRS_PP_FALSE_NORTHING );
    }

/* -------------------------------------------------------------------- */
/*      Define the cartesian coordinate system.                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode  *psCCS;

    psCCS = 
        CPLCreateXMLNode( 
            CPLCreateXMLNode( psCRS_XML, CXT_Element, "usesCartesianCS" ),
            CXT_Element, "CartesianCS" );

    CPLCreateXMLElementAndValue( psCCS, "csName", "Cartesian" );
    addID( psCCS, "csID", "4400", "EPSG", "6.0" );
    addAxis( psCCS, "E", NULL );
    addAxis( psCCS, "N", NULL );

    return psCRS_XML;
}

/************************************************************************/
/*                            exportToXML()                             */
/************************************************************************/

OGRErr OGRSpatialReference::exportToXML( char **ppszRawXML, 
                                         const char * ) const

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
/*                           OSRExportToXML()                           */
/************************************************************************/

OGRErr OSRExportToXML( OGRSpatialReferenceH hSRS, char **ppszRawXML, 
                       const char *pszDialect )

{
    return ((OGRSpatialReference *) hSRS)->exportToXML( ppszRawXML, 
                                                        pszDialect );
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
                                const char *pszSourceKey,
                                const char *pszTargetKey )

{
    CPLXMLNode *psIDNode = CPLGetXMLNode( psSrcXML, pszSourceKey );

    if( psIDNode == NULL )
        return;

    if( CPLGetXMLNode( psIDNode, "code" ) == NULL 
        || CPLGetXMLNode( psIDNode, "codeSpace" ) == NULL )
        return;

    poSRS->SetAuthority( pszTargetKey, 
                         CPLGetXMLValue(psIDNode,"codeSpace",""),
                         atoi(CPLGetXMLValue(psIDNode,"code","0")));
}

/************************************************************************/
/*                         getNormalizedValue()                         */
/*                                                                      */
/*      Parse a node to get it's numerical value, and then normalize    */
/*      into meters of degrees depending on the measure type.           */
/************************************************************************/

static double getNormalizedValue( CPLXMLNode *psNode, const char *pszPath,
                                  const char *pszMeasure, double dfDefault )

{
    CPLXMLNode *psTargetNode;
    CPLXMLNode *psValueNode;

    if( pszPath == NULL || strlen(pszPath) == 0 )
        psTargetNode = psNode;
    else
        psTargetNode = CPLGetXMLNode( psNode, pszPath );
    
    if( psTargetNode == NULL )
        return dfDefault;

    for( psValueNode = psTargetNode->psChild; 
         psValueNode != NULL && psValueNode->eType != CXT_Text;
         psValueNode = psValueNode->psNext ) {}

    if( psValueNode == NULL )
        return dfDefault;
    
    // Add normalization later.

    return atof(psValueNode->pszValue);
}

/************************************************************************/
/*                        importGeogCSFromXML()                         */
/************************************************************************/

static OGRErr importGeogCSFromXML( OGRSpatialReference *poSRS, 
                                   CPLXMLNode *psCRS )

{
    const char     *pszGeogName, *pszDatumName, *pszEllipsoidName, *pszPMName;
    double         dfSemiMajor, dfInvFlattening, dfPMOffset = 0.0;

    pszGeogName = 
        CPLGetXMLValue( psCRS, "srsName", "Unnamed GeogCS" );

/* -------------------------------------------------------------------- */
/*      Get datum name.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode     *psDatum;

    psDatum = CPLGetXMLNode( psCRS, "usesGeodeticDatum.GeodeticDatum" );
    pszDatumName = 
        CPLGetXMLValue( psDatum, "datumName", "Unnamed Datum" );

/* -------------------------------------------------------------------- */
/*      Get ellipsoid information.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psE;

    psE = CPLGetXMLNode( psDatum, "usesEllipsoid.Ellipsoid" );
    pszEllipsoidName = 
        CPLGetXMLValue( psE, "ellipsoidName", "Unnamed Ellipsoid" );

    dfSemiMajor = getNormalizedValue( psE, "semiMajorAxis", "Linear", 
                                      SRS_WGS84_SEMIMAJOR );

    dfInvFlattening = 
        getNormalizedValue( psE, "secondDefiningParameter.inverseFlattening", 
                            "Unitless", 0.0 );

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

    psPM = CPLGetXMLNode( psDatum, "usesPrimeMeridian.PrimeMeridian" );
    if( psPM == NULL )
    {
        pszPMName = "Greenwich";
        dfPMOffset = 0.0;
    }
    else
    {
        pszPMName = CPLGetXMLValue( psPM, "meridianName", 
                                    "Unnamed Prime Meridian");
        dfPMOffset = 
            getNormalizedValue( psPM, "greenwichLongitude.angle",
                                "Angular", 0.0 );
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
#ifdef notdef
    CPLXMLNode *psAxis;

    psAxis = CPLGetXMLNode( psGeo2DCRS, 
                            "EllipsoidalCoordinateSystem.CoordinateAxis" );
    importXMLUnits( psAxis, "AngularUnit", poSRS, "GEOGCS" );
#endif

/* -------------------------------------------------------------------- */
/*      Can we set authorities for any of the levels?                   */
/* -------------------------------------------------------------------- */
    importXMLAuthority( psCRS, poSRS, "srsID", "GEOGCS" );
    importXMLAuthority( psDatum, poSRS, "datumID", "GEOGCS|DATUM" );
    importXMLAuthority( psE, poSRS, "ellipsoidID", 
                        "GEOGCS|DATUM|SPHEROID" );
    importXMLAuthority( psDatum, poSRS, 
                        "usesPrimeMeridian.PrimeMeridian.meridianID",
                        "GEOGCS|PRIMEM" );

    poSRS->Fixup();
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                        importProjCSFromXML()                         */
/************************************************************************/

static OGRErr importProjCSFromXML( OGRSpatialReference *poSRS, 
                                   CPLXMLNode *psCRS )

{
    CPLXMLNode *psSubXML;
    OGRErr eErr;

/* -------------------------------------------------------------------- */
/*      Setup the PROJCS node with a name.                              */
/* -------------------------------------------------------------------- */
    poSRS->SetProjCS( CPLGetXMLValue( psCRS, "srsName", "Unnamed" ) );

    importXMLAuthority( psCRS, poSRS, "srsID", "PROJCS" );

/* -------------------------------------------------------------------- */
/*      Try to set the GEOGCS info.                                     */
/* -------------------------------------------------------------------- */
    
    psSubXML = CPLGetXMLNode( psCRS, "baseCRS.GeographicCRS" );
    if( psSubXML != NULL )
    {
        eErr = importGeogCSFromXML( poSRS, psSubXML );
        if( eErr != OGRERR_NONE )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Get the conversion node.  It should be the only child of the    */
/*      definedByConversion node.                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psConv = NULL;

    psSubXML = CPLGetXMLNode( psCRS, "definedByConversion" );
    if( psSubXML != NULL )
        psConv = psSubXML->psChild;

    if( psConv == NULL || psConv->eType != CXT_Element )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find a conversion node under the definedByConversion\n"
                  "node of the ProjectedCRS." );
        return OGRERR_CORRUPT_DATA;
    }

/* -------------------------------------------------------------------- */
/*      Transverse Mercator.                                            */
/* -------------------------------------------------------------------- */
    if( EQUAL(psConv->pszValue,"TransverseMercatorConversion") )
    {
        poSRS->SetTM( 
            getNormalizedValue( psConv, 
                                "usesLatitudeOfNaturalOriginValue.value", 
                                "Linear", 0.0 ),
            getNormalizedValue( psConv, 
                                "usesLongitudeOfNaturalOriginValue.value", 
                                "Angular", 0.0),
            getNormalizedValue( psConv, 
                                "usesScaleFactorAtNaturalOriginValue.value", 
                                "Unitless", 1.0),
            getNormalizedValue( psConv, "usesFalseEastingValue.value", 
                                "Linear", 0.0),
            getNormalizedValue( psConv, "usesFalseNorthingValue.value", 
                                "Linear", 0.0) );
    }

/* -------------------------------------------------------------------- */
/*      Didn't recognise?                                               */
/* -------------------------------------------------------------------- */
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Conversion %s not recognised.", 
                  psConv->pszValue );
        return OGRERR_CORRUPT_DATA;
    }


/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    poSRS->Fixup();

    // Need to get linear units here!

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
/*      Import according to the root node type.                         */
/* -------------------------------------------------------------------- */
    if( EQUAL(psTree->pszValue,"GeographicCRS") )
    {
        return importGeogCSFromXML( this, psTree );
    }

    if( EQUAL(psTree->pszValue,"ProjectedCRS") )
    {
        return importProjCSFromXML( this, psTree );
    }

    return OGRERR_UNSUPPORTED_SRS;
}

/************************************************************************/
/*                          OSRImportFromXML()                          */
/************************************************************************/

OGRErr OSRImportFromXML( OGRSpatialReferenceH hSRS, const char *pszXML )

{
    return ((OGRSpatialReference *) hSRS)->importFromXML( pszXML );
}
