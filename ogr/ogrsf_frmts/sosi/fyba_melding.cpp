/******************************************************************************
 *
 * Project:  FYBA Callbacks
 * Purpose:  Needed by FYBA - however we do not want to display most messages
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <windows.h>
#include "fyba.h"

static short sProsent;

void LC_Error(short feil_nr, const char *logtx, const char *vartx)
{
    char szErrMsg[260] = {};
    char *pszFeilmelding = NULL;

    // Translate all to English.
    // Egen enkel implementasjon av feilhandtering
    /* Hent feilmeldingstekst og strategi */
    const short strategi = LC_StrError(feil_nr, &pszFeilmelding);
    switch (strategi)
    {
        case 2:
            sprintf(szErrMsg, "%s", "Observer følgende! \n\n");
            break;
        case 3:
            sprintf(szErrMsg, "%s", "Det er oppstått en feil! \n\n");
            break;
        case 4:
            sprintf(szErrMsg, "%s", "Alvorlig feil avslutt programmet! \n\n");
            break;
        default: /*szErrMsg[0]='\0';*/
            break;
    }
}

void LC_StartMessage(const char *pszFilnavn)
{
}

void LC_ShowMessage(double prosent)  // TODO: prosent?
{
}

void LC_EndMessage(void)
{
}

short LC_Cancel(void)
{
    // Not supported.
    return FALSE;
}
