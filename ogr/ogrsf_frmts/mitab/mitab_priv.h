/**********************************************************************
 * $Id: mitab_priv.h,v 1.11 1999/11/14 04:43:56 daniel Exp $
 *
 * Name:     mitab_priv.h
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Header file containing private definitions for the library.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Daniel Morissette
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_priv.h,v $
 * Revision 1.11  1999/11/14 04:43:56  daniel
 * Support dataset with no .MAP/.ID files
 *
 * Revision 1.10  1999/11/11 01:22:05  stephane
 * Remove DebugFeature call, Point Reading error, add IsValidFeature() to test correctly if we are on a feature
 *
 * Revision 1.9  1999/11/09 07:37:22  daniel
 * Support for deleted records when reading TABDATFiles
 *
 * Revision 1.8  1999/11/08 04:34:54  stephane
 * mid/mif support
 *
 * Revision 1.7  1999/10/18 15:40:27  daniel
 * Added TABMAPObjectBlock::WriteIntMBRCoord()
 *
 * Revision 1.6  1999/10/01 03:45:27  daniel
 * Small modifs for tuning of write mode
 *
 * Revision 1.5  1999/09/28 02:53:09  warmerda
 * Removed nMIDatumID.
 *
 * Revision 1.4  1999/09/26 14:59:37  daniel
 * Implemented write support
 *
 * Revision 1.3  1999/09/23 19:50:12  warmerda
 * added nMIDatumId
 *
 * Revision 1.2  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#ifndef _MITAB_PRIV_H_INCLUDED_
#define _MITAB_PRIV_H_INCLUDED_

#include "cpl_conv.h"
#include "cpl_string.h"

class TABFeature;
class TABMAPToolBlock;
class TABMAPIndexBlock;

/*---------------------------------------------------------------------
 * Access mode: Read or Write
 *--------------------------------------------------------------------*/
typedef enum
{
    TABRead,
    TABWrite,
    TABReadWrite  /* ReadWrite not implemented yet */
} TABAccess;

/*---------------------------------------------------------------------
 * Supported .MAP block types (the first byte at the beginning of a block)
 *--------------------------------------------------------------------*/
#define TAB_RAWBIN_BLOCK        -1
#define TABMAP_HEADER_BLOCK     0
#define TABMAP_INDEX_BLOCK      1
#define TABMAP_OBJECT_BLOCK     2
#define TABMAP_COORD_BLOCK      3
#define TABMAP_GARB_BLOCK       4
#define TABMAP_TOOL_BLOCK       5
#define TABMAP_LAST_VALID_BLOCK_TYPE  5


/*---------------------------------------------------------------------
 * struct TABMAPIndexEntry - Entries found in type 1 blocks of .MAP files
 *
 * We will use this struct to rebuild the geographic index in memory
 *--------------------------------------------------------------------*/

typedef struct TABMAPIndexEntry_t
{
    // These members refer to the info we find in the file
    GInt32      XMin;
    GInt32      YMin;
    GInt32      XMax;
    GInt32      YMax;
    GInt32      nBlockPtr;

    // Ptr to the actual index block or NULL if not loaded
    TABMAPIndexBlock *poBlock;  
}TABMAPIndexEntry;


/*---------------------------------------------------------------------
 * TABVertex 
 *--------------------------------------------------------------------*/
typedef struct TABVertex_t
{
    double x;
    double y;
} TABVertex;

/*---------------------------------------------------------------------
 * TABFieldType - Native MapInfo attribute field types
 *--------------------------------------------------------------------*/
typedef enum
{
    TABFUnknown = 0,
    TABFChar,
    TABFInteger,
    TABFSmallInt,
    TABFDecimal,
    TABFFloat,
    TABFDate,
    TABFLogical
} TABFieldType;

/*---------------------------------------------------------------------
 * TABDATFieldDef
 *--------------------------------------------------------------------*/
typedef struct TABDATFieldDef_t
{
    char        szName[11];
    char        cType;
    GByte       byLength;
    GByte       byDecimals;

    TABFieldType eTABType;
} TABDATFieldDef;

/*---------------------------------------------------------------------
 * TABMAPCoordSecHdr
 * struct used in the TABMAPCoordBlock to store info about the coordinates
 * for a section of a PLINE MULTIPLE or a REGION.
 *--------------------------------------------------------------------*/
typedef struct TABMAPCoordSecHdr_t
{
    GInt16      numVertices;
    GInt16      numHoles;
    GInt32      nXMin;
    GInt32      nYMin;
    GInt32      nXMax;
    GInt32      nYMax;

    GInt32      nDataOffset;
    int         nVertexOffset;
} TABMAPCoordSecHdr;

/*---------------------------------------------------------------------
 * TABProjInfo
 * struct used to store the projection parameters from the .MAP header
 *--------------------------------------------------------------------*/
typedef struct TABProjInfo_t
{
    GByte       nProjId;           // See MapInfo Ref. Manual, App. F and G
    GByte       nEllipsoidId;
    GByte       nUnitsId;
    double      adProjParams[6];   // params in same order as in .MIF COORDSYS

    double      dDatumShiftX;
    double      dDatumShiftY;
    double      dDatumShiftZ;
    double      adDatumParams[5];

} TABProjInfo;


/*---------------------------------------------------------------------
 * TABPenDef - Pen definition information
 *--------------------------------------------------------------------*/
typedef struct TABPenDef_t
{
    GInt32      nRefCount;
    GByte       nLineWidth;
    GByte       nLinePattern;
    GByte       nLineStyle;
    GInt32      rgbColor;
} TABPenDef;

/*---------------------------------------------------------------------
 * TABBrushDef - Brush definition information
 *--------------------------------------------------------------------*/
typedef struct TABBrushDef_t
{
    GInt32      nRefCount;
    GByte       nFillPattern;
    GByte       bTransparentFill; // 1 = Transparent
    GInt32      rgbFGColor;
    GInt32      rgbBGColor;
} TABBrushDef;

/*---------------------------------------------------------------------
 * TABFontDef - Font Name information
 *--------------------------------------------------------------------*/
typedef struct TABFontDef_t
{
    GInt32      nRefCount;
    char        szFontName[33];
} TABFontDef;

/*---------------------------------------------------------------------
 * TABSymbolDef - Symbol definition information
 *--------------------------------------------------------------------*/
typedef struct TABSymbolDef_t
{
    GInt32      nRefCount;
    GInt16      nSymbolNo;
    GInt16      nPointSize;
    GByte       _nUnknownValue_;// Style???
    GInt32      rgbColor;
} TABSymbolDef;


/*---------------------------------------------------------------------
 *                      class TABToolDefTable
 *
 * Class to handle the list of Drawing Tool Definitions for a dataset
 *
 * This class also contains methods to read tool defs from the file and
 * write them to the file.
 *--------------------------------------------------------------------*/

class TABToolDefTable
{
  protected:
    TABPenDef   **m_papsPen;
    int         m_numPen;
    int         m_numAllocatedPen;
    TABBrushDef **m_papsBrush;
    int         m_numBrushes;
    int         m_numAllocatedBrushes;
    TABFontDef  **m_papsFont;
    int         m_numFonts;
    int         m_numAllocatedFonts;
    TABSymbolDef **m_papsSymbol;
    int         m_numSymbols;
    int         m_numAllocatedSymbols;

  public:
    TABToolDefTable();
    ~TABToolDefTable();

    int         ReadAllToolDefs(TABMAPToolBlock *poToolBlock);
    int         WriteAllToolDefs(TABMAPToolBlock *poToolBlock);

    TABPenDef   *GetPenDefRef(int nIndex);
    int         AddPenDefRef(TABPenDef *poPenDef);
    int         GetNumPen();

    TABBrushDef *GetBrushDefRef(int nIndex);
    int         AddBrushDefRef(TABBrushDef *poBrushDef);
    int         GetNumBrushes();

    TABFontDef  *GetFontDefRef(int nIndex);
    int         AddFontDefRef(TABFontDef *poFontDef);
    int         GetNumFonts();

    TABSymbolDef *GetSymbolDefRef(int nIndex);
    int         AddSymbolDefRef(TABSymbolDef *poSymbolDef);
    int         GetNumSymbols();

};



/*=====================================================================
          Classes to handle .MAP files low-level blocks
 =====================================================================*/

/*---------------------------------------------------------------------
 *                      class TABMAPBlockManager
 *
 * This class is used to keep track of allocated blocks and is used
 * by various classes that need to allocate a new block in a .MAP file.
 *--------------------------------------------------------------------*/
class TABMAPBlockManager
{
  protected:
    int         m_nBlockSize;
    GInt32      m_nLastAllocatedBlock;
  public:
    TABMAPBlockManager(int nBlockSize=512) {m_nBlockSize=nBlockSize;
                                            m_nLastAllocatedBlock = -1; };
    ~TABMAPBlockManager()  {};

    GInt32      AllocNewBlock()   {if (m_nLastAllocatedBlock==-1)
                                        m_nLastAllocatedBlock = 0;
                                   else
                                        m_nLastAllocatedBlock+=m_nBlockSize;
                                   return m_nLastAllocatedBlock; };
    void        Reset()  {m_nLastAllocatedBlock=-1; };
};

/*---------------------------------------------------------------------
 *                      class TABRawBinBlock
 *
 * This is the base class used for all other data block types... it 
 * contains all the base functions to handle binary data.
 *--------------------------------------------------------------------*/

class TABRawBinBlock
{
  protected:
    FILE        *m_fp;          /* Associated file handle               */
    TABAccess   m_eAccess;      /* Read/Write access mode               */

    int         m_nBlockType;

    GByte       *m_pabyBuf;     /* Buffer to contain the block's data    */
    int         m_nBlockSize;   /* Size of current block (and buffer)    */
    int         m_nSizeUsed;    /* Number of bytes used in buffer        */
    GBool       m_bHardBlockSize;/* TRUE=Blocks MUST always be nSize bytes  */
                                 /* FALSE=last block may be less than nSize */
    int         m_nFileOffset;  /* Location of current block in the file */
    int         m_nCurPos;      /* Next byte to read from m_pabyBuf[]    */
    int         m_nFirstBlockPtr;/* Size of file header when different from */
                                 /* block size (used by GotoByteInFile())   */

  public:
    TABRawBinBlock(TABAccess eAccessMode = TABRead,
                   GBool bHardBlockSize = TRUE);
    virtual ~TABRawBinBlock();

    virtual int ReadFromFile(FILE *fpSrc, int nOffset, int nSize = 512);
    virtual int CommitToFile();

    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);

    int         GetBlockType();
    virtual int GetBlockClass() { return TAB_RAWBIN_BLOCK; };

    GInt32      GetStartAddress() {return m_nFileOffset;};
#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

    int         GotoByteRel(int nOffset);
    int         GotoByteInBlock(int nOffset);
    int         GotoByteInFile(int nOffset);
    void        SetFirstBlockPtr(int nOffset);

    int         GetNumUnusedBytes();
    int         GetFirstUnusedByteOffset();
    int         GetCurAddress();

    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    GByte       ReadByte();
    GInt16      ReadInt16();
    GInt32      ReadInt32();
    float       ReadFloat();
    double      ReadDouble();

    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);
    int         WriteByte(GByte byValue);
    int         WriteInt16(GInt16 n16Value);
    int         WriteInt32(GInt32 n32Value);
    int         WriteFloat(float fValue);
    int         WriteDouble(double dValue);
    int         WriteZeros(int nBytesToWrite);
    int         WritePaddedString(int nFieldSize, const char *pszString);
};


/*---------------------------------------------------------------------
 *                      class TABMAPHeaderBlock
 *
 * Class to handle Read/Write operation on .MAP Header Blocks 
 *--------------------------------------------------------------------*/

class TABMAPHeaderBlock: public TABRawBinBlock
{
  protected:
    TABProjInfo m_sProj;

  public:
    TABMAPHeaderBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPHeaderBlock();

    virtual int CommitToFile();

    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);

    virtual int GetBlockClass() { return TABMAP_HEADER_BLOCK; };

    int         Int2Coordsys(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2Int(double dX, double dY, GInt32 &nX, GInt32 &nY);
    int         ComprInt2Coordsys(GInt32 nCenterX, GInt32 nCenterY, 
                                  int nDeltaX, int nDeltaY, 
                                  double &dX, double &dY);
    int         Int2CoordsysDist(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2IntDist(double dX, double dY, GInt32 &nX, GInt32 &nY);
    int         SetCoordsysBounds(double dXMin, double dYMin, 
                                  double dXMax, double dYMax);

    int         GetMapObjectSize(int nObjType);
    GBool       MapObjectUsesCoordBlock(int nObjType);

    int         GetProjInfo(TABProjInfo *psProjInfo);
    int         SetProjInfo(TABProjInfo *psProjInfo);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

    // Instead of having over 30 get/set methods, we'll make all data 
    // members public and we will initialize them in the overloaded
    // LoadFromFile().  For this reason, this class should be used with care.

    GInt16      m_nVersionNumber;
    GInt16      m_nBlockSize;
    
    double      m_dCoordsys2DistUnits;
    GInt32      m_nXMin;
    GInt32      m_nYMin;
    GInt32      m_nXMax;
    GInt32      m_nYMax;

    GInt32      m_nFirstIndexBlock;
    GInt32      m_nFirstGarbageBlock;
    GInt32      m_nFirstToolBlock;
    GInt32      m_numPointObjects;
    GInt32      m_numLineObjects;
    GInt32      m_numRegionObjects;
    GInt32      m_numTextObjects;
    GInt32      m_nMaxCoordBufSize;

    GByte       m_nDistUnitsCode;       // See Appendix F
    GByte       m_nMaxSpIndexDepth;
    GByte       m_nCoordPrecision;      // Num. decimal places on coord.
    GByte       m_nCoordOriginQuadrant;
    GByte       m_nReflectXAxisCoord;
    GByte       m_nMaxObjLenArrayId;     // See gabyObjLenArray[]
    GByte       m_numPenDefs;
    GByte       m_numBrushDefs;
    GByte       m_numSymbolDefs;
    GByte       m_numFontDefs;
    GInt16      m_numMapToolBlocks;

    double      m_XScale;
    double      m_YScale;
    double      m_XDispl;
    double      m_YDispl;
    
};

/*---------------------------------------------------------------------
 *                      class TABMAPIndexBlock
 *
 * Class to handle Read/Write operation on .MAP Index Blocks (Type 01)
 *--------------------------------------------------------------------*/

class TABMAPIndexBlock: public TABRawBinBlock
{
  protected:
    int         m_numEntries;
    TABMAPIndexEntry **m_papsEntries;

    int         ReadNextEntry(TABMAPIndexEntry *psEntry);
    int         WriteNextEntry(TABMAPIndexEntry *psEntry);

    // Use these to keep track of current block's MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    int         ReadAllEntries();

  public:
    TABMAPIndexBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPIndexBlock();

    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_INDEX_BLOCK; };

    int         GetNumFreeEntries();
    int         AddEntry(GInt32 XMin, GInt32 YMin,
                         GInt32 XMax, GInt32 YMax,
                         GInt32 nBlockPtr, TABMAPIndexBlock *poBlock=NULL);
    int         GetMaxDepth();
    void        GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                       GInt32 &nXMax, GInt32 &nYMax);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPObjectBlock
 *
 * Class to handle Read/Write operation on .MAP Object data Blocks (Type 02)
 *--------------------------------------------------------------------*/

class TABMAPObjectBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 4 bytes header */
    GInt32      m_nFirstCoordBlock;
    GInt32      m_nLastCoordBlock;
    GInt32      m_nCenterX;
    GInt32      m_nCenterY;

    // In order to compute block center, we need to keep track of MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

  public:
    TABMAPObjectBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPObjectBlock();

    virtual int CommitToFile();
    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);

    virtual int GetBlockClass() { return TABMAP_OBJECT_BLOCK; };

    virtual int ReadIntCoord(GBool bCompressed, GInt32 &nX, GInt32 &nY);
    int         WriteIntCoord(GInt32 nX, GInt32 nY, GBool bUpdateMBR=TRUE);
    int         WriteIntMBRCoord(GInt32 nXMin, GInt32 nYMin,
                                 GInt32 nXMax, GInt32 nYMax,
                                 GBool bUpdateMBR =TRUE);

    void        AddCoordBlockRef(GInt32 nCoordBlockAddress);

    void        GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                       GInt32 &nXMax, GInt32 &nYMax);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPCoordBlock
 *
 * Class to handle Read/Write operation on .MAP Coordinate Blocks (Type 03)
 *--------------------------------------------------------------------*/

class TABMAPCoordBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 8 bytes header */
    GInt32      m_nNextCoordBlock;
    int         m_numBlocksInChain;

    GInt32      m_nCenterX;
    GInt32      m_nCenterY;

    // In order to compute block center, we need to keep track of MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    TABMAPBlockManager *m_poBlockManagerRef;

    int         m_nTotalDataSize;       // Num bytes in whole chain of blocks
    int         m_nFeatureDataSize;     // Num bytes for current feature coords
    
    GInt32      m_nFeatureXMin;         // Used to keep track of current 
    GInt32      m_nFeatureYMin;         // feature MBR.
    GInt32      m_nFeatureXMax;
    GInt32      m_nFeatureYMax;

  public:
    TABMAPCoordBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPCoordBlock();

    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_COORD_BLOCK; };

    void        SetMAPBlockManagerRef(TABMAPBlockManager *poBlockManager);
    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);
    void        SetComprCoordOrigin(GInt32 nX, GInt32 nY);
    int         ReadIntCoord(GBool bCompressed, GInt32 &nX, GInt32 &nY);
    int         ReadIntCoords(GBool bCompressed, int numCoords, GInt32 *panXY);
    int         ReadCoordSecHdrs(GBool bCompressed, int numSections,
                                 TABMAPCoordSecHdr *pasHdrs,
                                 int    &numVerticesTotal);
    int         WriteCoordSecHdrs(int numSections, TABMAPCoordSecHdr *pasHdrs);

    void        SetNextCoordBlock(GInt32 nNextCoordBlockAddress);
    int         WriteIntCoord(GInt32 nX, GInt32 nY, GBool bUpdateMBR=TRUE);

    int         GetNumBlocksInChain() { return m_numBlocksInChain; };

    void        ResetTotalDataSize() {m_nTotalDataSize = 0;};
    int         GetTotalDataSize() {return m_nTotalDataSize;};

    void        StartNewFeature();
    int         GetFeatureDataSize() {return m_nFeatureDataSize;};
    void        GetFeatureMBR(GInt32 &nXMin, GInt32 &nYMin, 
                              GInt32 &nXMax, GInt32 &nYMax);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPToolBlock
 *
 * Class to handle Read/Write operation on .MAP Drawing Tool Blocks (Type 05)
 *
 * In addition to handling the I/O, this class also maintains the list 
 * of Tool definitions in memory.
 *--------------------------------------------------------------------*/

class TABMAPToolBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 8 bytes header */
    GInt32      m_nNextToolBlock;
    int         m_numBlocksInChain;

    TABMAPBlockManager *m_poBlockManagerRef;

  public:
    TABMAPToolBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPToolBlock();

    virtual int InitBlockFromData(GByte *pabyBuf, int nSize, 
                              GBool bMakeCopy = TRUE,
                              FILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(FILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_TOOL_BLOCK; };

    void        SetMAPBlockManagerRef(TABMAPBlockManager *poBlockManager);
    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);

    void        SetNextToolBlock(GInt32 nNextCoordBlockAddress);

    GBool       EndOfChain();
    int         GetNumBlocksInChain() { return m_numBlocksInChain; };

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};


/*=====================================================================
       Classes to deal with .MAP files at the MapInfo object level
 =====================================================================*/

/*---------------------------------------------------------------------
 *                      class TABIDFile
 *
 * Class to handle Read/Write operation on .ID files... the .ID file
 * contains an index to the objects in the .MAP file by object id.
 *--------------------------------------------------------------------*/

class TABIDFile
{
  private:
    char        *m_pszFname;
    FILE        *m_fp;
    TABAccess   m_eAccessMode;

    TABRawBinBlock *m_poIDBlock;
    int         m_nBlockSize;
    GInt32      m_nMaxId;

   public:
    TABIDFile();
    ~TABIDFile();

    int         Open(const char *pszFname, const char *pszAccess);
    int         Close();

    GInt32      GetObjPtr(GInt32 nObjId);
    int         SetObjPtr(GInt32 nObjId, GInt32 nObjPtr);
    GInt32      GetMaxObjId();

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPFile
 *
 * Class to handle Read/Write operation on .MAP files... this class hides
 * all the dealings with blocks, indexes, etc.
 * Use this class to deal with MapInfo objects directly.
 *--------------------------------------------------------------------*/

class TABMAPFile
{
  private:
    char        *m_pszFname;
    FILE        *m_fp;
    TABAccess   m_eAccessMode;

    TABMAPBlockManager m_oBlockManager;

    TABMAPHeaderBlock   *m_poHeader;

    // Members used to access objects using the spatial index
    TABMAPIndexBlock  *m_poSpIndex;

    // Member used to access objects using the object ids (.ID file)
    TABIDFile   *m_poIdIndex;

    // Current object data block.
    TABMAPObjectBlock *m_poCurObjBlock;
    int         m_nCurObjPtr;
    int         m_nCurObjType;
    int         m_nCurObjId;
    TABMAPCoordBlock *m_poCurCoordBlock;

    // Drawing Tool Def. table (takes care of all drawing tools in memory)
    TABToolDefTable *m_poToolDefTable;

    // Coordinates filter... default is MBR of the whole file
    TABVertex   m_sMinFilter;
    TABVertex   m_sMaxFilter;
    GInt32      m_XMinFilter;
    GInt32      m_YMinFilter;
    GInt32      m_XMaxFilter;
    GInt32      m_YMaxFilter;

    int         CommitObjBlock(GBool bInitNewBlock =TRUE);

    int         InitDrawingTools();
    int         CommitDrawingTools();

    int         CommitSpatialIndex();

  public:
    TABMAPFile();
    ~TABMAPFile();

    int         Open(const char *pszFname, const char *pszAccess,
                     GBool bNoErrorMsg = FALSE );
    int         Close();

    int         Int2Coordsys(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2Int(double dX, double dY, GInt32 &nX, GInt32 &nY);
    int         Int2CoordsysDist(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2IntDist(double dX, double dY, GInt32 &nX, GInt32 &nY);
    int         SetCoordFilter(TABVertex &sMin, TABVertex &sMax);
    int         SetCoordsysBounds(double dXMin, double dYMin, 
                                  double dXMax, double dYMax);

    GInt32      GetMaxObjId();
    int         MoveToObjId(int nObjId);
    int         PrepareNewObj(int nObjId, GByte nObjType);

    int         GetCurObjType();
    int         GetCurObjId();
    TABMAPObjectBlock *GetCurObjBlock();
    TABMAPCoordBlock  *GetCurCoordBlock();
    TABMAPCoordBlock  *GetCoordBlock(int nFileOffset);
    TABMAPHeaderBlock *GetHeaderBlock();
    TABIDFile         *GetIDFileRef();

    int         ReadPenDef(int nPenIndex, TABPenDef *psDef);
    int         ReadBrushDef(int nBrushIndex, TABBrushDef *psDef);
    int         ReadFontDef(int nFontIndex, TABFontDef *psDef);
    int         ReadSymbolDef(int nSymbolIndex, TABSymbolDef *psDef);
    int         WritePenDef(TABPenDef *psDef);
    int         WriteBrushDef(TABBrushDef *psDef);
    int         WriteFontDef(TABFontDef *psDef);
    int         WriteSymbolDef(TABSymbolDef *psDef);

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};


/*---------------------------------------------------------------------
 *                      class TABDATFile
 *
 * Class to handle Read/Write operation on .DAT files... the .DAT file
 * contains the table of attribute field values.
 *--------------------------------------------------------------------*/

class TABDATFile
{
  private:
    char        *m_pszFname;
    FILE        *m_fp;
    TABAccess   m_eAccessMode;

    TABRawBinBlock *m_poHeaderBlock;
    int         m_numFields;
    TABDATFieldDef *m_pasFieldDef;

    TABRawBinBlock *m_poRecordBlock;
    int         m_nBlockSize;
    int         m_nRecordSize;
    int         m_nCurRecordId;
    GBool       m_bCurRecordDeletedFlag;

    GInt32      m_numRecords;
    GInt32      m_nFirstRecordPtr;
    GBool       m_bWriteHeaderInitialized;

    int         InitWriteHeader();
    int         WriteHeader();

   public:
    TABDATFile();
    ~TABDATFile();

    int         Open(const char *pszFname, const char *pszAccess);
    int         Close();

    int         GetNumFields();
    TABFieldType GetFieldType(int nFieldId);
    int         GetFieldWidth(int nFieldId);
    int         GetFieldPrecision(int nFieldId);
    int         ValidateFieldInfoFromTAB(int iField, const char *pszName,
                                         TABFieldType eType,
                                         int nWidth, int nPrecision);

    int         AddField(const char *pszName, TABFieldType eType,
                         int nWidth, int nPrecision=0);

    GInt32      GetNumRecords();
    TABRawBinBlock *GetRecordBlock(int nRecordId);
    GBool       IsCurrentRecordDeleted() { return m_bCurRecordDeletedFlag;};
    int         CommitRecordToFile();

    const char  *ReadCharField(int nWidth);
    GInt32      ReadIntegerField();
    GInt16      ReadSmallIntField();
    double      ReadFloatField();
    double      ReadDecimalField(int nWidth);
    const char  *ReadLogicalField();
    const char  *ReadDateField();

    int         WriteCharField(const char *pszValue, int nWidth);
    int         WriteIntegerField(GInt32 nValue);
    int         WriteSmallIntField(GInt16 nValue);
    int         WriteFloatField(double dValue);
    int         WriteDecimalField(double dValue, int nWidth, int nPrecision);
    int         WriteLogicalField(const char *pszValue);
    int         WriteDateField(const char *pszValue);

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class MIDDATAFile
 *
 * Class to handle a file pointer with a copy of the latest readed line
 *
 *--------------------------------------------------------------------*/

class MIDDATAFile
{
   public:
      MIDDATAFile();
     ~MIDDATAFile();

     int         Open(const char *pszFname, const char *pszAccess);
     int         Close();

     const char *GetLine();
     const char *GetLastLine();
     int Rewind();
     void SaveLine(const char *pszLine);
     const char *GetSavedLine();
     void WriteLine(const char*, ...);
     GBool IsValidFeature(const char *pszString);

//  Translation information
     void SetTranslation(double, double, double, double);
     double GetXTrans(double);
     double GetYTrans(double);
     double GetXMultiplier(){return m_dfXMultiplier;}
     const char *GetDelimiter(){return m_pszDelimiter;}
     void SetDelimiter(const char *pszDelimiter){m_pszDelimiter = pszDelimiter;}

     private:
       FILE *m_fp;
       const char *m_pszDelimiter;

       // 512 is a limit for the length of a line
#define MIDMAXCHAR 512
       char m_szLastRead[MIDMAXCHAR];
       char m_szSavedLine[MIDMAXCHAR];

       char        *m_pszFname;
       TABAccess   m_eAccessMode;
       double      m_dfXMultiplier;
       double      m_dfYMultiplier;
       double      m_dfXDisplacement;
       double      m_dfYDisplacement;
};



/*=====================================================================
                        Function prototypes
 =====================================================================*/

TABRawBinBlock *TABCreateMAPBlockFromFile(FILE *fpSrc, int nOffset, 
                                          int nSize = 512, 
                                          GBool bHardBlockSize = TRUE,
                                          TABAccess eAccessMode = TABRead);


#endif /* _MITAB_PRIV_H_INCLUDED_ */

