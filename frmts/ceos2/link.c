/******************************************************************************
 *
 * Project:  ASI CEOS Translator
 * Purpose:  Link list function replacements.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ceos.h"

/************************************************************************/
/*                             ceos2CreateLink()                             */
/************************************************************************/

Link_t *ceos2CreateLink(void *pObject)

{
    Link_t *psLink = (Link_t *)CPLCalloc(sizeof(Link_t), 1);

    psLink->object = pObject;

    return psLink;
}

/************************************************************************/
/*                            DestroyList()                             */
/************************************************************************/

void DestroyList(Link_t *psList)

{
    while (psList != NULL)
    {
        Link_t *psNext = psList->next;

        CPLFree(psList);
        psList = psNext;
    }
}

/************************************************************************/
/*                             InsertLink()                             */
/************************************************************************/

Link_t *InsertLink(Link_t *psList, Link_t *psLink)

{
    psLink->next = psList;

    return psLink;
}

/************************************************************************/
/*                              AddLink()                               */
/************************************************************************/

Link_t *AddLink(Link_t *psList, Link_t *psLink)

{
    Link_t *psNode;

    if (psList == NULL)
        return psLink;

    for (psNode = psList; psNode->next != NULL; psNode = psNode->next)
    {
    }

    psNode->next = psLink;

    return psList;
}
