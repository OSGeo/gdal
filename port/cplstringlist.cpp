/******************************************************************************
 * $Id$
 *
 * Project:  GDAL 
 * Purpose:  CPLStringList implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_string.h"
#include <string>

CPL_CVSID("$Id: cplstring.cpp 22648 2011-07-05 23:14:50Z warmerdam $");

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

CPLStringList::CPLStringList()

{
    papszList = NULL;
    nCount = 0;
    nAllocation = 0;
    bOwnList = FALSE;
}

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

/**
 * CPLStringList constructor.
 *
 * @param papszList the NULL terminated list of strings to consume.
 * @param bTakeOwnership TRUE if the CPLStringList should take ownership
 * of the list of strings which implies responsibility to free them.
 */

CPLStringList::CPLStringList( char **papszList, int bTakeOwnership )

{
    papszList = NULL;
    nCount = 0;
    nAllocation = 0;
    bOwnList = FALSE;
    Assign( papszList, bTakeOwnership );
}

/************************************************************************/
/*                           ~CPLStringList()                           */
/************************************************************************/

CPLStringList::~CPLStringList()

{
    Clear();
}

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

/**
 * Clear the string list. 
 */
CPLStringList &CPLStringList::Clear()

{
    if( bOwnList )
    {
        CSLDestroy( papszList );
        papszList = NULL;

        bOwnList = FALSE;
        nAllocation = 0;
        nCount = 0;
    }
    
    return *this;
}

/************************************************************************/
/*                               Assign()                               */
/************************************************************************/

/**
 * Assign a list of strings. 
 *
 * 
 * @param papszList the NULL terminated list of strings to consume.
 * @param bTakeOwnership TRUE if the CPLStringList should take ownership
 * of the list of strings which implies responsibility to free them.
 *
 * @return a reference to the CPLStringList on which it was invoked.
 */

CPLStringList &CPLStringList::Assign( char **papszList, int bTakeOwnership )

{
    Clear();

    this->papszList = papszList;
    bOwnList = bTakeOwnership;

    if( papszList == NULL || *papszList == NULL )
        nCount = 0;
    else
        nCount = -1;      // unknown

    nAllocation = 0;  

    return *this;
}

/************************************************************************/
/*                               Count()                                */
/************************************************************************/

/**
 * @return count of strings in the list, zero if empty.
 */

int CPLStringList::Count() const

{
    if( nCount == -1 )
    {
        if( papszList == NULL )
        {
            nCount = nAllocation = 0;
        }
        else
        {
            nCount = CSLCount( papszList );
            nAllocation = MAX(nCount+1,nAllocation);
        }
    }
    
    return nCount;
}

/************************************************************************/
/*                           MakeOurOwnCopy()                           */
/*                                                                      */
/*      If we don't own the list, a copy is made which we own.          */
/*      Necessary if we are going to modify the list.                   */
/************************************************************************/

void CPLStringList::MakeOurOwnCopy()

{
    if( bOwnList )
        return;

    if( papszList == NULL )
        return;

    Count();
    papszList = CSLDuplicate( papszList );
    nAllocation = nCount+1;
}

/************************************************************************/
/*                          EnsureAllocation()                          */
/*                                                                      */
/*      Ensure we have enough room allocated for at least the           */
/*      requested number of strings (so nAllocation will be at least    */
/*      one more than the target)                                       */
/************************************************************************/

void CPLStringList::EnsureAllocation( int nMaxList )

{
    if( !bOwnList )
        MakeOurOwnCopy();

    if( nAllocation <= nMaxList )
    {
        nAllocation = MAX(nAllocation*2 + 20,nMaxList+1);
		if( papszList == NULL )
		{
			papszList = (char **) CPLCalloc(nAllocation,sizeof(char*));
			bOwnList = TRUE;
			nCount = 0;
		}
		else
			papszList = (char **) CPLRealloc(papszList, nAllocation*sizeof(char*));
    }
}

/************************************************************************/
/*                         AddStringDirectly()                          */
/************************************************************************/

/**
 * Add a string to the list.
 *
 * This method is similar to AddString(), but ownership of the
 * pszNewString is transferred to the CPLStringList class.
 *
 * @param pszNewString the string to add to the list. 
 */

CPLStringList &CPLStringList::AddStringDirectly( char *pszNewString )

{
    if( nCount == -1 )
        Count();

    EnsureAllocation( nCount+1 );

    papszList[nCount++] = pszNewString;
    papszList[nCount] = NULL;

    return *this;
}

/************************************************************************/
/*                             AddString()                              */
/************************************************************************/

/**
 * Add a string to the list.
 *
 * A copy of the passed in string is made and inserted in the list.
 *
 * @param pszNewString the string to add to the list. 
 */

CPLStringList &CPLStringList::AddString( const char *pszNewString )

{
    return AddStringDirectly( CPLStrdup( pszNewString ) );
}

/************************************************************************/
/*                            AddNameValue()                            */
/************************************************************************/

/**
 * A a name=value entry to the list.
 *
 * A key=value string is prepared and appended to the list.  There is no
 * check for other values for the same key in the list.
 *
 * @param pszKey the key name to add.
 * @param pszValue the key value to add.
 */

CPLStringList &CPLStringList::AddNameValue( const char  *pszKey, 
                                            const char *pszValue )

{
    if (pszKey == NULL || pszValue==NULL)
        return *this;

    MakeOurOwnCopy();

    char *pszLine;
    pszLine = (char *) CPLMalloc(strlen(pszKey)+strlen(pszValue)+2);
    sprintf( pszLine, "%s=%s", pszKey, pszValue );
    AddStringDirectly( pszLine );

    return *this;
}

/************************************************************************/
/*                            SetNameValue()                            */
/************************************************************************/

/**
 * Set name=value entry in the list. 
 *
 * Similar to AddNameValue(), except if there is already a value for
 * the key in the list it is replaced instead of adding a new entry to
 * the list.  If pszValue is NULL any existing key entry is removed.
 * 
 * @param pszKey the key name to add.
 * @param pszValue the key value to add.
 */

CPLStringList &CPLStringList::SetNameValue( const char *pszKey, 
                                            const char *pszValue )

{
    int iKey = FindName( pszKey );

    if( iKey == -1 )
        return AddNameValue( pszKey, pszValue );

    Count();
    MakeOurOwnCopy();

    if( pszValue == NULL ) // delete entry
    {
        CPLFree( papszList[iKey] );
        while( papszList[iKey+1] != NULL )
            papszList[iKey] = papszList[iKey+1];
        papszList[iKey+1] = NULL;
        nCount--;
    }
    else
    {
        char *pszLine;
        pszLine = (char *) CPLMalloc(strlen(pszKey)+strlen(pszValue)+2);
        sprintf( pszLine, "%s=%s", pszKey, pszValue );
        
        papszList[iKey] = pszLine;
    }

    return *this;
}

/************************************************************************/
/*                              operator[]                              */
/************************************************************************/

/**
 * Fetch entry "i".
 *
 * Fetches the requested item in the list.  Note that the returned string
 * remains owned by the CPLStringList.  If "i" is out of range NULL is
 * returned.
 *
 * @param i the index of the list item to return.
 * @return selected entry in the list.  
 */
char *CPLStringList::operator[]( int i )

{
    if( nCount == -1 )
        Count();
    
    if( i < 0 || i >= nCount )
        return NULL;
    else
        return papszList[i];
}

const char *CPLStringList::operator[]( int i ) const

{
    if( nCount == -1 )
        Count();
    
    if( i < 0 || i >= nCount )
        return NULL;
    else
        return papszList[i];
}

/************************************************************************/
/*                             StealList()                              */
/************************************************************************/

/**
 * Seize ownership of underlying string array.
 *
 * This method is simmilar to List(), except that the returned list is
 * now owned by the caller and the CPLStringList is emptied.  
 *
 * @return the C style string list. 
 */
char **CPLStringList::StealList()

{
    char **papszRetList = papszList;

    bOwnList = FALSE;
    papszList = NULL;
    nCount = 0;
    nAllocation = 0;
    
    return papszRetList;
}
