// Utility functions for COM and STL
#pragma once

#include <comdef.h>
#include <vector>

template<class CoCls,class Ifc>
class LocalPtr      // COM object with back-door access to C++ object.
{
private:
    CComObject<CoCls>* m_pObj;
    CComPtr<Ifc> m_pIfc;
public:
                    LocalPtr(void* pZero=0) {
                        ATLASSERT(!pZero);
                        m_pObj=0;
                    }
    HRESULT         CopyTo(Ifc** val) {
                        return m_pIfc.CopyTo(val);
                    }
    BOOL            Create() {
                        m_pIfc=0;
                        m_pObj=0;
                        if (FAILED(CComObject<CoCls>::CreateInstance(&m_pObj))) return FALSE;
                        m_pIfc=m_pObj;
                        return TRUE;
                    }
    CComPtr<Ifc>    GetInterface() const {
                        return m_pIfc;
                    }
    CoCls*          operator->() {
                        return m_pObj;
                    }
};

template<class T>
class Array2D       // Two dimensional array.
{
private:
    std::vector<T>  m_arPack;
    int             m_n1;
    int             m_n2;
public:
                    Array2D() {
                        m_n1=0;
                        m_n2=0;
                    }
    void            resize(int n1,int n2) {
                        m_arPack.resize(n1*n2);
                        m_n1=n1;
                        m_n2=n2;
                    }
    const T&        at(int i1,int i2) const {
                        return m_arPack[i1+i2*m_n1];  // Using SAFEARRAY ordering.
                    }
    T&              at(int i1,int i2) {
                        return m_arPack[i1+i2*m_n1];  // Using SAFEARRAY ordering.
                    }
    int             size() const {
                        return m_n1*m_n2;
                    }
    int             size1() const {
                        return m_n1;
                    }
    int             size2() const {
                        return m_n2;
                    }
};

template<class T>
class Array3D   // Three dimensional array.
{
private:
    std::vector<T>  m_arPack;
    int             m_n1;
    int             m_n2;
    int             m_n3;
public:
                    Array3D() {
                        m_n1=0;
                        m_n2=0;
                        m_n3=0;
                    }
    void            resize(int n1,int n2,int n3) {
                        m_arPack.resize(n1*n2*n3);
                        m_n1=n1;
                        m_n2=n2;
                        m_n3=n3;
                    }
    const T&        at(int i1,int i2,int i3) const {
                        return m_arPack[i1+i2*m_n1+i3*m_n1*m_n2];  // Using SAFEARRAY ordering.
                    }
    T&              at(int i1,int i2,int i3) {
                        return m_arPack[i1+i2*m_n1+i3*m_n1*m_n2];  // Using SAFEARRAY ordering.
                    }
    int             size() const {
                        return m_n1*m_n2*m_n3;
                    }
    int             size1() const {
                        return m_n1;
                    }
    int             size2() const {
                        return m_n2;
                    }
    int             size3() const {
                        return m_n3;
                    }
};

typedef std::vector<_bstr_t> StringArray;
typedef std::vector<bool> BoolArray;
typedef std::vector<BYTE> ByteArray;
typedef std::vector<short> ShortArray;
typedef std::vector<long> LongArray;
typedef std::vector<double> DoubleArray;
typedef std::vector<_variant_t> VariantArray;
typedef Array2D<double> DoubleArray2D;
typedef Array3D<BYTE> ByteArray3D;

template<class Elt>
HRESULT CreateSafeArray(SAFEARRAY * * val,const std::vector<Elt>& array,WORD vt)
{
	ATLASSERT(val != NULL);
    int n=array.size();
    SAFEARRAYBOUND bounds[1];
    bounds[0].cElements=n;
    bounds[0].lLbound=0;
    *val=SafeArrayCreate(vt,1,bounds);
    if (!*val) return E_FAIL;
    ATLASSERT(SafeArrayGetElemsize(*val)==sizeof(Elt));
    void* pData;
    HRESULT r=SafeArrayAccessData(*val,&pData);
    if (FAILED(r)) return r;
    memcpy(pData,&array[0],n*sizeof(Elt));
    r=SafeArrayUnaccessData(*val);
    ATLASSERT(r==S_OK);
	return r;
}

template<class Elt>
HRESULT CreateSafeArray(SAFEARRAY * * val,const Array3D<Elt>& array,WORD vt)
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
    *val=SafeArrayCreate(vt,3,bounds);
    if (!*val) return E_FAIL;
    ATLASSERT(SafeArrayGetElemsize(*val)==sizeof(Elt));
    void* pData;
    HRESULT r=SafeArrayAccessData(*val,&pData);
    if (FAILED(r)) return r;
    memcpy(pData,&array.at(0,0,0),n1*n2*n3*sizeof(Elt));
    r=SafeArrayUnaccessData(*val);
    ATLASSERT(r==S_OK);
	return r;
}

template<class CoCls,class Ifc>
HRESULT CreateSafeInterfaceArray(SAFEARRAY * * val,const std::vector<LocalPtr<CoCls,Ifc> >& arPtr)
{
	ATLASSERT(val != NULL);
    int n=arPtr.size();
    SAFEARRAYBOUND bounds[1];
    bounds[0].cElements=n;
    bounds[0].lLbound=0;
    *val=SafeArrayCreate(VT_UNKNOWN,1,bounds);
    if (!*val) return E_FAIL;
    for (int i=0;i<n;i++)
    {
        long index[1];
        index[0]=i;
        HRESULT r=SafeArrayPutElement(*val,index,(Ifc*)arPtr[i].GetInterface());
        ATLASSERT(r==S_OK);
    }
	return S_OK;
}

template<class T>
bool ReadSafeArray(std::vector<T>& array,SAFEARRAY * sa)
{
    if (!sa) return false;
    if (SafeArrayGetDim(sa)!=1) return false;
    UINT size=SafeArrayGetElemsize(sa);
    if (size!=sizeof(T)) return false;
    long lo,hi;
    if (!SUCCEEDED(SafeArrayGetLBound(sa,1,&lo))) return false;
    if (!SUCCEEDED(SafeArrayGetUBound(sa,1,&hi))) return false;
    void* pData;
    if (!SUCCEEDED(SafeArrayAccessData(sa,&pData))) return false;
    array.resize(1+hi-lo);
    memcpy(&array[0],pData,(1+hi-lo)*sizeof(T));
    SafeArrayUnaccessData(sa);
    return true;
}

template<class T>
bool ReadSafeArray(Array2D<T>& array,SAFEARRAY * sa,WORD vt)
{
	ATLASSERT(val != NULL);
    int n1=array.size1();
    int n2=array.size2();
    SAFEARRAYBOUND bounds[2];
    bounds[0].cElements=n1;
    bounds[0].lLbound=0;
    bounds[1].cElements=n2;
    bounds[1].lLbound=0;
    *val=SafeArrayCreate(vt,2,bounds);
    if (!*val) return E_FAIL;
    ATLASSERT(SafeArrayGetElemsize(*val)==sizeof(T));
    void* pData;
    HRESULT r=SafeArrayAccessData(*val,&pData);
    if (FAILED(r)) return r;
    memcpy(pData,&array.at(0,0),n1*n2*sizeof(T));
    r=SafeArrayUnaccessData(*val);
    ATLASSERT(r==S_OK);
	return r;
}

// Specialize template functions which cannot use "memcpy" for element access.
HRESULT CreateSafeArray(SAFEARRAY * * val,const StringArray& array);
HRESULT CreateSafeArray(SAFEARRAY * * val,const BoolArray& array);
HRESULT CreateSafeArray(SAFEARRAY * * val,const VariantArray& array);
HRESULT CreateSafeArray(SAFEARRAY * * val,const Array3D<bool>& array);
