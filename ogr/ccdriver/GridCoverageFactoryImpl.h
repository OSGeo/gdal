// GridCoverageFactoryImpl.h : Declaration of the CGridCoverageFactoryImpl

#ifndef __GRIDCOVERAGEFACTORYIMPL_H_
#define __GRIDCOVERAGEFACTORYIMPL_H_

#include "COGRRealGC.h"
#include "CoverageCategory.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CGridCoverageFactoryImpl
class ATL_NO_VTABLE CGridCoverageFactoryImpl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CGridCoverageFactoryImpl, &CLSID_GridCoverageFactoryImpl>,
	public IGridCoverageFactory
{
  public:
    CGridCoverageFactoryImpl()
	{
	}

    virtual ~CGridCoverageFactoryImpl()
	{
            printf( "~CGridCoverageFactoryImpl()\n" );
	}

    DECLARE_PROTECT_FINAL_CONSTRUCT()

//        DECLARE_REGISTRY( &CLSID_GridCoverageFactoryImpl,
//                          L"OGRCoverage.GridCoverageFactoryImpl.1",
//                          L"OGRCoverage.GridCoverageFactoryImpl",
//                          0, THREADFLAGS_APARTMENT );

        DECLARE_REGISTRY_RESOURCEID( IDR_GRIDCOVERAGEFACTORYIMPL );
    
        BEGIN_COM_MAP(CGridCoverageFactoryImpl)
	COM_INTERFACE_ENTRY(IGridCoverageFactory)
        END_COM_MAP()

        BEGIN_CATEGORY_MAP(CGridCoverageFactoryImpl)
        IMPLEMENTED_CATEGORY(CATID_OgcGridCoverageFactory)
        END_CATEGORY_MAP()

// IGridCoverageFactoryImpl
        public:
// IGridCoverageFactory
    STDMETHOD(CreateFromName)(BSTR Name, IGridCoverage * * val)
	{
            try
                {
		    if (val == NULL)
                        return E_POINTER;
			    
                    // Create a new object.
                    CComObject<COGRRealGC> *pObj;
                    HRESULT hr=CComObject<COGRRealGC>::CreateInstance(&pObj);
                    ATLASSERT(SUCCEEDED(hr));
                    if (FAILED(hr)) return hr;

                    if (!pObj->Open(Name)) return E_INVALIDARG;

                    return CComPtr<IGridCoverage>(pObj).CopyTo(val);
                }
            catch (...)
                {
                }
            return E_FAIL;
	}
};

#endif //__GRIDCOVERAGEFACTORYIMPL_H_
