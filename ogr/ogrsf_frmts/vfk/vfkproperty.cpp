/******************************************************************************
 *
 * Project:  VFK Reader - Property definition
 * Purpose:  Implements VFKProperty class.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "vfkreader.h"
#include "vfkreaderp.h"

#include "cpl_conv.h"
#include "cpl_error.h"

/*!
  \brief Set VFK property (null)
*/
VFKProperty::VFKProperty() : m_bIsNull(true), m_iValue(0), m_dValue(0.0)
{
}

/*!
  \brief Set VFK property (integer)
*/
VFKProperty::VFKProperty(int iValue)
    : m_bIsNull(false), m_iValue(iValue), m_dValue(0.0)
{
}

/*!
  \brief Set VFK property (big integer)
*/
VFKProperty::VFKProperty(GIntBig iValue)
    : m_bIsNull(false), m_iValue(iValue), m_dValue(0.0)
{
}

/*!
  \brief Set VFK property (double)
*/
VFKProperty::VFKProperty(double dValue)
    : m_bIsNull(false), m_iValue(0), m_dValue(dValue)
{
}

/*!
  \brief Set VFK property (string)
*/
VFKProperty::VFKProperty(const char *pszValue)
    : m_bIsNull(false), m_iValue(0), m_dValue(0.0),
      m_strValue(nullptr != pszValue ? pszValue : "")
{
}

/*!
  \brief Set VFK property (string)
*/
VFKProperty::VFKProperty(CPLString const &strValue)
    : m_bIsNull(false), m_iValue(0), m_dValue(0.0), m_strValue(strValue)
{
}

/*!
  \brief VFK property destructor
*/
VFKProperty::~VFKProperty()
{
}

/*!
  \brief Get string property

  \param escape true to escape characters for SQL

  \return string buffer
*/
const char *VFKProperty::GetValueS(bool escape) const
{
    if (!escape)
        return m_strValue.c_str();

    CPLString strValue(m_strValue);
    size_t ipos = 0;
    while (std::string::npos != (ipos = strValue.find("'", ipos)))
    {
        strValue.replace(ipos, 1, "\'\'", 2);
        ipos += 2;
    }

    return CPLSPrintf("%s", strValue.c_str());
}
