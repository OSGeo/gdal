/**********************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReadState class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlreaderp.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

/************************************************************************/
/*                              Reset()                                 */
/************************************************************************/

void GMLReadState::Reset()
{
    m_poFeature = nullptr;
    m_poParentState = nullptr;

    osPath.clear();
    m_nPathLength = 0;
}

/************************************************************************/
/*                              PushPath()                              */
/************************************************************************/

void GMLReadState::PushPath(const char *pszElement, int nLen)

{
    if (m_nPathLength > 0)
        osPath.append(1, '|');
    if (m_nPathLength < static_cast<int>(aosPathComponents.size()))
    {
        if (nLen >= 0)
        {
            aosPathComponents[m_nPathLength].assign(pszElement, nLen);
            osPath.append(pszElement, nLen);
        }
        else
        {
            aosPathComponents[m_nPathLength].assign(pszElement);
            osPath.append(pszElement);
        }
    }
    else
    {
        aosPathComponents.push_back(pszElement);
        osPath.append(pszElement);
    }
    m_nPathLength++;
}

/************************************************************************/
/*                              PopPath()                               */
/************************************************************************/

void GMLReadState::PopPath()

{
    CPLAssert(m_nPathLength > 0);

    osPath.resize(osPath.size() - (aosPathComponents[m_nPathLength - 1].size() +
                                   ((m_nPathLength > 1) ? 1 : 0)));
    m_nPathLength--;
}
