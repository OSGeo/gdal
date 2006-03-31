/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple Features Provider Enumerator (SFCEnumerator)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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
 * Revision 1.4  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.3  1999/07/07 19:39:20  warmerda
 * added OpenAny()
 *
 * Revision 1.2  1999/06/08 17:51:55  warmerda
 * remoted unimplemented MoveNextOGISProvider()
 *
 * Revision 1.1  1999/06/08 03:51:00  warmerda
 * New
 *
 */

#ifndef _SFCENUMERATOR_H_INCLUDED
#define _SFCENUMERATOR_H_INCLUDED

#include <atldbcli.h>

/**
 * Specialized ATL CEnumerator that knows how to identify OGIS providers.
 */

class SFCDataSource;

class SFCEnumerator : public CEnumerator
{
  public:
    int          IsOGISProvider();

    SFCDataSource *OpenAny( const char * pszDataSource );
};


#endif /* ndef _SFCENUMERATOR_H_INCLUDED */
