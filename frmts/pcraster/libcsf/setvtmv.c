/*
 * setvtmv.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:05:06  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.2  1995/11/01 17:23:03  cees
.

 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* (LIBRARY_INTERNAL)
 */
void CsfSetVarTypeMV( CSF_VAR_TYPE *var, CSF_CR cellRepr)
{
/* assuming unions are left-alligned */
	if(IS_SIGNED(cellRepr))
		switch(LOG_CELLSIZE(cellRepr))
		{
		 case 2	:	*(INT4 *)var = MV_INT4;
		 		break;
		 case 1	:	*(INT2 *)var = MV_INT2;
		 		break;
		 default:	POSTCOND(LOG_CELLSIZE(cellRepr) == 0);
		 		*(INT1 *)var = MV_INT1;
		}
	else
	{
		((UINT4 *)var)[0] = MV_UINT4;
		((UINT4 *)var)[1] = MV_UINT4;
	}
}
