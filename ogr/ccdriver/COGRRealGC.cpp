/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS OGR Grid Coverage Implementation
 * Purpose:  Implementation of the COGRRealGC class.  This is an IGridCoverage
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
 * Revision 1.1  1999/07/25 02:07:30  warmerda
 * New
 *
 */

#include "COGRRealGC.h"
#include "Geometry.h"
#include "AffineGeoReferenceImpl.h"

/************************************************************************/
/*                             COGRRealGC()                             */
/************************************************************************/

COGRRealGC::COGRRealGC()

{
    m_hDS = NULL;
}

/************************************************************************/
/*                             !COGRRealGC()                             */
/************************************************************************/

COGRRealGC::~COGRRealGC()

{
    if( m_hDS != NULL )
        GDALClose( m_hDS );

    printf( "~COGRRealGC()\n" );
    m_hDS = NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

BOOL COGRRealGC::Open(_bstr_t filename)
{
    GDALAllRegister();

    m_filename = filename;

    printf( "GDALOpen(%s)\n", (const char *) filename );

    m_hDS = GDALOpen( filename, GA_ReadOnly );
    if( m_hDS == NULL )
        return FALSE;

    return SetupComObjects();
}

/************************************************************************/
/*                          SetupComObjects()                           */
/************************************************************************/
BOOL COGRRealGC::SetupComObjects()
{
    // Setup GridGeometry COM object.
    {
        // Assume WGS84.
        ISpatialReferencePtr pSR=CreateEpsgSRS(4326);

        CComPtr<IAffineGeoReference> pAffine=CAffineGeoReferenceImpl::Construct(Pos2D(0,0),Vec2D(0.1,0),Vec2D(0,0.1),pSR);

        m_pGG=CGridGeometryImpl::Construct(0,0,
                                           GDALGetRasterXSize(m_hDS),
                                           GDALGetRasterYSize(m_hDS),
                                           pAffine);
    }

    // Setup array of Dimension COM objects.
    {
        for (int i=0; i < GDALGetRasterCount(m_hDS); i++)
        {
            CDimensionImpl::Ptr pDim;
            if (!pDim.Create()) return FALSE;

            pDim->m_dt = DT_8BIT_U;
            pDim->m_interpretation = Undefined;
            pDim->m_min = _variant_t(0l);
            pDim->m_max = _variant_t(255l);
            pDim->m_nodata = _variant_t((long)-1);

            m_arDim.push_back(pDim);
        }
    }

    // Setup GridInfo COM object.
    {
        int      nBXSize, nBYSize;

        GDALGetBlockSize( GDALGetRasterBand(m_hDS,1), &nBXSize, &nBYSize );

        if (!m_pGI.Create()) 
            return FALSE;

        m_pGI->m_ByteOrdering = wkbNDR;
        m_pGI->m_OptimalRowSize = nBXSize;
        m_pGI->m_OptimalColSize = nBYSize;
        m_pGI->m_PixelOrdering = PixelInterleaved;
        m_pGI->m_ValueSequence = RowSequenceMinToMax;
        m_pGI->m_ValueInBytePacking = HiBitFirst;
    }

    return TRUE;
}

/************************************************************************/
/*                              GetPixel()                              */
/************************************************************************/
void COGRRealGC::GetPixel(VariantArray& arVal,IPoint* pt)
{
    Pos2D posGrid;
    m_pGG->PointToGrid(pt,&posGrid);
    double gx=floor(posGrid.x+0.5);
    double gy=floor(posGrid.y+0.5);


    if (gx<m_pGG->m_MinCol || gx>=m_pGG->m_MaxCol ||
        gy<m_pGG->m_MinCol || gy>=m_pGG->m_MaxCol)
    {
        _variant_t vnull;
        vnull.ChangeType(VT_NULL);
        for (int i=0;i<m_arDim.size();i++) arVal.push_back(vnull);
    }
    else
    {
        int    iBand, nBands = GDALGetRasterCount(m_hDS);

        arVal.resize(nBands);
        for( iBand = 0; iBand < nBands; iBand++ )
        {
            GDALRasterBandH       hBand;
            double                dfValue;

            hBand = GDALGetRasterBand( m_hDS, iBand+1 );
            GDALRasterIO( hBand, GF_Read,  
                          gx, gy, 1, 1, 
                          &dfValue, 1, 1, GDT_Float64, 0, 0 );
            arVal[iBand] = dfValue;
        }
    }
}

/************************************************************************/
/*                             DBRasterIO()                             */
/************************************************************************/

HRESULT COGRRealGC::DBRasterIO( int colLo, int rowLo, int colHi, int rowHi,
                                SAFEARRAY ** val, VARTYPE vt )

{
    SAFEARRAYBOUND     bounds[3];
    GByte              *pData;
    HRESULT            hr;
    CPLErr             eErr;

    if (val == NULL)
        return E_POINTER;

/* -------------------------------------------------------------------- */
/*      What GDAL data type corresponds to the requested VARTYPE?       */
/* -------------------------------------------------------------------- */
    GDALDataType      eType;
    int               nTypeSize;

    switch( vt )
    {
        case VT_BOOL:
            eType = GDT_Byte;
            break;

        case VT_UI1:
            eType = GDT_Byte;
            break;

        case VT_I4:
            eType = GDT_Int32;
            break;

        case VT_R8:
            eType = GDT_Float64;
            break;

        default:
            return E_FAIL;
    }

    nTypeSize = GDALGetDataTypeSize( eType ) / 8;

/* -------------------------------------------------------------------- */
/*      Prepare the safe array.                                         */
/* -------------------------------------------------------------------- */
    int                nXWSize, nYWSize, nBands;

    nXWSize = colHi - colLo;
    nYWSize = rowHi - rowLo;
    nBands = GDALGetRasterCount( m_hDS );

    printf( "nXWSize=%d, nYWSize=%d\n", nXWSize, nYWSize );

    bounds[2].cElements = nYWSize;
    bounds[2].lLbound = 0;
    bounds[1].cElements = nXWSize;
    bounds[1].lLbound = 0;
    bounds[0].cElements = nBands;
    bounds[0].lLbound = 0;

    *val = SafeArrayCreate( VT_UI1, 3, bounds );
    
    if( ! (*val) )
        return E_FAIL;

/* -------------------------------------------------------------------- */
/*      Assign the pixel values to the array.                           */
/* -------------------------------------------------------------------- */
    hr = SafeArrayAccessData( *val, (void **) &pData );
    if( FAILED(hr) )
        return hr;

    eErr = CE_None;
    for( int iBand = 0; 
         eErr == CE_None && iBand < GDALGetRasterCount(m_hDS); 
         iBand++ )
    {
        GDALRasterBandH       hBand;
        
        hBand = GDALGetRasterBand( m_hDS, iBand+1 );
        
        eErr = 
            GDALRasterIO( hBand, GF_Read, 
                          colLo, rowLo, nXWSize, nYWSize,
                          ((GByte *) pData) + iBand * nTypeSize,
                          nXWSize, nYWSize, eType, 
                          nTypeSize * nBands, 
                          nTypeSize * nBands * nXWSize );
    }

    hr = SafeArrayUnaccessData( *val );
    if( FAILED(hr) )
        return hr;

/* -------------------------------------------------------------------- */
/*      If the output is supposed to be boolean we make an extra        */
/*      pass converting the type.                                       */
/* -------------------------------------------------------------------- */
    if( vt == VT_BOOL )
    {
        unsigned char      *pBData = (unsigned char *) pData;

        for( int i = nBands*nXWSize*nYWSize-1; i >= 0; i-- )
        {
            *pBData = !! *pBData;
            pBData++;
        }
    }

    if( eErr != CE_None )
        return E_FAIL;
    else
        return S_OK;
}

/************************************************************************/
/* ==================================================================== */
/*      ICoverage Methods                                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              Extent2D()                              */
/************************************************************************/
STDMETHODIMP
COGRRealGC::Extent2D(double* minX, double* minY, double* maxX, double* maxY)
{
    try
    {
        if (!minX || !minY || !maxX || !maxY)
            return E_POINTER;

        CComPtr<IGeoReference> gr;
        m_pGG->get_GeoReference(&gr);

        BOOL bFirst=TRUE;
        for (int i=m_pGG->m_MinCol;i<=m_pGG->m_MaxCol;i++)
        {
            int jStep=m_pGG->m_MaxRow-m_pGG->m_MinRow;
            if (i==m_pGG->m_MinCol || i==m_pGG->m_MaxCol) jStep=1;
            for (int j=m_pGG->m_MinRow;j<=m_pGG->m_MaxRow;j+=jStep)
            {
                Pos2D posG(i-0.5,j-0.5);
                Pos2D posSR;
                gr->GridCoordinateToSRS(&posG,&posSR);
                if (bFirst)
                {
                    *minX=*maxX=posSR.x;
                    *minY=*maxY=posSR.y;
                    bFirst=FALSE;
                }
                else
                {
                    if (*minX>posSR.x) *minX=posSR.x;
                    if (*maxX<posSR.x) *maxX=posSR.x;
                    if (*minY>posSR.y) *minY=posSR.y;
                    if (*maxY<posSR.y) *maxY=posSR.y;
                }
            }
        }
        return bFirst?E_FAIL:S_OK;
    }
    catch (...)
    {
    }
    return E_FAIL;
}

/************************************************************************/
/*                             get_Domain()                             */
/************************************************************************/
STDMETHODIMP
COGRRealGC::get_Domain(IGeometry ** val)
{
    try
    {
        if (val == NULL)
            return E_POINTER;

        CComPtr<IGeoReference> gr;
        m_pGG->get_GeoReference(&gr);
        ISpatialReferencePtr pSR;
        gr->get_SpatialReference(&pSR);

        // Assume GeoReference is affine, and edges do not bulge.
        Pos2D arPt[5];
        gr->GridCoordinateToSRS(&Pos2D(m_pGG->m_MinCol-0.5,m_pGG->m_MinRow-0.5),&arPt[0]);
        gr->GridCoordinateToSRS(&Pos2D(m_pGG->m_MaxCol-0.5,m_pGG->m_MinRow-0.5),&arPt[1]);
        gr->GridCoordinateToSRS(&Pos2D(m_pGG->m_MaxCol-0.5,m_pGG->m_MaxRow-0.5),&arPt[2]);
        gr->GridCoordinateToSRS(&Pos2D(m_pGG->m_MinCol-0.5,m_pGG->m_MaxRow-0.5),&arPt[3]);
        arPt[4]=arPt[0];

        IGeometryPtr pDomain=ComposePolygon(pSR,5,arPt);
        return pDomain.CopyTo(val);
    }
    catch (...)
    {
    }
    return E_FAIL;
}

/************************************************************************/
/*                            get_Codomain()                            */
/************************************************************************/

STDMETHODIMP
COGRRealGC::get_Codomain(SAFEARRAY * * val)
{
    if (val == NULL)
        return E_POINTER;

    return CreateSafeInterfaceArray(val,m_arDim);
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/
STDMETHODIMP
COGRRealGC::Evaluate(IPoint * pt, SAFEARRAY * * val)
{
    if (val == NULL)
        return E_POINTER;
        
    VariantArray arVar;
    GetPixel(arVar,pt);
    return CreateSafeArray(val,arVar);
}

/************************************************************************/
/*                         EvaluateAsBoolean()                          */
/************************************************************************/
STDMETHODIMP
COGRRealGC::EvaluateAsBoolean(IPoint * pt, SAFEARRAY * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    VariantArray arVar;
    GetPixel(arVar,pt);

    BoolArray arBool;
    for (int i=0;i<arVar.size();i++)
    {
        bool v=false;   // Initialise to no-data value.
        try
        {
            v=(bool)arVar[i];
        }
        catch(...)
        {
            // Variant was probably undefined or null.
        }
        arBool.push_back(v);
    }

    return CreateSafeArray(val,arBool);
}


/************************************************************************/
/*                           EvaluateAsByte()                           */
/************************************************************************/
STDMETHODIMP
COGRRealGC::EvaluateAsByte(IPoint * pt, SAFEARRAY * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    VariantArray arVar;
    GetPixel(arVar,pt);

    ByteArray arByte;
    for (int i=0;i<arVar.size();i++)
    {
        BYTE v=0;   // Initialise to no-data value.
        try
        {
            v=(BYTE)arVar[i];
        }
        catch(...)
        {
            // Variant was probably undefined or null.
        }
        arByte.push_back(v);
    }

    return CreateSafeArray(val,arByte,VT_UI1);
}

/************************************************************************/
/*                         EvaluateAsInteger()                          */
/************************************************************************/
STDMETHODIMP
COGRRealGC::EvaluateAsInteger(IPoint * pt, SAFEARRAY * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    VariantArray arVar;
    GetPixel(arVar,pt);

    LongArray arLong;
    for (int i=0;i<arVar.size();i++)
    {
        long v=0;   // Initialise to no-data value.
        try
        {
            v=(long)arVar[i];
        }
        catch(...)
        {
            // Variant was probably undefined or null.
        }
        arLong.push_back(v);
    }

    return CreateSafeArray(val,arLong,VT_I4);
}

/************************************************************************/
/*                          EvaluateAsDouble()                          */
/************************************************************************/
STDMETHODIMP
COGRRealGC::EvaluateAsDouble(IPoint * pt, SAFEARRAY ** val)
{
    if (val == NULL)
        return E_POINTER;
            
    VariantArray arVar;
    GetPixel(arVar,pt);

    DoubleArray arDouble;
    for (int i=0;i<arVar.size();i++)
    {
        double v=0;   // Initialise to no-data value.
        try
        {
            v=(double)arVar[i];
        }
        catch(...)
        {
            // Variant was probably undefined or null.
        }
        arDouble.push_back(v);
    }

    return CreateSafeArray(val,arDouble,VT_R8);
}

/************************************************************************/
/* ==================================================================== */
/*                       IGridCoverage Methods                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          get_DataEditable()                          */
/************************************************************************/
STDMETHODIMP
COGRRealGC::get_DataEditable(VARIANT_BOOL * val)
{
    if (val == NULL)
        return E_POINTER;
    
    *val=FALSE;
    return S_OK;
}

/************************************************************************/
/*                       get_InterpolationType()                        */
/************************************************************************/
STDMETHODIMP 
COGRRealGC::get_InterpolationType(Interpolation * val)
{
    if (val == NULL)
        return E_POINTER;
    
    *val=NearestNeighbor;
    return S_OK;
}

/************************************************************************/
/*                            get_GridInfo()                            */
/************************************************************************/
STDMETHODIMP COGRRealGC::get_GridInfo(IGridInfo * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    return m_pGI.CopyTo(val);
}

/************************************************************************/
/*                          get_GridGeometry()                          */
/************************************************************************/
STDMETHODIMP COGRRealGC::get_GridGeometry(IGridGeometry * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    return m_pGG.CopyTo(val);
}

/************************************************************************/
/*                           get_NumSource()                            */
/************************************************************************/
STDMETHODIMP COGRRealGC::get_NumSource(LONG * val)
{
    if (val == NULL)
        return E_POINTER;
        
    *val=0;    
    return S_OK;
}

/************************************************************************/
/*                             get_Source()                             */
/************************************************************************/

STDMETHODIMP COGRRealGC::get_Source(LONG index, IGridCoverage * * val)
{
    if (val == NULL)
        return E_POINTER;
            
    return E_INVALIDARG;
}

/************************************************************************/
/*                            SetDataBlock()                            */
/************************************************************************/
STDMETHODIMP COGRRealGC::SetDataBlock(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, IStream * val)
{
    return E_NOTIMPL;
}

    STDMETHODIMP COGRRealGC::SetDataBlockAsBoolean(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, SAFEARRAY * val)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP COGRRealGC::SetDataBlockAsByte(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, SAFEARRAY * val)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP COGRRealGC::SetDataBlockAsInteger(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, SAFEARRAY * val)
    {
        return E_NOTIMPL;
    }
    STDMETHODIMP COGRRealGC::SetDataBlockAsDouble(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, SAFEARRAY * val)
    {
        return E_NOTIMPL;
    }

/////////////////////////////////////////////////////////////////////////////
// COGRRealGC

HRESULT COGRRealGC::GetDataBlock(LONG colLo, LONG rowLo, LONG colHi, LONG rowHi, IStream * * val)
{
	if (val == NULL)
		return E_POINTER;

    ByteArray arData;

    // notdef
    //if (!m_bmp.GetBlock(colLo,rowLo,colHi,rowHi,arData)) return E_FAIL;

    int len=arData.size();
    HGLOBAL hGlobal=GlobalAlloc(GMEM_MOVEABLE,len);
    if (!hGlobal) return E_FAIL;
    LPVOID pData=GlobalLock(hGlobal);
    if (!pData)
    {
        GlobalFree(hGlobal);
        return E_FAIL;
    }
    memcpy(pData,&arData[0],len);
    GlobalUnlock(hGlobal);

    if (FAILED(CreateStreamOnHGlobal(hGlobal,TRUE,val)))
    {
        GlobalFree(hGlobal);
        return E_FAIL;
    }

    // Trim stream back to unpadded length.
    ULARGE_INTEGER s;
    s.QuadPart=len;
    (*val)->SetSize(s);

    return S_OK;
}

/************************************************************************/
/*                       GetDataBlockAsBoolean()                        */
/************************************************************************/

HRESULT COGRRealGC::GetDataBlockAsBoolean(LONG colLo, LONG rowLo, 
                                          LONG colHi, LONG rowHi, 
                                          SAFEARRAY * * val)

{
    return DBRasterIO( colLo, rowLo, colHi, rowHi, val, VT_BOOL );
}

/************************************************************************/
/*                         GetDataBlockAsByte()                         */
/************************************************************************/

HRESULT COGRRealGC::GetDataBlockAsByte(LONG colLo, LONG rowLo, 
                                       LONG colHi, LONG rowHi, 
                                       SAFEARRAY * * val)

{
    return DBRasterIO( colLo, rowLo, colHi, rowHi, val, VT_UI1 );
}

/************************************************************************/
/*                       GetDataBlockAsInteger()                        */
/************************************************************************/

HRESULT COGRRealGC::GetDataBlockAsInteger(LONG colLo, LONG rowLo, 
                                          LONG colHi, LONG rowHi, 
                                          SAFEARRAY * * val)
{
    return DBRasterIO( colLo, rowLo, colHi, rowHi, val, VT_I4 );
}

/************************************************************************/
/*                        GetDataBlockAsDouble()                        */
/************************************************************************/
HRESULT COGRRealGC::GetDataBlockAsDouble(LONG colLo, LONG rowLo, 
                                         LONG colHi, LONG rowHi, 
                                         SAFEARRAY * * val)
{
    return DBRasterIO( colLo, rowLo, colHi, rowHi, val, VT_R8 );
}

