#include "csf.h"
#include "csfimpl.h"

/* read attribute control block (LIBRARY_INTERNAL)
 */
void CsfReadAttrBlock(
	MAP *m,              /* map handle */
	CSF_FADDR32 pos,           /* file position of block to be read */
	ATTR_CNTRL_BLOCK *b) /* write-only. attribute control block read */
{
	int i;
	if (fseek(m->fp, (long)pos, SEEK_SET) != 0 )
            return;
	for(i=0; i < NR_ATTR_IN_BLOCK; i++)
	{
	 m->read((void *)&(b->attrs[i].attrId), sizeof(UINT2),(size_t)1,m->fp);
	 m->read((void *)&(b->attrs[i].attrOffset), sizeof(CSF_FADDR32),(size_t)1,m->fp);
	 m->read((void *)&(b->attrs[i].attrSize), sizeof(UINT4),(size_t)1,m->fp);
	}
	m->read((void *)&(b->next), sizeof(CSF_FADDR32),(size_t)1,m->fp);
}
