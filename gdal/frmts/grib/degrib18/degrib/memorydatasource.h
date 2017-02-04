#ifndef MEMORYDATASOURCE_H
#define MEMORYDATASOURCE_H

#include "datasource.h"
#include <stdio.h>

// Compatibility hack for non-C++11 compilers
#if !(__cplusplus >= 201103L || _MSC_VER >= 1500)
#define override
#endif

class MemoryDataSource : public DataSource
{
public:
        MemoryDataSource(unsigned char * block, long length);
        virtual ~MemoryDataSource();
        virtual size_t DataSourceFread(void* lpBuf, size_t size, size_t count) override;
        virtual int DataSourceFgetc() override;
        virtual int DataSourceUngetc(int c) override;
        virtual int DataSourceFseek(long offset, int origin) override;
        virtual int DataSourceFeof() override;
        virtual long DataSourceFtell() override;
private:
        long seekPos;
        long blockLength;
        bool eof;
        unsigned char * memoryBlock;
};

#endif /* MEMORYDATASOURCE_H */
