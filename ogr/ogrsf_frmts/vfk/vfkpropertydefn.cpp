/******************************************************************************
 *
 * Project:  VFK Reader - Data block property definition
 * Purpose:  Implements VFKPropertyDefn class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, 2012, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

/*!
  \brief VFKPropertyDefn constructor

  \param pszName property name
  \param pszType property type (original, string)
  \param pszEncoding encoding (only for "text" type)
*/
VFKPropertyDefn::VFKPropertyDefn(const char *pszName, const char *pszType,
                                 const char *pszEncoding)
    : m_pszName(CPLStrdup(pszName)), m_pszType(CPLStrdup(pszType)),
      m_pszEncoding(nullptr), m_nWidth(0), m_nPrecision(0)
{
    char *poWidth = m_pszType + 1;
    char *poChar = m_pszType + 1;
    int nLength = 0;  // Used after for.
    for (; *poChar && *poChar != '.'; nLength++, poChar++)
        ;

    char *pszWidth = static_cast<char *>(CPLMalloc(nLength + 1));
    strncpy(pszWidth, poWidth, nLength);
    pszWidth[nLength] = '\0';

    m_nWidth = atoi(pszWidth);
    CPLFree(pszWidth);

    // Type.
    if (*m_pszType == 'N')
    {
        if (*poChar == '.')
        {
            m_eFType = OFTReal;
            m_nPrecision = atoi(poChar + 1);
        }
        else
        {
            if (m_nWidth < 10)
                m_eFType = OFTInteger;
            else
            {
                m_eFType = OFTInteger64;
            }
        }
    }
    else if (*m_pszType == 'T')
    {
        // String.
        m_eFType = OFTString;
        m_pszEncoding = CPLStrdup(pszEncoding);
    }
    else if (*m_pszType == 'D')
    {
        // Date.
        // m_eFType = OFTDateTime;
        m_eFType = OFTString;
        m_nWidth = 25;
    }
    else
    {
        // Unknown - string.
        m_eFType = OFTString;
        m_pszEncoding = CPLStrdup(pszEncoding);
    }
}

/*!
  \brief VFKPropertyDefn destructor
*/
VFKPropertyDefn::~VFKPropertyDefn()
{
    CPLFree(m_pszName);
    CPLFree(m_pszType);
    if (m_pszEncoding)
        CPLFree(m_pszEncoding);
}

/*!
  \brief Get SQL data type

  \return string with data type ("text" by default)
*/
CPLString VFKPropertyDefn::GetTypeSQL() const
{
    switch (m_eFType)
    {
        case OFTInteger:
            return CPLString("integer");
        case OFTInteger64:
            return CPLString("bigint");
        case OFTReal:
            return CPLString("real");
        case OFTString:
            return CPLString("text");
        default:
            return CPLString("text");
    }
}
