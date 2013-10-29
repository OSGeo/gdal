/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for objects with metadata, etc.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          GDALMajorObject()                           */
/************************************************************************/

GDALMajorObject::GDALMajorObject()

{
    nFlags = GMO_VALID;
}

/************************************************************************/
/*                          ~GDALMajorObject()                          */
/************************************************************************/

GDALMajorObject::~GDALMajorObject()

{
    if( (nFlags & GMO_VALID) == 0 )
        CPLDebug( "GDAL", "In ~GDALMajorObject on invalid object" );

    nFlags &= ~GMO_VALID;
}

/************************************************************************/
/*                           GetDescription()                           */
/************************************************************************/

/**
 * \brief Fetch object description. 
 *
 * The semantics of the returned description are specific to the derived
 * type.  For GDALDatasets it is the dataset name.  For GDALRasterBands
 * it is actually a description (if supported) or "".
 *
 * This method is the same as the C function GDALGetDescription().
 * 
 * @return non-null pointer to internal description string.
 */

const char *GDALMajorObject::GetDescription() const

{
    return sDescription.c_str();
}

/************************************************************************/
/*                         GDALGetDescription()                         */
/************************************************************************/

/**
 * \brief Fetch object description. 
 *
 * @see GDALMajorObject::GetDescription()
 */ 

const char * CPL_STDCALL GDALGetDescription( GDALMajorObjectH hObject )

{
    VALIDATE_POINTER1( hObject, "GDALGetDescription", NULL );

    return ((GDALMajorObject *) hObject)->GetDescription();
}

/************************************************************************/
/*                           SetDescription()                           */
/************************************************************************/

/**
 * \brief Set object description. 
 *
 * The semantics of the description are specific to the derived
 * type.  For GDALDatasets it is the dataset name.  For GDALRasterBands
 * it is actually a description (if supported) or "".
 *
 * Normally application code should not set the "description" for 
 * GDALDatasets.  It is handled internally.  
 *
 * This method is the same as the C function GDALSetDescription().
 */

void GDALMajorObject::SetDescription( const char * pszNewDesc ) 

{
    sDescription = pszNewDesc;
}

/************************************************************************/
/*                         GDALSetDescription()                         */
/************************************************************************/

/**
 * \brief Set object description. 
 *
 * @see GDALMajorObject::SetDescription()
 */ 

void CPL_STDCALL GDALSetDescription( GDALMajorObjectH hObject, const char *pszNewDesc )

{
    VALIDATE_POINTER0( hObject, "GDALSetDescription" );

    ((GDALMajorObject *) hObject)->SetDescription( pszNewDesc );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

/**
 * \brief Fetch list of metadata domains.
 *
 * The returned string list is the list of (non-empty) metadata domains.
 *
 * This method does the same thing as the C function GDALGetMetadataDomainList().
 * 
 * @return NULL or a string list. Must be freed with CSLDestroy()
 *
 * @since GDAL 2.0
 */

char **GDALMajorObject::GetMetadataDomainList()
{
    return CSLDuplicate(oMDMD.GetDomainList());
}

/************************************************************************/
/*                      BuildMetadataDomainList()                       */
/************************************************************************/

/**
 * \brief Helper function for custom implementations of GetMetadataDomainList()
 *
 *
 * @param papszList initial list of domains. May be NULL. Will become invalid
 *                  after function call (use return value)
 * @param bCheckNonEmpty if TRUE, each candidate domain will be tested to be non
 *                       empty
 * @param ... NULL terminated variadic list of candidate domains.
 *
 * @return NULL or a string list. Must be freed with CSLDestroy()
 *
 * @since GDAL 2.0
 */

char **GDALMajorObject::BuildMetadataDomainList(char** papszList, int bCheckNonEmpty, ...)
{
    va_list args;
    const char* pszDomain;
    va_start(args, bCheckNonEmpty);

    while( (pszDomain = va_arg(args, const char*)) != NULL )
    {
        if( CSLFindString(papszList, pszDomain) < 0 &&
            (!bCheckNonEmpty || GetMetadata(pszDomain) != NULL) )
        {
            papszList = CSLAddString(papszList, pszDomain);
        }
    }

    va_end(args);

    return papszList;
}

/************************************************************************/
/*                    GDALGetMetadataDomainList()                       */
/************************************************************************/

/**
 * \brief Fetch list of metadata domains.
 *
 * @see GDALMajorObject::GetMetadataDomainList()
 *
 * @since GDAL 2.0
 */ 

char ** CPL_STDCALL 
GDALGetMetadataDomainList( GDALMajorObjectH hObject)

{
    VALIDATE_POINTER1( hObject, "GetMetadataDomainList", NULL );

    return ((GDALMajorObject *) hObject)->GetMetadataDomainList();
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

/**
 * \brief Fetch metadata.
 *
 * The returned string list is owned by the object, and may change at
 * any time.  It is formated as a "Name=value" list with the last pointer
 * value being NULL.  Use the the CPL StringList functions such as 
 * CSLFetchNameValue() to manipulate it. 
 *
 * Note that relatively few formats return any metadata at this time. 
 *
 * This method does the same thing as the C function GDALGetMetadata().
 *
 * @param pszDomain the domain of interest.  Use "" or NULL for the default
 * domain.
 * 
 * @return NULL or a string list. 
 */

char **GDALMajorObject::GetMetadata( const char * pszDomain )

{
    return oMDMD.GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GDALGetMetadata()                           */
/************************************************************************/

/**
 * \brief Fetch metadata.
 *
 * @see GDALMajorObject::GetMetadata()
 */ 

char ** CPL_STDCALL 
GDALGetMetadata( GDALMajorObjectH hObject, const char * pszDomain )

{
    VALIDATE_POINTER1( hObject, "GDALGetMetadata", NULL );

    return ((GDALMajorObject *) hObject)->GetMetadata(pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

/** 
 * \brief Set metadata. 
 *
 * The C function GDALSetMetadata() does the same thing as this method.
 *
 * @param papszMetadataIn the metadata in name=value string list format to
 * apply.  
 * @param pszDomain the domain of interest.  Use "" or NULL for the default
 * domain. 
 * @return CE_None on success, CE_Failure on failure and CE_Warning if the
 * metadata has been accepted, but is likely not maintained persistently 
 * by the underlying object between sessions.
 */

CPLErr GDALMajorObject::SetMetadata( char ** papszMetadataIn, 
                                     const char * pszDomain )

{
    nFlags |= GMO_MD_DIRTY;
    return oMDMD.SetMetadata( papszMetadataIn, pszDomain );
}

/************************************************************************/
/*                          GDALSetMetadata()                           */
/************************************************************************/

/**
 * \brief Set metadata. 
 *
 * @see GDALMajorObject::SetMetadata()
 */ 

CPLErr CPL_STDCALL 
GDALSetMetadata( GDALMajorObjectH hObject, char **papszMD, 
                 const char *pszDomain )

{
    VALIDATE_POINTER1( hObject, "GDALSetMetadata", CE_Failure );

    return ((GDALMajorObject *) hObject)->SetMetadata( papszMD, pszDomain );
}


/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

/**
 * \brief Fetch single metadata item.
 *
 * The C function GDALGetMetadataItem() does the same thing as this method.
 *
 * @param pszName the key for the metadata item to fetch.
 * @param pszDomain the domain to fetch for, use NULL for the default domain.
 *
 * @return NULL on failure to find the key, or a pointer to an internal
 * copy of the value string on success.
 */

const char *GDALMajorObject::GetMetadataItem( const char * pszName, 
                                              const char * pszDomain )

{
    return oMDMD.GetMetadataItem( pszName, pszDomain );
}

/************************************************************************/
/*                        GDALGetMetadataItem()                         */
/************************************************************************/

/**
 * \brief Fetch single metadata item.
 *
 * @see GDALMajorObject::GetMetadataItem()
 */ 

const char * CPL_STDCALL GDALGetMetadataItem( GDALMajorObjectH hObject, 
                                 const char *pszName, 
                                 const char *pszDomain )

{
    VALIDATE_POINTER1( hObject, "GDALGetMetadataItem", NULL );

    return ((GDALMajorObject *) hObject)->GetMetadataItem( pszName, pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

/**
 * \brief Set single metadata item.
 *
 * The C function GDALSetMetadataItem() does the same thing as this method.
 *
 * @param pszName the key for the metadata item to fetch.
 * @param pszValue the value to assign to the key.
 * @param pszDomain the domain to set within, use NULL for the default domain.
 *
 * @return CE_None on success, or an error code on failure.
 */

CPLErr GDALMajorObject::SetMetadataItem( const char * pszName, 
                                         const char * pszValue, 
                                         const char * pszDomain )

{
    nFlags |= GMO_MD_DIRTY;
    return oMDMD.SetMetadataItem( pszName, pszValue,  pszDomain );
}

/************************************************************************/
/*                        GDALSetMetadataItem()                         */
/************************************************************************/

/**
 * \brief Set single metadata item.
 *
 * @see GDALMajorObject::SetMetadataItem()
 */ 

CPLErr CPL_STDCALL 
GDALSetMetadataItem( GDALMajorObjectH hObject, 
                     const char *pszName, const char *pszValue, 
                     const char *pszDomain )

{
    VALIDATE_POINTER1( hObject, "GDALSetMetadataItem", CE_Failure );

    return ((GDALMajorObject *) hObject)->SetMetadataItem( pszName, pszValue,
                                                           pszDomain );
}

/************************************************************************/
/*                             GetMOFlags()                             */
/************************************************************************/

int GDALMajorObject::GetMOFlags()

{
    return nFlags;
}

/************************************************************************/
/*                             SetMOFlags()                             */
/************************************************************************/

void GDALMajorObject::SetMOFlags( int nNewFlags )

{
    nFlags = nNewFlags;
}

