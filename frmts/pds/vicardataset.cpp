/******************************************************************************
 *
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARDataset
 * Author:   Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 *
 * NOTE: This driver code is loosely based on the ISIS and PDS drivers.
 * It is not intended to diminish the contribution of the original authors
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
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

constexpr int NULL1 = 0;
constexpr int NULL2 = -32768;
constexpr double NULL3 = -32768.0;

#include "cpl_port.h"

#ifdef HAVE_STRINGS_H
#undef HAVE_STRINGS_H
#endif

#ifdef HAVE_STRING_H
#undef HAVE_STRING_H
#endif

#include "cpl_safemaths.hpp"
#include "cpl_vax.h"
#include "cpl_vsi_error.h"
#include "vicardataset.h"
#include "nasakeywordhandler.h"
#include "vicarkeywordhandler.h"

#include "gtiff.h"
#include "geotiff.h"
#include "tifvsi.h"
#include "xtiffio.h"
#include "gt_wkt_srs_priv.h"

#include <exception>
#include <limits>
#include <string>

CPL_CVSID("$Id$")

/* GeoTIFF 1.0 geokeys */

static const geokey_t GTiffAsciiKeys[] = {
    GTCitationGeoKey,
    GeogCitationGeoKey,
    PCSCitationGeoKey,
    VerticalCitationGeoKey
};

static const geokey_t GTiffDoubleKeys[] = {
    GeogInvFlatteningGeoKey,
    GeogSemiMajorAxisGeoKey,
    GeogSemiMinorAxisGeoKey,
    ProjAzimuthAngleGeoKey,
    ProjCenterLatGeoKey,
    ProjCenterLongGeoKey,
    ProjFalseEastingGeoKey,
    ProjFalseNorthingGeoKey,
    ProjFalseOriginEastingGeoKey,
    ProjFalseOriginLatGeoKey,
    ProjFalseOriginLongGeoKey,
    ProjFalseOriginNorthingGeoKey,
    ProjLinearUnitSizeGeoKey,
    ProjNatOriginLatGeoKey,
    ProjNatOriginLongGeoKey,
    ProjOriginLatGeoKey,
    ProjOriginLongGeoKey,
    ProjRectifiedGridAngleGeoKey,
    ProjScaleAtNatOriginGeoKey,
    ProjScaleAtOriginGeoKey,
    ProjStdParallel1GeoKey,
    ProjStdParallel2GeoKey,
    ProjStdParallelGeoKey,
    ProjStraightVertPoleLongGeoKey,
    GeogLinearUnitSizeGeoKey,
    GeogAngularUnitSizeGeoKey,
    GeogPrimeMeridianLongGeoKey,
    ProjCenterEastingGeoKey,
    ProjCenterNorthingGeoKey,
    ProjScaleAtCenterGeoKey
};

static const geokey_t GTiffShortKeys[] = {
    GTModelTypeGeoKey,
    GTRasterTypeGeoKey,
    GeogAngularUnitsGeoKey,
    GeogEllipsoidGeoKey,
    GeogGeodeticDatumGeoKey,
    GeographicTypeGeoKey,
    ProjCoordTransGeoKey,
    ProjLinearUnitsGeoKey,
    ProjectedCSTypeGeoKey,
    ProjectionGeoKey,
    GeogPrimeMeridianGeoKey,
    GeogLinearUnitsGeoKey,
    GeogAzimuthUnitsGeoKey,
    VerticalCSTypeGeoKey,
    VerticalDatumGeoKey,
    VerticalUnitsGeoKey
};

/************************************************************************/
/*                     OGRVICARBinaryPrefixesLayer                      */
/************************************************************************/

class OGRVICARBinaryPrefixesLayer final: public OGRLayer
{
        VSILFILE* m_fp = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;
        int m_iRecord = 0;
        int m_nRecords = 0;
        vsi_l_offset m_nFileOffset = 0;
        vsi_l_offset m_nStride = 0;
        bool m_bError = false;
        bool m_bByteSwapIntegers = false;
        RawRasterBand::ByteOrder m_eBREALByteOrder;

        enum Type
        {
            FIELD_UNKNOWN,
            FIELD_UNSIGNED_CHAR,
            FIELD_UNSIGNED_SHORT,
            FIELD_UNSIGNED_INT,
            FIELD_SHORT,
            FIELD_INT,
            FIELD_FLOAT,
            FIELD_DOUBLE,
        };
        static Type GetTypeFromString(const char* pszStr);

        struct Field
        {
            int nOffset;
            Type eType;
        };
        std::vector<Field> m_aoFields;
        std::vector<GByte> m_abyRecord;

        OGRFeature* GetNextRawFeature();

    public:
        OGRVICARBinaryPrefixesLayer(VSILFILE* fp, int nRecords,
                                    const CPLJSONObject& oDef,
                                    vsi_l_offset nFileOffset,
                                    vsi_l_offset nStride,
                                    RawRasterBand::ByteOrder eBINTByteOrder,
                                    RawRasterBand::ByteOrder eBREALByteOrder);
        ~OGRVICARBinaryPrefixesLayer();

        bool HasError() const { return m_bError; }

        void ResetReading() override { m_iRecord = 0; }
        OGRFeatureDefn* GetLayerDefn() override { return m_poFeatureDefn; }
        OGRFeature* GetNextFeature() override;
        int TestCapability(const char*) override { return false; }
};

/************************************************************************/
/*                       GetTypeFromString()                            */
/************************************************************************/

OGRVICARBinaryPrefixesLayer::Type
            OGRVICARBinaryPrefixesLayer::GetTypeFromString(const char* pszStr)
{
    if( EQUAL(pszStr, "unsigned char") || EQUAL(pszStr, "unsigned byte") )
        return FIELD_UNSIGNED_CHAR;
    if( EQUAL(pszStr, "unsigned short") )
        return FIELD_UNSIGNED_SHORT;
    if( EQUAL(pszStr, "unsigned int") )
        return FIELD_UNSIGNED_INT;
    if( EQUAL(pszStr, "short") )
        return FIELD_SHORT;
    if( EQUAL(pszStr, "int") )
        return FIELD_INT;
    if( EQUAL(pszStr, "float") )
        return FIELD_FLOAT;
    if( EQUAL(pszStr, "double") )
        return FIELD_DOUBLE;
    return FIELD_UNKNOWN;
}

/************************************************************************/
/*                     OGRVICARBinaryPrefixesLayer()                    */
/************************************************************************/

OGRVICARBinaryPrefixesLayer::OGRVICARBinaryPrefixesLayer(
                                    VSILFILE* fp,
                                    int nRecords,
                                    const CPLJSONObject& oDef,
                                    vsi_l_offset nFileOffset,
                                    vsi_l_offset nStride,
                                    RawRasterBand::ByteOrder eBINTByteOrder,
                                    RawRasterBand::ByteOrder eBREALByteOrder):
    m_fp(fp),
    m_nRecords(nRecords),
    m_nFileOffset(nFileOffset),
    m_nStride(nStride),
#ifdef CPL_LSB
    m_bByteSwapIntegers(eBINTByteOrder != RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN),
#else
    m_bByteSwapIntegers(eBINTByteOrder != RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN),
#endif
    m_eBREALByteOrder(eBREALByteOrder)
{
    m_poFeatureDefn = new OGRFeatureDefn("binary_prefixes");
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);
    int nRecordSize = oDef.GetInteger("size");
    const auto oFields = oDef.GetObj("fields");
    if( oFields.IsValid() && oFields.GetType() == CPLJSONObject::Type::Array )
    {
        auto oFieldsArray = oFields.ToArray();
        int nOffset = 0;
        for( int i = 0; i < oFieldsArray.Size(); i++ )
        {
            auto oField = oFieldsArray[i];
            if( oField.GetType() == CPLJSONObject::Type::Object )
            {
                auto osName = oField.GetString("name");
                auto osType = oField.GetString("type");
                auto bHidden = oField.GetBool("hidden");
                auto eType = GetTypeFromString(osType.c_str());
                if( eType == FIELD_UNKNOWN )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Field %s of type %s not supported",
                             osName.c_str(), osType.c_str());
                    m_bError = true;
                    return;
                }
                else if( !osName.empty() )
                {
                    OGRFieldType eFieldType(OFTMaxType);
                    Field f;
                    f.nOffset = nOffset;
                    f.eType = eType;
                    switch(eType)
                    {
                        case FIELD_UNSIGNED_CHAR: nOffset += 1; eFieldType = OFTInteger; break;
                        case FIELD_UNSIGNED_SHORT: nOffset += 2; eFieldType = OFTInteger; break;
                        case FIELD_UNSIGNED_INT: nOffset += 4; eFieldType = OFTInteger64; break;
                        case FIELD_SHORT: nOffset += 2; eFieldType = OFTInteger; break;
                        case FIELD_INT: nOffset += 4; eFieldType = OFTInteger; break;
                        case FIELD_FLOAT: nOffset += 4; eFieldType = OFTReal; break;
                        case FIELD_DOUBLE: nOffset += 8; eFieldType = OFTReal; break;
                        default: CPLAssert(false); break;
                    }
                    if( nOffset > nRecordSize )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Field definitions not consistent with declared record size");
                        m_bError = true;
                        return;
                    }
                    if( !bHidden )
                    {
                        m_aoFields.push_back(f);
                        OGRFieldDefn oFieldDefn(osName.c_str(), eFieldType);
                        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                }
                else
                {
                    m_bError = true;
                }
            }
            else
            {
                m_bError = true;
            }
            if( m_bError )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Error while reading binary prefix definition");
                return;
            }
        }
    }
    m_abyRecord.resize(nRecordSize);
}

/************************************************************************/
/*                    ~OGRVICARBinaryPrefixesLayer()                    */
/************************************************************************/

OGRVICARBinaryPrefixesLayer::~OGRVICARBinaryPrefixesLayer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature* OGRVICARBinaryPrefixesLayer::GetNextRawFeature()
{
    if( m_iRecord >= m_nRecords )
        return nullptr;

    if( VSIFSeekL(m_fp, m_nFileOffset + m_iRecord * m_nStride, SEEK_SET) != 0 ||
        VSIFReadL(&m_abyRecord[0], m_abyRecord.size(), 1, m_fp) != 1 )
    {
        return nullptr;
    }

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    for( int i = 0; i < poFeature->GetFieldCount(); i++ )
    {
        int nOffset = m_aoFields[i].nOffset;
        switch( m_aoFields[i].eType )
        {
            case FIELD_UNSIGNED_CHAR:
                poFeature->SetField(i, m_abyRecord[nOffset]);
                break;
            case FIELD_UNSIGNED_SHORT:
            {
                unsigned short v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_bByteSwapIntegers )
                {
                    CPL_SWAP16PTR(&v);
                }
                poFeature->SetField(i, v);
                break;
            }
            case FIELD_UNSIGNED_INT:
            {
                unsigned int v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_bByteSwapIntegers )
                {
                    CPL_SWAP32PTR(&v);
                }
                poFeature->SetField(i, static_cast<GIntBig>(v));
                break;
            }
            case FIELD_SHORT:
            {
                short v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_bByteSwapIntegers )
                {
                    CPL_SWAP16PTR(&v);
                }
                poFeature->SetField(i, v);
                break;
            }
            case FIELD_INT:
            {
                int v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_bByteSwapIntegers )
                {
                    CPL_SWAP32PTR(&v);
                }
                poFeature->SetField(i, v);
                break;
            }
            case FIELD_FLOAT:
            {
                float v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_eBREALByteOrder == RawRasterBand::ByteOrder::ORDER_VAX )
                {
                    CPLVaxToIEEEFloat(&v);
                }
                else if( m_eBREALByteOrder !=
#ifdef CPL_LSB
                            RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN
#else
                            RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN
#endif
                       )
                {
                    CPL_SWAP32PTR(&v);
                }
                poFeature->SetField(i, v);
                break;
            }
            case FIELD_DOUBLE:
            {
                double v;
                memcpy(&v, &m_abyRecord[nOffset], sizeof(v));
                if( m_eBREALByteOrder == RawRasterBand::ByteOrder::ORDER_VAX )
                {
                    CPLVaxToIEEEDouble(&v);
                }
                else if( m_eBREALByteOrder !=
#ifdef CPL_LSB
                            RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN
#else
                            RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN
#endif
                       )
                {
                    CPL_SWAP64PTR(&v);
                }
                poFeature->SetField(i, v);
                break;
            }
            default:
                CPLAssert(false);
        }
    }
    poFeature->SetFID(m_iRecord);
    m_iRecord++;
    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRVICARBinaryPrefixesLayer::GetNextFeature()
{
    while( true )
    {
        auto poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                         VICARRawRasterBand                           */
/************************************************************************/

class VICARRawRasterBand final: public RawRasterBand
{
    protected:
        friend class VICARDataset;

    public:
                 VICARRawRasterBand(
                                VICARDataset *poDSIn, int nBandIn, VSILFILE* fpRawIn,
                                vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                int nLineOffsetIn,
                                GDALDataType eDataTypeIn,
                                ByteOrder eByteOrderIn);

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;

        virtual CPLErr  IRasterIO( GDALRWFlag, int, int, int, int,
                                void *, int, int, GDALDataType,
                                GSpacing nPixelSpace, GSpacing nLineSpace,
                                GDALRasterIOExtraArg* psExtraArg ) override;

};

/************************************************************************/
/*                        VICARRawRasterBand()                          */
/************************************************************************/

VICARRawRasterBand::VICARRawRasterBand(
                                VICARDataset *poDSIn, int nBandIn, VSILFILE* fpRawIn,
                                vsi_l_offset nImgOffsetIn, int nPixelOffsetIn,
                                int nLineOffsetIn,
                                GDALDataType eDataTypeIn,
                                ByteOrder eByteOrderIn):
    RawRasterBand(poDSIn, nBandIn, fpRawIn, nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn,
                  eDataTypeIn, eByteOrderIn, RawRasterBand::OwnFP::NO)
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VICARRawRasterBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    VICARDataset* poGDS = reinterpret_cast<VICARDataset*>(poDS);
    if( !poGDS->m_bIsLabelWritten )
        poGDS->WriteLabel();
    return RawRasterBand::IReadBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr VICARRawRasterBand::IWriteBlock( int nXBlock, int nYBlock,
                                        void *pImage )

{
    VICARDataset* poGDS = reinterpret_cast<VICARDataset*>(poDS);
    if( !poGDS->m_bIsLabelWritten )
        poGDS->WriteLabel();
    return RawRasterBand::IWriteBlock( nXBlock, nYBlock, pImage );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VICARRawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    VICARDataset* poGDS = reinterpret_cast<VICARDataset*>(poDS);
    if( !poGDS->m_bIsLabelWritten )
        poGDS->WriteLabel();
    return RawRasterBand::IRasterIO( eRWFlag,
                                     nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize,
                                     eBufType,
                                     nPixelSpace, nLineSpace,
                                     psExtraArg );
}

/************************************************************************/
/*                        VICARBASICRasterBand                          */
/************************************************************************/

class VICARBASICRasterBand final: public GDALPamRasterBand
{
    public:
        VICARBASICRasterBand(VICARDataset *poDSIn, int nBandIn,
                             GDALDataType eType);

        virtual CPLErr          IReadBlock( int, int, void * ) override;
        virtual CPLErr          IWriteBlock( int, int, void * ) override;
};

/************************************************************************/
/*                        VICARBASICRasterBand()                        */
/************************************************************************/

VICARBASICRasterBand::VICARBASICRasterBand(VICARDataset *poDSIn, int nBandIn,
                                           GDALDataType eType)
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDSIn->GetRasterXSize();
    nBlockYSize = 1;
    eDataType = eType;
}


namespace
{
class DecodeEncodeException: public std::exception
{
    public:
        DecodeEncodeException() = default;
};
}

//////////////////////////////////////////////////////////////////////////
/// Below functions are adapted from Public Domain VICAR project
/// from https://github.com/nasa/VICAR/blob/master/vos/rtl/source/basic_compression.c
//////////////////////////////////////////////////////////////////////////

/* masking array used in the algorithm to take out bits in memory */
const unsigned int cod1mask[25] = {0x0,0x1,0x3,0x7,0xf,0x1f,
                                   0x3f,0x7f,0xff,0x1ff,0x3ff,
                                   0x7ff,0xfff,0x1fff,0x3fff,
                                   0x7fff,0xffff,0x1ffff,
                                   0x3ffff,0x7ffff,0xfffff,
                                   0x1fffff,0x3fffff,0x7fffff,
                                   0xffffff};

/*****************************************************/
/* This function is a helper function for the BASIC  */
/* compression algorithm to get a specified number   */
/* of bits from buffer, convert it to a number and   */
/* return it.                                        */
/*****************************************************/
static unsigned char grab1(int nbit,
                           const unsigned char* buffer,
                           size_t buffer_size,
                           size_t& buffer_pos,
                           int& bit1ptr) /* bit position in the current byte of encrypted or decrypted buffer*/
{
    unsigned char val;
    int shift = 8-nbit-(bit1ptr);

    if( buffer_pos >= buffer_size )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Out of decoding buffer");
        throw DecodeEncodeException();
    }

    if (shift>0)
    {
        val = (buffer[buffer_pos]>>shift) & cod1mask[nbit];
        bit1ptr += nbit;

        return val;
    }
    if (shift<0)
    {
        unsigned v1 = buffer[buffer_pos] & cod1mask[nbit+shift];
        buffer_pos++;

        if( buffer_pos >= buffer_size )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Out of decoding buffer");
            throw DecodeEncodeException();
        }

        unsigned v2 = (buffer[buffer_pos]>>(8+shift)) & cod1mask[-shift];

        val = static_cast<unsigned char>((v1<<(-shift))+v2);

        bit1ptr = -shift;

        return val;
    }
    val = buffer[buffer_pos] & cod1mask[nbit];
    buffer_pos++;
    bit1ptr = 0;

    return val;
}

/*****************************************************/
/* This function is the decoding algorithm for BASIC */
/* compression.  The encoded buffer is passed into   */
/* code and the decoded buffer is passed out in buf. */
/*****************************************************/
static void basic_decode(const unsigned char* code,
                         size_t code_size,
                         unsigned char* buf,
                         int ns, int wid)
{
    int runInt = -3;
    unsigned char runChar;
    unsigned int nval = 999999;
    static const int cmprtrns1[7] = { -3, -2, -1, 0, 1, 2, 3 };
    size_t buffer_pos = 0;
    int bit1ptr = 0;
    unsigned int old = 0;
    const int ptop = ns*wid;

    for(int iw = 0; iw < wid; iw++)
    {
        for (int ip=iw;ip<ptop;ip+=wid)
        {
            if (runInt>(-3))
            {
                buf[ip] = static_cast<unsigned char>(nval);
                runInt--;
                continue;
            }
            unsigned char val = grab1(3, code, code_size, buffer_pos, bit1ptr);

            if (val<7)
            {
                nval = CPLUnsanitizedAdd<unsigned>(old,cmprtrns1[val]);
                buf[ip] = static_cast<unsigned char>(nval);
                old = nval;
                continue;
            }
            val = grab1(1, code, code_size, buffer_pos, bit1ptr);

            if (val)
            {
                runChar = grab1(4, code, code_size, buffer_pos, bit1ptr);
                if (runChar==15)
                {
                    runChar = grab1(8, code, code_size, buffer_pos, bit1ptr);

                    if (runChar==255)
                    {
                        unsigned char part0 = grab1(8, code, code_size, buffer_pos, bit1ptr);
                        unsigned char part1 = grab1(8, code, code_size, buffer_pos, bit1ptr);
                        unsigned char part2 = grab1(8, code, code_size, buffer_pos, bit1ptr);
                        runInt = part0 | (part1 << 8) | (part2 << 16);
                    }
                    else
                        runInt = runChar + 15;
                }
                else
                    runInt = runChar;

                val = grab1(3, code, code_size, buffer_pos, bit1ptr);
                if (val<7)
                    nval = CPLUnsanitizedAdd<unsigned>(old,cmprtrns1[val]);
                else
                    nval = grab1(8, code, code_size, buffer_pos, bit1ptr);
                buf[ip] = static_cast<unsigned char>(nval);
                old = nval;
            }
            else
            {
                val = grab1(8, code, code_size, buffer_pos, bit1ptr);
                buf[ip] = val;
                old = val;
            }
        }
    }
}

/*****************************************************/
/* This function is a helper function for the BASIC  */
/* encoding operation.  It puts the value in val into*/
/* memory location pointed by pcode1+reg1 into nbit  */
/* number of bits.                                   */
/*****************************************************/
static void emit1(unsigned char val, int nbit, unsigned char *reg1,
                  int& bit1ptr,
                  unsigned char* coded_buffer,
                  size_t& coded_buffer_pos,
                  size_t coded_buffer_size)
{
    int shift;

    shift = 8-nbit-bit1ptr;
    if (shift>0)
    {
        *reg1 = static_cast<unsigned char>(*reg1|(val<<shift));
        bit1ptr += nbit;
        return;
    }
    if (shift<0)
    {
        if( coded_buffer_pos >= coded_buffer_size )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Out of encoding buffer");
            throw DecodeEncodeException();
        }
        coded_buffer[coded_buffer_pos] = static_cast<unsigned char>(*reg1|(val>>(-shift)));
        coded_buffer_pos++;
        *reg1 = static_cast<unsigned char>(val<<(8+shift));
        bit1ptr = -shift;
        return;
    }
    if( coded_buffer_pos >= coded_buffer_size )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Out of encoding buffer");
        throw DecodeEncodeException();
    }
    coded_buffer[coded_buffer_pos] = static_cast<unsigned char>(*reg1|val);
    coded_buffer_pos++;

    *reg1 = 0;
    bit1ptr = 0;
}

/*****************************************************/
/* This function is meat of the BASIC encoding       */
/* algorithm.  This function is called repeatedly by */
/* the basic_encode function to compress the data    */
/* according to its run length (run), last 2         */
/* different values (vold and old), and the current  */
/* value (val), into the memory location pointed by  */
/* pcode1+reg1.                                      */
/*****************************************************/
static void basic_encrypt(int *run, int *old, int *vold, int val,
                          unsigned char *reg1,
                          int& bit1ptr,
                          unsigned char* coded_buffer,
                          size_t& coded_buffer_pos,
                          size_t coded_buffer_size)
{
    if(*run<4)
    {
        if(abs(*old-*vold)<4)
            emit1((unsigned char)(*old-*vold+3),3,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        else
        {
            emit1((unsigned char)14,4,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            emit1((unsigned char)(*old),8,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        }

        while(*run>1)
        {
            emit1((unsigned char)3,3,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            (*run)--;
        }

        *vold = *old; *old = val;
    }
    else
    {
        emit1((unsigned char)15,4,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        if (*run<19)
        {
                emit1((unsigned char)(*run-4),4,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        }
        else
        {
            emit1((unsigned char)15,4,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            if(*run<274)
            {
                emit1((char)(*run-19),8,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            }
            else
            {
                emit1((unsigned char)255,8,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);

                unsigned char part0 = static_cast<unsigned char>((*run-4) & 0xff);
                unsigned char part1 = static_cast<unsigned char>(((*run-4) >> 8) & 0xff);
                unsigned char part2 = static_cast<unsigned char>(((*run-4) >> 16) & 0xff);
                emit1(part0, 8, reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
                emit1(part1, 8, reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
                emit1(part2, 8, reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            }
        }
        if (abs(*old-*vold)<4)
        {
            emit1((unsigned char)(*old-*vold+3),3,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        }
        else
        {
            emit1((unsigned char)7,3,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
            emit1((unsigned char)(*old),8,reg1,bit1ptr,coded_buffer,coded_buffer_pos,coded_buffer_size);
        }
        *vold = *old; *old = val; *run = 1;
    }
}

/*****************************************************/
/* This function loops through the data given by     */
/* unencodedBuf, keeping track of run length.  When  */
/* the value of the data changes, it passes the run  */
/* length, last 2 differing values, the current      */
/* value, and the pointer in pcode1 buffer to encode */
/* the data into, to basic_encrypt function.         */
/*****************************************************/
static void basic_encode(const unsigned char *unencodedBuf,
                         unsigned char *coded_buffer,
                         size_t coded_buffer_size,
                         int ns, int wid, size_t *totBytes)
{
    int val = 0;
    int bit1ptr=0;
    const int ptop = ns*wid;
    unsigned char reg1 = 0;
    int run = 0;
    int old = unencodedBuf[0];
    int vold = 999999;

    size_t coded_buffer_pos = 0;

    for (int iw=0;iw<wid;iw++)
    {
        for (int ip=iw;ip<ptop;ip+=wid)
        {
            val = unencodedBuf[ip];

            if (val==old) run++;
            else
                basic_encrypt(&run, &old, &vold, val, &reg1, bit1ptr, coded_buffer, coded_buffer_pos, coded_buffer_size);
        }
    }

    /* purge of last code */
    basic_encrypt(&run, &old, &vold, val, &reg1, bit1ptr, coded_buffer, coded_buffer_pos, coded_buffer_size);

    if( coded_buffer_pos >= coded_buffer_size )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Out of encoding buffer");
        throw DecodeEncodeException();
    }
    coded_buffer[coded_buffer_pos] = reg1;

    *totBytes = coded_buffer_pos;
    if(bit1ptr > 0) (*totBytes)++;
}

//////////////////////////////////////////////////////////////////////////
/// End of VICAR code
//////////////////////////////////////////////////////////////////////////

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VICARBASICRasterBand::IReadBlock( int /*nXBlock*/, int nYBlock, void *pImage )

{
    VICARDataset* poGDS = reinterpret_cast<VICARDataset*>(poDS);

    const int nRecord = (nBand - 1) * nRasterYSize + nYBlock;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);

    if( poGDS->eAccess == GA_Update &&
        poGDS->m_anRecordOffsets[nRecord+1] == 0 )
    {
        memset( pImage, 0, static_cast<size_t>(nDTSize) * nRasterXSize );
        return CE_None;
    }

    // Find at which offset the compressed record is.
    // For BASIC compression, each compressed run is preceded by a uint32 value
    // givin its size, including the size of this uint32 value
    // For BASIC2 compression, the uint32 sizes of all records are put
    // immediately after the label.
    for( ; poGDS->m_nLastRecordOffset <= nRecord;
                                poGDS->m_nLastRecordOffset++ )
    {
        CPLAssert( poGDS->m_anRecordOffsets[poGDS->m_nLastRecordOffset+1] == 0 );

        if( poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC )
        {
            VSIFSeekL( poGDS->fpImage,
                        poGDS->m_anRecordOffsets[
                        poGDS->m_nLastRecordOffset] - sizeof(GUInt32),
                        SEEK_SET );
        }
        else
        {
            VSIFSeekL( poGDS->fpImage,
                        poGDS->m_nImageOffsetWithoutNBB +
                            static_cast<vsi_l_offset>(sizeof(GUInt32)) *
                                poGDS->m_nLastRecordOffset,
                        SEEK_SET );
        }
        GUInt32 nSize;
        VSIFReadL( &nSize, 1, sizeof(nSize), poGDS->fpImage);
        CPL_LSBPTR32(&nSize);
        if( (poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC &&
             nSize <= sizeof(GUInt32)) ||
            (poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC2 &&
             nSize == 0) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong size at record %d",
                    poGDS->m_nLastRecordOffset);
            return CE_Failure;
        }

        poGDS->m_anRecordOffsets[poGDS->m_nLastRecordOffset+1] =
            poGDS->m_anRecordOffsets[poGDS->m_nLastRecordOffset] + nSize;
    }

    unsigned int nSize;
    if( poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC )
    {
        nSize = static_cast<unsigned>(poGDS->m_anRecordOffsets[nRecord+1] -
                    poGDS->m_anRecordOffsets[nRecord] - sizeof(GUInt32));
    }
    else
    {
        nSize = static_cast<unsigned>(poGDS->m_anRecordOffsets[nRecord+1] -
                    poGDS->m_anRecordOffsets[nRecord]);
    }
    if( nSize > 100 * 1000 * 1000 ||
        (nSize > 1000 && (nSize - 11) / 4 > static_cast<unsigned>(nRasterXSize) * nDTSize) )
    {
        return CE_Failure;
    }
    if( poGDS->m_abyCodedBuffer.size() < nSize )
    {
        try
        {
            poGDS->m_abyCodedBuffer.resize(nSize);
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what() );
            return CE_Failure;
        }
    }
    if( VSIFSeekL(poGDS->fpImage,
                  poGDS->m_anRecordOffsets[nRecord], SEEK_SET) != 0 ||
        VSIFReadL(&poGDS->m_abyCodedBuffer[0], nSize, 1, poGDS->fpImage) != 1 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot read record %d", nRecord);
        return CE_Failure;
    }

    try
    {
        basic_decode(poGDS->m_abyCodedBuffer.data(), nSize,
                     static_cast<unsigned char*>(pImage),
                     nRasterXSize, nDTSize);
    }
    catch( const DecodeEncodeException& )
    {
        return CE_Failure;
    }
#ifdef CPL_MSB
    if( nDTSize > 1 )
    {
        GDALSwapWords(pImage, nDTSize, nRasterXSize, nDTSize);
    }
#endif
    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr VICARBASICRasterBand::IWriteBlock( int /*nXBlock*/, int nYBlock,
                                          void *pImage )

{
    VICARDataset* poGDS = reinterpret_cast<VICARDataset*>(poDS);
    if( poGDS->eAccess == GA_ReadOnly )
        return CE_Failure;
    if( !poGDS->m_bIsLabelWritten )
    {
        poGDS->WriteLabel();
        poGDS->m_nLabelSize = VSIFTellL(poGDS->fpImage);
        poGDS->m_anRecordOffsets[0] = poGDS->m_nLabelSize;
        if( poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC )
        {
            poGDS->m_anRecordOffsets[0] += sizeof(GUInt32);
        }
        else
        {
            poGDS->m_anRecordOffsets[0] +=
                static_cast<vsi_l_offset>(sizeof(GUInt32)) * nRasterYSize;
        }
    }
    if( nYBlock != poGDS->m_nLastRecordOffset )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Lines must be written in sequential order");
        return CE_Failure;
    }

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const size_t nMaxEncodedSize =
        static_cast<size_t>(nRasterXSize) * nDTSize + static_cast<size_t>(nRasterXSize) * nDTSize / 2 + 11;
    if( poGDS->m_abyCodedBuffer.size() < nMaxEncodedSize  )
    {
        try
        {
            poGDS->m_abyCodedBuffer.resize(nMaxEncodedSize);
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what() );
            return CE_Failure;
        }
    }

#ifdef CPL_MSB
    if( nDTSize > 1 )
    {
        GDALSwapWords(pImage, nDTSize, nRasterXSize, nDTSize);
    }
#endif

    size_t nCodedSize = 0;
    try
    {
        basic_encode(static_cast<unsigned char *>(pImage),
                     &poGDS->m_abyCodedBuffer[0],
                     poGDS->m_abyCodedBuffer.size(),
                     nRasterXSize, nDTSize, &nCodedSize);
    }
    catch( const DecodeEncodeException& )
    {
        return CE_Failure;
    }

#ifdef CPL_MSB
    if( nDTSize > 1 )
    {
        GDALSwapWords(pImage, nDTSize, nRasterXSize, nDTSize);
    }
#endif

    if( poGDS->m_eCompress == VICARDataset::COMPRESS_BASIC )
    {
        VSIFSeekL(poGDS->fpImage, poGDS->m_anRecordOffsets[nYBlock] - sizeof(GUInt32), SEEK_SET);
        GUInt32 nSizeToWrite = static_cast<GUInt32>(nCodedSize + sizeof(GUInt32));
        CPL_LSBPTR32(&nSizeToWrite);
        VSIFWriteL(&nSizeToWrite, sizeof(GUInt32), 1, poGDS->fpImage);
        VSIFWriteL(poGDS->m_abyCodedBuffer.data(), nCodedSize, 1, poGDS->fpImage);
        poGDS->m_anRecordOffsets[nYBlock + 1] = poGDS->m_anRecordOffsets[nYBlock] + nCodedSize + sizeof(GUInt32);
    }
    else
    {
        VSIFSeekL(poGDS->fpImage, poGDS->m_nLabelSize + static_cast<vsi_l_offset>(nYBlock) * sizeof(GUInt32), SEEK_SET);
        GUInt32 nSizeToWrite = static_cast<GUInt32>(nCodedSize);
        CPL_LSBPTR32(&nSizeToWrite);
        VSIFWriteL(&nSizeToWrite, sizeof(GUInt32), 1, poGDS->fpImage);
        VSIFSeekL(poGDS->fpImage, poGDS->m_anRecordOffsets[nYBlock], SEEK_SET);
        VSIFWriteL(poGDS->m_abyCodedBuffer.data(), nCodedSize, 1, poGDS->fpImage);
        poGDS->m_anRecordOffsets[nYBlock + 1] = poGDS->m_anRecordOffsets[nYBlock] + nCodedSize;
    }

    poGDS->m_nLastRecordOffset ++;

    return CE_None;
}

/************************************************************************/
/*                            VICARDataset()                            */
/************************************************************************/

VICARDataset::VICARDataset()

{
    m_oJSonLabel.Deinit();
    m_oSrcJSonLabel.Deinit();
}

/************************************************************************/
/*                           ~VICARDataset()                            */
/************************************************************************/

VICARDataset::~VICARDataset()

{
    if( !m_bIsLabelWritten )
        WriteLabel();
    VICARDataset::FlushCache(true);
    PatchLabel();
    if( fpImage != nullptr )
        VSIFCloseL( fpImage );
}


/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference* VICARDataset::GetSpatialRef() const

{
    if( !m_oSRS.IsEmpty() )
        return &m_oSRS;

    return GDALPamDataset::GetSpatialRef();
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

CPLErr VICARDataset::SetSpatialRef( const OGRSpatialReference* poSRS )
{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetSpatialRef( poSRS );
    if( poSRS )
        m_oSRS = *poSRS;
    else
        m_oSRS.Clear();
    InvalidateLabel();
    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VICARDataset::GetGeoTransform( double * padfTransform )

{
    if( m_bGotTransform )
    {
        memcpy( padfTransform, &m_adfGeoTransform[0], sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr VICARDataset::SetGeoTransform( double * padfTransform )

{
    if( eAccess == GA_ReadOnly )
        return GDALPamDataset::SetGeoTransform( padfTransform );
    if( padfTransform[1] <= 0.0 || padfTransform[1] != -padfTransform[5] ||
        padfTransform[2] != 0.0 || padfTransform[4] != 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only north-up geotransform with square pixels supported");
        return CE_Failure;
    }
    m_bGotTransform = true;
    memcpy( &m_adfGeoTransform[0], padfTransform, sizeof(double) * 6 );
    InvalidateLabel();
    return CE_None;
}

/************************************************************************/
/*                         GetLabelOffset()                             */
/************************************************************************/

int VICARDataset::GetLabelOffset( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->pabyHeader == nullptr || poOpenInfo->fpL == nullptr )
        return -1;

    std::string osHeader;
    const char *pszHeader = reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    // Some PDS3 images include a VICAR header pointed by ^IMAGE_HEADER.
    // If the user sets GDAL_TRY_PDS3_WITH_VICAR=YES, then we will gracefully
    // hand over the file to the VICAR dataset.
    vsi_l_offset nOffset = 0;
    if( CPLTestBool(CPLGetConfigOption("GDAL_TRY_PDS3_WITH_VICAR", "NO")) &&
        !STARTS_WITH(poOpenInfo->pszFilename, "/vsisubfile/") &&
        (nOffset = VICARDataset::GetVICARLabelOffsetFromPDS3(pszHeader, poOpenInfo->fpL, osHeader)) > 0 )
    {
        pszHeader = osHeader.c_str();
    }

    if( (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 )
    {
        // If opening in vector-only mode, then check when have NBB != 0
        const char* pszNBB = strstr(pszHeader, "NBB");
        if( pszNBB == nullptr )
            return -1;
        const char* pszEqualSign = strchr(pszNBB, '=');
        if( pszEqualSign == nullptr )
            return -1;
        if( atoi(pszEqualSign+1) == 0 )
            return -1;
    }
    if( strstr(pszHeader, "LBLSIZE") != nullptr &&
        strstr(pszHeader, "FORMAT") != nullptr &&
        strstr(pszHeader, "NL") != nullptr &&
        strstr(pszHeader, "NS") != nullptr &&
        strstr(pszHeader, "NB") != nullptr )
    {
        return static_cast<int>(nOffset);
    }
    return -1;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VICARDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    return GetLabelOffset(poOpenInfo) >= 0;
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool VICARDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    if( !RawDataset::GetRawBinaryLayout(sLayout) )
        return false;
    sLayout.osRawFilename = GetDescription();
    return true;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **VICARDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(
        nullptr, FALSE, "", "json:VICAR", nullptr);
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **VICARDataset::GetMetadata( const char* pszDomain )
{
    if( pszDomain != nullptr && EQUAL( pszDomain, "json:VICAR" ) )
    {
        if( m_aosVICARMD.empty() )
        {
            if( eAccess == GA_Update && !m_oJSonLabel.IsValid() )
            {
                BuildLabel();
            }
            CPLAssert( m_oJSonLabel.IsValid() );
            const CPLString osJson = m_oJSonLabel.Format(CPLJSONObject::PrettyFormat::Pretty);
            m_aosVICARMD.InsertString(0, osJson.c_str());
        }
        return m_aosVICARMD.List();
    }
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           InvalidateLabel()                          */
/************************************************************************/

void VICARDataset::InvalidateLabel()
{
    m_oJSonLabel.Deinit();
    m_aosVICARMD.Clear();
}

/************************************************************************/
/*                             SetMetadata()                            */
/************************************************************************/

CPLErr VICARDataset::SetMetadata( char** papszMD, const char* pszDomain )
{
    if( m_bUseSrcLabel && eAccess == GA_Update && pszDomain != nullptr &&
        EQUAL( pszDomain, "json:VICAR" ) )
    {
        m_oSrcJSonLabel.Deinit();
        InvalidateLabel();
        if( papszMD != nullptr && papszMD[0] != nullptr )
        {
            CPLJSONDocument oJSONDocument;
            const GByte *pabyData = reinterpret_cast<const GByte *>(papszMD[0]);
            if( !oJSONDocument.LoadMemory( pabyData ) )
            {
                return CE_Failure;
            }

            m_oSrcJSonLabel = oJSONDocument.GetRoot();
            if( !m_oSrcJSonLabel.IsValid() )
            {
                return CE_Failure;
            }
        }
        return CE_None;
    }
    return GDALPamDataset::SetMetadata(papszMD, pszDomain);
}

/************************************************************************/
/*                         SerializeString()                            */
/************************************************************************/

static std::string SerializeString(const std::string& s)
{
    return '\'' + CPLString(s).replaceAll('\'', "''").replaceAll('\n', "\\n") + '\'';
}

/************************************************************************/
/*                        WriteLabelItemValue()                         */
/************************************************************************/

static void WriteLabelItemValue(std::string& osLabel,
                                const CPLJSONObject& obj)
{
    const auto eType(obj.GetType());
    if( eType == CPLJSONObject::Type::Boolean )
    {
        osLabel += CPLSPrintf("%d", obj.ToBool() ? 1 : 0);
    }
    else if( eType == CPLJSONObject::Type::Integer )
    {
        osLabel += CPLSPrintf("%d", obj.ToInteger());
    }
    else if( eType == CPLJSONObject::Type::Long )
    {
        std::string osVal(CPLSPrintf("%.18g",
                                     static_cast<double>(obj.ToLong())));
        if( osVal.find('.') == std::string::npos )
            osVal += ".0";
        osLabel += osVal;
    }
    else if( eType == CPLJSONObject::Type::Double )
    {
        double dfVal = obj.ToDouble();
        if( dfVal >= static_cast<double>(std::numeric_limits<GIntBig>::min()) &&
            dfVal <= static_cast<double>(std::numeric_limits<GIntBig>::max()) &&
            static_cast<double>(static_cast<GIntBig>(dfVal)) == dfVal )
        {
            std::string osVal(CPLSPrintf("%.18g", dfVal));
            if( osVal.find('.') == std::string::npos )
                osVal += ".0";
            osLabel += osVal;
        }
        else
        {
            osLabel += CPLSPrintf("%.15g", dfVal);
        }
    }
    else if ( eType == CPLJSONObject::Type::String )
    {
        osLabel += SerializeString(obj.ToString());
    }
    else if ( eType == CPLJSONObject::Type::Array )
    {
        const auto oArray = obj.ToArray();
        osLabel += '(';
        for( int i = 0; i < oArray.Size(); i++ )
        {
            if( i > 0 )
                osLabel += ',';
            WriteLabelItemValue(osLabel, oArray[i]);
        }
        osLabel += ')';
    }
    else if ( eType == CPLJSONObject::Type::Null )
    {
        osLabel += "'NULL'";
    }
    else
    {
        osLabel += SerializeString(obj.Format(CPLJSONObject::PrettyFormat::Plain));
    }
}

/************************************************************************/
/*                      SanitizeItemName()                              */
/************************************************************************/

static std::string SanitizeItemName(const std::string& osItemName)
{
    std::string osRet(osItemName);
    if( osRet.size() > 32 )
        osRet.resize(32);
    if( osRet.empty() )
        return "UNNAMED";
    if( osRet[0] < 'A' || osRet[0] > 'Z' )
        osRet[0] = 'X'; // item name must start with a letter
    for( size_t i = 1; i < osRet.size(); i++ )
    {
        char ch = osRet[i];
        if( ch >= 'a' && ch <= 'z' )
            osRet[i] = ch - 'a' + 'A';
        else if( !((ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') ||
                   ch == '_') )
        {
            osRet[i] = '_';
        }
    }
    if( osRet != osItemName )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Label item name %s has been sanitized to %s",
                 osItemName.c_str(), osRet.c_str());
    }
    return osRet;
}

/************************************************************************/
/*                        WriteLabelItem()                              */
/************************************************************************/

static void WriteLabelItem(std::string& osLabel,
                           const CPLJSONObject& obj,
                           const std::string& osItemName = std::string())
{
    osLabel += ' ';
    osLabel += SanitizeItemName(osItemName.empty() ? obj.GetName() : osItemName);
    osLabel += '=';
    WriteLabelItemValue(osLabel, obj);
}

/************************************************************************/
/*                           WriteLabel()                               */
/************************************************************************/

void VICARDataset::WriteLabel()
{
    m_bIsLabelWritten = true;

    if( !m_oJSonLabel.IsValid() )
        BuildLabel();

    std::string osLabel;
    auto children = m_oJSonLabel.GetChildren();
    for( const auto& child: children )
    {
        const auto osName(child.GetName());
        if( osName == "LBLSIZE" || osName == "PROPERTY" || osName == "TASK" )
            continue;
        std::string osNameSubst;
        if( osName == "DAT_TIM" || osName == "USER" )
        {
            osNameSubst = osName + '_';
        }
        WriteLabelItem(osLabel, child, osNameSubst);
    }

    auto property = m_oJSonLabel.GetObj("PROPERTY");
    if( property.IsValid() && property.GetType() == CPLJSONObject::Type::Object )
    {
        children = property.GetChildren();
        for( const auto& child: children )
        {
            if( child.GetType() == CPLJSONObject::Type::Object )
            {
                osLabel += " PROPERTY=" + SerializeString(child.GetName());
                auto childrenProperty = child.GetChildren();
                for( const auto& childProperty: childrenProperty )
                {
                    const auto osName(child.GetName());
                    std::string osNameSubst;
                    if( osName == "LBLSIZE" || osName == "PROPERTY" ||
                        osName == "TASK" || osName == "DAT_TIM" || osName == "USER" )
                    {
                        osNameSubst = osName + '_';
                    }
                    WriteLabelItem(osLabel, childProperty, osNameSubst);
                }
            }
        }
    }

    auto task = m_oJSonLabel.GetObj("TASK");
    if( task.IsValid() && task.GetType() == CPLJSONObject::Type::Object )
    {
        children = task.GetChildren();
        for( const auto& child: children )
        {
            if( child.GetType() == CPLJSONObject::Type::Object )
            {
                osLabel += " TASK=" + SerializeString(child.GetName());
                auto oUser = child.GetObj("USER");
                if( oUser.IsValid() )
                    WriteLabelItem(osLabel, oUser);
                auto oDatTim = child.GetObj("DAT_TIM");
                if( oDatTim.IsValid() )
                    WriteLabelItem(osLabel, oDatTim);
                auto childrenProperty = child.GetChildren();
                for( const auto& childProperty: childrenProperty )
                {
                    const auto osName(child.GetName());
                    if( osName == "USER" || osName == "DAT_TIM" )
                        continue;
                    std::string osNameSubst;
                    if( osName == "LBLSIZE" || osName == "PROPERTY" ||
                        osName == "TASK" )
                    {
                        osNameSubst = osName + '_';
                    }
                    WriteLabelItem(osLabel, childProperty, osNameSubst);
                }
            }
        }
    }

    // Figure out label size, round it to the next multiple of RECSIZE
    constexpr size_t MAX_LOG10_LBLSIZE = 10;
    size_t nLabelSize = strlen("LBLSIZE=") + MAX_LOG10_LBLSIZE + osLabel.size();
    nLabelSize =
        (nLabelSize + m_nRecordSize - 1) / m_nRecordSize * m_nRecordSize;
    std::string osLabelSize(CPLSPrintf("LBLSIZE=%d", static_cast<int>(nLabelSize)));
    while( osLabelSize.size() < strlen("LBLSIZE=") + MAX_LOG10_LBLSIZE )
        osLabelSize += ' ';
    osLabel = osLabelSize + osLabel;
    CPLAssert( osLabel.size() <= nLabelSize );

    // Write label
    VSIFSeekL(fpImage, 0, SEEK_SET);
    VSIFWriteL(osLabel.data(), 1, osLabel.size(), fpImage);
    const size_t nZeroPadding = nLabelSize - osLabel.size();
    if( nZeroPadding )
    {
        VSIFWriteL(std::string(nZeroPadding, '\0').data(),
                   1, nZeroPadding, fpImage);
    }

    if( m_bInitToNodata && m_eCompress == COMPRESS_NONE )
    {
        const int nDTSize = GDALGetDataTypeSizeBytes(GetRasterBand(1)->GetRasterDataType());
        VSIFTruncateL( fpImage, VSIFTellL(fpImage) +
            static_cast<vsi_l_offset>(nRasterXSize) * nRasterYSize * nBands * nDTSize );
    }

    // Patch band offsets to take into account label
    for( int i = 0; i < nBands; i++ )
    {
        auto poBand = dynamic_cast<VICARRawRasterBand*>(GetRasterBand(i+1));
        if( poBand )
            poBand->nImgOffset += nLabelSize;
    }
}

/************************************************************************/
/*                           PatchLabel()                               */
/************************************************************************/

void VICARDataset::PatchLabel()
{
    if( eAccess == GA_ReadOnly || m_eCompress == COMPRESS_NONE )
        return;

    VSIFSeekL(fpImage, 0, SEEK_END);
    const vsi_l_offset nFileSize = VSIFTellL(fpImage);
    VSIFSeekL(fpImage, 0, SEEK_SET);
    std::string osBuffer;
    osBuffer.resize(1024);
    size_t nRead = VSIFReadL(&osBuffer[0], 1, 1024, fpImage);

    {
        CPLString osEOCI1;
        osEOCI1.Printf("%u", static_cast<unsigned>(nFileSize));
        while( osEOCI1.size() < 10 )
            osEOCI1 += ' ';
        size_t nPos = osBuffer.find("EOCI1=");
        CPLAssert(nPos <= nRead - (strlen("EOCI1=") + 10));
        memcpy(&osBuffer[nPos + strlen("EOCI1=")], osEOCI1.data(), 10);
    }

    {
        CPLString osEOCI2;
        osEOCI2.Printf("%u", static_cast<unsigned>(nFileSize >> 32));
        while( osEOCI2.size() < 10 )
            osEOCI2 += ' ';
        size_t nPos = osBuffer.find("EOCI2=");
        CPLAssert(nPos <= nRead - (strlen("EOCI2=") + 10));
        memcpy(&osBuffer[nPos + strlen("EOCI2=")], osEOCI2.data(), 10);
    }
    VSIFSeekL(fpImage, 0, SEEK_SET);
    VSIFWriteL(&osBuffer[0], 1, nRead, fpImage);
}

/**
 * Get or create CPLJSONObject.
 * @param  oParent Parent CPLJSONObject.
 * @param  osKey  Key name.
 * @return         CPLJSONObject class instance.
 */
static CPLJSONObject GetOrCreateJSONObject(CPLJSONObject &oParent,
                                           const std::string &osKey)
{
    CPLJSONObject oChild = oParent[osKey];
    if( oChild.IsValid() && oChild.GetType() != CPLJSONObject::Type::Object )
    {
        oParent.Delete( osKey );
        oChild.Deinit();
    }

    if( !oChild.IsValid() )
    {
        oChild = CPLJSONObject();
        oParent.Add( osKey, oChild );
    }
    return oChild;
}

/************************************************************************/
/*                           BuildLabel()                               */
/************************************************************************/

void VICARDataset::BuildLabel()
{
    CPLJSONObject oLabel = m_oSrcJSonLabel;
    if( !oLabel.IsValid() )
    {
        oLabel = CPLJSONObject();
    }

    oLabel.Set("LBLSIZE", 0); // to be overridden later

    if( !oLabel.GetObj("TYPE").IsValid() )
        oLabel.Set("TYPE", "IMAGE");

    const auto eType = GetRasterBand(1)->GetRasterDataType();
    const char* pszFormat = "";
    switch( eType )
    {
        case GDT_Byte: pszFormat = "BYTE"; break;
        case GDT_Int16: pszFormat = "HALF"; break;
        case GDT_Int32: pszFormat = "FULL"; break;
        case GDT_Float32: pszFormat = "REAL"; break;
        case GDT_Float64: pszFormat = "DOUB"; break;
        case GDT_CFloat32: pszFormat = "COMP"; break;
        default: CPLAssert(false); break;
    }
    oLabel.Set("FORMAT", pszFormat);

    oLabel.Set("BUFSIZ", m_nRecordSize); // arbitrary value
    oLabel.Set("DIM", 3);
    oLabel.Set("EOL", 0);
    oLabel.Set("RECSIZE", m_nRecordSize);
    oLabel.Set("ORG", "BSQ");
    oLabel.Set("NL", nRasterYSize);
    oLabel.Set("NS", nRasterXSize);
    oLabel.Set("NB", nBands);
    oLabel.Set("N1", nRasterXSize);
    oLabel.Set("N2", nRasterYSize);
    oLabel.Set("N3", nBands);
    oLabel.Set("N4", 0);
    oLabel.Set("NBB", 0);
    oLabel.Set("NLB", 0);
    oLabel.Set("HOST", "X86-64-LINX");
    oLabel.Set("INTFMT", "LOW");
    oLabel.Set("REALFMT", "RIEEE");
    oLabel.Set("BHOST", "X86-64-LINX");
    oLabel.Set("BINTFMT", "LOW");
    if( !oLabel.GetObj("BLTYPE").IsValid() )
        oLabel.Set("BLTYPE", "");
    oLabel.Set("COMPRESS", m_eCompress == COMPRESS_BASIC ? "BASIC":
                           m_eCompress == COMPRESS_BASIC2 ? "BASIC2": "NONE");
    if( m_eCompress == COMPRESS_NONE )
    {
        oLabel.Set("EOCI1", 0);
        oLabel.Set("EOCI2", 0);
    }
    else
    {
        // To be later patched. Those fake values must take 10 bytes
        // (8 + 2 single quotes) so that they can be later replaced by a
        // integer of maximum value 4294967295 (10 digits)
        oLabel.Set("EOCI1", "XXXXXXXX");
        oLabel.Set("EOCI2", "XXXXXXXX");
    }

    if( m_bUseSrcMap )
    {
        auto oMap = oLabel.GetObj("PROPERTY/MAP");
        if( oMap.IsValid() &&
            oMap.GetType() == CPLJSONObject::Type::Object )
        {
            if( !m_osTargetName.empty() )
                oMap.Set( "TARGET_NAME", m_osTargetName );
            if( !m_osLatitudeType.empty() )
                oMap.Set( "COORDINATE_SYSTEM_NAME", m_osLatitudeType );
            if( !m_osLongitudeDirection.empty() )
                oMap.Set( "POSITIVE_LONGITUDE_DIRECTION", m_osLongitudeDirection );
        }
    }
    else if( m_bGeoRefFormatIsMIPL )
    {
        auto oProperty = oLabel.GetObj("PROPERTY");
        if( oProperty.IsValid() )
        {
            oProperty.Delete( "MAP" );
            oProperty.Delete( "GEOTIFF" );
        }
        if( !m_oSRS.IsEmpty() )
        {
            BuildLabelPropertyMap(oLabel);
        }
    }
    else
    {
        auto oProperty = oLabel.GetObj("PROPERTY");
        if( oProperty.IsValid() )
        {
            oProperty.Delete( "MAP" );
            oProperty.Delete( "GEOTIFF" );
        }
        if( !m_oSRS.IsEmpty() )
        {
            BuildLabelPropertyGeoTIFF(oLabel);
        }
    }

    m_oJSonLabel = oLabel;
}

/************************************************************************/
/*                        BuildLabelPropertyMap()                       */
/************************************************************************/

void VICARDataset::BuildLabelPropertyMap(CPLJSONObject& oLabel)
{
    if( m_oSRS.IsProjected() || m_oSRS.IsGeographic() )
    {
        auto oProperty = GetOrCreateJSONObject(oLabel, "PROPERTY");
        auto oMap = GetOrCreateJSONObject(oProperty, "MAP");

        const char* pszDatum = m_oSRS.GetAttrValue("DATUM");
        CPLString osTargetName( m_osTargetName );
        if( osTargetName.empty() )
        {
            if( pszDatum && STARTS_WITH(pszDatum, "D_") )
            {
                osTargetName = pszDatum + 2;
            }
            else if( pszDatum )
            {
                osTargetName = pszDatum;
            }
        }
        if( !osTargetName.empty() )
            oMap.Add( "TARGET_NAME", osTargetName );

        oMap.Add( "A_AXIS_RADIUS", m_oSRS.GetSemiMajor() / 1000.0 );
        oMap.Add( "B_AXIS_RADIUS", m_oSRS.GetSemiMajor() / 1000.0 );
        oMap.Add( "C_AXIS_RADIUS", m_oSRS.GetSemiMinor() / 1000.0 );

        if( !m_osLatitudeType.empty() )
            oMap.Add( "COORDINATE_SYSTEM_NAME", m_osLatitudeType );
        else
            oMap.Add( "COORDINATE_SYSTEM_NAME", "PLANETOCENTRIC" );

        if( !m_osLongitudeDirection.empty() )
            oMap.Add( "POSITIVE_LONGITUDE_DIRECTION", m_osLongitudeDirection );
        else
            oMap.Add( "POSITIVE_LONGITUDE_DIRECTION", "EAST" );

        const char* pszProjection = m_oSRS.GetAttrValue("PROJECTION");
        if( pszProjection == nullptr )
        {
            oMap.Add( "MAP_PROJECTION_TYPE", "SIMPLE_CYLINDRICAL" );
            oMap.Add( "CENTER_LONGITUDE", 0.0 );
            oMap.Add( "CENTER_LATITUDE", 0.0 );
        }
        else if( EQUAL(pszProjection, SRS_PT_EQUIRECTANGULAR) )
        {
            oMap.Add( "MAP_PROJECTION_TYPE", "EQUIRECTANGULAR" );
            if( m_oSRS.GetNormProjParm( SRS_PP_LATITUDE_OF_ORIGIN, 0.0 )
                                                                != 0.0 )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                        "Ignoring %s. Only 0 value supported",
                        SRS_PP_LATITUDE_OF_ORIGIN);
            }
            oMap.Add( "CENTER_LONGITUDE",
                m_oSRS.GetNormProjParm(SRS_PP_CENTRAL_MERIDIAN, 0.0) );
            const double dfCenterLat =
                m_oSRS.GetNormProjParm(SRS_PP_STANDARD_PARALLEL_1, 0.0);
            oMap.Add( "CENTER_LATITUDE", dfCenterLat );
        }
        else if( EQUAL(pszProjection, SRS_PT_SINUSOIDAL) )
        {
            oMap.Add( "MAP_PROJECTION_TYPE", "SINUSOIDAL" );
            oMap.Add( "CENTER_LONGITUDE",
                m_oSRS.GetNormProjParm(SRS_PP_LONGITUDE_OF_CENTER, 0.0) );
            oMap.Add( "CENTER_LATITUDE", 0.0 );
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                    "Projection %s not supported",
                    pszProjection);
        }


        if( oMap["MAP_PROJECTION_TYPE"].IsValid() )
        {
            if( m_oSRS.GetNormProjParm( SRS_PP_FALSE_EASTING, 0.0 ) != 0.0 )
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                        "Ignoring %s. Only 0 value supported",
                        SRS_PP_FALSE_EASTING);
            }
            if( m_oSRS.GetNormProjParm( SRS_PP_FALSE_NORTHING, 0.0 ) != 0.0 )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Ignoring %s. Only 0 value supported",
                        SRS_PP_FALSE_NORTHING);
            }

            if( m_bGotTransform )
            {
                const double dfDegToMeter = m_oSRS.GetSemiMajor() * M_PI / 180.0;
                if( m_oSRS.IsProjected() )
                {
                    const double dfLinearUnits = m_oSRS.GetLinearUnits();
                    const double dfScale = m_adfGeoTransform[1] * dfLinearUnits;
                    oMap.Add( "SAMPLE_PROJECTION_OFFSET", -m_adfGeoTransform[0] * dfLinearUnits / dfScale - 0.5 );
                    oMap.Add( "LINE_PROJECTION_OFFSET", m_adfGeoTransform[3] * dfLinearUnits / dfScale - 0.5 );
                    oMap.Add( "MAP_SCALE", dfScale / 1000.0 );
                }
                else if( m_oSRS.IsGeographic() )
                {
                    const double dfScale = m_adfGeoTransform[1] * dfDegToMeter;
                    oMap.Add( "SAMPLE_PROJECTION_OFFSET", -m_adfGeoTransform[0] * dfDegToMeter / dfScale - 0.5 );
                    oMap.Add( "LINE_PROJECTION_OFFSET", m_adfGeoTransform[3] * dfDegToMeter / dfScale - 0.5 );
                    oMap.Add( "MAP_SCALE", dfScale / 1000.0 );
                }
            }
        }
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "SRS not supported");
    }
}

/************************************************************************/
/*                    BuildLabelPropertyGeoTIFF()                       */
/************************************************************************/

void VICARDataset::BuildLabelPropertyGeoTIFF(CPLJSONObject& oLabel)
{
    auto oProperty = GetOrCreateJSONObject(oLabel, "PROPERTY");
    auto oGeoTIFF = GetOrCreateJSONObject(oProperty, "GEOTIFF");

    // Ported from Vicar Open Source: Afids expects to be able to read
    // NITF_NROWS and NITF_NCOLS

    oGeoTIFF.Add("NITF_NROWS", nRasterYSize);
    oGeoTIFF.Add("NITF_NCOLS", nRasterXSize);

    // Create a in-memory GeoTIFF file

    char szFilename[100] = {};
    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/vicar_tmp_%p.tif", this);
    GDALDriver* poGTiffDriver = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
    if( poGTiffDriver == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GTiff driver not available");
        return;
    }
    const char* const apszOptions[] = { "GEOTIFF_VERSION=1.0", nullptr };
    auto poDS = std::unique_ptr<GDALDataset>(
        poGTiffDriver->Create(szFilename, 1, 1, 1, GDT_Byte, apszOptions));
    if( !poDS )
        return;
    poDS->SetSpatialRef(&m_oSRS);
    if( m_bGotTransform )
        poDS->SetGeoTransform(&m_adfGeoTransform[0]);
    poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GetMetadataItem(GDALMD_AREA_OR_POINT));
    poDS.reset();

    // Open it with libtiff/libgeotiff
    VSILFILE* fpL = VSIFOpenL( szFilename, "r" );
    if( fpL == nullptr )
    {
        VSIUnlink(szFilename);
        return;
    }

    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "r", fpL );
    CPLAssert(hTIFF);

    GTIF *hGTIF = GTIFNew(hTIFF);
    CPLAssert(hGTIF);

    // Get geotiff keys and write them as VICAR metadata
    for( const auto& gkey: GTiffShortKeys )
    {
        unsigned short val = 0;
        if( GDALGTIFKeyGetSHORT(hGTIF, gkey, &val, 0, 1) )
        {
            oGeoTIFF.Add(
                CPLString(GTIFKeyName(gkey)).toupper(),
                CPLSPrintf("%d(%s)", val, GTIFValueNameEx(hGTIF, gkey, val)));
        }
    }

    for( const auto& gkey: GTiffDoubleKeys )
    {
        double val = 0;
        if( GDALGTIFKeyGetDOUBLE(hGTIF, gkey, &val, 0, 1) )
        {
            oGeoTIFF.Add(
                CPLString(GTIFKeyName(gkey)).toupper(),
                CPLSPrintf("%.18g", val));
        }
    }

    for( const auto& gkey: GTiffAsciiKeys)
    {
        char szAscii[1024];
        if( GDALGTIFKeyGetASCII(hGTIF, gkey, szAscii, static_cast<int>(sizeof(szAscii))) )
        {
            oGeoTIFF.Add(
                CPLString(GTIFKeyName(gkey)).toupper(),
                szAscii);
        }
    }

    GTIFFree(hGTIF);

    // Get geotiff tags and write them as VICAR metadata
    const std::map<int, const char*> oMapTagCodeToName = {
        { TIFFTAG_GEOPIXELSCALE, "MODELPIXELSCALETAG" },
        { TIFFTAG_GEOTIEPOINTS, "MODELTIEPOINTTAG" },
        { TIFFTAG_GEOTRANSMATRIX, "MODELTRANSFORMATIONTAG" }
    };

    for( const auto& kv: oMapTagCodeToName )
    {
        uint16_t nCount = 0;
        double* padfValues = nullptr;
        if( TIFFGetField(hTIFF, kv.first, &nCount, &padfValues) )
        {
            std::string osVal("(");
            for( uint16_t i = 0; i < nCount; ++i )
            {
                if( i > 0 )
                    osVal += ',';
                osVal += CPLSPrintf("%.18g", padfValues[i]);
            }
            osVal += ')';
            oGeoTIFF.Add(kv.second, osVal);
        }
    }

    XTIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
    VSIUnlink(szFilename);
}

/************************************************************************/
/*                       ReadProjectionFromMapGroup()                   */
/************************************************************************/

void VICARDataset::ReadProjectionFromMapGroup()
{
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    const char* value = GetKeyword("MAP.MAP_SCALE");
    if (strlen(value) > 0 ) {
        dfXDim = CPLAtof(value) * 1000.0;
        dfYDim = CPLAtof(value) * -1 * 1000.0;
    }

    const double dfSampleOffset_Shift =
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Shift", "0.5" ));

    const double dfLineOffset_Shift =
        CPLAtof(CPLGetConfigOption( "PDS_LineProjOffset_Shift", "0.5" ));

    const double dfSampleOffset_Mult =
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Mult", "-1.0") );

    const double dfLineOffset_Mult =
        CPLAtof( CPLGetConfigOption( "PDS_LineProjOffset_Mult", "1.0") );

    /***********   Grab LINE_PROJECTION_OFFSET ************/
    double dfULYMap = 0.5;

    value = GetKeyword("MAP.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0) {
        const double yulcenter = CPLAtof(value);
        dfULYMap = ((yulcenter + dfLineOffset_Shift) * -dfYDim * dfLineOffset_Mult);
    }
    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    double dfULXMap=0.5;

    value = GetKeyword("MAP.SAMPLE_PROJECTION_OFFSET");
    if( strlen(value) > 0 ) {
        const double xulcenter = CPLAtof(value);
        dfULXMap = ((xulcenter + dfSampleOffset_Shift) * dfXDim * dfSampleOffset_Mult);
    }

/* ==================================================================== */
/*      Get the coordinate system.                                      */
/* ==================================================================== */
    bool bProjectionSet = true;

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    const CPLString target_name = GetKeyword("MAP.TARGET_NAME");

    /**********   Grab MAP_PROJECTION_TYPE *****/
    const CPLString map_proj_name
        = GetKeyword( "MAP.MAP_PROJECTION_TYPE");

    /******  Grab semi_major & convert to KM ******/
    const double semi_major
        = CPLAtof(GetKeyword( "MAP.A_AXIS_RADIUS")) * 1000.0;

    /******  Grab semi-minor & convert to KM ******/
    const double semi_minor
        = CPLAtof(GetKeyword( "MAP.C_AXIS_RADIUS")) * 1000.0;

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(GetKeyword( "MAP.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    const double center_lon
        = CPLAtof(GetKeyword( "MAP.CENTER_LONGITUDE"));

    /**********   Grab 1st std parallel *******/
    const double first_std_parallel =
        CPLAtof(GetKeyword( "MAP.FIRST_STANDARD_PARALLEL"));

    /**********   Grab 2nd std parallel *******/
    const double second_std_parallel =
        CPLAtof(GetKeyword( "MAP.SECOND_STANDARD_PARALLEL"));

    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    bool bIsGeographic = true;
    value = GetKeyword("MAP.COORDINATE_SYSTEM_NAME");
    if (EQUAL( value, "PLANETOCENTRIC" ))
        bIsGeographic = false;

/**   Set oSRS projection and parameters --- all PDS supported types added if apparently supported in oSRS
      "AITOFF",  ** Not supported in GDAL??
      "ALBERS",
      "BONNE",
      "BRIESEMEISTER",   ** Not supported in GDAL??
      "CYLINDRICAL EQUAL AREA",
      "EQUIDISTANT",
      "EQUIRECTANGULAR",
      "GNOMONIC",
      "HAMMER",    ** Not supported in GDAL??
      "HENDU",     ** Not supported in GDAL??
      "LAMBERT AZIMUTHAL EQUAL AREA",
      "LAMBERT CONFORMAL",
      "MERCATOR",
      "MOLLWEIDE",
      "OBLIQUE CYLINDRICAL",
      "ORTHOGRAPHIC",
      "SIMPLE CYLINDRICAL",
      "SINUSOIDAL",
      "STEREOGRAPHIC",
      "TRANSVERSE MERCATOR",
      "VAN DER GRINTEN",     ** Not supported in GDAL??
      "WERNER"     ** Not supported in GDAL??
**/
    CPLDebug( "PDS", "using projection %s\n\n", map_proj_name.c_str());

    OGRSpatialReference oSRS;

    if ((EQUAL( map_proj_name, "EQUIRECTANGULAR" )) ||
        (EQUAL( map_proj_name, "SIMPLE_CYLINDRICAL" )) ||
        (EQUAL( map_proj_name, "EQUIDISTANT" )) )  {
        oSRS.SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) {
        oSRS.SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "SINUSOIDAL" )) {
        oSRS.SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "MERCATOR" )) {
        oSRS.SetMercator ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "STEREOGRAPHIC" )) {
        if ((fabs(center_lat)-90) < 0.0000001) {
            oSRS.SetPS ( center_lat, center_lon, 1, 0, 0 );
        } else {
            oSRS.SetStereographic ( center_lat, center_lon, 1, 0, 0 );
        }
    } else if (EQUAL( map_proj_name, "POLAR_STEREOGRAPHIC")) {
        oSRS.SetPS ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "TRANSVERSE_MERCATOR" )) {
        oSRS.SetTM ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "LAMBERT_CONFORMAL_CONIC" )) {
        oSRS.SetLCC ( first_std_parallel, second_std_parallel,
                      center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "LAMBERT_AZIMUTHAL_EQUAL_AREA" )) {
        oSRS.SetLAEA( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "CYLINDRICAL_EQUAL_AREA" )) {
        oSRS.SetCEA  ( first_std_parallel, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "MOLLWEIDE" )) {
        oSRS.SetMollweide ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "ALBERS" )) {
        oSRS.SetACEA ( first_std_parallel, second_std_parallel,
                       center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "BONNE" )) {
        oSRS.SetBonne ( first_std_parallel, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "GNOMONIC" )) {
        oSRS.SetGnomonic ( center_lat, center_lon, 0, 0 );
#ifdef FIXME
    } else if (EQUAL( map_proj_name, "OBLIQUE_CYLINDRICAL" )) {
        // hope Swiss Oblique Cylindrical is the same
        oSRS.SetSOC ( center_lat, center_lon, 0, 0 );
#endif
    } else {
        CPLDebug( "VICAR",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name.c_str() );
        bProjectionSet = false;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        const CPLString proj_target_name = map_proj_name + " " + target_name;
        oSRS.SetProjCS(proj_target_name); //set ProjCS keyword

        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        const CPLString geog_name = "GCS_" + target_name;

        //The datum and sphere names will be the same basic name aas the planet
        const CPLString datum_name = "D_" + target_name;
        CPLString sphere_name = target_name; // + "_IAU_IAG");  //Might not be IAU defined so don't add

        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double iflattening = 0.0;
        if ((semi_major - semi_minor) < 0.0000001)
            iflattening = 0;
        else
            iflattening = semi_major / (semi_major - semi_minor);

        //Set the body size but take into consideration which proj is being used to help w/ compatibility
        //Notice that most PDS projections are spherical based on the fact that ISIS/PICS are spherical
        //Set the body size but take into consideration which proj is being used to help w/ proj4 compatibility
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS does it internally
        if ( ( (EQUAL( map_proj_name, "STEREOGRAPHIC" ) && (fabs(center_lat) == 90)) ) ||
             (EQUAL( map_proj_name, "POLAR_STEREOGRAPHIC" )))
        {
            if (bIsGeographic) {
                //Geograpraphic, so set an ellipse
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            } else {
                //Geocentric, so force a sphere using the semi-minor axis. I hope...
                sphere_name += "_polarRadius";
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_minor, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }
        else if ( (EQUAL( map_proj_name, "SIMPLE_CYLINDRICAL" )) ||
                  (EQUAL( map_proj_name, "EQUIDISTANT" )) ||
                  (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "STEREOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "SINUSOIDAL" )) ) {
            //isis uses the spherical equation for these projections so force a sphere
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else if (EQUAL( map_proj_name, "EQUIRECTANGULAR" )) {
            //isis uses local radius as a sphere, which is pre-calculated in the PDS label as the semi-major
            sphere_name += "_localRadius";
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else
        {
            //All other projections: Mercator, Transverse Mercator, Lambert Conformal, etc.
            //Geographic, so set an ellipse
            if (bIsGeographic) {
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            }
            else
            {
                //Geocentric, so force a sphere. I hope...
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }

        m_oSRS = oSRS;
        m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    if( bProjectionSet )
    {
        m_bGotTransform = true;
        m_adfGeoTransform[0] = dfULXMap;
        m_adfGeoTransform[1] = dfXDim;
        m_adfGeoTransform[2] = 0.0;
        m_adfGeoTransform[3] = dfULYMap;
        m_adfGeoTransform[4] = 0.0;
        m_adfGeoTransform[5] = dfYDim;
    }
}

/************************************************************************/
/*                    ReadProjectionFromGeoTIFFGroup()                  */
/************************************************************************/

void VICARDataset::ReadProjectionFromGeoTIFFGroup()
{
    m_bGeoRefFormatIsMIPL = true;

    // We will build a in-memory temporary GeoTIFF file from the VICAR GEOTIFF
    // metadata items.

    char szFilename[100] = {};
    snprintf( szFilename, sizeof(szFilename),
              "/vsimem/vicar_tmp_%p.tif", this);

/* -------------------------------------------------------------------- */
/*      Initialization of libtiff and libgeotiff.                       */
/* -------------------------------------------------------------------- */
    GTiffOneTimeInit();
    LibgeotiffOneTimeInit();

/* -------------------------------------------------------------------- */
/*      Initialize access to the memory geotiff structure.              */
/* -------------------------------------------------------------------- */
    VSILFILE* fpL = VSIFOpenL( szFilename, "w" );
    if( fpL == nullptr )
        return;

    TIFF *hTIFF = VSI_TIFFOpen( szFilename, "w", fpL );

    if( hTIFF == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TIFF/GeoTIFF structure is corrupt." );
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write some minimal set of image parameters.                     */
/* -------------------------------------------------------------------- */
    TIFFSetField( hTIFF, TIFFTAG_IMAGEWIDTH, 1 );
    TIFFSetField( hTIFF, TIFFTAG_IMAGELENGTH, 1 );
    TIFFSetField( hTIFF, TIFFTAG_BITSPERSAMPLE, 8 );
    TIFFSetField( hTIFF, TIFFTAG_SAMPLESPERPIXEL, 1 );
    TIFFSetField( hTIFF, TIFFTAG_ROWSPERSTRIP, 1 );
    TIFFSetField( hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG );
    TIFFSetField( hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK );

/* -------------------------------------------------------------------- */
/*      Write geotiff keys from VICAR metadata                          */
/* -------------------------------------------------------------------- */
    GTIF *hGTIF = GTIFNew(hTIFF);
    CPLAssert(hGTIF);

    for( const auto& gkey: GTiffAsciiKeys )
    {
        const char* pszValue = GetKeyword(
            ("GEOTIFF." + CPLString(GTIFKeyName(gkey)).toupper()).c_str(), nullptr );
        if( pszValue )
        {
            GTIFKeySet(hGTIF, gkey, TYPE_ASCII,
                       static_cast<int>(strlen(pszValue)), pszValue);
        }
    }

    for( const auto& gkey: GTiffDoubleKeys )
    {
        const char* pszValue = GetKeyword(
            ("GEOTIFF." + CPLString(GTIFKeyName(gkey)).toupper()).c_str(), nullptr );
        if( pszValue )
        {
            GTIFKeySet(hGTIF, gkey, TYPE_DOUBLE, 1, CPLAtof(pszValue));
        }
    }

    for( const auto& gkey: GTiffShortKeys )
    {
        const char* pszValue = GetKeyword(
            ("GEOTIFF." + CPLString(GTIFKeyName(gkey)).toupper()).c_str(), nullptr );
        if( pszValue )
        {
            GTIFKeySet(hGTIF, gkey, TYPE_SHORT, 1, atoi(pszValue));
        }
    }

    GTIFWriteKeys( hGTIF );
    GTIFFree( hGTIF );

/* -------------------------------------------------------------------- */
/*      Write geotiff tags from VICAR metadata                          */
/* -------------------------------------------------------------------- */

    const std::map<const char*, int> oMapTagNameToCode = {
        { "MODELPIXELSCALETAG", TIFFTAG_GEOPIXELSCALE },
        { "MODELTIEPOINTTAG", TIFFTAG_GEOTIEPOINTS },
        { "MODELTRANSFORMATIONTAG", TIFFTAG_GEOTRANSMATRIX },
    };

    for( const auto& kv: oMapTagNameToCode )
    {
        const char* pszValue = GetKeyword(
            (std::string("GEOTIFF.") + kv.first).c_str(), nullptr);
        if( pszValue )
        {
            // Remove leading ( and trailing ), and replace comma by space
            // to separate on it.
            const CPLStringList aosTokens(CSLTokenizeString2(
                CPLString(pszValue).replaceAll('(',"").replaceAll(')', "").
                  replaceAll(',', ' ').c_str(),
                " ", 0));
            std::vector<double> adfValues;
            for( int i = 0; i < aosTokens.size(); ++i )
                adfValues.push_back(CPLAtof(aosTokens[i]));
            TIFFSetField(hTIFF, kv.second, aosTokens.size(), &adfValues[0]);
        }
    }

/* -------------------------------------------------------------------- */
/*      Finalize the geotiff file.                                      */
/* -------------------------------------------------------------------- */

    char bySmallImage = 0;

    TIFFWriteEncodedStrip( hTIFF, 0, &bySmallImage, 1 );
    TIFFWriteDirectory( hTIFF );

    XTIFFClose( hTIFF );
    CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));

/* -------------------------------------------------------------------- */
/*      Get georeferencing from file.                                   */
/* -------------------------------------------------------------------- */
    auto poGTiffDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(szFilename));
    if( poGTiffDS )
    {
        auto poSRS = poGTiffDS->GetSpatialRef();
        if( poSRS )
            m_oSRS = *poSRS;

        if( poGTiffDS->GetGeoTransform(&m_adfGeoTransform[0]) == CE_None )
        {
            m_bGotTransform = true;
        }

        const char* pszAreaOrPoint = poGTiffDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        if( pszAreaOrPoint )
            GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, pszAreaOrPoint);
    }

    VSIUnlink(szFilename);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VICARDataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a VICAR dataset?                            */
/* -------------------------------------------------------------------- */
    const int nLabelOffset = GetLabelOffset( poOpenInfo );
    if( nLabelOffset < 0 )
        return nullptr;
    if( nLabelOffset > 0 )
    {
        CPLString osSubFilename;
        osSubFilename.Printf("/vsisubfile/%d,%s",
                             nLabelOffset,
                             poOpenInfo->pszFilename);
        GDALOpenInfo oOpenInfo(osSubFilename.c_str(), poOpenInfo->eAccess);
        return Open(&oOpenInfo);
    }

    VICARDataset *poDS = new VICARDataset();
    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    if( ! poDS->oKeywords.Ingest( poDS->fpImage, poOpenInfo->pabyHeader ) ) {
        delete poDS;
        return nullptr;
    }

    /************ CHECK INSTRUMENT/DATA *****************/

    bool bIsDTM = false;
    const char* value = poDS->GetKeyword( "DTM.DTM_OFFSET" );
    if (!EQUAL(value,"") ) {
        bIsDTM = true;
    }

    bool bInstKnown = false;
    // Check for HRSC
    if ( EQUAL(poDS->GetKeyword("BLTYPE"),"M94_HRSC") )
        bInstKnown = true;
    // Check for Framing Camera on Dawn
    else if ( EQUAL(poDS->GetKeyword("INSTRUMENT_ID"),"FC2") )
        bInstKnown = true;

    /************ Grab dimensions *****************/

    const int nCols = atoi(poDS->GetKeyword("NS"));
    const int nRows = atoi(poDS->GetKeyword("NL"));
    const int nBands = atoi(poDS->GetKeyword("NB"));

    if( !GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, false) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be a VICAR file, but failed to find some "
                  "required keywords.",
                  poOpenInfo->pszFilename );
        delete poDS;
        return nullptr;
    }

    const GDALDataType eDataType = GetDataTypeFromFormat(poDS->GetKeyword( "FORMAT" ));
    if( eDataType == GDT_Unknown )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not find known VICAR label entries!\n");
        delete poDS;
        return nullptr;
    }
    double dfNoData = 0.0;
    if (eDataType == GDT_Byte) {
        dfNoData = NULL1;
    }
    else if (eDataType == GDT_Int16) {
        dfNoData = NULL2;
    }
    else if (eDataType == GDT_Float32) {
        dfNoData = NULL3;
    }

    /***** CHECK ENDIANNESS **************/

    RawRasterBand::ByteOrder eByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
    if( GDALDataTypeIsInteger(eDataType) )
    {
        value = poDS->GetKeyword( "INTFMT", "LOW" );
        if (EQUAL(value,"LOW") ) {
            eByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
        }
        else if( EQUAL(value, "HIGH") ) {
            eByteOrder = RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "INTFMT=%s layout not supported.", value);
            delete poDS;
            return nullptr;
        }
    }
    else
    {
        value = poDS->GetKeyword( "REALFMT", "VAX" );
        if (EQUAL(value,"RIEEE") ) {
            eByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
        }
        else if (EQUAL(value,"IEEE") ) {
            eByteOrder = RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
        }
        else if (EQUAL(value,"VAX") ) {
            eByteOrder = RawRasterBand::ByteOrder::ORDER_VAX;
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "REALFMT=%s layout not supported.", value);
            delete poDS;
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    if( poDS->GetKeyword("MAP.MAP_PROJECTION_TYPE")[0] != '\0' )
    {
        poDS->ReadProjectionFromMapGroup();
    }
    else if( poDS->GetKeyword("GEOTIFF.GTMODELTYPEGEOKEY")[0] != '\0' ||
             poDS->GetKeyword("GEOTIFF.MODELTIEPOINTTAG")[0] != '\0' )
    {
        poDS->ReadProjectionFromGeoTIFFGroup();
    }

    if( !poDS->m_bGotTransform )
        poDS->m_bGotTransform = CPL_TO_BOOL(
                GDALReadWorldFile( poOpenInfo->pszFilename, "wld",
                               &poDS->m_adfGeoTransform[0] ));

    poDS->eAccess = poOpenInfo->eAccess;
    poDS->m_oJSonLabel = poDS->oKeywords.GetJsonObject();

/* -------------------------------------------------------------------- */
/*      Compute the line offsets.                                        */
/* -------------------------------------------------------------------- */

    GUInt64 nPixelOffset;
    GUInt64 nLineOffset;
    GUInt64 nBandOffset;
    GUInt64 nImageOffsetWithoutNBB;
    GUInt64 nNBB;
    GUInt64 nImageSize;
    if( !GetSpacings(poDS->oKeywords, nPixelOffset, nLineOffset, nBandOffset,
                     nImageOffsetWithoutNBB, nNBB, nImageSize) ||
         nImageOffsetWithoutNBB >
             std::numeric_limits<GUInt64>::max() - (nNBB + nBandOffset * (nBands - 1)) )
    {
        CPLDebug("VICAR", "Invalid spacings found");
        delete poDS;
        return nullptr;
    }

    poDS->m_nRecordSize = atoi(poDS->GetKeyword("RECSIZE", ""));

    if( nNBB != 0 )
    {
        const char* pszBLType = poDS->GetKeyword("BLTYPE", nullptr);
        const char* pszVicarConf = CPLFindFile("gdal", "vicar.json");
        if( pszBLType && pszVicarConf && poDS->m_nRecordSize > 0 )
        {

            RawRasterBand::ByteOrder eBINTByteOrder =
                RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
            value = poDS->GetKeyword( "BINTFMT", "LOW" );
            if (EQUAL(value,"LOW") ) {
                eBINTByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
            }
            else if( EQUAL(value, "HIGH") ) {
                eBINTByteOrder = RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
            }
            else
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                        "BINTFMT=%s layout not supported.", value);
            }

            RawRasterBand::ByteOrder eBREALByteOrder =
                RawRasterBand::ByteOrder::ORDER_VAX;
            value = poDS->GetKeyword( "BREALFMT", "VAX" );
            if (EQUAL(value,"RIEEE") ) {
                eBREALByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
            }
            else if (EQUAL(value,"IEEE") ) {
                eBREALByteOrder = RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
            }
            else if (EQUAL(value,"VAX") ) {
                eBREALByteOrder = RawRasterBand::ByteOrder::ORDER_VAX;
            }
            else
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                        "BREALFMT=%s layout not supported.", value);
            }

            CPLJSONDocument oDoc;
            if( oDoc.Load(pszVicarConf) )
            {
                const auto oRoot = oDoc.GetRoot();
                if( oRoot.GetType() == CPLJSONObject::Type::Object )
                {
                    auto oDef = oRoot.GetObj(pszBLType);
                    if( oDef.IsValid() &&
                        oDef.GetType() == CPLJSONObject::Type::Object &&
                        static_cast<GUInt64>(oDef.GetInteger("size")) == nNBB )
                    {
                        auto poLayer = std::unique_ptr<OGRVICARBinaryPrefixesLayer>(
                            new OGRVICARBinaryPrefixesLayer(
                                poDS->fpImage,
                                static_cast<int>(nImageSize / poDS->m_nRecordSize),
                                oDef,
                                nImageOffsetWithoutNBB,
                                poDS->m_nRecordSize,
                                eBINTByteOrder,
                                eBREALByteOrder));
                        if( !poLayer->HasError() )
                        {
                            poDS->m_poLayer = std::move(poLayer);
                        }
                    }
                }
            }
        }
    }

    poDS->m_nImageOffsetWithoutNBB =
                        static_cast<vsi_l_offset>(nImageOffsetWithoutNBB);

    CPLString osCompress = poDS->GetKeyword("COMPRESS", "NONE");
    if( EQUAL(osCompress, "BASIC") || EQUAL(osCompress, "BASIC2") )
    {
        if( poOpenInfo->eAccess == GA_Update )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Update of compressed VICAR file not supported");
            delete poDS;
            return nullptr;
        }
        poDS->SetMetadataItem("COMPRESS", osCompress, "IMAGE_STRUCTURE");
        poDS->m_eCompress = EQUAL(osCompress, "BASIC") ?
                                    COMPRESS_BASIC : COMPRESS_BASIC2;
        if( poDS->nRasterYSize > 100 * 1000 * 1000 / nBands )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many records for compressed dataset");
            delete poDS;
            return nullptr;
        }
        if( !GDALDataTypeIsInteger(eDataType) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Data type incompatible of compression");
            delete poDS;
            return nullptr;
        }
        // To avoid potential issues in basic_decode()
        const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
        if( nDTSize == 0 || poDS->nRasterXSize > INT_MAX / nDTSize )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too large scanline");
            delete poDS;
            return nullptr;
        }
        const int nRecords = poDS->nRasterYSize * nBands;
        try
        {
            // + 1 to store implicitly the size of the last record
            poDS->m_anRecordOffsets.resize( nRecords + 1 );
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what() );
            delete poDS;
            return nullptr;
        }
        if( poDS->m_eCompress == COMPRESS_BASIC )
        {
            poDS->m_anRecordOffsets[0] =
                poDS->m_nImageOffsetWithoutNBB + sizeof(GUInt32);
        }
        else
        {
            poDS->m_anRecordOffsets[0] =
                poDS->m_nImageOffsetWithoutNBB + sizeof(GUInt32) * nRecords;
        }
    }
    else if( !EQUAL(osCompress, "NONE") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "COMPRESS=%s not supported", osCompress.c_str());
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nBands; i++ )
    {
        GDALRasterBand *poBand;

        if( poDS->m_eCompress == COMPRESS_BASIC ||
            poDS->m_eCompress == COMPRESS_BASIC2 )
        {
            poBand = new VICARBASICRasterBand( poDS, i+1, eDataType );
        }
        else
        {
            poBand = new VICARRawRasterBand( poDS, i+1, poDS->fpImage,
                                 static_cast<vsi_l_offset>(
                                    nImageOffsetWithoutNBB + nNBB + nBandOffset * i),
                                 static_cast<int>(nPixelOffset),
                                 static_cast<int>(nLineOffset),
                                 eDataType,
                                 eByteOrder );
        }

        poDS->SetBand( i+1, poBand );
        //only set NoData if instrument is supported
        if (bInstKnown)
            poBand->SetNoDataValue( dfNoData );
        if (bIsDTM) {
            poBand->SetScale( static_cast<double>(
                CPLAtof(poDS->GetKeyword( "DTM.DTM_SCALING_FACTOR") ) ) );
            poBand->SetOffset( static_cast<double>(
                CPLAtof(poDS->GetKeyword( "DTM.DTM_OFFSET") ) ) );
            const char* pszMin = poDS->GetKeyword( "DTM.DTM_MINIMUM_DN", nullptr );
            const char* pszMax = poDS->GetKeyword( "DTM.DTM_MAXIMUM_DN", nullptr );
            if (pszMin != nullptr && pszMax != nullptr )
                poBand->SetStatistics(CPLAtofM(pszMin),CPLAtofM(pszMax),0,0);
            const char* pszNoData = poDS->GetKeyword( "DTM.DTM_MISSING_DN", nullptr );
            if (pszNoData != nullptr )
                poBand->SetNoDataValue( CPLAtofM(pszNoData) );
        } else if (EQUAL( poDS->GetKeyword( "BLTYPE"), "M94_HRSC" )) {
            double scale=CPLAtof(poDS->GetKeyword("DLRTO8.REFLECTANCE_SCALING_FACTOR","-1."));
            if (scale < 0.) {
                scale = CPLAtof(poDS->GetKeyword( "HRCAL.REFLECTANCE_SCALING_FACTOR","1."));
            }
            poBand->SetScale( scale );
            double offset=CPLAtof(poDS->GetKeyword("DLRTO8.REFLECTANCE_OFFSET","-1."));
            if (offset < 0.) {
                offset = CPLAtof(poDS->GetKeyword( "HRCAL.REFLECTANCE_OFFSET","0."));
            }
            poBand->SetOffset( offset );
        }
        const char* pszMin = poDS->GetKeyword( "STATISTICS.MINIMUM", nullptr );
        const char* pszMax = poDS->GetKeyword( "STATISTICS.MAXIMUM", nullptr );
        const char* pszMean = poDS->GetKeyword( "STATISTICS.MEAN", nullptr );
        const char* pszStdDev = poDS->GetKeyword( "STATISTICS.STANDARD_DEVIATION", nullptr );
        if (pszMin != nullptr && pszMax != nullptr && pszMean != nullptr && pszStdDev != nullptr )
                poBand->SetStatistics(CPLAtofM(pszMin),CPLAtofM(pszMax),CPLAtofM(pszMean),CPLAtofM(pszStdDev));
    }

/* -------------------------------------------------------------------- */
/*      Instrument-specific keywords as metadata.                       */
/* -------------------------------------------------------------------- */

/******************   HRSC    ******************************/

    if (EQUAL( poDS->GetKeyword( "BLTYPE"), "M94_HRSC" ) ) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", poDS->GetKeyword( "M94_INSTRUMENT.INSTRUMENT_HOST_NAME") );
        poDS->SetMetadataItem( "PRODUCT_TYPE", poDS->GetKeyword( "TYPE"));

        if (EQUAL( poDS->GetKeyword( "M94_INSTRUMENT.DETECTOR_ID"), "MEX_HRSC_SRC" )) {
            static const char * const apszKeywords[] =  {
                        "M94_ORBIT.IMAGE_TIME",
                        "FILE.EVENT_TYPE",
                        "FILE.PROCESSING_LEVEL_ID",
                        "M94_INSTRUMENT.DETECTOR_ID",
                        "M94_CAMERAS.EXPOSURE_DURATION",
                        "HRCONVER.INSTRUMENT_TEMPERATURE", nullptr
                    };
            for( int i = 0; apszKeywords[i] != nullptr; i++ ) {
                const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
                if( pszKeywordValue != nullptr )
                    poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
            }
        } else {
            static const char * const apszKeywords[] =  {
                "M94_ORBIT.START_TIME", "M94_ORBIT.STOP_TIME",
                "M94_INSTRUMENT.DETECTOR_ID",
                "M94_CAMERAS.MACROPIXEL_SIZE",
                "FILE.EVENT_TYPE",
                "M94_INSTRUMENT.MISSION_PHASE_NAME",
                "HRORTHO.SPICE_FILE_NAME",
                "HRCONVER.MISSING_FRAMES", "HRCONVER.OVERFLOW_FRAMES", "HRCONVER.ERROR_FRAMES",
                "HRFOOT.BEST_GROUND_SAMPLING_DISTANCE",
                "DLRTO8.RADIANCE_SCALING_FACTOR", "DLRTO8.RADIANCE_OFFSET",
                "DLRTO8.REFLECTANCE_SCALING_FACTOR", "DLRTO8.REFLECTANCE_OFFSET",
                "HRCAL.RADIANCE_SCALING_FACTOR", "HRCAL.RADIANCE_OFFSET",
                "HRCAL.REFLECTANCE_SCALING_FACTOR", "HRCAL.REFLECTANCE_OFFSET",
                "HRORTHO.DTM_NAME", "HRORTHO.EXTORI_FILE_NAME", "HRORTHO.GEOMETRIC_CALIB_FILE_NAME",
                nullptr
            };
            for( int i = 0; apszKeywords[i] != nullptr; i++ ) {
                const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i], nullptr );
                if( pszKeywordValue != nullptr )
                    poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
            }
        }
    }
    if (bIsDTM && EQUAL( poDS->GetKeyword( "MAP.TARGET_NAME"), "MARS" )) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "MARS_EXPRESS" );
        poDS->SetMetadataItem( "PRODUCT_TYPE", "DTM");
        static const char * const apszKeywords[] = {
            "DTM.DTM_MISSING_DN", "DTM.DTM_OFFSET", "DTM.DTM_SCALING_FACTOR", "DTM.DTM_A_AXIS_RADIUS",
            "DTM.DTM_B_AXIS_RADIUS", "DTM.DTM_C_AXIS_RADIUS", "DTM.DTM_DESC", "DTM.DTM_MINIMUM_DN",
            "DTM.DTM_MAXIMUM_DN", nullptr };
        for( int i = 0; apszKeywords[i] != nullptr; i++ ) {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != nullptr )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
    }

/******************   DAWN   ******************************/
    else if (EQUAL( poDS->GetKeyword( "INSTRUMENT_ID"), "FC2" )) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "DAWN" );
        static const char * const apszKeywords[] =  {"ORBIT_NUMBER","FILTER_NUMBER",
        "FRONT_DOOR_STATUS",
        "FIRST_LINE",
        "FIRST_LINE_SAMPLE",
        "PRODUCER_INSTITUTION_NAME",
        "SOURCE_FILE_NAME",
        "PROCESSING_LEVEL_ID",
        "TARGET_NAME",
        "LIMB_IN_IMAGE",
        "POLE_IN_IMAGE",
        "REFLECTANCE_SCALING_FACTOR",
        "SPICE_FILE_NAME",
        "SPACECRAFT_CENTRIC_LATITUDE",
        "SPACECRAFT_EASTERN_LONGITUDE",
        "FOOTPRINT_POSITIVE_LONGITUDE",
            nullptr };
        for( int i = 0; apszKeywords[i] != nullptr; i++ ) {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != nullptr )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
    }
    else if (bIsDTM && ( EQUAL( poDS->GetKeyword( "TARGET_NAME"), "VESTA" ) || EQUAL( poDS->GetKeyword( "TARGET_NAME"), "CERES" )))
    {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "DAWN" );
        poDS->SetMetadataItem( "PRODUCT_TYPE", "DTM");
        static const char * const apszKeywords[] = {
            "DTM_MISSING_DN", "DTM_OFFSET", "DTM_SCALING_FACTOR", "DTM_A_AXIS_RADIUS",
            "DTM_B_AXIS_RADIUS", "DTM_C_AXIS_RADIUS", "DTM_MINIMUM_DN",
            "DTM_MAXIMUM_DN", "MAP_PROJECTION_TYPE", "COORDINATE_SYSTEM_NAME",
            "POSITIVE_LONGITUDE_DIRECTION", "MAP_SCALE",
            "CENTER_LONGITUDE", "LINE_PROJECTION_OFFSET", "SAMPLE_PROJECTION_OFFSET",
            nullptr };
        for( int i = 0; apszKeywords[i] != nullptr; i++ )
        {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != nullptr )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *VICARDataset::GetKeyword( const char *pszPath,
                                      const char *pszDefault )

{
    return oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                        GetDataTypeFromFormat()                       */
/************************************************************************/

GDALDataType VICARDataset::GetDataTypeFromFormat(const char* pszFormat)
{
    if (EQUAL( pszFormat, "BYTE" ))
        return GDT_Byte;

    if (EQUAL( pszFormat, "HALF" ) || EQUAL( pszFormat, "WORD") )
        return GDT_Int16;

    if (EQUAL( pszFormat, "FULL" ) || EQUAL( pszFormat, "LONG") )
        return GDT_Int32;

    if (EQUAL( pszFormat, "REAL" ))
        return GDT_Float32;

    if (EQUAL( pszFormat, "DOUB" ))
        return GDT_Float64;

    if (EQUAL( pszFormat, "COMP" ) || EQUAL( pszFormat, "COMPLEX" ))
        return GDT_CFloat32;

    return GDT_Unknown;
}

/************************************************************************/
/*                             GetSpacings()                            */
/************************************************************************/

bool VICARDataset::GetSpacings(const VICARKeywordHandler& keywords,
                              GUInt64& nPixelOffset,
                              GUInt64& nLineOffset,
                              GUInt64& nBandOffset,
                              GUInt64& nImageOffsetWithoutNBB,
                              GUInt64& nNBB,
                              GUInt64& nImageSize)
{
    const GDALDataType eDataType = GetDataTypeFromFormat(keywords.GetKeyword( "FORMAT", "" ));
    if( eDataType == GDT_Unknown )
        return false;
    const GUInt64 nItemSize = GDALGetDataTypeSizeBytes(eDataType);
    const char* value = keywords.GetKeyword( "ORG", "BSQ" );
    // number of bytes of binary prefix before each record
    nNBB = atoi(keywords.GetKeyword("NBB", ""));
    const GUInt64 nCols64 = atoi(keywords.GetKeyword("NS", ""));
    const GUInt64 nRows64 = atoi(keywords.GetKeyword("NL", ""));
    const GUInt64 nBands64 = atoi(keywords.GetKeyword("NB", ""));
    try
    {
        if (EQUAL(value,"BIP") )
        {
            nPixelOffset = (CPLSM(nItemSize) * CPLSM(nBands64)).v();
            nBandOffset = nItemSize;
            nLineOffset = (CPLSM(nNBB) + CPLSM(nPixelOffset) * CPLSM(nCols64)).v();
            nImageSize = (CPLSM(nLineOffset) * CPLSM(nRows64)).v();
        }
        else if (EQUAL(value,"BIL") )
        {
            nPixelOffset = nItemSize;
            nBandOffset = (CPLSM(nItemSize) * CPLSM(nCols64)).v();
            nLineOffset = (CPLSM(nNBB) + CPLSM(nBandOffset) * CPLSM(nBands64)).v();
            nImageSize = (CPLSM(nLineOffset) * CPLSM(nRows64)).v();
        }
        else if (EQUAL(value,"BSQ") )
        {
            nPixelOffset = nItemSize;
            nLineOffset = (CPLSM(nNBB) + CPLSM(nPixelOffset) * CPLSM(nCols64)).v();
            nBandOffset = (CPLSM(nLineOffset) * CPLSM(nRows64)).v();
            nImageSize = (CPLSM(nBandOffset) * CPLSM(nBands64)).v();
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "ORG=%s layout not supported.", value);
            return false;
        }
    }
    catch( const CPLSafeIntOverflow& )
    {
        return false;
    }

    const GUInt64 nLabelSize = atoi(keywords.GetKeyword("LBLSIZE", ""));
    const GUInt64 nRecordSize = atoi(keywords.GetKeyword("RECSIZE", ""));
    const GUInt64 nNLB = atoi(keywords.GetKeyword("NLB", ""));
    try
    {
        nImageOffsetWithoutNBB = (CPLSM(nLabelSize) + CPLSM(nRecordSize) * CPLSM(nNLB) + CPLSM(nNBB)).v();
        nImageOffsetWithoutNBB -= nNBB;
    }
    catch( const CPLSafeIntOverflow& )
    {
        return false;
    }
    return true;
}

/************************************************************************/
/*                           Create()                                   */
/************************************************************************/

GDALDataset *VICARDataset::Create(const char* pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char** papszOptions)
{
    return CreateInternal(pszFilename, nXSize, nYSize, nBands, eType,
                          papszOptions);
}

VICARDataset *VICARDataset::CreateInternal(const char* pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType,
                                  char** papszOptions)
{
    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_Int32 &&
        eType != GDT_Float32 && eType != GDT_Float64 && eType != GDT_CFloat32 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type");
        return nullptr;
    }

    const int nPixelOffset = GDALGetDataTypeSizeBytes(eType);
    if( nXSize == 0 || nYSize == 0  || nPixelOffset > INT_MAX / nXSize )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported raster dimensions");
        return nullptr;
    }
    const int nLineOffset = nXSize * nPixelOffset;

    if( nBands == 0 || nBands > 32767 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    const char* pszCompress = CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    CompressMethod eCompress = COMPRESS_NONE;
    if( EQUAL(pszCompress, "NONE") )
    {
        eCompress = COMPRESS_NONE;
    }
    else if( EQUAL(pszCompress, "BASIC") )
    {
        eCompress = COMPRESS_BASIC;
    }
    else if( EQUAL(pszCompress, "BASIC2") )
    {
        eCompress = COMPRESS_BASIC2;
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported COMPRESS value");
        return nullptr;
    }
    if( eCompress != COMPRESS_NONE &&
        (!GDALDataTypeIsInteger(eType) || nBands != 1) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BASIC/BASIC2 compression only supports one-band integer datasets");
        return nullptr;
    }

    std::vector<vsi_l_offset> anRecordOffsets;
    if( eCompress != COMPRESS_NONE )
    {
        const GUInt64 nMaxEncodedSize =
            static_cast<GUInt64>(nXSize) * nPixelOffset + static_cast<GUInt64>(nXSize) * nPixelOffset / 2 + 11;
        // To avoid potential later int overflows
        if( nMaxEncodedSize > static_cast<GUInt64>(INT_MAX) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too large scanline");
            return nullptr;
        }
        if( nYSize > 100 * 1000 * 1000 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many records for compressed dataset");
            return nullptr;
        }
        try
        {
            // + 1 to store implicitly the size of the last record
            anRecordOffsets.resize( nYSize + 1 );
        }
        catch( const std::exception& e )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what() );
            return nullptr;
        }
    }

    CPLJSONObject oSrcJSonLabel;
    oSrcJSonLabel.Deinit();

    const char* pszLabel = CSLFetchNameValue(papszOptions, "LABEL");
    if( pszLabel )
    {
        CPLJSONDocument oJSONDocument;
        if( pszLabel[0] == '{' )
        {
            const GByte *pabyData = reinterpret_cast<const GByte *>(pszLabel);
            if( !oJSONDocument.LoadMemory( pabyData ) )
            {
                return nullptr;
            }
        }
        else
        {
            if( !oJSONDocument.Load(pszLabel) )
            {
                return nullptr;
            }
        }

        oSrcJSonLabel = oJSONDocument.GetRoot();
        if( !oSrcJSonLabel.IsValid() )
        {
            return nullptr;
        }
    }

    VSILFILE* fp = VSIFOpenExL(pszFilename, "wb+", true);
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Cannot create %s: %s",
                  pszFilename, VSIGetLastErrorMsg() );
        return nullptr;
    }

    VICARDataset* poDS = new VICARDataset();
    poDS->fpImage = fp;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->m_nRecordSize = nLineOffset;
    poDS->m_bIsLabelWritten = false;
    poDS->m_bGeoRefFormatIsMIPL =
        EQUAL(CSLFetchNameValueDef(papszOptions, "GEOREF_FORMAT", "MIPL"), "MIPL");
    poDS->m_bUseSrcLabel = CPLFetchBool(papszOptions, "USE_SRC_LABEL", true);
    poDS->m_bUseSrcMap = CPLFetchBool(papszOptions, "USE_SRC_MAP", false);
    poDS->m_osLatitudeType = CSLFetchNameValueDef(papszOptions,
                                                  "COORDINATE_SYSTEM_NAME", "");
    poDS->m_osLongitudeDirection = CSLFetchNameValueDef(papszOptions,
                                                  "POSITIVE_LONGITUDE_DIRECTION", "");
    poDS->m_osTargetName = CSLFetchNameValueDef(papszOptions,
                                                  "TARGET_NAME", "");
    poDS->m_bInitToNodata = true;
    poDS->m_oSrcJSonLabel = oSrcJSonLabel;
    poDS->m_eCompress = eCompress;
    poDS->m_anRecordOffsets = std::move(anRecordOffsets);
    poDS->eAccess = GA_Update;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    const vsi_l_offset nBandOffset =
        static_cast<vsi_l_offset>(nLineOffset) * nYSize;
    for( int i = 0; i < nBands; i++ )
    {
        GDALRasterBand *poBand;
        if( eCompress != COMPRESS_NONE )
        {
            poBand = new VICARBASICRasterBand( poDS, i+1, eType );
        }
        else
        {
            poBand = new VICARRawRasterBand(
                poDS, i+1, poDS->fpImage,
                i * nBandOffset, // will be set later to final value since we need to include the label size
                nPixelOffset,
                nLineOffset,
                eType,
                RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN );
        }
        poDS->SetBand( i+1, poBand );
    }

    return poDS;
}

/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset* VICARDataset::CreateCopy( const char *pszFilename,
                                       GDALDataset *poSrcDS,
                                       int /*bStrict*/,
                                       char ** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData )
{
    if( poSrcDS->GetRasterCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported band count");
        return nullptr;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();
    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    auto poDS = CreateInternal(
        pszFilename, nXSize, nYSize, nBands, eType, papszOptions );
    if( poDS == nullptr )
        return nullptr;

    double adfGeoTransform[6] = { 0.0 };
    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None
        && (adfGeoTransform[0] != 0.0
            || adfGeoTransform[1] != 1.0
            || adfGeoTransform[2] != 0.0
            || adfGeoTransform[3] != 0.0
            || adfGeoTransform[4] != 0.0
            || adfGeoTransform[5] != 1.0) )
    {
        poDS->SetGeoTransform( adfGeoTransform );
    }

    auto poSrcSRS = poSrcDS->GetSpatialRef();
    if( poSrcSRS )
    {
        poDS->SetSpatialRef( poSrcSRS );
    }

    if( poDS->m_bUseSrcLabel && !poDS->m_oSrcJSonLabel.IsValid() )
    {
        char** papszMD_VICAR = poSrcDS->GetMetadata("json:VICAR");
        if( papszMD_VICAR != nullptr )
        {
            poDS->SetMetadata( papszMD_VICAR, "json:VICAR" );
        }
    }

    poDS->m_bInitToNodata = false;
    CPLErr eErr = GDALDatasetCopyWholeRaster( poSrcDS, poDS,
                                           nullptr, pfnProgress, pProgressData );
    poDS->FlushCache(false);
    if( eErr != CE_None )
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                     GetVICARLabelOffsetFromPDS3()                    */
/************************************************************************/

vsi_l_offset VICARDataset::GetVICARLabelOffsetFromPDS3(const char* pszHdr,
                                                       VSILFILE* fp,
                                                       std::string& osVICARHeader)
{
    const char* pszPDSVersionID = strstr(pszHdr,"PDS_VERSION_ID");
    int nOffset = 0;
    if (pszPDSVersionID)
        nOffset = static_cast<int>(pszPDSVersionID - pszHdr);

    NASAKeywordHandler  oKeywords;
    if( oKeywords.Ingest( fp, nOffset ) )
    {
        const int nRecordBytes = atoi(oKeywords.GetKeyword("RECORD_BYTES", "0"));
        const int nImageHeader = atoi(oKeywords.GetKeyword("^IMAGE_HEADER", "0"));
        if( nRecordBytes > 0 && nImageHeader > 0 )
        {
            const auto nImgHeaderOffset =
                static_cast<vsi_l_offset>(nImageHeader - 1) * nRecordBytes;
            osVICARHeader.resize(1024);
            size_t nMemb;
            if( VSIFSeekL( fp, nImgHeaderOffset, SEEK_SET ) == 0 &&
                (nMemb = VSIFReadL( &osVICARHeader[0], 1,
                            osVICARHeader.size(), fp )) != 0 &&
                osVICARHeader.find("LBLSIZE") != std::string::npos )
            {
                osVICARHeader.resize(nMemb);
                return nImgHeaderOffset;
            }
        }
    }
    return 0;
}

/************************************************************************/
/*                         GDALRegister_VICAR()                         */
/************************************************************************/

void GDALRegister_VICAR()

{
    if( GDALGetDriverByName( "VICAR" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "VICAR" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "MIPL VICAR file" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/vicar.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 Int32 Float32 Float64 CFloat32" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='GEOREF_FORMAT' type='string-select' "
    "description='How to encode georeferencing information' "
    "default='MIPL'>"
"     <Value>MIPL</Value>"
"     <Value>GEOTIFF</Value>"
"  </Option>"
"  <Option name='COORDINATE_SYSTEM_NAME' type='string-select' "
    "description='Value of MAP.COORDINATE_SYSTEM_NAME' default='PLANETOCENTRIC'>"
"     <Value>PLANETOCENTRIC</Value>"
"     <Value>PLANETOGRAPHIC</Value>"
"  </Option>"
"  <Option name='POSITIVE_LONGITUDE_DIRECTION' type='string-select' "
    "description='Value of MAP.POSITIVE_LONGITUDE_DIRECTION' "
    "default='EAST'>"
"     <Value>EAST</Value>"
"     <Value>WEST</Value>"
"  </Option>"
"  <Option name='TARGET_NAME' type='string' description='Value of "
    "MAP.TARGET_NAME'/>"
"  <Option name='USE_SRC_LABEL' type='boolean' "
    "description='Whether to use source label in VICAR to VICAR conversions' "
    "default='YES'/>"
"  <Option name='USE_SRC_MAP' type='boolean' "
    "description='Whether to use MAP property from source label in "
                 "VICAR to VICAR conversions' "
    "default='NO'/>"
"  <Option name='LABEL' type='string' "
    "description='Label to use, either as a JSON string or a filename containing one'/>"
"  <Option name='COMPRESS' type='string-select' "
    "description='Compression method' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>BASIC</Value>"
"     <Value>BASIC2</Value>"
"  </Option>"
"</CreationOptionList>"
    );

    poDriver->pfnOpen = VICARDataset::Open;
    poDriver->pfnIdentify = VICARDataset::Identify;
    poDriver->pfnCreate = VICARDataset::Create;
    poDriver->pfnCreateCopy = VICARDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
