// ProjDef.cpp : Implementation of CProjDef
#include "stdafx.h"
#include "COMTest1.h"
#include "ProjDef.h"
#include "proj_api.h"

/////////////////////////////////////////////////////////////////////////////
// CProjDef


STDMETHODIMP CProjDef::Initialize(BSTR proj_string, int *success)
{
	USES_CONVERSION;
	psProj = pj_init_plus( W2A(proj_string) );

	if( psProj != NULL )
		*success = 1;
	else
	{
		SetProjError( "pj_init_plus failed." );
		//SetError( (const char *) W2A(proj_string) );
		*success = 0;
	}

	return S_OK;
}

STDMETHODIMP CProjDef::TransformPoint3D(IUnknown *srcProj, double *x, double *y, double *z, int *success)
{
	void *psProjOther;
	IProjDef *srcProjReal;

	srcProj->QueryInterface( IID_IProjDef, (void **) &srcProjReal );
	srcProjReal->GetHandle( (long *) &psProjOther );

	if( psProjOther == NULL || psProj == NULL )
	{
		SetError( "One of projections not set." );
		*success = 0;
		return E_FAIL;
	}
	
	if( pj_is_latlong( psProjOther ) )
	{
		*x *= DEG_TO_RAD;
		*y *= DEG_TO_RAD;
	}

	*success = pj_transform( psProjOther, psProj, 1, 0, x, y, z ) == 0;
	if( ! *success )
		SetProjError( "pj_transform failed." );

	else if( pj_is_latlong( psProj ) )
	{
		*x *= RAD_TO_DEG;
		*y *= RAD_TO_DEG;
	}

	return S_OK;
}

STDMETHODIMP CProjDef::GetHandle(long *pHandle)
{
	*pHandle = (long) psProj;

	return S_OK;
}

STDMETHODIMP CProjDef::IsLatLong(int *result)
{
	if( psProj != NULL )
	{
		*result = pj_is_latlong( (projPJ) psProj );
		return S_OK;
	}
	else
	{
		SetError( "Projection is null" );
		return E_FAIL;
	}
}

STDMETHODIMP CProjDef::GetLastError( BSTR *error )
{
	*error = SysAllocString( sLastError );

	return S_OK;
}

void  CProjDef::SetError( const char *pszMessage )

{
	USES_CONVERSION;

	if( sLastError != NULL )
	{
		SysFreeString( sLastError );
		sLastError = NULL;
	}

	sLastError = SysAllocString( A2BSTR( pszMessage ) );		
}

void  CProjDef::SetProjError( const char *pszMessage )

{
	int *pj_errno = pj_get_errno_ref();

	if( *pj_errno > 0 )
		SetError( strerror( *pj_errno ) );
	else if( *pj_errno < 0 )
		SetError( pj_strerrno( *pj_errno ) );
	else
		SetError( pszMessage );
}
