#include "csf.h"
#include "csfimpl.h"

/* read an attribute (LIBRARY_INTERNAL)
 * MgetAttribute reads an attribute if it is available.
 * Be aware that you can't pass a simple pointer to some 
 * (array of) structure(s) due to alignment en endian problems.
 * At some time there will be a separate get function for each attribute
 * returns 0 if the attribute is not found, arg id if
 * the attribute is found.
 */
CSF_ATTR_ID CsfGetAttribute(
	 MAP *m, /* map handle */
	 CSF_ATTR_ID id, /* id of attribute to be read */
	 size_t  elSize, /* size of each data-element */
	 size_t *nmemb, /* write-only. How many elSize members are read. */
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
		(void)csf_fseek(m->fp, pos, SEEK_SET); 
		m->read(attr,elSize, (size_t)(*nmemb),m->fp);
		return(id);
	}
	else 
		*nmemb = 0;
error:	return(0);	/* not available  or an error */
} /* MgetAttribute */
