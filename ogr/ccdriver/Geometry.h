// Routines for Geometry and Spatial Reference systems.
#pragma once
#include "Cover.h"

struct Pos2D : public WKSPoint
{
    Pos2D(double ax=0,double ay=0) {x=ax;y=ay;}
    Pos2D(WKSPoint pt) {x=pt.x;y=pt.y;}
};

struct Vec2D : public WKSVector
{
    Vec2D(double ax=0,double ay=0) {x=ax;y=ay;}
    Vec2D(WKSVector pt) {x=pt.x;y=pt.y;}
};

inline Pos2D operator+(const Pos2D &p,const Vec2D &v) {return Pos2D(p.x+v.x,p.y+v.y);}
inline Vec2D operator*(double f,const Vec2D &v) {return Vec2D(f*v.x,f*v.y);}

typedef CComPtr<IGeometry> IGeometryPtr;
typedef CComPtr<IPoint> IPointPtr;
typedef CComPtr<ISpatialReference> ISpatialReferencePtr;

IPointPtr ComposePoint(ISpatialReference* pSR,Pos2D pt);
IGeometryPtr ComposePolygon(ISpatialReference* pSR,int np,Pos2D arPt[]);
Pos2D DecomposePoint(IPoint* point,ISpatialReference* sr);
ISpatialReferencePtr CreateEpsgSRS(long code);
ISpatialReferencePtr CreateWKT_SRS( const char * );

