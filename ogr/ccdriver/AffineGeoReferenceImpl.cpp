// AffineGeoReferenceImpl.cpp : Implementation of CAffineGeoReferenceImpl
#include "stdafx.h"
#include "OGRComGrid.h"
#include "AffineGeoReferenceImpl.h"

/////////////////////////////////////////////////////////////////////////////
// CAffineGeoReferenceImpl

CComPtr<IAffineGeoReference> CAffineGeoReferenceImpl::Construct(Pos2D o,Vec2D x,Vec2D y,ISpatialReferencePtr pSR)
{
    CComObject<CAffineGeoReferenceImpl> *pObj;
    CComObject<CAffineGeoReferenceImpl>::CreateInstance(&pObj);
    if (!pObj->SetTransform(o,x,y)) return 0;
    pObj->m_SR=pSR;
    return CComPtr<IAffineGeoReference>(pObj);
}

BOOL CAffineGeoReferenceImpl::SetTransform(Pos2D org,Vec2D x,Vec2D y)
{
    m_o=org;
    m_x=x;
    m_y=y;

    // Get determinant.
    double det;
    det=x.x*y.y-x.y*y.x;
    if (det==0) return FALSE;

    // Get inverse.
    m_ix=Vec2D(y.y/det,-x.y/det);
    m_iy=Vec2D(-y.x/det,x.x/det);
    m_io=Pos2D(-m_ix.x*m_o.x-m_ix.y*m_o.y,-m_iy.x*m_o.x-m_iy.y*m_o.y);

    return TRUE;
}
