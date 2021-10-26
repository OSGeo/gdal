#include "csf.h"
#include "csfimpl.h"

/* get the number of entries with a negative
 * number for a type 1 legend
 */
static int NrLegendEntries(MAP *m)
{
	int size = (int)CsfAttributeSize(m, ATTR_ID_LEGEND_V2);
	if (size == 0)
	{
		if ( (size = -(int)CsfAttributeSize(m, ATTR_ID_LEGEND_V1)) != 0 )
			size -= CSF_LEGEND_ENTRY_SIZE;
	}
	return size/CSF_LEGEND_ENTRY_SIZE;
}
static int CmpEntries(
	CSF_LEGEND *e1,
	CSF_LEGEND *e2)
{
	return (int) ((e1->nr) - (e2->nr));
}

static void SortEntries(
	CSF_LEGEND *l, /* version 2 legend */
	size_t        nr) /* nr entries + name */
{
#ifndef USE_IN_PCR
	typedef int (*QSORT_CMP)(const void *e1, const void *e2);
#endif
	PRECOND(nr >= 1);
	qsort(l+1, (size_t)nr-1, sizeof(CSF_LEGEND), (QSORT_CMP)CmpEntries);
}

/* get the number of legend entries
 * MgetNrLegendEntries tries to find a version 2 or version 1
 * legend. The return number can be used to allocate the appropriate
 * array for legend. 
 * returns the number of entries in a legend plus 1 (for the name of the legend)
 * or 0 if there is no legend
 */
size_t MgetNrLegendEntries(
	MAP *m) /* the map pointer */
{
	return (size_t)ABS(NrLegendEntries(m));
}

/* read a legend
 * MgetLegend reads a version 2 and 1 legend.
 * Version 1 legend are converted to version 2: the first
 * array entry holds an empty string in the description field.
 * returns 
 * 0 if no legend is available or in case of an error,
 * nonzero otherwise
 */
int MgetLegend(
	MAP *m,        /* Map handle */
	CSF_LEGEND *l) /* array large enough to hold name and all entries, 
	                * the entries are sorted 
	                * struct CSF_LEGEND is typedef'ed in csfattr.h
	                */
{
	CSF_ATTR_ID id = NrLegendEntries(m) < 0 ? ATTR_ID_LEGEND_V1 : ATTR_ID_LEGEND_V2;
	size_t size;
	CSF_FADDR pos = CsfGetAttrPosSize(m, id, &size);
	size_t i,nr,start = 0;
        if (pos == 0)
        	return 0;
        if( csf_fseek(m->fp, pos, SEEK_SET) != 0 )
                return 0;
        if (id == ATTR_ID_LEGEND_V1)
        { 
        	/* empty name */
        	l[0].nr       = 0;
        	l[0].descr[0] = '\0';
        	start = 1; /* don't read in name */
        }
	nr = size/CSF_LEGEND_ENTRY_SIZE;
	for(i = start; i < nr+start; i++)
	{
		m->read(&(l[i].nr), sizeof(INT4), (size_t)1, m->fp);
		m->read(l[i].descr, sizeof(char), (size_t)CSF_LEGEND_DESCR_SIZE, m->fp);
	}
	SortEntries(l, nr+start);
	return 1;
}

/* write a legend
 * MputLegend writes a (version 2) legend to a map replacing
 * the old one if existent.
 * See  csfattr.h for the legend structure.
 *
 * returns 
 * 0 in case of an error,
 * nonzero otherwise
 *
 * Merrno
 * NOACCESS
 * WRITE_ERROR
 */
int MputLegend(
	MAP *m,        /* Map handle */
	CSF_LEGEND *l, /* read-write, array with name and entries, the entries
	                * are sorted before writing to the file.
	                * Strings are padded with zeros.
	                */
	size_t nrEntries) /* number of array elements. That is name plus real legend entries */
{
	int i = NrLegendEntries(m);
	CSF_ATTR_ID id = i < 0 ? ATTR_ID_LEGEND_V1 : ATTR_ID_LEGEND_V2;
	if (i)
		if (! MdelAttribute(m, id))
			return 0;
	SortEntries(l, nrEntries);
	if (CsfSeekAttrSpace(m, ATTR_ID_LEGEND_V2, (size_t)(nrEntries*CSF_LEGEND_ENTRY_SIZE)) == 0)
			return 0;
	for(i = 0; i < (int)nrEntries; i++)
	{
	     if(
		m->write(&(l[i].nr), sizeof(INT4), (size_t)1, m->fp) != 1 ||
		m->write(
		 CsfStringPad(l[i].descr,(size_t)CSF_LEGEND_DESCR_SIZE), 
		 sizeof(char), (size_t)CSF_LEGEND_DESCR_SIZE, m->fp) 
		 != CSF_LEGEND_DESCR_SIZE )
		 {
		 	M_ERROR(WRITE_ERROR);
		 	return 0;
		 }
	}
	return 1;
}
