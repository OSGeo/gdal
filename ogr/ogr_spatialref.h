/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes for manipulating spatial reference systems in a
 *           platform non-specific manner.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.8  1999/09/29 16:36:17  warmerda
 * added several new projections
 *
 * Revision 1.7  1999/09/15 20:34:21  warmerda
 * South_Oriented to SouthOrientated, prototype changes
 *
 * Revision 1.6  1999/09/09 13:53:47  warmerda
 * use lower case for degree and radian
 *
 * Revision 1.5  1999/07/29 17:29:45  warmerda
 * added various help methods for projections
 *
 * Revision 1.4  1999/07/14 05:23:38  warmerda
 * Added projection set methods, and #defined tokens
 *
 * Revision 1.3  1999/06/25 20:21:18  warmerda
 * fleshed out classes
 *
 * Revision 1.2  1999/05/31 17:14:34  warmerda
 * Fixed define.
 *
 * Revision 1.1  1999/05/20 14:35:00  warmerda
 * New
 */

#ifndef _OGR_SPATIALREF_H_INCLUDED
#define _OGR_SPATIALREF_H_INCLUDED

#include "ogr_core.h"

/************************************************************************/
/*                             OGR_SRSNode                              */
/************************************************************************/

/**
 * Objects of this class are used to represent value nodes in the parsed
 * representation of the WKT SRS format.  For instance UNIT["METER",1]
 * would be rendered into three OGR_SRSNodes.  The root node would have a
 * value of UNIT, and two children, the first with a value of METER, and the
 * second with a value of 1.
 *
 * Normally application code just interacts with the OGRSpatialReference
 * object, which uses the OGR_SRSNode to implement it's data structure;
 * however, this class is user accessable for detailed access to components
 * of an SRS definition.
 */

class OGR_SRSNode
{
    char	*pszValue;

    int		nChildren;
    OGR_SRSNode	**papoChildNodes;

    void	ClearChildren();
    
  public:
    		OGR_SRSNode(const char * = NULL);
    		~OGR_SRSNode();

    int         IsLeafNode() { return nChildren == 0; }
    
    int		GetChildCount() { return nChildren; }
    OGR_SRSNode *GetChild( int );

    OGR_SRSNode *GetNode( const char * );

    void	AddChild( OGR_SRSNode * );

    const char  *GetValue() { return pszValue; }
    void        SetValue( const char * );

    OGR_SRSNode *Clone();

    OGRErr      importFromWkt( char ** );
    OGRErr	exportToWkt( char ** );
};

/************************************************************************/
/*                         OGRSpatialReference                          */
/************************************************************************/

/**
 * This class respresents a OpenGIS Spatial Reference System, and contains
 * methods for converting between this object organization and well known
 * text (WKT) format.  This object is reference counted as one instance of
 * the object is normally shared between many OGRGeometry objects.
 *
 * Normally application code can fetch needed parameter values for this
 * SRS using GetAttrValue(), but in special cases the underlying parse tree
 * (or OGR_SRSNode objects) can be accessed more directly.
 *
 * At this time, no methods for reprojection, validation of SRS semantics
 * have been implemented.  This will follow at some point in the future. 
 */

class OGRSpatialReference
{
    int		nRefCount;

    OGR_SRSNode *poRoot;
    
  public:
                OGRSpatialReference(const char * = NULL);
                
    virtual    ~OGRSpatialReference();
    		
    int		Reference();
    int		Dereference();
    int		GetReferenceCount() { return nRefCount; }

    OGR_SRSNode *GetRoot() { return poRoot; }
    void        SetRoot( OGR_SRSNode * );
    
    OGR_SRSNode *GetAttrNode(const char *);
    const char  *GetAttrValue(const char *, int = 0);

    OGRErr	Validate();

    OGRSpatialReference *Clone();

    OGRErr      importFromWkt( char ** );
    OGRErr      exportToWkt( char ** );
    OGRErr      exportToProj4( char ** );

    OGRErr      SetNode( const char *, const char * );
    OGRErr      SetNode( const char *, double );

    OGRErr	SetLinearUnits( const char *pszName, double dfInMeters );
    double	GetLinearUnits( char ** = NULL );
    
    OGRErr	SetProjection( const char * );
    OGRErr	SetGeogCS( const char * pszGeogName,
                           const char * pszDatumName,
                           const char * pszEllipsoidName,
                           double dfSemiMajor, double dfInvFlattening,
                           const char * pszPMName = NULL,
                           double dfPMOffset = 0.0,
                           const char * pszUnits = NULL,
                           double dfConvertToRadians = 0.0 );
    double	GetSemiMajor( OGRErr * = NULL );
    double	GetSemiMinor( OGRErr * = NULL );
    double	GetInvFlattening( OGRErr * = NULL );
                           
    OGRErr	SetProjParm( const char *, double );
    double      GetProjParm( const char *, double = 0.0, OGRErr * = NULL );

    // Albers Conic Equal Area
    OGRErr      SetACEA( double dfStdP1, double dfStdP2,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

    // Azimuthal Equidistant
    OGRErr      SetAE( double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

    // Cylindrical Equal Area
    OGRErr      SetCEA( double dfStdP1, double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing );

    // Cassini-Soldner
    OGRErr      SetCS( double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

    // Equidistant Conic
    OGRErr      SetEC( double dfStdP1, double dfStdP2,
                       double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

    // Eckert IV
    OGRErr      SetEckertIV( double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

    // Eckert VI
    OGRErr      SetEckertVI( double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

    // Equirectangular
    OGRErr      SetEquirectangular(double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );

    // Gall Stereograpic
    OGRErr      SetGS( double dfCentralMeridian,
                       double dfFalseEasting, double dfFalseNorthing );
    
    // Gnomonic
    OGRErr      SetGnomonic(double dfCenterLat, double dfCenterLong,
                            double dfFalseEasting, double dfFalseNorthing );

    // Hotine Oblique Mercator
    OGRErr      SetHOM( double dfCenterLat, double dfCenterLong,
                        double dfAzimuth, double dfRectToSkew,
                        double dfScale,
                        double dfFalseEasting, double dfFalseNorthing );

    // Lambert Azimuthal Equal-Area
    OGRErr      SetLAEA( double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

    // Lambert Conformal Conic
    OGRErr      SetLCC( double dfStdP1, double dfStdP2,
                        double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );

    // Lambert Conformal Conic 1SP
    OGRErr      SetLCC1SP( double dfCenterLat, double dfCenterLong,
                           double dfScale,
                           double dfFalseEasting, double dfFalseNorthing );

    // Lambert Conformal Conic (Belgium)
    OGRErr      SetLCCB( double dfStdP1, double dfStdP2,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );
    
    // Miller Cylindrical
    OGRErr      SetMC( double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

    // Mercator
    OGRErr      SetMercator( double dfCenterLat, double dfCenterLong,
                             double dfScale, 
                             double dfFalseEasting, double dfFalseNorthing );

    // Mollweide
    OGRErr      SetMollweide( double dfCentralMeridian,
                              double dfFalseEasting, double dfFalseNorthing );

    // New Zealand Map Grid
    OGRErr      SetNZMG( double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

    // Oblique Stereographic
    OGRErr      SetOS( double dfOriginLat, double dfCMeridian,
                       double dfScale,
                       double dfFalseEasting,double dfFalseNorthing);
    
    // Orthographic
    OGRErr      SetOrthographic( double dfCenterLat, double dfCenterLong,
                                 double dfFalseEasting,double dfFalseNorthing);

    // Polyconic
    OGRErr      SetPolyconic( double dfCenterLat, double dfCenterLong,
                              double dfFalseEasting, double dfFalseNorthing );

    // Polar Stereographic
    OGRErr      SetPS( double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing);
    
    // Robinson
    OGRErr      SetRobinson( double dfCenterLong, 
                             double dfFalseEasting, double dfFalseNorthing );
    
    // Sinusoidal
    OGRErr      SetSinusoidal( double dfCenterLong, 
                               double dfFalseEasting, double dfFalseNorthing );
    
    // Stereographic
    OGRErr      SetStereographic( double dfCenterLat, double dfCenterLong,
                                  double dfScale,
                                 double dfFalseEasting,double dfFalseNorthing);
    
    // Transverse Mercator
    OGRErr      SetTM( double dfCenterLat, double dfCenterLong,
                       double dfScale,
                       double dfFalseEasting, double dfFalseNorthing );
    
    // Transverse Mercator (South Oriented)
    OGRErr      SetTMSO( double dfCenterLat, double dfCenterLong,
                         double dfScale,
                         double dfFalseEasting, double dfFalseNorthing );

    // VanDerGrinten
    OGRErr      SetVDG( double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );
};

/* ==================================================================== */
/*      Some "standard" strings.                                        */
/* ==================================================================== */

#define SRS_PT_ALBERS_CONIC_EQUAL_AREA					\
				"Albers_Conic_Equal_Area"
#define SRS_PT_AZIMUTHAL_EQUIDISTANT "Azimuthal_Equidistant"
#define SRS_PT_CASSINI_SOLDNER	"Cassini_Soldner"
#define SRS_PT_CYLINDRICAL_EQUAL_AREA "Cylindrical_Equal_Area"
#define SRS_PT_ECKERT_IV        "Eckert_IV"
#define SRS_PT_ECKERT_VI        "Eckert_VI"
#define SRS_PT_EQUIDISTANT_CONIC "Equidistant_Conic"
#define SRS_PT_EQUIRECTANGULAR  "Equirectangular"
#define SRS_PT_GALL_STEREOGRAPHIC "Gall_Stereographic"
#define SRS_PT_GNOMONIC		"Gnomonic"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR 					\
                                "Hotine_Oblique_Mercator"
#define SRS_PT_LABORDE_OBLIQUE_MERCATOR 				\
                                "Laborde_Oblique_Mercator"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP				\
                                "Lambert_Conformal_Conic_1SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP			        \
                                "Lambert_Conformal_Conic_2SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM		        \
                                "Lambert_Conformal_Conic_2SP_Belgium)"
#define SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA		        \
                                "Lambert_Azimuthal_Equal_Area"
#define SRS_PT_MERCATOR_1SP     "Mercator_1SP"
#define SRS_PT_MERCATOR_2SP     "Mercator_2SP"
#define SRS_PT_MILLER_CYLINDRICAL "Miller_Cylindrical"
#define SRS_PT_MOLLWEIDE        "Mollweide"
#define SRS_PT_NEW_ZEALAND_MAP_GRID					\
                                "New_Zealand_Map_Grid"
#define SRS_PT_OBLIQUE_STEREOGRAPHIC					\
                                "Oblique_Stereographic"
#define SRS_PT_ORTHOGRAPHIC	"Orthographic"
#define SRS_PT_POLAR_STEREOGRAPHIC					\
                                "Polar_Stereographic"
#define SRS_PT_POLYCONIC	"Polyconic"
#define SRS_PT_ROBINSON         "Robinson"
#define SRS_PT_SINUSOIDAL	"Sinusoidal"
#define SRS_PT_STEREOGRAPHIC	"Stereographic"
#define SRS_PT_SWISS_OBLIQUE_CYLINDRICAL 				\
                                "Swiss_Oblique_Cylindrical"
#define SRS_PT_TRANSVERSE_MERCATOR					\
                                "Transverse_Mercator"
#define SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED			\
                                "Transverse_Mercator_South_Orientated"
#define SRS_PT_TUNISIA_MINING_GRID					\
                                "Tunisia_Mining_Grid"
#define SRS_PT_VANDERGRINTEN	"VanDerGrinten"

                                

#define SRS_PP_CENTRAL_MERIDIAN		"central_meridian"
#define SRS_PP_SCALE_FACTOR     	"scale_factor"
#define SRS_PP_STANDARD_PARALLEL_1	"standard_parallel_1"
#define SRS_PP_STANDARD_PARALLEL_2	"standard_parallel_2"
#define SRS_PP_LONGITUDE_OF_CENTER      "longitude_of_center"
#define SRS_PP_LATITUDE_OF_CENTER       "latitude_of_center"
#define SRS_PP_LONGITUDE_OF_ORIGIN      "longitude_of_origin"
#define SRS_PP_LATITUDE_OF_ORIGIN       "latitude_of_origin"
#define SRS_PP_FALSE_EASTING		"false_easting"
#define SRS_PP_FALSE_NORTHING		"false_northing"
#define SRS_PP_AZIMUTH			"azimuth"
#define SRS_PP_LONGITUDE_OF_POINT_1     "longitude_of_point_1"
#define SRS_PP_LATITUDE_OF_POINT_1      "latitude_of_point_1"
#define SRS_PP_LONGITUDE_OF_POINT_2     "longitude_of_point_2"
#define SRS_PP_LATITUDE_OF_POINT_2      "latitude_of_point_2"
#define SRS_PP_LONGITUDE_OF_POINT_3     "longitude_of_point_3"
#define SRS_PP_LATITUDE_OF_POINT_3      "latitude_of_point_3"
#define SRS_PP_RECTIFIED_GRID_ANGLE	"rectified_grid_angle"
#define SRS_PP_LANDSAT_NUMBER		"landsat_number"
#define SRS_PP_PATH_NUMBER		"path_number"
#define SRS_PP_PERSPECTIVE_POINT_HEIGHT "perspective_point_height"
#define SRS_PP_FIPSZONE			"fipszone"
#define SRS_PP_ZONE			"zone"

#define SRS_UL_METER		"Meter"
#define SRS_UL_FOOT		"Foot (International)" /* or just "FOOT"? */
#define SRS_UL_FOOT_CONV                    "0.3048"
#define SRS_UL_US_FOOT          "U.S. Foot" /* or "US survey foot" */
#define SRS_UL_US_FOOT_CONV                 "0.3048006"
#define SRS_UL_NAUTICAL_MILE    "Nautical Mile"
#define SRS_UL_NAUTICAL_MILE_CONV           "1852.0"
#define SRS_UL_LINK		"Link"		/* Based on US Foot */
#define SRS_UL_LINK_CONV                    "0.20116684023368047"
#define SRS_UL_CHAIN		"Chain"		/* based on US Foot */
#define SRS_UL_CHAIN_CONV                   "2.0116684023368047"
#define SRS_UL_ROD		"Rod"		/* based on US Foot */
#define SRS_UL_ROD_CONV                     "5.02921005842012"

#define SRS_UA_DEGREE		"degree"
#define SRS_UA_DEGREE_CONV                  "0.0174532925199433"
#define SRS_UA_RADIAN		"radian"

#define SRS_PM_GREENWICH	"Greenwich"

#define SRS_DN_NAD27		"North American Datum 1927"
#define SRS_DN_NAD83		"North American Datum 1983"
#define SRS_DN_WGS84		"World Geodetic System 1984"

#define SRS_WGS84_SEMIMAJOR     6378137.0                                
#define SRS_WGS84_INVFLATTENING 298.257223563
                                

#endif /* ndef _OGR_SPATIALREF_H_INCLUDED */
