// GridGeometryImpl.h : Declaration of the CGridGeometryImpl

#ifndef __GRIDGEOMETRYIMPL_H_
#define __GRIDGEOMETRYIMPL_H_

#include "ComUtility.h"
#include "Geometry.h"

/////////////////////////////////////////////////////////////////////////////
// CGridGeometryImpl

class ATL_NO_VTABLE CGridGeometryImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CGridGeometryImpl, &CLSID_GridGeometryImpl>,
	public IGridGeometry
{
public:
    typedef LocalPtr<CGridGeometryImpl,IGridGeometry> Ptr;

    int m_MaxCol;
    int m_MaxRow;
    int m_MinCol;
    int m_MinRow;
    CComPtr<IGeoReference> m_gr;

	CGridGeometryImpl()
	{
	}
    static Ptr Construct(int MaxCol,int MaxRow,int MinCol,int MinRow,IGeoReference* gr);

DECLARE_PROTECT_FINAL_CONSTRUCT()

DECLARE_NO_REGISTRY()

BEGIN_COM_MAP(CGridGeometryImpl)
	COM_INTERFACE_ENTRY(IGridGeometry)
END_COM_MAP()

// IGridGeometryImpl
public:
// IGridGeometry
	STDMETHOD(get_MaxColumn)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_MaxCol;
        return S_OK;
	}
	STDMETHOD(get_MaxRow)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_MaxRow;
        return S_OK;
	}
	STDMETHOD(get_MinColumn)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_MinCol;
        return S_OK;
	}
	STDMETHOD(get_MinRow)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_MinRow;
        return S_OK;
	}
	STDMETHOD(get_GeoReference)(IGeoReference * * val)
	{
		return CComPtr<IGeoReference>(m_gr).CopyTo(val);
	}
	STDMETHOD(PointToGrid)(IPoint * pt, WKSPoint * posGrid)
	{
        try
        {
		    if (posGrid == NULL)
			    return E_POINTER;
			    
            ISpatialReferencePtr pSR;
            if (FAILED(m_gr->get_SpatialReference(&pSR))) return E_FAIL;
            WKSPoint posSRS=DecomposePoint(pt,pSR);
            m_gr->SRSToGridCoordinate(&posSRS,posGrid);
		    return S_OK;
        }
        catch (...)
        {
            return E_FAIL;
        }
	}
	STDMETHOD(GridToPoint)(WKSPoint * posGrid, IPoint * * point)
	{
        try
        {
		    if (point == NULL)
			    return E_POINTER;

            ISpatialReferencePtr pSR;
            if (FAILED(m_gr->get_SpatialReference(&pSR))) return E_FAIL;
            WKSPoint posSRS;
            m_gr->GridCoordinateToSRS(posGrid,&posSRS);
            IPointPtr pPoint=ComposePoint(pSR,posSRS);
		    return pPoint.CopyTo(point);
        }
        catch (...)
        {
            return E_FAIL;
        }
	}
};

#endif //__GRIDGEOMETRYIMPL_H_
