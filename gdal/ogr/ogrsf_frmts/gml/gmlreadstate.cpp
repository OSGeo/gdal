/**********************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReadState class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                            GMLReadState()                            */
/************************************************************************/

GMLReadState::GMLReadState() :
    m_poFeature(NULL),
    m_poParentState(NULL),
    m_nPathLength(0)
{}

/************************************************************************/
/*                           ~GMLReadState()                            */
/************************************************************************/

GMLReadState::~GMLReadState() {}

/************************************************************************/
/*                              Reset()                                 */
/************************************************************************/

void GMLReadState::Reset()
{
    m_poFeature = NULL;
    m_poParentState = NULL;

    osPath.resize(0);
    m_nPathLength = 0;
}

/************************************************************************/
/*                              PushPath()                              */
/************************************************************************/

void GMLReadState::PushPath( const char *pszElement, int nLen )

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
