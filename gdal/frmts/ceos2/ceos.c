/******************************************************************************
 * $Id$
 *
 * Project:  ASI CEOS Translator
 * Purpose:  Core CEOS functions.
 * Author:   Paul Lahaie, pjlahaie@atlsci.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc
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

#include "ceos.h"

CPL_CVSID("$Id$")

/* Function implementations of functions described in ceos.h */

void CeosUpdateHeaderFromBuffer(CeosRecord_t *record);

void InitEmptyCeosRecord(CeosRecord_t *record, int32 sequence, CeosTypeCode_t typecode, int32 length)
{
    if(record)
    {
	if((record->Buffer = HMalloc(length)) == NULL)
	{
	    return;
	}
	/* First we zero fill the buffer */
	memset(record->Buffer,0,length);

	/* Setup values inside the CeosRecord_t header */
	record->Sequence = sequence;
	record->Flavor = 0;
	record->FileId = 0;
	record->TypeCode = typecode;
	record->Subsequence = 0;
	record->Length = length;

	/* Now we fill in the buffer portion as well */
	NativeToCeos( record->Buffer+SEQUENCE_OFF, &(record->Sequence), sizeof(record->Sequence), sizeof( record->Sequence ) );
	memcpy(record->Buffer+TYPE_OFF, &( record->TypeCode.Int32Code ), sizeof( record->TypeCode.Int32Code ) );
	NativeToCeos( record->Buffer+LENGTH_OFF, &length,  sizeof( length ), sizeof( length ) );
    }
}

void InitCeosRecord(CeosRecord_t *record, uchar *buffer)
{
    if(record && buffer)
    {
	InitCeosRecordWithHeader(record, buffer, buffer+CEOS_HEADER_LENGTH);
    }
}

void InitCeosRecordWithHeader(CeosRecord_t *record, uchar *header, uchar *buffer)
{
    if(record && buffer && header)
    {
        if( record->Length != 0 )
            record->Length = DetermineCeosRecordBodyLength( header );

	if(record->Length < CEOS_HEADER_LENGTH ||
            (record->Buffer = HMalloc(record->Length)) == NULL)
	{
	    record->Length = 0;
	    return;
	}

	/* First copy the header then the buffer */
	memcpy(record->Buffer,header,CEOS_HEADER_LENGTH);
	/* Now we copy the rest */
        if( record->Length > CEOS_HEADER_LENGTH )
            memcpy(record->Buffer+CEOS_HEADER_LENGTH,buffer,record->Length-CEOS_HEADER_LENGTH);

	/* Now we fill in the rest of the structure! */
	memcpy(&(record->TypeCode.Int32Code),header+TYPE_OFF,sizeof(record->TypeCode.Int32Code));
	CeosToNative(&(record->Sequence),header+SEQUENCE_OFF,sizeof(record->Sequence), sizeof( record->Sequence ) );
    }
}

int DetermineCeosRecordBodyLength(const uchar *header)
{
    int i;

    if(header)
    {
	CeosToNative(&i,header+LENGTH_OFF,sizeof( i ), sizeof( i ) );

	return i;
    }

    return -1;
}

void DeleteCeosRecord(CeosRecord_t *record)
{
    if(record)
    {
	if(record->Buffer)
	{
	    HFree(record->Buffer);
	    record->Buffer = NULL;
	}
        HFree( record );
    }
}

void GetCeosRecordStruct(const CeosRecord_t *record,void *struct_ptr)
{
    if(record && struct_ptr && record->Buffer)
    {
	memcpy(record->Buffer,struct_ptr,record->Length);
    }
}

void PutCeosRecordStruct(CeosRecord_t *record,const void *struct_ptr)
{
    int Length;

    if(record && struct_ptr)
    {
	CeosToNative( &Length, struct_ptr, sizeof( Length ), sizeof( Length ) );
	memcpy(record->Buffer,struct_ptr,Length);
	CeosUpdateHeaderFromBuffer(record);
    }
}

void GetCeosField(CeosRecord_t *record, int32 start_byte,
                  const char *format, void *value)
{
    int field_size;
    char *d_ptr;
    char *mod_buf = NULL;

    field_size = atoi(format+1);

    if(field_size < 1)
    {
	return;
    }

    /* Check for out of bounds */
    if(start_byte + field_size - 1 > record->Length)
    {
	return;
    }

    if((mod_buf = (char *) HMalloc(field_size + 1)) == NULL)
    {
	return;
    }

    memcpy(mod_buf,record->Buffer+(start_byte-1), field_size);
    mod_buf[field_size] = '\0';

    /* Switch on format type */
    switch(format[0])
    {
    case 'b':
    case 'B':
	/* Binary data type */
	if(field_size > 1)
	{
	    CeosToNative( value, mod_buf, field_size, field_size );
	} else {
	    memcpy( value, mod_buf, field_size );
	}
	break;

    case 'i':
    case 'I':
	/* Integer type */
	*( (int *)value) = atoi(mod_buf);
	break;

    case 'f':
    case 'F':
    case 'e':
    case 'E':
	/* Double precision float data type */

	/* Change the 'D' exponent separators to 'e' */
	if( ( d_ptr = strchr(mod_buf, 'd') ) != NULL)
	{
	    *d_ptr = 'e';
	}
	if( ( d_ptr = strchr(mod_buf, 'D') ) != NULL)
	{
	    *d_ptr = 'e';
	}

	*( (double *)value) = strtod(mod_buf,NULL);
	break;
    case 'a':
    case 'A':
	/* ASCII..  We just easily extract it */
	( (char *)value)[field_size] = '\0';
	memcpy( value, mod_buf, field_size );
	break;

    default:
	/* Unknown format.  Do nothing. */
        break;
    }

    HFree(mod_buf);
}

void SetCeosField(CeosRecord_t *record, int32 start_byte, char *format, void *value)
{
    int field_size;
    char * temp_buf = NULL;
    char szPrintfFormat[ 20 ];

    field_size = 0;
    sscanf(&format[1], "%d", &field_size);
    if(field_size < 1)
    {
	return;
    }

    /* Check for bounds */
    if(start_byte + field_size - 1 > record->Length)
    {
	return;
    }

    /* Make a local buffer to print into */
    if((temp_buf = (char *) HMalloc(field_size+1)) == NULL)
    {
	return;
    }
    switch(format[0])
    {
    case 'b':
    case 'B':
	/* Binary data type */
	if(field_size > 1)
	{
	    NativeToCeos( value, temp_buf, field_size, field_size );
	} else {
	    memcpy(value,temp_buf,field_size);
	}
	break;

    case 'i':
    case 'I':
	/* Integer data type */
	snprintf( szPrintfFormat, sizeof(szPrintfFormat), "%%%s%c",format+1, 'd');
	snprintf( temp_buf, field_size+1, szPrintfFormat, *(int *) value);
	break;

    case 'f':
    case 'F':
	/* Double precision floating point data type */
	snprintf( szPrintfFormat, sizeof(szPrintfFormat), "%%%s%c", format+1, 'g');
	snprintf( temp_buf, field_size+1, szPrintfFormat, *(double *)value);
	break;

    case 'e':
    case 'E':
	/* Double precision floating point data type (forced exponent) */
	snprintf( szPrintfFormat, sizeof(szPrintfFormat), "%%%s%c", format+1, 'e');
	snprintf( temp_buf, field_size+1, szPrintfFormat, *(double *)value);
	break;

    case 'a':
    case 'A':
	strncpy(temp_buf,value,field_size+1);
	temp_buf[field_size] = '0';
	break;

    default:
	/* Unknown format */
	HFree(temp_buf);
        return;
    }

    memcpy(record->Buffer + start_byte -1, temp_buf, field_size);

    HFree(temp_buf);
}

void SetIntCeosField(CeosRecord_t *record, int32 start_byte, int32 length, int32 value)
{
    int integer_value = value;
    char total_len[12];   /* 12 because 2^32 -> 4294967296 + I + null */

    snprintf(total_len,sizeof(total_len),"I%d",length);
    SetCeosField(record,start_byte,total_len,&integer_value);
}

CeosRecord_t *FindCeosRecord(Link_t *record_list, CeosTypeCode_t typecode, int32 fileid, int32 flavor, int32 subsequence)
{
    Link_t *Link;
    CeosRecord_t *record;

    for( Link = record_list; Link != NULL; Link = Link->next )
    {
	record = (CeosRecord_t *)Link->object;

	if( (record->TypeCode.Int32Code == typecode.Int32Code)
	    && ( ( fileid == -1 ) || ( record->FileId == fileid  ) )
	    && ( ( flavor == -1 ) || ( record->Flavor == flavor ) )
	    && ( ( subsequence == -1 ) || ( record->Subsequence == subsequence ) ) )
	    return record;
    }

    return NULL;
}

CPL_INLINE static void CPL_IGNORE_RET_VAL_SIZET(CPL_UNUSED size_t unused) {}

void SerializeCeosRecordsToFile(Link_t *record_list, VSILFILE *fp)
{
    Link_t *list;
    CeosRecord_t crec;
    unsigned char *Buffer;

    list = record_list;

    while(list != NULL)
    {
	memcpy(&crec,list->object,sizeof(CeosRecord_t));
	Buffer = crec.Buffer;
	crec.Buffer = NULL;
	CPL_IGNORE_RET_VAL_SIZET(VSIFWriteL(&crec,sizeof(CeosRecord_t),1,fp));
	CPL_IGNORE_RET_VAL_SIZET(VSIFWriteL(Buffer,crec.Length,1,fp));
    }
}

void SerializeCeosRecordsFromFile(Link_t *record_list, VSILFILE *fp)
{
    CeosRecord_t *crec;
    Link_t *Link;

    while(!VSIFEofL(fp))
    {
	crec = HMalloc(sizeof(CeosRecord_t));
	CPL_IGNORE_RET_VAL_SIZET(VSIFReadL(crec,sizeof(CeosRecord_t),1,fp));
	crec->Buffer = HMalloc(crec->Length * sizeof(char) );
	CPL_IGNORE_RET_VAL_SIZET(VSIFReadL(crec->Buffer,sizeof(char),crec->Length,fp));
	Link = ceos2CreateLink(crec);
	AddLink(record_list,Link);
    }
}

void CeosUpdateHeaderFromBuffer(CeosRecord_t *record)
{
    if(record && record->Buffer)
    {
	CeosToNative( &( record->Length ), record->Buffer+LENGTH_OFF, sizeof(record->Length ), sizeof( record->Length ) );
	memcpy(&(record->TypeCode.Int32Code),record->Buffer+TYPE_OFF,sizeof(record->TypeCode.Int32Code));
	CeosToNative(&(record->Sequence),record->Buffer+SEQUENCE_OFF,sizeof(record->Sequence ), sizeof( record->Sequence ) );
    }
    if(record)
        record->Subsequence = 0;
}

#ifdef CPL_LSB

static
void swapbyte(void *dst,void *src,size_t toswap)
{
    size_t i,e;
    unsigned char *in = (unsigned char *) src;
    unsigned char *out = (unsigned char *) dst;

    for(i = 0,e=toswap;i < toswap;i++,e--)
    {
	out[i] = in[e-1];
    }
}

void NativeToCeos( void *dst, const void *src, const size_t len, const size_t swapunit)
{
    size_t i;
    size_t l_remainder;
    size_t units;


    l_remainder = len % swapunit;

    units = len - l_remainder;

    for(i = 0;i < units; i += swapunit )
    {
	swapbyte( ( unsigned char *) dst + i, ( unsigned char * ) src + i, swapunit);
    }

    if(l_remainder)
    {
	memcpy( ( unsigned char * ) dst + i, ( unsigned char * ) src + i, l_remainder );
    }
}

#endif
