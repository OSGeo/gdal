// GridInfoImpl.h : Declaration of the CGridInfoImpl

#ifndef __GRIDINFOIMPL_H_
#define __GRIDINFOIMPL_H_

#include "ComUtility.h"

/////////////////////////////////////////////////////////////////////////////
// CGridInfoImpl
class ATL_NO_VTABLE CGridInfoImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CGridInfoImpl, &CLSID_GridInfoImpl>,
	public IGridInfo
{
public:
    typedef LocalPtr<CGridInfoImpl,IGridInfo> Ptr;

    int m_OptimalRowSize;
    int m_OptimalColSize;
    ByteOrdering m_ByteOrdering;
    PixelOrdering m_PixelOrdering;
    ValueSequence m_ValueSequence;
    ValueInBytePacking m_ValueInBytePacking;

	CGridInfoImpl()
	{
        m_OptimalRowSize=0;
        m_OptimalColSize=0;
        m_ByteOrdering=wkbNDR;
        m_PixelOrdering=PixelInterleaved;
        m_ValueSequence=RowSequenceMinToMax;
        m_ValueInBytePacking=HiBitFirst;
	}

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CGridInfoImpl)
	COM_INTERFACE_ENTRY(IGridInfo)
END_COM_MAP()

// IGridInfoImpl
public:
// IGridInfo
	STDMETHOD(get_ByteOrdering)(ByteOrdering * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_ByteOrdering;
        return S_OK;
	}
	STDMETHOD(get_HasArbitraryOverview)(VARIANT_BOOL * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=FALSE;
        return S_OK;
	}
    STDMETHOD(get_OptimalAdapter)(IGridCoverageAdapter** val)
    {
		if (val == NULL)
			return E_POINTER;
			
		*val=0;
        return S_OK;
    }
	STDMETHOD(get_OptimalRowSize)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_OptimalRowSize;
        return S_OK;
	}
	STDMETHOD(get_OptimalColumnSize)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_OptimalColSize;
        return S_OK;
	}
	STDMETHOD(get_PixelOrdering)(PixelOrdering * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_PixelOrdering;
        return S_OK;
	}
	STDMETHOD(get_ValueInBytePacking)(ValueInBytePacking * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_ValueInBytePacking;
        return S_OK;
	}
	STDMETHOD(get_ValueSequence)(ValueSequence * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_ValueSequence;
        return S_OK;
	}
	STDMETHOD(get_NumOverview)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=0;
        return S_OK;
	}
	STDMETHOD(get_OverviewGridGeometry)(LONG nOverview, IGridGeometry * * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		return E_INVALIDARG;
	}
};

#endif //__GRIDINFOIMPL_H_
