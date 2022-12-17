/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  CPLStringList implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_string.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

CPLStringList::CPLStringList() = default;

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

/**
 * CPLStringList constructor.
 *
 * @param papszListIn the NULL terminated list of strings to consume.
 * @param bTakeOwnership TRUE if the CPLStringList should take ownership
 * of the list of strings which implies responsibility to free them.
 */

CPLStringList::CPLStringList(char **papszListIn, int bTakeOwnership)
    : CPLStringList()

{
    Assign(papszListIn, bTakeOwnership);
}

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

/**
 * CPLStringList constructor.
 *
 * The input list is copied.
 *
 * @param papszListIn the NULL terminated list of strings to ingest.
 */

CPLStringList::CPLStringList(CSLConstList papszListIn) : CPLStringList()

{
    Assign(CSLDuplicate(papszListIn));
}

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

//! Copy constructor
CPLStringList::CPLStringList(const CPLStringList &oOther) : CPLStringList()

{
    operator=(oOther);
}

/************************************************************************/
/*                           CPLStringList()                            */
/************************************************************************/

//! Move constructor
CPLStringList::CPLStringList(CPLStringList &&oOther) : CPLStringList()

{
    operator=(std::move(oOther));
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

CPLStringList &CPLStringList::operator=(const CPLStringList &oOther)
{
    if (this != &oOther)
    {
        Assign(oOther.papszList, FALSE);

        // We don't want to just retain a reference to the others list
        // as we don't want to make assumptions about its lifetime that
        if (!MakeOurOwnCopy())
        {
            papszList = nullptr;
            bOwnList = FALSE;
            nAllocation = 0;
            nCount = 0;
        }
        else
        {
            bIsSorted = oOther.bIsSorted;
        }
    }

    return *this;
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

CPLStringList &CPLStringList::operator=(CPLStringList &&oOther)
{
    if (this != &oOther)
    {
        Clear();
        papszList = oOther.papszList;
        oOther.papszList = nullptr;
        nCount = oOther.nCount;
        oOther.nCount = 0;
        nAllocation = oOther.nAllocation;
        oOther.nAllocation = 0;
        bOwnList = oOther.bOwnList;
        oOther.bOwnList = false;
        bIsSorted = oOther.bIsSorted;
        oOther.bIsSorted = true;
    }

    return *this;
}

/************************************************************************/
/*                             operator=()                              */
/************************************************************************/

CPLStringList &CPLStringList::operator=(CSLConstList papszListIn)
{
    if (papszListIn != papszList)
    {
        Assign(CSLDuplicate(papszListIn));
        bIsSorted = false;
    }

    return *this;
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
    if (bOwnList)
    {
        CSLDestroy(papszList);
        papszList = nullptr;

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
 * @param papszListIn the NULL terminated list of strings to consume.
 * @param bTakeOwnership TRUE if the CPLStringList should take ownership
 * of the list of strings which implies responsibility to free them.
 *
 * @return a reference to the CPLStringList on which it was invoked.
 */

CPLStringList &CPLStringList::Assign(char **papszListIn, int bTakeOwnership)

{
    Clear();

    papszList = papszListIn;
    bOwnList = CPL_TO_BOOL(bTakeOwnership);

    if (papszList == nullptr || *papszList == nullptr)
        nCount = 0;
    else
        nCount = -1;  // unknown

    nAllocation = 0;
    bIsSorted = FALSE;

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
    if (nCount == -1)
    {
        if (papszList == nullptr)
        {
            nCount = 0;
            nAllocation = 0;
        }
        else
        {
            nCount = CSLCount(papszList);
            nAllocation = std::max(nCount + 1, nAllocation);
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

bool CPLStringList::MakeOurOwnCopy()

{
    if (bOwnList)
        return true;

    if (papszList == nullptr)
        return true;

    Count();
    char **papszListNew = CSLDuplicate(papszList);
    if (papszListNew == nullptr)
    {
        return false;
    }
    papszList = papszListNew;
    bOwnList = true;
    nAllocation = nCount + 1;
    return true;
}

/************************************************************************/
/*                          EnsureAllocation()                          */
/*                                                                      */
/*      Ensure we have enough room allocated for at least the           */
/*      requested number of strings (so nAllocation will be at least    */
/*      one more than the target)                                       */
/************************************************************************/

bool CPLStringList::EnsureAllocation(int nMaxList)

{
    if (!bOwnList)
    {
        if (!MakeOurOwnCopy())
            return false;
    }

    if (papszList == nullptr || nAllocation <= nMaxList)
    {
        // we need to be able to store nMaxList+1 as an int,
        // and allocate (nMaxList+1) * sizeof(char*) bytes
        if (nMaxList < 0 || nMaxList > std::numeric_limits<int>::max() - 1 ||
            static_cast<size_t>(nMaxList) >
                std::numeric_limits<size_t>::max() / sizeof(char *) - 1)
        {
            return false;
        }
        int nNewAllocation = nMaxList + 1;
        if (nNewAllocation <= (std::numeric_limits<int>::max() - 20) / 2 /
                                  static_cast<int>(sizeof(char *)))
            nNewAllocation = std::max(nNewAllocation * 2 + 20, nMaxList + 1);
        if (papszList == nullptr)
        {
            papszList = static_cast<char **>(
                VSI_CALLOC_VERBOSE(nNewAllocation, sizeof(char *)));
            bOwnList = true;
            nCount = 0;
            if (papszList == nullptr)
                return false;
        }
        else
        {
            char **papszListNew = static_cast<char **>(VSI_REALLOC_VERBOSE(
                papszList, nNewAllocation * sizeof(char *)));
            if (papszListNew == nullptr)
                return false;
            papszList = papszListNew;
        }
        nAllocation = nNewAllocation;
    }
    return true;
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

CPLStringList &CPLStringList::AddStringDirectly(char *pszNewString)

{
    if (nCount == -1)
        Count();

    if (!EnsureAllocation(nCount + 1))
    {
        VSIFree(pszNewString);
        return *this;
    }

    papszList[nCount++] = pszNewString;
    papszList[nCount] = nullptr;

    bIsSorted = false;

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

CPLStringList &CPLStringList::AddString(const char *pszNewString)

{
    char *pszDupString = VSI_STRDUP_VERBOSE(pszNewString);
    if (pszDupString == nullptr)
        return *this;
    return AddStringDirectly(pszDupString);
}

/************************************************************************/
/*                            AddNameValue()                            */
/************************************************************************/

/**
 * Add a name=value entry to the list.
 *
 * A key=value string is prepared and appended to the list.  There is no
 * check for other values for the same key in the list.
 *
 * @param pszKey the key name to add.
 * @param pszValue the key value to add.
 */

CPLStringList &CPLStringList::AddNameValue(const char *pszKey,
                                           const char *pszValue)

{
    if (pszKey == nullptr || pszValue == nullptr)
        return *this;

    if (!MakeOurOwnCopy())
        return *this;

    /* -------------------------------------------------------------------- */
    /*      Format the line.                                                */
    /* -------------------------------------------------------------------- */
    if (strlen(pszKey) >
            std::numeric_limits<size_t>::max() - strlen(pszValue) ||
        strlen(pszKey) + strlen(pszValue) >
            std::numeric_limits<size_t>::max() - 2)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Too big strings in AddNameValue()");
        return *this;
    }
    const size_t nLen = strlen(pszKey) + strlen(pszValue) + 2;
    char *pszLine = static_cast<char *>(VSI_MALLOC_VERBOSE(nLen));
    if (pszLine == nullptr)
        return *this;
    snprintf(pszLine, nLen, "%s=%s", pszKey, pszValue);

    /* -------------------------------------------------------------------- */
    /*      If we don't need to keep the sort order things are pretty       */
    /*      straight forward.                                               */
    /* -------------------------------------------------------------------- */
    if (!IsSorted())
        return AddStringDirectly(pszLine);

    /* -------------------------------------------------------------------- */
    /*      Find the proper insertion point.                                */
    /* -------------------------------------------------------------------- */
    CPLAssert(IsSorted());
    const int iKey = FindSortedInsertionPoint(pszLine);
    InsertStringDirectly(iKey, pszLine);
    bIsSorted = true;  // We have actually preserved sort order.

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

CPLStringList &CPLStringList::SetNameValue(const char *pszKey,
                                           const char *pszValue)

{
    int iKey = FindName(pszKey);

    if (iKey == -1)
        return AddNameValue(pszKey, pszValue);

    Count();
    if (!MakeOurOwnCopy())
        return *this;

    CPLFree(papszList[iKey]);
    if (pszValue == nullptr)  // delete entry
    {

        // shift everything down by one.
        do
        {
            papszList[iKey] = papszList[iKey + 1];
        } while (papszList[iKey++] != nullptr);

        nCount--;
    }
    else
    {
        if (strlen(pszKey) >
                std::numeric_limits<size_t>::max() - strlen(pszValue) ||
            strlen(pszKey) + strlen(pszValue) >
                std::numeric_limits<size_t>::max() - 2)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Too big strings in AddNameValue()");
            return *this;
        }
        const size_t nLen = strlen(pszKey) + strlen(pszValue) + 2;
        char *pszLine = static_cast<char *>(VSI_MALLOC_VERBOSE(nLen));
        if (pszLine == nullptr)
            return *this;
        snprintf(pszLine, nLen, "%s=%s", pszKey, pszValue);

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
char *CPLStringList::operator[](int i)

{
    if (nCount == -1)
        Count();

    if (i < 0 || i >= nCount)
        return nullptr;

    return papszList[i];
}

const char *CPLStringList::operator[](int i) const

{
    if (nCount == -1)
        Count();

    if (i < 0 || i >= nCount)
        return nullptr;

    return papszList[i];
}

/************************************************************************/
/*                             StealList()                              */
/************************************************************************/

/**
 * Seize ownership of underlying string array.
 *
 * This method is similar to List(), except that the returned list is
 * now owned by the caller and the CPLStringList is emptied.
 *
 * @return the C style string list.
 */
char **CPLStringList::StealList()

{
    char **papszRetList = papszList;

    bOwnList = false;
    papszList = nullptr;
    nCount = 0;
    nAllocation = 0;

    return papszRetList;
}

static int CPLCompareKeyValueString(const char *pszKVa, const char *pszKVb)
{
    const char *pszItera = pszKVa;
    const char *pszIterb = pszKVb;
    while (true)
    {
        char cha = *pszItera;
        char chb = *pszIterb;
        if (cha == '=' || cha == '\0')
        {
            if (chb == '=' || chb == '\0')
                return 0;
            else
                return -1;
        }
        if (chb == '=' || chb == '\0')
        {
            return 1;
        }
        if (cha >= 'a' && cha <= 'z')
            cha -= ('a' - 'A');
        if (chb >= 'a' && chb <= 'z')
            chb -= ('a' - 'A');
        if (cha < chb)
            return -1;
        else if (cha > chb)
            return 1;
        pszItera++;
        pszIterb++;
    }
}

/************************************************************************/
/*                            llCompareStr()                            */
/*                                                                      */
/*      Note this is case insensitive!  This is because we normally     */
/*      treat key value keywords as case insensitive.                   */
/************************************************************************/
static int llCompareStr(const void *a, const void *b)
{
    return CPLCompareKeyValueString(
        *static_cast<const char **>(const_cast<void *>(a)),
        *static_cast<const char **>(const_cast<void *>(b)));
}

/************************************************************************/
/*                                Sort()                                */
/************************************************************************/

/**
 * Sort the entries in the list and mark list sorted.
 *
 * Note that once put into "sorted" mode, the CPLStringList will attempt to
 * keep things in sorted order through calls to AddString(),
 * AddStringDirectly(), AddNameValue(), SetNameValue(). Complete list
 * assignments (via Assign() and operator= will clear the sorting state.
 * When in sorted order FindName(), FetchNameValue() and FetchNameValueDef()
 * will do a binary search to find the key, substantially improve lookup
 * performance in large lists.
 */

CPLStringList &CPLStringList::Sort()

{
    Count();
    if (!MakeOurOwnCopy())
        return *this;

    if (nCount)
        qsort(papszList, nCount, sizeof(char *), llCompareStr);
    bIsSorted = true;

    return *this;
}

/************************************************************************/
/*                              FindName()                              */
/************************************************************************/

/**
 * Get index of given name/value keyword.
 *
 * Note that this search is for a line in the form name=value or name:value.
 * Use FindString() or PartialFindString() for searches not based on name=value
 * pairs.
 *
 * @param pszKey the name to search for.
 *
 * @return the string list index of this name, or -1 on failure.
 */

int CPLStringList::FindName(const char *pszKey) const

{
    if (!IsSorted())
        return CSLFindName(papszList, pszKey);

    // If we are sorted, we can do an optimized binary search.
    int iStart = 0;
    int iEnd = nCount - 1;
    size_t nKeyLen = strlen(pszKey);

    while (iStart <= iEnd)
    {
        const int iMiddle = (iEnd + iStart) / 2;
        const char *pszMiddle = papszList[iMiddle];

        if (EQUALN(pszMiddle, pszKey, nKeyLen) &&
            (pszMiddle[nKeyLen] == '=' || pszMiddle[nKeyLen] == ':'))
            return iMiddle;

        if (CPLCompareKeyValueString(pszKey, pszMiddle) < 0)
            iEnd = iMiddle - 1;
        else
            iStart = iMiddle + 1;
    }

    return -1;
}

/************************************************************************/
/*                            FetchBool()                               */
/************************************************************************/
/**
 *
 * Check for boolean key value.
 *
 * In a CPLStringList of "Name=Value" pairs, look to see if there is a key
 * with the given name, and if it can be interpreted as being TRUE.  If
 * the key appears without any "=Value" portion it will be considered true.
 * If the value is NO, FALSE or 0 it will be considered FALSE otherwise
 * if the key appears in the list it will be considered TRUE.  If the key
 * doesn't appear at all, the indicated default value will be returned.
 *
 * @param pszKey the key value to look for (case insensitive).
 * @param bDefault the value to return if the key isn't found at all.
 *
 * @return true or false
 */

bool CPLStringList::FetchBool(const char *pszKey, bool bDefault) const

{
    const char *pszValue = FetchNameValue(pszKey);

    if (pszValue == nullptr)
        return bDefault;

    return CPLTestBool(pszValue);
}

/************************************************************************/
/*                            FetchBoolean()                            */
/************************************************************************/
/**
 *
 * DEPRECATED: Check for boolean key value.
 *
 * In a CPLStringList of "Name=Value" pairs, look to see if there is a key
 * with the given name, and if it can be interpreted as being TRUE.  If
 * the key appears without any "=Value" portion it will be considered true.
 * If the value is NO, FALSE or 0 it will be considered FALSE otherwise
 * if the key appears in the list it will be considered TRUE.  If the key
 * doesn't appear at all, the indicated default value will be returned.
 *
 * @param pszKey the key value to look for (case insensitive).
 * @param bDefault the value to return if the key isn't found at all.
 *
 * @return TRUE or FALSE
 */

int CPLStringList::FetchBoolean(const char *pszKey, int bDefault) const

{
    return FetchBool(pszKey, CPL_TO_BOOL(bDefault)) ? TRUE : FALSE;
}

/************************************************************************/
/*                           FetchNameValue()                           */
/************************************************************************/

/**
 * Fetch value associated with this key name.
 *
 * If this list sorted, a fast binary search is done, otherwise a linear
 * scan is done.  Name lookup is case insensitive.
 *
 * @param pszName the key name to search for.
 *
 * @return the corresponding value or NULL if not found.  The returned string
 * should not be modified and points into internal object state that may
 * change on future calls.
 */

const char *CPLStringList::FetchNameValue(const char *pszName) const

{
    const int iKey = FindName(pszName);

    if (iKey == -1)
        return nullptr;

    CPLAssert(papszList[iKey][strlen(pszName)] == '=' ||
              papszList[iKey][strlen(pszName)] == ':');

    return papszList[iKey] + strlen(pszName) + 1;
}

/************************************************************************/
/*                         FetchNameValueDef()                          */
/************************************************************************/

/**
 * Fetch value associated with this key name.
 *
 * If this list sorted, a fast binary search is done, otherwise a linear
 * scan is done.  Name lookup is case insensitive.
 *
 * @param pszName the key name to search for.
 * @param pszDefault the default value returned if the named entry isn't found.
 *
 * @return the corresponding value or the passed default if not found.
 */

const char *CPLStringList::FetchNameValueDef(const char *pszName,
                                             const char *pszDefault) const

{
    const char *pszValue = FetchNameValue(pszName);
    if (pszValue == nullptr)
        return pszDefault;

    return pszValue;
}

/************************************************************************/
/*                            InsertString()                            */
/************************************************************************/

/**
 * \fn CPLStringList *CPLStringList::InsertString( int nInsertAtLineNo,
 *                                                 const char *pszNewLine );
 *
 * \brief Insert into the list at identified location.
 *
 * This method will insert a string into the list at the identified
 * location.  The insertion point must be within or at the end of the list.
 * The following entries are pushed down to make space.
 *
 * @param nInsertAtLineNo the line to insert at, zero to insert at front.
 * @param pszNewLine to the line to insert.  This string will be copied.
 */

/************************************************************************/
/*                        InsertStringDirectly()                        */
/************************************************************************/

/**
 * Insert into the list at identified location.
 *
 * This method will insert a string into the list at the identified
 * location.  The insertion point must be within or at the end of the list.
 * The following entries are pushed down to make space.
 *
 * @param nInsertAtLineNo the line to insert at, zero to insert at front.
 * @param pszNewLine to the line to insert, the ownership of this string
 * will be taken over the by the object.  It must have been allocated on the
 * heap.
 */

CPLStringList &CPLStringList::InsertStringDirectly(int nInsertAtLineNo,
                                                   char *pszNewLine)

{
    if (nCount == -1)
        Count();

    if (!EnsureAllocation(nCount + 1))
    {
        VSIFree(pszNewLine);
        return *this;
    }

    if (nInsertAtLineNo < 0 || nInsertAtLineNo > nCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLStringList::InsertString() requested beyond list end.");
        return *this;
    }

    bIsSorted = false;

    for (int i = nCount; i > nInsertAtLineNo; i--)
        papszList[i] = papszList[i - 1];

    papszList[nInsertAtLineNo] = pszNewLine;
    papszList[++nCount] = nullptr;

    return *this;
}

/************************************************************************/
/*                      FindSortedInsertionPoint()                      */
/*                                                                      */
/*      Find the location at which the indicated line should be         */
/*      inserted in order to keep things in sorted order.               */
/************************************************************************/

int CPLStringList::FindSortedInsertionPoint(const char *pszLine)

{
    CPLAssert(IsSorted());

    int iStart = 0;
    int iEnd = nCount - 1;

    while (iStart <= iEnd)
    {
        const int iMiddle = (iEnd + iStart) / 2;
        const char *pszMiddle = papszList[iMiddle];

        if (CPLCompareKeyValueString(pszLine, pszMiddle) < 0)
            iEnd = iMiddle - 1;
        else
            iStart = iMiddle + 1;
    }

    iEnd++;
    CPLAssert(iEnd >= 0 && iEnd <= nCount);
    CPLAssert(iEnd == 0 ||
              CPLCompareKeyValueString(pszLine, papszList[iEnd - 1]) >= 0);
    CPLAssert(iEnd == nCount ||
              CPLCompareKeyValueString(pszLine, papszList[iEnd]) <= 0);

    return iEnd;
}
