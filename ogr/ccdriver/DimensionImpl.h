// DimensionImpl.h : Declaration of the CDimensionImpl

#ifndef __DIMENSIONIMPL_H_
#define __DIMENSIONIMPL_H_

#include "ComUtility.h"
#include "ColorTableImpl.h"

/////////////////////////////////////////////////////////////////////////////
// CDimensionImpl
class ATL_NO_VTABLE CDimensionImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CDimensionImpl, &CLSID_DimensionImpl>,
	public IDimension
{
public:
    typedef LocalPtr<CDimensionImpl,IDimension> Ptr;

    StringArray m_arCat;
    ColorInterpretation m_interpretation;
    CComBSTR m_desc;
    DimensionType m_dt;
    CComVariant m_min;
    CComVariant m_max;
    CComVariant m_nodata;
    CColorTableImpl::Ptr m_pColorTable;

	CDimensionImpl()
	{
	}

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CDimensionImpl)
	COM_INTERFACE_ENTRY(IDimension)
END_COM_MAP()

// IDimensionImpl
public:
// IDimension
	STDMETHOD(get_Categories)(SAFEARRAY * * val)
	{
		if (val == NULL)
			return E_POINTER;
			
        return CreateSafeArray(val,m_arCat);
	}
	STDMETHOD(get_ColorInterpretation)(ColorInterpretation * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_interpretation;
        return S_OK;
	}
	STDMETHOD(get_ColorTable)(IColorTable * * val)
	{
		if (val == NULL)
			return E_POINTER;
			
        return m_pColorTable.CopyTo(val);
	}
	STDMETHOD(get_Description)(BSTR * val)
	{
		return m_desc.CopyTo(val);
	}
	STDMETHOD(get_DimensionType)(DimensionType * val)
	{
		if (val == NULL)
			return E_POINTER;

		*val=m_dt;
        return S_OK;
	}
	STDMETHOD(get_MinimumValue)(VARIANT * val)
	{
		return ::VariantCopy(val,&m_min);
	}
	STDMETHOD(get_MaximumValue)(VARIANT * val)
	{
		return VariantCopy(val,&m_max);
	}
	STDMETHOD(get_NodataValue)(VARIANT * val)
	{
		return VariantCopy(val,&m_nodata);
	}
};

#endif //__DIMENSIONIMPL_H_
