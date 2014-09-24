/*
 * wattrblk.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.3  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.2  2005/09/29 18:43:23  cees
x86_64

Revision 1.1.1.1  2000/01/04 21:05:15  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/08/31  15:36:16  cees
 * added c2man doc
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* write an attribute control block (LIBRARY_INTERNAL)
 * returns 0 if successful,
 * 1 if seeking or writing failed
 */
int CsfWriteAttrBlock(
	MAP *m,          /* map handle */ 
	CSF_FADDR pos,       /* file position where the block is written */
	ATTR_CNTRL_BLOCK *b) /* attribute control block to be written */
{
 int i;

 if ( fseek(m->fp,(long) pos, SEEK_SET) )
 	return 1;

 for(i=0; i < NR_ATTR_IN_BLOCK; i++)
  if ( m->write(&(b->attrs[i].attrId), sizeof(UINT2),(size_t)1,m->fp)    != 1 ||
       m->write(&(b->attrs[i].attrOffset),sizeof(CSF_FADDR),(size_t)1,m->fp) != 1 ||
       m->write(&(b->attrs[i].attrSize), sizeof(UINT4),(size_t)1,m->fp)  != 1 
     )
     return 1;
 
 return m->write(&(b->next), sizeof(CSF_FADDR),(size_t)1,m->fp) != 1;
}
