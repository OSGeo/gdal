/******************************************************************************
 * $Id$
 *
 * Project:  FYBA Callbacks
 * Purpose:  Needed by FYBA - however we do not want to display most messages
 * Author:   Thomas Hirsch, <thomas.hirsch statkart no>
 *
 ******************************************************************************
 * Copyright (c) 2010, Thomas Hirsch
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

#include <windows.h>
#include "fyba.h"

static short sProsent;

void LC_Error(short feil_nr, const char *logtx, const char *vartx)
{
   char szErrMsg[260];
   short strategi;
   char *pszFeilmelding;


   // Egen enkel implementasjon av feilhandtering
   /* Hent feilmeldingstekst og strategi */
   strategi = LC_StrError(feil_nr,&pszFeilmelding);
   switch(strategi) {
      case 2:  sprintf(szErrMsg,"%s","Observer følgende! \n\n");break;
      case 3:  sprintf(szErrMsg,"%s","Det er oppstått en feil! \n\n");break;
      case 4:  sprintf(szErrMsg,"%s","Alvorlig feil avslutt programmet! \n\n");break;
      default: szErrMsg[0]='\0';
   }

   if (strategi > 2) {
      Beep(100,500);
   }
}


void LC_StartMessage(const char *pszFilnavn)
{
}

void LC_ShowMessage(double prosent)
{
}

void LC_EndMessage(void)
{
}

short LC_Cancel(void)
{
      /* Not supported */
      return FALSE;
}
