/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALMultiDomainMetadata class.  This class
 *           manages metadata items for a variable list of domains.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_priv.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_pam.h"

//! @cond Doxygen_Suppress
/************************************************************************/
/*                      GDALMultiDomainMetadata()                       */
/************************************************************************/

GDALMultiDomainMetadata::GDALMultiDomainMetadata() = default;

/************************************************************************/
/*                      ~GDALMultiDomainMetadata()                      */
/************************************************************************/

GDALMultiDomainMetadata::~GDALMultiDomainMetadata() = default;

/************************************************************************/
/*                               Clear()                                */
/************************************************************************/

void GDALMultiDomainMetadata::Clear()

{
    aosDomainList.clear();
    oMetadata.clear();
}

/************************************************************************/
/*                           SanitizeDomain()                           */
/************************************************************************/

static inline const char *SanitizeDomain(const char *pszDomain)
{
    return pszDomain ? pszDomain : "";
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALMultiDomainMetadata::GetMetadata(const char *pszDomain)

{
    const auto oIter = oMetadata.find(SanitizeDomain(pszDomain));
    if (oIter == oMetadata.end())
        return nullptr;
    return oIter->second.List();
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALMultiDomainMetadata::SetMetadata(CSLConstList papszMetadata,
                                            const char *pszDomain)

{
    pszDomain = SanitizeDomain(pszDomain);

    auto oIter = oMetadata.find(pszDomain);
    if (oIter == oMetadata.end())
    {
        aosDomainList.AddString(pszDomain);
        oIter =
            oMetadata.insert(std::pair(aosDomainList.back(), CPLStringList()))
                .first;
    }

    auto &oMDList = oIter->second;
    oMDList = papszMetadata;

    // we want to mark name/value pair domains as being sorted for fast
    // access.
    if (!STARTS_WITH_CI(pszDomain, "xml:") &&
        !STARTS_WITH_CI(pszDomain, "json:") && !EQUAL(pszDomain, "SUBDATASETS"))
    {
        oMDList.Sort();
    }

    return CE_None;
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALMultiDomainMetadata::GetMetadataItem(const char *pszName,
                                                     const char *pszDomain)

{
    const auto oIter = oMetadata.find(SanitizeDomain(pszDomain));
    if (oIter == oMetadata.end())
        return nullptr;
    return oIter->second.FetchNameValue(pszName);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALMultiDomainMetadata::SetMetadataItem(const char *pszName,
                                                const char *pszValue,
                                                const char *pszDomain)

{
    pszDomain = SanitizeDomain(pszDomain);

    /* -------------------------------------------------------------------- */
    /*      Create the domain if it does not already exist.                 */
    /* -------------------------------------------------------------------- */

    auto oIter = oMetadata.find(pszDomain);
    if (oIter == oMetadata.end())
    {
        aosDomainList.AddString(pszDomain);
        oIter =
            oMetadata.insert(std::pair(aosDomainList.back(), CPLStringList()))
                .first;
    }

    /* -------------------------------------------------------------------- */
    /*      Set the value in the domain list.                               */
    /* -------------------------------------------------------------------- */
    oIter->second.SetNameValue(pszName, pszValue);

    return CE_None;
}

/************************************************************************/
/*                              XMLInit()                               */
/*                                                                      */
/*      This method should be invoked on the parent of the              */
/*      <Metadata> elements.                                            */
/************************************************************************/

int GDALMultiDomainMetadata::XMLInit(const CPLXMLNode *psTree, int /* bMerge */)
{
    const CPLXMLNode *psMetadata = nullptr;

    /* ==================================================================== */
    /*      Process all <Metadata> elements, each for one domain.           */
    /* ==================================================================== */
    for (psMetadata = psTree->psChild; psMetadata != nullptr;
         psMetadata = psMetadata->psNext)
    {
        if (psMetadata->eType != CXT_Element ||
            !EQUAL(psMetadata->pszValue, "Metadata"))
            continue;

        const char *pszDomain = CPLGetXMLValue(psMetadata, "domain", "");
        const char *pszFormat = CPLGetXMLValue(psMetadata, "format", "");

        // Make sure we have a CPLStringList for this domain,
        // without wiping out an existing one.
        if (GetMetadata(pszDomain) == nullptr)
            SetMetadata(nullptr, pszDomain);

        auto oIter = oMetadata.find(pszDomain);
        CPLAssert(oIter != oMetadata.end());

        auto &oMDList = oIter->second;

        /* --------------------------------------------------------------------
         */
        /*      XML format subdocuments. */
        /* --------------------------------------------------------------------
         */
        if (EQUAL(pszFormat, "xml"))
        {
            // Find first non-attribute child of current element.
            const CPLXMLNode *psSubDoc = psMetadata->psChild;
            while (psSubDoc != nullptr && psSubDoc->eType == CXT_Attribute)
                psSubDoc = psSubDoc->psNext;

            char *pszDoc = CPLSerializeXMLTree(psSubDoc);

            oMDList.Clear();
            oMDList.AddStringDirectly(pszDoc);
        }

        /* --------------------------------------------------------------------
         */
        /*      JSon format subdocuments. */
        /* --------------------------------------------------------------------
         */
        else if (EQUAL(pszFormat, "json"))
        {
            // Find first text child of current element.
            const CPLXMLNode *psSubDoc = psMetadata->psChild;
            while (psSubDoc != nullptr && psSubDoc->eType != CXT_Text)
                psSubDoc = psSubDoc->psNext;
            if (psSubDoc)
            {
                oMDList.Clear();
                oMDList.AddString(psSubDoc->pszValue);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Name value format. */
        /*      <MDI key="...">value_Text</MDI> */
        /* --------------------------------------------------------------------
         */
        else
        {
            for (const CPLXMLNode *psMDI = psMetadata->psChild;
                 psMDI != nullptr; psMDI = psMDI->psNext)
            {
                if (!EQUAL(psMDI->pszValue, "MDI") ||
                    psMDI->eType != CXT_Element || psMDI->psChild == nullptr ||
                    psMDI->psChild->psNext == nullptr ||
                    psMDI->psChild->eType != CXT_Attribute ||
                    psMDI->psChild->psChild == nullptr)
                    continue;

                char *pszName = psMDI->psChild->psChild->pszValue;
                char *pszValue = psMDI->psChild->psNext->pszValue;
                if (pszName != nullptr && pszValue != nullptr)
                    oMDList.SetNameValue(pszName, pszValue);
            }
        }
    }

    return !aosDomainList.empty();
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

CPLXMLNode *GDALMultiDomainMetadata::Serialize() const

{
    CPLXMLNode *psFirst = nullptr;

    for (const auto &[pszDomainName, oList] : oMetadata)
    {
        CSLConstList papszMD = oList.List();
        // Do not serialize empty domains.
        if (papszMD == nullptr || papszMD[0] == nullptr)
            continue;

        CPLXMLNode *psMD = CPLCreateXMLNode(nullptr, CXT_Element, "Metadata");

        if (strlen(pszDomainName) > 0)
            CPLCreateXMLNode(CPLCreateXMLNode(psMD, CXT_Attribute, "domain"),
                             CXT_Text, pszDomainName);

        bool bFormatXMLOrJSon = false;

        if (STARTS_WITH_CI(pszDomainName, "xml:") && CSLCount(papszMD) == 1)
        {
            CPLXMLNode *psValueAsXML = CPLParseXMLString(papszMD[0]);
            if (psValueAsXML != nullptr)
            {
                bFormatXMLOrJSon = true;

                CPLCreateXMLNode(
                    CPLCreateXMLNode(psMD, CXT_Attribute, "format"), CXT_Text,
                    "xml");

                CPLAddXMLChild(psMD, psValueAsXML);
            }
        }

        if (STARTS_WITH_CI(pszDomainName, "json:") && CSLCount(papszMD) == 1)
        {
            bFormatXMLOrJSon = true;

            CPLCreateXMLNode(CPLCreateXMLNode(psMD, CXT_Attribute, "format"),
                             CXT_Text, "json");
            CPLCreateXMLNode(psMD, CXT_Text, *papszMD);
        }

        if (!bFormatXMLOrJSon)
        {
            CPLXMLNode *psLastChild = nullptr;
            // To go after domain attribute.
            if (psMD->psChild != nullptr)
            {
                psLastChild = psMD->psChild;
                while (psLastChild->psNext != nullptr)
                    psLastChild = psLastChild->psNext;
            }
            for (int i = 0; papszMD[i] != nullptr; i++)
            {
                char *pszKey = nullptr;

                const char *pszRawValue =
                    CPLParseNameValue(papszMD[i], &pszKey);

                CPLXMLNode *psMDI =
                    CPLCreateXMLNode(nullptr, CXT_Element, "MDI");
                if (psLastChild == nullptr)
                    psMD->psChild = psMDI;
                else
                    psLastChild->psNext = psMDI;
                psLastChild = psMDI;

                CPLSetXMLValue(psMDI, "#key", pszKey);
                CPLCreateXMLNode(psMDI, CXT_Text, pszRawValue);

                CPLFree(pszKey);
            }
        }

        if (psFirst == nullptr)
            psFirst = psMD;
        else
            CPLAddXMLSibling(psFirst, psMD);
    }

    return psFirst;
}

//! @endcond
