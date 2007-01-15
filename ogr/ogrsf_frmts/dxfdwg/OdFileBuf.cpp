#include <stdlib.h>
#include "OdaCommon.h"
#include "OdFileBuf.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

OdBaseFileBuf::OdBaseFileBuf()
    : m_fp(0)
	  , m_FileName("")
    , m_length(ERR_VAL)
    , m_shMode(Oda::kShareDenyNo)
{}

void OdBaseFileBuf::close()
{
  m_length = ERR_VAL;
  m_FileName = "";
  if (m_fp)
  {
    if (fclose(m_fp) != 0)
		{
      ODA_FAIL(); // eFileCloseError;
		}
    m_fp = 0;
  }
}

#ifdef _WIN32_WCE
#define _IOFBF          0x0000
#endif

void OdBaseFileBuf::open(const OdChar *path, const char *access)
{
  m_fp = fopen(path, access);
  if (m_fp)
  {
    setvbuf(m_fp, 0, _IOFBF, 8192);
    m_FileName = path;
  }
  else
    throw OdError_CantOpenFile(path);
}


////////////////////////////////////////////////////////////////////
OdWrFileBuf::OdWrFileBuf()
    : m_position(ERR_VAL)
  {}

OdWrFileBuf::~OdWrFileBuf()
{
  close();
}


void OdWrFileBuf::open(
  const OdChar* path, 
  Oda::FileShareMode /*shMode*/, 
  Oda::FileAccessMode /*nDesiredAccess*/, 
  Oda::FileCreationDisposition /*nCreationDisposition*/)
{
  OdString  sMode = "wb";

  //if (shMode == Oda::kShareDenyReadWrite)

  m_position = 0;
  m_length = 0;
  OdBaseFileBuf::open(path, "wb");
  return;
}

void OdWrFileBuf::close()
{
  m_position = ERR_VAL;
  OdBaseFileBuf::close();
}

OdUInt32 OdWrFileBuf::length()
{
  return m_length;
}


OdUInt32 OdWrFileBuf::seek(OdInt32 offset, OdDb::FilerSeekType whence)
{
  switch (whence) {
  case OdDb::kSeekFromStart:
    m_position = offset;
    break;
  case OdDb::kSeekFromCurrent:
    m_position += offset;
    break;
  case OdDb::kSeekFromEnd:
    m_position = m_length - offset;
    break;
  }
  if (fseek(m_fp, m_position, SEEK_SET) != 0)
    m_position = ERR_VAL;  // Error
  return m_position;
}

OdUInt32 OdWrFileBuf::tell()
{
  return m_position;
}

bool OdWrFileBuf::isEof()
{
  return (m_position >= m_length);
}

OdUInt32 OdWrFileBuf::getShareMode()
{
  return (OdUInt32)m_shMode;
}

void OdWrFileBuf::putByte(OdUInt8 val)
{
	if(::fputc(val,m_fp)==EOF)
	{
    throw OdError_FileWriteError(m_FileName);
	}
  if (++m_position > m_length)
    m_length = m_position;
}

void OdWrFileBuf::putBytes(const void* buff, OdUInt32 nByteLen)
{
	if(::fwrite(buff, 1, nByteLen, m_fp) < nByteLen)
	{
    throw OdError_FileWriteError(m_FileName);
	}
  m_position += nByteLen;
  if (m_position > m_length)
    m_length = m_position;
}


////////////////////////////////////////////////////////////////////////////////
const int OdRdFileBuf::m_BufSize = 8192;
const int OdRdFileBuf::m_PosMask(~(8192-1));


OdRdFileBuf::OdRdFileBuf()
    : m_Counter(0L)
{
  init();
}

void OdRdFileBuf::init()
{
   for (int i = 0; i < NUM_BUFFERS; i++)
  {
    m_DataBlock[i].buf = NULL;
    m_DataBlock[i].counter = -1L;
    m_DataBlock[i].validbytes=0;
    m_DataBlock[i].startaddr = ERR_VAL;
  }
}


OdRdFileBuf::~OdRdFileBuf()
{
  close();
}

void OdRdFileBuf::close()
{
  // indicate buffers no longer in use
  for (int i = 0; i < NUM_BUFFERS; i++)
  {
    if (m_DataBlock[i].buf)
      ::odrxFree(m_DataBlock[i].buf);
    m_DataBlock[i].buf = NULL;
    m_DataBlock[i].counter = -1L;
    m_DataBlock[i].validbytes=0;
    m_DataBlock[i].startaddr = ERR_VAL;
  }
  OdBaseFileBuf::close();
}

void OdRdFileBuf::open(
  const OdChar * fname, 
  Oda::FileShareMode shMode,
  Oda::FileAccessMode /*nDesiredAccess*/, 
  Oda::FileCreationDisposition /*nCreationDisposition*/)
{
  OdString caMode;

  if (shMode == Oda::kShareDenyWrite || shMode == Oda::kShareDenyReadWrite)
    caMode = "r+b";
  else 
    caMode = "rb";

  OdBaseFileBuf::open(fname, caMode.c_str());

  // Get file length
  OdUInt32 curLoc = ftell(m_fp);
	fseek(m_fp, 0L, 2);
	m_length = ftell(m_fp);
  fseek(m_fp, curLoc, 0);

  int i;

  m_BufBytes= 0;
  m_BytesLeft= 0;
  m_BufPos=0L;  // to start reads from 0L
  m_pCurBuf=NULL;
  m_pNextChar = m_pCurBuf;
  m_UsingBlock = -1;
  m_PhysFilePos=0L;

  for (i = 0; i < NUM_BUFFERS; i++)
  {
    m_DataBlock[i].buf = (OdUInt8*)::odrxAlloc(m_BufSize);
    m_DataBlock[i].validbytes=0;
    m_DataBlock[i].counter = -1L;
    m_DataBlock[i].startaddr = ERR_VAL;
  }
  seek(0L, OdDb::kSeekFromStart);  // initial seek, gets a buffer & stuff
  return;
}

bool OdRdFileBuf::filbuf( )
{
  int i, minindex;
  OdInt32 minlru;
  struct blockstru *minptr;

  m_UsingBlock = -1;
  /* see if we are holding it already */
  for (i = 0; i < NUM_BUFFERS; i++)
  {
    if (m_DataBlock[i].startaddr==m_BufPos)
      break;
  }

  if (i < NUM_BUFFERS)   // we are already holding this part of the file
  {
    m_pCurBuf=m_DataBlock[i].buf;
    m_BufPos=m_DataBlock[i].startaddr;
    m_BytesLeft=m_BufBytes=m_DataBlock[i].validbytes;
    m_pNextChar=m_pCurBuf;
    m_DataBlock[i].counter=m_Counter++;
    m_UsingBlock=i;
    return true;
  }

  /* not holding it, so look for a buffer to read into */
  /* first see if any are not yet loaded */
  minptr=NULL;
  minindex=0;

  for (i = 0; i < NUM_BUFFERS; i++)
  {
    if (m_DataBlock[i].startaddr==ERR_VAL)
    {
      minindex=i;
      minptr=&m_DataBlock[i];
      break;
    }
  }

  /* if all were used, then look for the least recently used one */
  if (minptr==NULL)
  {
    minlru=0x7FFFFFFF;
    minptr=NULL;
    minindex=0;

    for (i = 0; i < NUM_BUFFERS; i++) {
      if (m_DataBlock[i].counter<0L)
        m_DataBlock[i].counter=0L;
      if (m_DataBlock[i].counter<minlru) {
        minlru=m_DataBlock[i].counter;
        minptr=&m_DataBlock[i];
        minindex=i;
      }
    }
  }

  if (minptr==NULL)
  {
    ODA_FAIL();
    return false;  /* couldn't find one */
  }
  /* if we are not already physically at the read location, move there */
  /* then read into the buffer */
  if (m_PhysFilePos!=m_BufPos /*|| readerror*/)
  {
    fseek(m_fp, m_BufPos, SEEK_SET);
  }
  m_BufBytes = m_BytesLeft =
    (short) fread(minptr->buf, 1, m_BufSize, m_fp);
  m_PhysFilePos=m_BufPos+m_BufBytes;
  if (m_BufBytes > 0)
  {
    minptr->validbytes = m_BufBytes;
    minptr->startaddr = m_BufPos;
    minptr->counter=m_Counter++;
    m_pCurBuf=minptr->buf;
    m_pNextChar = m_pCurBuf;
    m_UsingBlock=minindex;
    return true;
  }
  return false;
}

OdUInt32 OdRdFileBuf::getShareMode()
{
  return (OdUInt32)m_shMode;
}

OdUInt32 OdRdFileBuf::length()
{
  return m_length;
}


OdUInt32 OdRdFileBuf::seek(OdInt32 offset, OdDb::FilerSeekType whence)
{
  int bytestoadvance;

  switch (whence)
  {
  case OdDb::kSeekFromStart:
    break;
  case OdDb::kSeekFromCurrent:
    offset += (m_BufPos+(m_pNextChar - m_pCurBuf));
    break;
  case OdDb::kSeekFromEnd:
    offset = m_length - offset;
    break;
  }

  ODA_ASSERT(offset >= 0);
  // from here on assume whence is 0
  // if it's not in the area we're holding, seek to it
  if (OdUInt32(offset) < m_BufPos || OdUInt32(offset) >= m_BufPos + m_BufBytes)
  {
    m_BufPos=offset & m_PosMask;
    if (!filbuf( )) // locates it if we're already holding in another block
    {
      m_pNextChar = NULL;
      m_pCurBuf = NULL;
      m_BytesLeft = 0;
      throw OdError(eEndOfFile);
      //return ERR_VAL;
    }
  }
  m_pNextChar = (m_pCurBuf + (bytestoadvance=(OdUInt16)(offset - m_BufPos)));
  m_BytesLeft = m_BufBytes - bytestoadvance;
  return(offset);
}

OdUInt32 OdRdFileBuf::tell()
{
  return (m_BufPos + (m_pNextChar - m_pCurBuf));
}


bool OdRdFileBuf::isEof()
{
  if (m_BytesLeft > 0)
    return false;
  if (m_length == 0)
    return true;
  m_BufPos += m_BufBytes;
  return !filbuf();
}


OdUInt8 OdRdFileBuf::getByte()
{
  m_DataBlock[m_UsingBlock].counter=m_Counter++;
  if (m_BytesLeft<=0) {
    m_BufPos+=m_BufBytes;
    if (!filbuf())
    {
      throw OdError(eEndOfFile);
    }
  }
  m_BytesLeft--;
  return (*m_pNextChar++);
}

void OdRdFileBuf::getBytes(void* buffer, OdUInt32 nLen)
{
  OdInt32 bytesleft;
  OdUInt16 bytestoread;
  unsigned char *buf=(unsigned char *)buffer;

  if (nLen > 0)
  {
    m_DataBlock[m_UsingBlock].counter=m_Counter++;
    bytesleft = nLen;

    while (bytesleft > 0L && !isEof( )) {
      if ((OdInt32)m_BytesLeft<bytesleft) bytestoread=(OdUInt16)m_BytesLeft;
      else bytestoread=(OdUInt16)bytesleft;

      memcpy(buf,m_pNextChar,bytestoread);
      m_BytesLeft -= bytestoread;
      m_pNextChar += bytestoread;
      buf += bytestoread;
      bytesleft -= bytestoread;
    }
    if (bytesleft > 0L)
      throw OdError(eEndOfFile);
  }
}
