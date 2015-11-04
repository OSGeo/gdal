/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM rule class.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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
 ****************************************************************************/
#include "gnm.h"
#include "gnm_priv.h"


GNMRule::GNMRule()
{
    m_bValid = false;
    m_bAllow = false;
    m_bAny = false;
}

GNMRule::GNMRule(const std::string &oRule)
{
    m_soRuleString = oRule;
    m_bValid = ParseRuleString();
}

GNMRule::GNMRule(const char *pszRule)
{
    m_soRuleString = pszRule;
    m_bValid = ParseRuleString();
}

GNMRule::GNMRule(const GNMRule &oRule)
{
    m_soSrcLayerName = oRule.m_soSrcLayerName;
    m_soTgtLayerName = oRule.m_soTgtLayerName;
    m_soConnLayerName = oRule.m_soConnLayerName;
    m_bAllow = oRule.m_bAllow;
    m_bValid = oRule.m_bValid;
    m_bAny = oRule.m_bAny;
    m_soRuleString = oRule.m_soRuleString;
}

GNMRule::~GNMRule()
{

}

bool GNMRule::IsValid() const
{
    return m_bValid;
}

bool GNMRule::IsAcceptAny() const
{
    return m_bAny;
}

GNMRuleType GNMRule::GetType() const
{
    return GRTConnection;
}

bool GNMRule::CanConnect(const CPLString &soSrcLayerName,
                         const CPLString &soTgtLayerName,
                         const CPLString &soConnLayerName)
{
    if(IsAcceptAny())
        return m_bAllow;

    if(m_soSrcLayerName == soSrcLayerName &&
       m_soTgtLayerName == soTgtLayerName)
    {
        if(soConnLayerName.empty())
            return m_bAllow;
        else
            return m_bAllow && m_soConnLayerName == soConnLayerName;
    }

    return false;
}

CPLString GNMRule::GetSourceLayerName() const
{
    return m_soSrcLayerName;
}

CPLString GNMRule::GetTargetLayerName() const
{
    return m_soTgtLayerName;
}

CPLString GNMRule::GetConnectorLayerName() const
{
    return m_soConnLayerName;
}

const char *GNMRule::c_str() const
{
    return m_soRuleString.c_str();
}

GNMRule::operator const char *() const
{
    return c_str();
}

bool GNMRule::ParseRuleString()
{
    CPLStringList aTokens (CSLTokenizeString2(m_soRuleString.c_str(), " ", CSLT_STRIPLEADSPACES |
                                            CSLT_STRIPENDSPACES));

    // the minimum rule consist 3 tokens
    int nTokenCount = aTokens.Count();
    if(nTokenCount < 3)
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Need more than %d tokens. Failed to parse rule: %s",
                  nTokenCount, m_soRuleString.c_str() );
        return false;
    }

    if(EQUAL(aTokens[0], GNM_RULEKW_ALLOW))
        m_bAllow = true;
    else if(EQUAL(aTokens[0], GNM_RULEKW_DENY))
        m_bAllow = false;
    else
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "First token is invalid. Failed to parse rule: %s",
                  m_soRuleString.c_str() );
        return false;
    }

    // now just test if the value == connects
    // in future shoult set rule type

    if(!EQUAL(aTokens[1], GNM_RULEKW_CONNECTS))
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Not a CONNECTS rule. Failed to parse rule: %s",
                  m_soRuleString.c_str() );
        return false;
    }

    if(EQUAL(aTokens[2], GNM_RULEKW_ANY))
    {
        m_bAny = true;
        return true;
    }
    else
    {
        if(nTokenCount < 5)
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "Not an ANY rule, but have only %d tokens. Failed to parse rule: %s",
                      nTokenCount, m_soRuleString.c_str() );
            return false;
        }
        m_soSrcLayerName = aTokens[2];
        m_soTgtLayerName = aTokens[4];
    }

    if(nTokenCount < 7) // skip 5 and 6 parameters
        return true;
    else
        m_soConnLayerName = aTokens[6];

    return true;
}


