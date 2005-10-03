#include "csf.h"
#include "csfimpl.h"

/* make block empty
 */
static void InitBlock(
	ATTR_CNTRL_BLOCK *b) /* write-only */
{
	int i;
 	for (i = 0 ; i < NR_ATTR_IN_BLOCK; i++)
	{
		b->attrs[i].attrId   = END_OF_ATTRS;
		b->attrs[i].attrSize   = 0;
		b->attrs[i].attrOffset = 0;
	}
	b->next = 0;
}

/* replace an attribute (LIBRARY_INTERNAL)
 *
 */
CSF_ATTR_ID CsfUpdateAttribute(
	MAP *m,       		/* map handle */
	CSF_ATTR_ID id,               /* attribute identification */
	size_t itemSize,        /* size of each attribute element.
	                         * 1 or sizeof(char) in case of a
	                         * string 
	                         */
	size_t nitems,          /* number of attribute elements or
	                         * strlen+1 in case of a variable character
	                         * string field. Don't forget to pad a
	                         * non-variable field with '\0'!
	                         */
	void *attr)       /* buffer containing attribute */
{
	PRECOND(CsfValidSize(itemSize));
	if (CsfAttributeSize(m,id))
		if (! MdelAttribute(m,id))
			return 0;
	return CsfPutAttribute(m,id,itemSize,nitems, attr);
}



/* write an attribute to a map (LIBRARY_INTERNAL)
 * MputAttribute writes exactly the number of bytes specified
 * by the size argument starting at the address of argument
 * attr. Which means that you can't simply pass a structure or an 
 * array of structures as argument attr, due to the alignment
 * of fields within a structure and internal swapping. You can
 * only pass an array of elementary types (UINT1, REAL4, etc.)
 * or character string.
 * If one wants to refresh an attribute, one should first
 * call MdelAttribute to delete the attribute and then use
 * MputAttribute to write the new value.
 * returns argument id or 0 in case of error.
 *
 * Merrno
 * ATTRDUPL
 * NOACCESS
 * WRITE_ERROR
 */
CSF_ATTR_ID CsfPutAttribute(
	MAP *m,       		/* map handle */
	CSF_ATTR_ID id,               /* attribute identification */
	size_t itemSize,        /* size of each attribute element.
	                         * 1 or sizeof(char) in case of a
	                         * string 
	                         */
	size_t nitems,          /* number of attribute elements or
	                         * strlen+1 in case of a variable character
	                         * string field. Don't forget to pad a
	                         * non-variable field with '\0'!
	                         */
	void *attr)       /* buffer containing attribute */
{
	size_t size = nitems * itemSize;
	
	PRECOND(CsfValidSize(itemSize));
	PRECOND(size > 0);

	if (CsfSeekAttrSpace(m,id,size) == 0)
		goto error;

	if (m->write(attr, itemSize, nitems, m->fp) != nitems) 
	{
		M_ERROR(WRITE_ERROR);
		goto error;
	}
	return(id); 		/* succes */
error:	return(0);	/* failure */
} 

/* seek to space for attribute  (LIBRARY_INTERNAL)
 * CsfSeekAttrSpace seeks to the a point in the file where
 * the attribute must be stored and update the attribute control 
 * blocks accordingly.
 * Writing can still fail since there is no check if that space is really
 * avalaible on the device. After this call returns the file is already
 * seeked to the point the functions returns.
 * returns the file position or 0 in case of error.
 *
 * Merrno
 * ATTRDUPL
 * NOACCESS
 * WRITE_ERROR
 */
CSF_FADDR CsfSeekAttrSpace( 
	MAP *m,       		/* map handle */
	CSF_ATTR_ID id,               /* attribute identification only for check if avalaible */
	size_t size)            /* size to be seeked to */
{
	ATTR_CNTRL_BLOCK b;
	CSF_FADDR currBlockPos, prevBlockPos=USED_UNINIT_ZERO, newPos, endBlock, resultPos=0;
	int noPosFound;
	int i;

	if (MattributeAvail(m ,id))
	{
		M_ERROR(ATTRDUPL);
		goto error;
	}

	if (! WRITE_ENABLE(m))
	{
		M_ERROR(NOACCESS);
		goto error;
	}

	currBlockPos = m->main.attrTable;
        noPosFound = 1;
	while (noPosFound) 
	{	
		if (currBlockPos == 0)
		{
			if (m->main.attrTable == 0)
			{ /* FIRST BLOCK */
				newPos =( (CSF_FADDR)(m->raster.nrRows)*
					   (CSF_FADDR)(m->raster.nrCols)*
					  (CSF_FADDR)(CELLSIZE(RgetCellRepr(m))))
					  + ADDR_DATA;
				m->main.attrTable = newPos;
			}
			else
			{ /* NEW/NEXT BLOCK */
				newPos = b.attrs[LAST_ATTR_IN_BLOCK].attrOffset 
					+
					b.attrs[LAST_ATTR_IN_BLOCK].attrSize;
				b.next = newPos;
				if (CsfWriteAttrBlock(m, prevBlockPos, &b))
				{
					M_ERROR(WRITE_ERROR);
					resultPos = 0;
				}
			}
			InitBlock(&b);
			b.attrs[0].attrOffset =
				newPos + SIZE_OF_ATTR_CNTRL_BLOCK;
			currBlockPos = newPos;
			noPosFound = 0;
		}
		else
			CsfReadAttrBlock(m, currBlockPos, &b);
		i = 0; /* this is also the right index if a new block
			   is added ! */
		while (noPosFound  && i < NR_ATTR_IN_BLOCK) 
			switch (b.attrs[i].attrId)
			{
				case END_OF_ATTRS:
					POSTCOND(i >= 1);
					/* i >= 1 , no block otherwise */
					b.attrs[i].attrOffset =
						b.attrs[i-1].attrOffset  +
						b.attrs[i-1].attrSize;
					noPosFound = 0;
                                        break;
				case ATTR_NOT_USED:
					if (i == NR_ATTR_IN_BLOCK)
						endBlock = b.next;
					else
						endBlock = b.attrs[i+1].attrOffset;
					if (( endBlock - b.attrs[i].attrOffset)
						>= size)
						/* this position can
							hold the attr */
						noPosFound = 0;
                                        else
                                            i++;
                                        break;
				 default:
                                            i++;
			} /* switch */
/*		if (b.next == 0) 
                     ? When did I change this CW
		       remember this block position since it may be have
		       to rewritten 
*/
		prevBlockPos = currBlockPos;
		if (noPosFound)
			currBlockPos = b.next;
 	} /* while */

	b.attrs[i].attrSize = size;
	b.attrs[i].attrId   = id;
	resultPos = b.attrs[i].attrOffset;

	if (CsfWriteAttrBlock(m, currBlockPos, &b))
	{
		M_ERROR(WRITE_ERROR);
		resultPos = 0;
	}
	fseek(m->fp, (long)resultPos, SEEK_SET); /* fsetpos() is better */
error:	return resultPos;
} /* CsfSeekAttrSpace */
