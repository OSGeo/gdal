#include "stdafx.h"
#include "Geometry.h"

// All routines relying on CadcorpSF.dll are in this file.
#import "CadcorpSF.dll" rename_namespace("SF")

SF::ICadcorpGeometryFactoryPtr GetGeometryFactory()
{
    static SF::ICadcorpGeometryFactoryPtr pGeometryFactory;
    if (!pGeometryFactory.GetInterfacePtr())
    {
        pGeometryFactory=SF::ICadcorpGeometryFactoryPtr(__uuidof(SF::GeometryFactory));
    }
    return pGeometryFactory;
}

SF::ICadcorpSpatialReferenceAuthorityFactoryPtr GetSpatialReferenceAuthorityFactory()
{
    static SF::ICadcorpSpatialReferenceAuthorityFactoryPtr pSpatialReferenceAuthorityFactory;
    if (!pSpatialReferenceAuthorityFactory.GetInterfacePtr())
    {
        pSpatialReferenceAuthorityFactory=SF::ICadcorpSpatialReferenceAuthorityFactoryPtr(__uuidof(SF::SpatialReferenceAuthorityFactory));
    }
    return pSpatialReferenceAuthorityFactory;
}

IPointPtr ComposePoint(ISpatialReference* pSR,Pos2D pt)
//_O Combine WKSPoint and ISpatialReference into an IPoint.
{
    // Warning: May throw exception.
    SF::ICadcorpGeometryFactoryPtr pGF=GetGeometryFactory();
    CComQIPtr<SF::ISpatialReference> pSR2(pSR);
    SF::IPointPtr pPoint=pGF->CreatePoint(pt.x,pt.y,pSR2);
    return CComQIPtr<IPoint>(pPoint);
}

IGeometryPtr ComposePolygon(ISpatialReference* pSR,int np,Pos2D arPt[])
{
    // Warning: May throw exception.
    SF::ICadcorpGeometryFactoryPtr pGF=GetGeometryFactory();
    CComQIPtr<SF::ISpatialReference> pSR2(pSR);
    SF::IGeometryPtr pPoly=pGF->CreatePolygonFromWKSPointArray(np,(SF::WKSPoint*)arPt,pSR2);
    return CComQIPtr<IGeometry>(pPoly);
}

Pos2D DecomposePoint(IPoint* point,ISpatialReference* sr)
//_O Get WKSPoint out of an IPoint, doing a projection if required.
{
    // Warning: May throw exception.
    SF::IPointPtr pPoint=point;
    SF::ISpatialReferencePtr pSR=sr;
    if (pSR!=pPoint->GetSpatialReference())
    {
        pPoint=pPoint->Project(pSR);
    }
    Pos2D pt;
    pPoint->Coords(&pt.x,&pt.y);
    return pt;
}

ISpatialReferencePtr CreateEpsgSRS(long code)
{
    // Warning: May throw exception.
    SF::ISpatialReferenceAuthorityFactoryPtr pSRF=GetSpatialReferenceAuthorityFactory();

    try
    {
        SF::ISpatialReferencePtr pSR=pSRF->CreateProjectedCoordinateSystem(code);
        if (pSR) return CComQIPtr<ISpatialReference>(pSR);
    }
    catch (...)
    {
    }

    try
    {
        SF::ISpatialReferencePtr pSR=pSRF->CreateGeographicCoordinateSystem(code);
        if (pSR) return CComQIPtr<ISpatialReference>(pSR);
    }
    catch (...)
    {
    }

    return 0;
}
