// AffineGeoReferenceImpl.h : Declaration of the CAffineGeoReferenceImpl

#ifndef __AFFINEGEOREFERENCEIMPL_H_
#define __AFFINEGEOREFERENCEIMPL_H_

#include "Geometry.h"

/////////////////////////////////////////////////////////////////////////////
// CAffineGeoReferenceImpl
class ATL_NO_VTABLE CAffineGeoReferenceImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CAffineGeoReferenceImpl, &CLSID_AffineGeoReferenceImpl>,
	public IAffineGeoReference
{
private:
    Vec2D m_x;
    Vec2D m_y;
    Pos2D m_o;
    Vec2D m_ix;
    Vec2D m_iy;
    Pos2D m_io;
public:
    CComPtr<ISpatialReference> m_SR;

	CAffineGeoReferenceImpl()
	{
        SetTransform(Pos2D(),Vec2D(1,0),Vec2D(0,1));
	}
    static CComPtr<IAffineGeoReference> Construct(Pos2D o,Vec2D x,Vec2D y,ISpatialReferencePtr pSR);
    BOOL SetTransform(Pos2D org,Vec2D x,Vec2D y);

DECLARE_PROTECT_FINAL_CONSTRUCT()

DECLARE_NO_REGISTRY()
    
BEGIN_COM_MAP(CAffineGeoReferenceImpl)
	COM_INTERFACE_ENTRY(IAffineGeoReference)
	COM_INTERFACE_ENTRY(IGeoReference)
END_COM_MAP()

// IAffineGeoReferenceImpl
public:
// IAffineGeoReference
	STDMETHOD(get_Vertical)(WKSVector * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_y;
        return S_OK;
	}
	STDMETHOD(get_Horizontal)(WKSVector * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_x;
        return S_OK;
	}
	STDMETHOD(get_Origin)(WKSPoint * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_o;
        return S_OK;
	}
// IGeoReference
	STDMETHOD(get_SpatialReference)(ISpatialReference * * spatialRef)
	{
        return m_SR.CopyTo(spatialRef);			
	}
    STDMETHOD(Compatible)(IGeoReference* GeoReference,VARIANT_BOOL* val)
    {
		if (val == NULL)
			return E_POINTER;
			
		return E_NOTIMPL;
    }
	STDMETHOD(ExportToWKB)(VARIANT * wkb)
	{
		if (wkb == NULL)
			return E_POINTER;
			
		return E_NOTIMPL;
	}
	STDMETHOD(GridCoordinateToSRS)(WKSPoint * posGrid, WKSPoint * posSRS)
	{
		if (posSRS == NULL)
			return E_POINTER;

        *posSRS=m_o+posGrid->x*m_x+posGrid->y*m_y;
		return S_OK;
	}
	STDMETHOD(SRSToGridCoordinate)(WKSPoint * posSRS, WKSPoint * posGrid)
	{
		if (posGrid == NULL)
			return E_POINTER;
			
        *posGrid=m_io+posSRS->x*m_ix+posSRS->y*m_iy;
		return S_OK;
	}
};

#endif //__AFFINEGEOREFERENCEIMPL_H_
