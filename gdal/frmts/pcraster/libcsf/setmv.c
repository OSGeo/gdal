/*
 * setmv.c
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

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/06  13:04:27  cees
 * changed for appCR field
 * added c2man docs
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* set a memory location to a missing value
 * SetMV sets a memory location to a missing value
 * (using the application cell representation).
 * SetMV is quite slow but handy as in the example
 * below. In general one should use assignment for
 * integers (e.g. v = MV_UINT1) or the macro's
 * SET_MV_REAL4 and SET_MV_REAL8
 *
 * EXAMPLE
 * .so examples/border.tr
 */
void SetMV(
	const MAP *m, /* map handle */
	void *c)      /* write-only. location set to missing value */
{
	SetMVcellRepr(m->appCR, c);
}

/* set a memory location to a missing value
 * SetMVcellRepr sets a memory location to a missing value
 * (using the application cell representation).
 * In general one should use assignment for
 * integers (e.g. v = MV_UINT1) or the macro's
 * SET_MV_REAL4 and SET_MV_REAL8
 *
 */
void SetMVcellRepr(
	CSF_CR cellRepr, /* cell representation, one of the CR_* constants */
	void *c)      /* write-only. location set to missing value */
{
	switch (cellRepr)
	{
		case CR_INT1  : *((INT1 *)c) = MV_INT1;
				break;
		case CR_INT2  : *((INT2 *)c) = MV_INT2;
				break;
		case CR_INT4  : *((INT4 *)c) = MV_INT4;
				break;
		case CR_UINT1 : *((UINT1 *)c) = MV_UINT1;
				break;
		case CR_UINT2 : *((UINT2 *)c) = MV_UINT2;
				break;
		case CR_REAL8 :
				((UINT4 *)c)[1] = MV_UINT4;
		default       : POSTCOND(
					cellRepr == CR_REAL8 ||
					cellRepr == CR_REAL4 ||
					cellRepr == CR_UINT4 );
				*((UINT4 *)c) = MV_UINT4;
	}
} 
