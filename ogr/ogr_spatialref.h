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
 * Revision 1.15  2000/03/20 22:39:49  warmerda
 * Added IsSame( method.
 *
 * Revision 1.14  2000/03/20 14:59:35  warmerda
 * added OGRCoordinateTransformation
 *
 * Revision 1.13  2000/03/16 19:04:01  warmerda
 * added SetTMG, moved constants to ogr_srs_api.h
 *
 * Revision 1.12  2000/01/26 21:22:18  warmerda
 * added tentative MakeValueSafe implementation
 *
 * Revision 1.11  2000/01/11 22:12:13  warmerda
 * added InsertChild
 *
 * Revision 1.10  2000/01/06 19:46:10  warmerda
 * added special logic for setting, and recognising UTM
 *
 * Revision 1.9  1999/11/18 19:02:20  warmerda
 * expanded tabs
 *
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

#include "ogr_srs_api.h"

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
    char        *pszValue;

    int         nChildren;
    OGR_SRSNode **papoChildNodes;

    void        ClearChildren();
    
  public:
                OGR_SRSNode(const char * = NULL);
                ~OGR_SRSNode();

    int         IsLeafNode() { return nChildren == 0; }
    
    int         GetChildCount() { return nChildren; }
    OGR_SRSNode *GetChild( int );

    OGR_SRSNode *GetNode( const char * );

    void        InsertChild( OGR_SRSNode *, int );
    void        AddChild( OGR_SRSNode * );
    int         FindChild( const char * );
    void        DestroyChild( int );

    const char  *GetValue() { return pszValue; }
    void        SetValue( const char * );

    void	MakeValueSafe();

    OGR_SRSNode *Clone();

    OGRErr      importFromWkt( char ** );
    OGRErr      exportToWkt( char ** );
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
    int         nRefCount;

    OGR_SRSNode *poRoot;
    
  public:
                OGRSpatialReference(const char * = NULL);
                
    virtual    ~OGRSpatialReference();
                
    int         Reference();
    int         Dereference();
    int         GetReferenceCount() { return nRefCount; }

    OGRSpatialReference *Clone();
    OGRSpatialReference *CloneGeogCS();

    OGRErr      importFromWkt( char ** );
    OGRErr      exportToWkt( char ** );
    OGRErr      exportToProj4( char ** );
    OGRErr      importFromEPSG( int );

    OGRErr      Validate();
    OGRErr      StripCTParms( OGR_SRSNode * = NULL );

    // Machinary for accessing parse nodes
    OGR_SRSNode *GetRoot() { return poRoot; }
    void        SetRoot( OGR_SRSNode * );
    
    OGR_SRSNode *GetAttrNode(const char *);
    const char  *GetAttrValue(const char *, int = 0);

    OGRErr      SetNode( const char *, const char * );
    OGRErr      SetNode( const char *, double );

    // Set/get geographic components
    OGRErr      SetLinearUnits( const char *pszName, double dfInMeters );
    double      GetLinearUnits( char ** = NULL );

    int         IsGeographic();
    int         IsProjected();
    int         IsSameGeogCS( OGRSpatialReference * );
    int         IsSame( OGRSpatialReference * );
    
    OGRErr      SetProjection( const char * );
    OGRErr      SetGeogCS( const char * pszGeogName,
                           const char * pszDatumName,
                           const char * pszEllipsoidName,
                           double dfSemiMajor, double dfInvFlattening,
                           const char * pszPMName = NULL,
                           double dfPMOffset = 0.0,
                           const char * pszUnits = NULL,
                           double dfConvertToRadians = 0.0 );
    double      GetSemiMajor( OGRErr * = NULL );
    double      GetSemiMinor( OGRErr * = NULL );
    double      GetInvFlattening( OGRErr * = NULL );

    OGRErr      SetAuthority( const char * pszTargetKey, 
                              const char * pszAuthority, 
                              int nCode );
                           
    OGRErr      SetProjParm( const char *, double );
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

    // Tunesia Mining Grid 
    OGRErr      SetTMG( double dfCenterLat, double dfCenterLong, 
                        double dfFalseEasting, double dfFalseNorthing );

    // Transverse Mercator (South Oriented)
    OGRErr      SetTMSO( double dfCenterLat, double dfCenterLong,
                         double dfScale,
                         double dfFalseEasting, double dfFalseNorthing );

    // VanDerGrinten
    OGRErr      SetVDG( double dfCenterLong,
                        double dfFalseEasting, double dfFalseNorthing );

    // Universal Transverse Mercator
    OGRErr      SetUTM( int nZone, int bNorth = TRUE );
    int		GetUTMZone( int *pbNorth = NULL );
};

/************************************************************************/
/*                     OGRCoordinateTransformation                      */
/*                                                                      */
/*      This is really just used as a base class for a private          */
/*      implementation.                                                 */
/************************************************************************/

class OGRCoordinateTransformation
{
public:
    virtual ~OGRCoordinateTransformation();

    // From CT_CoordinateTransformation

    virtual OGRSpatialReference *GetSourceCS() = 0;
    virtual OGRSpatialReference *GetTargetCS() = 0;

    // From CT_MathTransform

    virtual int Transform( int nCount, 
                           double *x, double *y, double *z = NULL ) = 0;

};

OGRCoordinateTransformation*
OGRCreateCoordinateTransformation( OGRSpatialReference *poSource, 
                                   OGRSpatialReference *poTarget );

#endif /* ndef _OGR_SPATIALREF_H_INCLUDED */
