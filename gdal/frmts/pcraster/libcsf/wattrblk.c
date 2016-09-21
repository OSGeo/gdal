#include "csf.h"
#include "csfimpl.h"

/* write an attribute control block (LIBRARY_INTERNAL)
 * returns 0 if successful,
 * 1 if seeking or writing failed
 */
int CsfWriteAttrBlock(
	MAP *m,          /* map handle */
	CSF_FADDR32 pos,       /* file position where the block is written */
	ATTR_CNTRL_BLOCK *b) /* attribute control block to be written */
{
 int i;

 if ( fseek(m->fp,(long) pos, SEEK_SET) )
 	return 1;

 for(i=0; i < NR_ATTR_IN_BLOCK; i++)
  if ( m->write(&(b->attrs[i].attrId), sizeof(UINT2),(size_t)1,m->fp)    != 1 ||
       m->write(&(b->attrs[i].attrOffset),sizeof(CSF_FADDR32),(size_t)1,m->fp) != 1 ||
       m->write(&(b->attrs[i].attrSize), sizeof(UINT4),(size_t)1,m->fp)  != 1
     )
     return 1;

 return m->write(&(b->next), sizeof(CSF_FADDR32),(size_t)1,m->fp) != 1;
}
