/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Fetch properties from ICommand, IRowsetInfo, etc.
 * Author:   Ken Shih, kshih@home.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.3  2002/08/29 19:01:06  warmerda
 * added debug call
 *
 * Revision 1.2  2002/08/13 14:38:19  warmerda
 * reformat, add header
 *
 */

#include "sftraceback.h"

/************************************************************************/
/*                     SFGetDataSourceProperties()                      */
/*                                                                      */
/*      Get Data Source Prop from a ICommand pointer                    */
/*      Inteface passed in is released.                                 */
/************************************************************************/
IDBProperties *SFGetDataSourceProperties(ICommand *pICommand)
{
	HRESULT	hr;
	IGetDataSource *pIGetDataSource; // Session Interface

	if (!pICommand)
		return NULL;

	// IGetDataSource is a mandatory interface on session
	hr = pICommand->GetDBSession(IID_IGetDataSource,(IUnknown **) &pIGetDataSource);
	pICommand->Release();

	if (SUCCEEDED(hr))
	{
		return SFGetDataSourceProperties(pIGetDataSource);	
	}

	return NULL;
}


/************************************************************************/
/*                     SFGetDataSourceProperties()                      */
/*                                                                      */
/*      Get Data Source Prop from a IRowsetInfo pointer                 */
/*      Inteface passed in is released.                                 */
/************************************************************************/
IDBProperties *SFGetDataSourceProperties(IRowsetInfo* pIRInfo)
{
	HRESULT hr;

	if (!pIRInfo)
		return NULL;

	// The parent of the RInfo can be either a command or session.

	// Try the command first.
	ICommand *pICommand;
	hr = pIRInfo->GetSpecification(IID_ICommand, (IUnknown **) &pICommand);

	if (SUCCEEDED(hr))
	{
		pIRInfo->Release();
		return SFGetDataSourceProperties(pICommand);
	}

	// Try the session now.
	IGetDataSource *pIGetDataSource;
	hr = pIRInfo->GetSpecification(IID_IGetDataSource,
                                       (IUnknown **) &pIGetDataSource);
	pIRInfo->Release();

	if (SUCCEEDED(hr))
	{
		return SFGetDataSourceProperties(pIGetDataSource);
	}

        CPLDebug( "OGR_OLEDB", 
                  "Got IRowsetInfo, but not ICommand, nor IGetDataSource" );

	return NULL;
}

/************************************************************************/
/*                     SFGetDataSourceProperties()                      */
/*                                                                      */
/*      Get Data Source proper from a Session pointer (IGetDataSource)  */
/*      Interface passed in is released.                                */
/************************************************************************/
IDBProperties *SFGetDataSourceProperties(IGetDataSource *pIGetDataSource)
{
	if (!pIGetDataSource)
		return NULL;

	IDBProperties *pIDBProp;

	pIGetDataSource->GetDataSource(IID_IDBProperties, 
                                       (IUnknown **) &pIDBProp);
	pIGetDataSource->Release();

	return pIDBProp;
}

/************************************************************************/
/*                     SFGetDataSourceProperties()                      */
/*                                                                      */
/*      Get Data Source proper from an IUnknown                         */
/*      but must be a rowset/command or session                         */
/*      Interface passed in is released.                                */
/************************************************************************/
IDBProperties *SFGetDataSourceProperties(IUnknown *pIUnknown)
{
	HRESULT	hr;

	// Rowset
	IRowsetInfo *pRInfo;

	hr = pIUnknown->QueryInterface(IID_IRowsetInfo, (void **) &pRInfo);
	if (SUCCEEDED(hr))
	{
		pIUnknown->Release();
		return SFGetDataSourceProperties(pRInfo);
	}

	// Command
	ICommand	*pICommand;
	
	hr = pIUnknown->QueryInterface(IID_ICommand, (void **) &pICommand);
	if (SUCCEEDED(hr))
	{
		pIUnknown->Release();
		return SFGetDataSourceProperties(pICommand);
	}

	// Session
	IGetDataSource *pIGetDataSource;

	hr = pIUnknown->QueryInterface(IID_IGetDataSource, 
                                       (void **) &pIGetDataSource);
	if (SUCCEEDED(hr))
	{
		pIUnknown->Release();
		return SFGetDataSourceProperties(pIGetDataSource);
	}

	// Data Source itself
	IDBProperties *pIDBProperties;
	pIUnknown->QueryInterface(IID_IDBProperties, 
                                  (void **) &pIDBProperties);
	pIUnknown->Release();

	return pIDBProperties;
}
