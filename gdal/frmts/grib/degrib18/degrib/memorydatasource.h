#ifndef MEMORYDATASOURCE_H
#define MEMORYDATASOURCE_H

#include "datasource.h"
#include <stdio.h>

class MemoryDataSource : public DataSource
{
public:
	MemoryDataSource(unsigned char * block, long length);
	virtual ~MemoryDataSource();
	virtual size_t DataSourceFread(void* lpBuf, size_t size, size_t count);
	virtual int DataSourceFgetc();
	virtual int DataSourceUngetc(int c);
	virtual int DataSourceFseek(long offset, int origin);
	virtual int DataSourceFeof();
	virtual long DataSourceFtell();
private:
	long seekPos;
	long blockLength;
	bool eof;
	unsigned char * memoryBlock;
};

#endif /* MEMORYDATASOURCE_H */
