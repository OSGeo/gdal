// ColorTableImpl.h : Declaration of the CColorTableImpl

#ifndef __COLORTABLEIMPL_H_
#define __COLORTABLEIMPL_H_

#include "ComUtility.h"

/////////////////////////////////////////////////////////////////////////////
// CColorTableImpl
class ATL_NO_VTABLE CColorTableImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CColorTableImpl, &CLSID_ColorTableImpl>,
	public IColorTable
{
public:
    typedef LocalPtr<CColorTableImpl,IColorTable> Ptr;

    std::vector<ColorEntry> m_arCol;
    ColorEntryInterpretation m_interpretation;

	CColorTableImpl()
	{
	}

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CColorTableImpl)
	COM_INTERFACE_ENTRY(IColorTable)
END_COM_MAP()

// IColorTableImpl
public:
// IColorTable
	STDMETHOD(get_NumColor)(LONG * val)
	{
		if (val == NULL)
			return E_POINTER;
			
		*val=m_arCol.size();
        return S_OK;
	}
	STDMETHOD(get_Interpretation)(ColorEntryInterpretation * Interpretation)
	{
		if (Interpretation == NULL)
			return E_POINTER;
			
		*Interpretation=m_interpretation;
        return S_OK;
	}
	STDMETHOD(Color)(LONG index, ColorEntry * Color)
	{
		if (Color == NULL)
			return E_POINTER;

        if (index<0 || index>=m_arCol.size())
            return E_INVALIDARG;
        *Color=m_arCol[index];			
		return S_OK;
	}
	STDMETHOD(ColorAsRGB)(LONG index, ColorEntry * Color)
	{
		if (Color == NULL)
			return E_POINTER;

        if (index<0 || index>=m_arCol.size())
            return E_INVALIDARG;

        switch (m_interpretation)
        {
            case Gray:
            {
                // TODO
                return E_NOTIMPL;
            }
            case RGB:
            {
                *Color=m_arCol[index];
                return S_OK;
            }
            case CMYK:
            {
                // TODO
                return E_NOTIMPL;
            }
            case HLS:
            {
                // TODO
                return E_NOTIMPL;
            }
        }
		return E_INVALIDARG;
	}
};

#endif //__COLORTABLEIMPL_H_
