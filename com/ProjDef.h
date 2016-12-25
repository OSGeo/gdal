// ProjDef.h : Declaration of the CProjDef

#ifndef __PROJDEF_H_
#define __PROJDEF_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CProjDef
class ATL_NO_VTABLE CProjDef : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CProjDef, &CLSID_ProjDef>,
	public IDispatchImpl<IProjDef, &IID_IProjDef, &LIBID_PROJ4Lib>
{
public:
	CProjDef()
	{
		psProj = NULL;
		sLastError = NULL;
		SetError("");
	}

DECLARE_REGISTRY_RESOURCEID(IDR_PROJDEF)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CProjDef)
	COM_INTERFACE_ENTRY(IProjDef)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// IProjDef
public:
	STDMETHOD(GetLastError)( BSTR *error );
	STDMETHOD(IsLatLong)(/*[out]*/ int *result);
	STDMETHOD(GetHandle)(long *pHandle);
	STDMETHOD(TransformPoint3D)(IUnknown *srcProj, double *x, double *y, double *z, int *success);
	STDMETHOD(Initialize)(BSTR proj_string, int *success);

private:
	void  SetError( const char *pszMessage );
	void  SetProjError( const char *pszMessage );

	BSTR  sLastError;
	void *psProj;

};

#endif //__PROJDEF_H_
