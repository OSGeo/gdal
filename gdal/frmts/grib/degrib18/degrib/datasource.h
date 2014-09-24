#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <stdio.h>

class DataSource
{
public:
        virtual ~DataSource() {}
	virtual size_t DataSourceFread(void* lpBuf, size_t size, size_t count) = 0;
	virtual int DataSourceFgetc() = 0;
	virtual int DataSourceUngetc(int c) = 0;
	virtual int DataSourceFseek(long offset, int origin) = 0;
	virtual int DataSourceFeof() = 0;
	virtual long DataSourceFtell() = 0;
};

#endif /* DATASOURCE_H */
