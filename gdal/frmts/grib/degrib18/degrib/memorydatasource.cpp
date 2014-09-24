#include "memorydatasource.h"
#include <memory.h>
#include <string.h>

MemoryDataSource::MemoryDataSource(unsigned char * block, long length)
: seekPos(0)
, blockLength(length)
, eof(false)
, memoryBlock(block)
{
}

MemoryDataSource::~MemoryDataSource()
{
}

size_t MemoryDataSource::DataSourceFread(void* lpBuf, size_t size, size_t count)
{
        if (seekPos + size * count > (size_t) blockLength)
	{
		count = (blockLength - seekPos) / size;
		eof = true;
	}
	else
		eof = false; // feof also "resets" after a good read

	memcpy(lpBuf, memoryBlock + seekPos, size * count);
	seekPos += size * count;

	return count;

}

int MemoryDataSource::DataSourceFgetc()
{
	int returnVal = EOF;
	if (seekPos >= blockLength)
		eof = true;
	else
	{
		unsigned char c =  *((unsigned char*)(memoryBlock + seekPos));
		++seekPos;
		returnVal = (int)c;
		eof = false;
	}
	return returnVal;		
}

int MemoryDataSource::DataSourceUngetc(int c)
{
	eof = false;
	int returnVal = c;
	if ((c != EOF) && (seekPos > 0))
	{
		--seekPos;
		*((unsigned char*)(memoryBlock + seekPos)) = c;
	}
	else
		returnVal = EOF;
	return returnVal;
}

int MemoryDataSource::DataSourceFseek(long offset, int origin)
{
	switch (origin)
	{
		case SEEK_CUR :
			seekPos += offset;
			break;
		case SEEK_END :
			seekPos = blockLength + offset; // offset MUST be negative
			break;
		case SEEK_SET :
			seekPos = offset;
			break;
	}

	eof = false;

	return 0;
}

int MemoryDataSource::DataSourceFeof()
{
	return eof;	
}

long MemoryDataSource::DataSourceFtell()
{
	return seekPos;
}
