/*
 * getattr.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:38  cees
Initial import Cees

Revision 2.1  1996/12/29 19:35:21  cees
src tree clean up

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

 * Revision 1.3  1995/11/01  17:23:03  cees
 * .
 *
 * Revision 1.2  1994/09/06  13:39:59  cees
 * added c2man docs
 * removed Merrno settin g if attr. is not avalaible
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* read an attribute (LIBRARY_INTERNAL)
 * MgetAttribute reads an attribute if it is available.
 * Be aware that you can't pass a simple pointer to some 
 * (array of) structure(s) due to allignment en endian problems.
 * At some time there will be a seperate get function for each attribute
 * returns 0 if the attribute is not found, arg id if
 * the attribute is found.
 */
CSF_ATTR_ID CsfGetAttribute(
	 MAP *m, /* map handle */
	 CSF_ATTR_ID id, /* id of attribute to be read */
	 size_t  elSize, /* size of each data-element */
	 size_t *nmemb, /* write-only. how many elSize mebers are read */
	 void  *attr) /* write-only. buffer where attribute is read in.
	               * Must be big enough to hold buffer.
	               */
{
	ATTR_CNTRL_BLOCK b;
	CSF_FADDR pos;
	PRECOND(CsfValidSize(elSize));
	CHECKHANDLE_GOTO(m, error);

	if (! READ_ENABLE(m))
	{
		M_ERROR(NOACCESS);
		goto error;
	}

	if (CsfGetAttrBlock(m, id, &b) != 0) 
	{
		int i = CsfGetAttrIndex(id, &b);
		*nmemb =	b.attrs[i].attrSize;
		POSTCOND( ((*nmemb) % elSize) == 0);
		*nmemb /= elSize;
		POSTCOND( (*nmemb) > 0);
		pos =	b.attrs[i].attrOffset;
		(void)fseek(m->fp, (long)pos, SEEK_SET); 
		m->read(attr,elSize, (size_t)(*nmemb),m->fp);
		return(id);
	}
	else 
		*nmemb = 0;
error:	return(0);	/* not available  or an error */
} /* MgetAttribute */
