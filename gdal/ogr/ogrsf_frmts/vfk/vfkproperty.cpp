/******************************************************************************
 * $Id$
 *
 * Project:  VFK Reader - Property definition
 * Purpose:  Implements VFKProperty class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

/*!
  \brief Set VFK property (null)
*/
VFKProperty::VFKProperty()
    : m_bIsNull(TRUE), m_nValue(0), m_dValue(0.0)
{
}

/*!
  \brief Set VFK property (integer)
*/
VFKProperty::VFKProperty(int iValue) 
    : m_bIsNull(FALSE), m_nValue(iValue), m_dValue(0.0)
{
}

/*!
  \brief Set VFK property (double)
*/
VFKProperty::VFKProperty(double dValue)
    : m_bIsNull(FALSE), m_nValue(0), m_dValue(dValue)
{
}

/*!
  \brief Set VFK property (string)
*/
VFKProperty::VFKProperty(const char *pszValue)
    : m_bIsNull(FALSE), m_nValue(0), m_dValue(0.0), m_strValue(0 != pszValue ? pszValue : "")
{
}

/*!
  \brief Set VFK property (string)
*/
VFKProperty::VFKProperty(CPLString const& strValue)
    : m_bIsNull(FALSE), m_nValue(0), m_dValue(0.0), m_strValue(strValue)
{
}

/*!
  \brief VFK property destructor
*/
VFKProperty::~VFKProperty()
{
}

/*!
  \brief Copy constructor.
*/
VFKProperty::VFKProperty(VFKProperty const& other)
    : m_bIsNull(other.m_bIsNull),
      m_nValue(other.m_nValue), m_dValue(other.m_dValue), m_strValue(other.m_strValue)
{
}

/*!
  \brief Assignment operator.
*/
VFKProperty& VFKProperty::operator=(VFKProperty const& other)
{
    if (&other != this) {
        m_bIsNull = other.m_bIsNull;
        m_nValue = other.m_nValue;
        m_dValue = other.m_dValue;
        m_strValue = other.m_strValue;
    }
    return *this;
}

/*!
  \brief Get string property
  
  \param escape TRUE to escape characters for SQL
  
  \return string buffer
*/
const char *VFKProperty::GetValueS(bool escape) const
{
    size_t ipos;

    if (!escape)
        return m_strValue.c_str();

    CPLString strValue(m_strValue);
    ipos = 0;
    while (std::string::npos != (ipos = strValue.find("'", ipos))) {
        strValue.replace(ipos, 1, "\'\'", 2);
        ipos += 2;
    }

    return CPLSPrintf("%s", strValue.c_str());
}
