#include "stdafx.h"
#include "ComUtility.h"

HRESULT CreateSafeArray(SAFEARRAY * * val,const StringArray& array)
{
	ATLASSERT(val != NULL);
    int n=array.size();
    SAFEARRAYBOUND bounds[1];
    bounds[0].cElements=n;
    bounds[0].lLbound=0;
    *val=SafeArrayCreate(VT_BSTR,1,bounds);
    if (!*val) return E_FAIL;
    for (int i=0;i<n;i++)
    {
        long index[1];
        index[0]=i;
        HRESULT r=SafeArrayPutElement(*val,index,(BSTR)array[i]);
        ATLASSERT(r==S_OK);
    }
	return S_OK;
}

HRESULT CreateSafeArray(SAFEARRAY * * val,const BoolArray& array)
{
	ATLASSERT(val != NULL);
    int n=array.size();
    SAFEARRAYBOUND bounds[1];
    bounds[0].cElements=n;
    bounds[0].lLbound=0;
    *val=SafeArrayCreate(VT_BOOL,1,bounds);
    if (!*val) return E_FAIL;
    for (int i=0;i<n;i++)
    {
        long index[1];
        index[0]=i;
        VARIANT_BOOL b=array[i];
        HRESULT r=SafeArrayPutElement(*val,index,&b);
        ATLASSERT(r==S_OK);
    }
	return S_OK;
}

HRESULT CreateSafeArray(SAFEARRAY * * val,const VariantArray& array)
{
	ATLASSERT(val != NULL);
    int n=array.size();
    SAFEARRAYBOUND bounds[1];
    bounds[0].cElements=n;
    bounds[0].lLbound=0;
    *val=SafeArrayCreate(VT_VARIANT,1,bounds);
    if (!*val) return E_FAIL;
    for (int i=0;i<n;i++)
    {
        long index[1];
        index[0]=i;
        VARIANT v=array[i];
        if (FAILED(SafeArrayPutElement(*val,index,&v))) return E_FAIL;
    }
    return S_OK;
}

HRESULT CreateSafeArray(SAFEARRAY * * val,const Array3D<bool>& array)
{
	ATLASSERT(val != NULL);
    int n1=array.size1();
    int n2=array.size2();
    int n3=array.size3();
    SAFEARRAYBOUND bounds[3];
    bounds[0].cElements=n1;
    bounds[0].lLbound=0;
    bounds[1].cElements=n2;
    bounds[1].lLbound=0;
    bounds[2].cElements=n3;
    bounds[2].lLbound=0;
    *val=SafeArrayCreate(VT_BOOL,3,bounds);
    if (!*val) return E_FAIL;
    ATLASSERT(SafeArrayGetElemsize(*val)==2);
    ATLASSERT(sizeof(bool)==1);
    void* pData;
    HRESULT r=SafeArrayAccessData(*val,&pData);
    if (FAILED(r)) return r;
    const bool* pSrc=&array.at(0,0,0);
    VARIANT_BOOL* pDst=(VARIANT_BOOL*)pData;
    ATLASSERT(SafeArrayGetElemsize(*val)==sizeof(VARIANT_BOOL));
    for (int i=0;i<n1*n2*n3;i++)
    {
        *(pDst++)=*(pSrc++);
    }
    r=SafeArrayUnaccessData(*val);
    ATLASSERT(r==S_OK);
	return r;
}

bool ReadSafeArray(DoubleArray2D& array,SAFEARRAY * sa)
{
    if (!sa) return false;
    if (SafeArrayGetDim(sa)!=2) return false;
    UINT size=SafeArrayGetElemsize(sa);
    if (size!=sizeof(double)) return false;
    long lo1,hi1;
    long lo2,hi2;
    if (!SUCCEEDED(SafeArrayGetLBound(sa,1,&lo1))) return false;
    if (!SUCCEEDED(SafeArrayGetUBound(sa,1,&hi1))) return false;
    if (!SUCCEEDED(SafeArrayGetLBound(sa,2,&lo2))) return false;
    if (!SUCCEEDED(SafeArrayGetUBound(sa,2,&hi2))) return false;
    int n1=1+hi1-lo1;
    int n2=1+hi2-lo2;
    void* pData;
    if (!SUCCEEDED(SafeArrayAccessData(sa,&pData))) return false;
    array.resize(n1,n2);
    memcpy(&array.at(0,0),pData,n1*n2*sizeof(double));
    SafeArrayUnaccessData(sa);
    return true;
}
