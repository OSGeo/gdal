/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS OGR Grid Coverage Implementation
 * Purpose:  Definition of the COGRRealGC class.  This is an IGridCoverage
 *           object representing a real underlying GDAL data store. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/07/25 02:07:31  warmerda
 * New
 *
 */

#ifndef _COGRREALGC_H_INCLUDED
#define _COGRREALGC_H_INCLUDED

#ifndef _OGRCOMGRID_H_INCLUDED
#  include "ogrcomgrid.h"
#endif

#include "GridGeometryImpl.h"
#include "GridInfoImpl.h"
#include "DimensionImpl.h"

/************************************************************************/
/*                              COGRRealGC                              */
/************************************************************************/

class ATL_NO_VTABLE COGRRealGC : 
	public CComObjectRootEx<CComSingleThreadModel>,
        public CComCoClass<COGRRealGC, &CLSID_OGRRealGC>,
	public IGridCoverage
{
    GDALDatasetH                        m_hDS;

    _bstr_t                             m_filename;
    CGridGeometryImpl::Ptr              m_pGG;
    CGridInfoImpl::Ptr                  m_pGI;
    std::vector<CDimensionImpl::Ptr>    m_arDim;

    void GetPixel(VariantArray& arVal,IPoint* pt);
    BOOL SetupComObjects();

    HRESULT DBRasterIO( int rowLo, int rowHi, int colLo, int colHi,
                        SAFEARRAY ** val, VARTYPE vt );
    
  public:
                    COGRRealGC();
    virtual         ~COGRRealGC();

    BOOL            Open( _bstr_t filename );

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    DECLARE_NO_REGISTRY()
    
    BEGIN_COM_MAP(COGRRealGC)
        COM_INTERFACE_ENTRY(ICoverage)
	COM_INTERFACE_ENTRY(IGridCoverage)
    END_COM_MAP()

// ICoverage
    STDMETHOD(Extent2D)(double* minX, double* minY, double* maxX, double* maxY);
    STDMETHOD(get_Domain)(IGeometry * * val);
    STDMETHOD(get_Codomain)(SAFEARRAY * * val);
    STDMETHOD(Evaluate)(IPoint * pt, SAFEARRAY * * val);
    STDMETHOD(EvaluateAsBoolean)(IPoint * pt, SAFEARRAY * * val);
    STDMETHOD(EvaluateAsByte)(IPoint * pt, SAFEARRAY * * val);
    STDMETHOD(EvaluateAsInteger)(IPoint * pt, SAFEARRAY * * val);
    STDMETHOD(EvaluateAsDouble)(IPoint * pt, SAFEARRAY * * val);

// IGridCoverage
    STDMETHOD(get_DataEditable)(VARIANT_BOOL * val);
    STDMETHOD(get_InterpolationType)(Interpolation * val);
    STDMETHOD(get_GridInfo)(IGridInfo * * val);
    STDMETHOD(get_GridGeometry)(IGridGeometry * * val);
    STDMETHOD(get_NumSource)(LONG * val);
    STDMETHOD(get_Source)(LONG index, IGridCoverage * * val);
    STDMETHOD(GetDataBlock)(LONG colLo, LONG rowLo,
                            LONG colHi, LONG rowHi, IStream ** val);
    STDMETHOD(GetDataBlockAsBoolean)(LONG colLo, LONG rowLo,
                                     LONG colHi, LONG rowHi, SAFEARRAY ** val);
    STDMETHOD(GetDataBlockAsByte)(LONG colLo, LONG rowLo,
                                  LONG colHi, LONG rowHi, SAFEARRAY * * val);
    STDMETHOD(GetDataBlockAsInteger)(LONG colLo, LONG rowLo,
                                     LONG colHi, LONG rowHi, SAFEARRAY ** val);
    STDMETHOD(GetDataBlockAsDouble)(LONG colLo, LONG rowLo,
                                    LONG colHi, LONG rowHi, SAFEARRAY ** val);
    STDMETHOD(SetDataBlock)(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi,
                            IStream * val);
    STDMETHOD(SetDataBlockAsBoolean)(LONG colLo, LONG rowLo,
                                     LONG colHi, LONG rowHi, SAFEARRAY * val);
    STDMETHOD(SetDataBlockAsByte)(LONG colLo, LONG rowLo,
                                  LONG colHi, LONG rowHi, SAFEARRAY * val);
    STDMETHOD(SetDataBlockAsInteger)(LONG colLo, LONG rowLo,
                                     LONG colHi, LONG rowHi, SAFEARRAY * val);
    STDMETHOD(SetDataBlockAsDouble)(LONG colLo, LONG rowLo,
                                    LONG colHi, LONG rowHi, SAFEARRAY * val);
};

#endif /* ndef ..._H_INCLUDED */
