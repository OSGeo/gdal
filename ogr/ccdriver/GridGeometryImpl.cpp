// GridGeometryImpl.cpp : Implementation of CGridGeometryImpl
#include "stdafx.h"
#include "OGRComGrid.h"
#include "GridGeometryImpl.h"

/////////////////////////////////////////////////////////////////////////////
// CGridGeometryImpl

CGridGeometryImpl::Ptr CGridGeometryImpl::Construct(int MinCol,int MinRow,int MaxCol,int MaxRow,IGeoReference* gr)
{
    Ptr pObj;
    if (!pObj.Create()) return 0;
    pObj->m_MaxCol=MaxCol;
    pObj->m_MaxRow=MaxRow;
    pObj->m_MinCol=MinCol;
    pObj->m_MinRow=MinRow;
    pObj->m_gr=gr;
    return pObj;
}

