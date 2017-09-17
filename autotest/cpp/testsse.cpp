#include <stdio.h>

#include "gdalsse_priv.h"

#define MY_ASSERT(x)   do { if (!(x)) { printf("test at line %d failed !\n", __LINE__); exit(1); } } while(0)

int main()
{
    {
        double x = 1.23;
        XMMReg4Double reg = XMMReg4Double::Load1ValHighAndLow(&x);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == x);
        MY_ASSERT(res[1] == x);
        MY_ASSERT(res[2] == x);
        MY_ASSERT(res[3] == x);
    }

    {
        unsigned char input[] = { 1, 2, 3, 4 };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        unsigned char output[4];
        reg.Store4Val(output);
        MY_ASSERT(output[0] == input[0]);
        MY_ASSERT(output[1] == input[1]);
        MY_ASSERT(output[2] == input[2]);
        MY_ASSERT(output[3] == input[3]);
    }

    {
        unsigned short input[] = { 1, 65535, 3, 65534 };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        unsigned short output[4];
        reg.Store4Val(output);
        MY_ASSERT(output[0] == input[0]);
        MY_ASSERT(output[1] == input[1]);
        MY_ASSERT(output[2] == input[2]);
        MY_ASSERT(output[3] == input[3]);
    }

    {
        short input[] = { 1, 32767, 3, -32768 };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);
    }

    {
        float input[] = { 1.0f, 2.0f, 3.0f, 4.0f };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        float output[4];
        reg.Store4Val(output);
        MY_ASSERT(output[0] == input[0]);
        MY_ASSERT(output[1] == input[1]);
        MY_ASSERT(output[2] == input[2]);
        MY_ASSERT(output[3] == input[3]);
    }

    {
        double input[] = { 1.0, 2.0, 3.0, 4.0 };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        double res[4];
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        MY_ASSERT(reg.GetHorizSum() == input[0] + input[1] + input[2] + input[3]);

        double input2[] = { 100.0, 200.0 };
        reg.AddToLow( XMMReg2Double::Load2Val(input2) );
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0] + input2[0]);
        MY_ASSERT(res[1] == input[1] + input2[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);
    }

    {
        double input[] = { 1.0, 2.0, 3.0, 4.0 };
        double input2[] = { 10.0, 9.0, 8.0, 7.0 };
        XMMReg4Double reg = XMMReg4Double::Load4Val(input);
        XMMReg4Double reg2 = XMMReg4Double::Load4Val(input2);

        double res[4];

        (reg + reg2).Store4Val(res);
        MY_ASSERT(res[0] == input[0] + input2[0]);
        MY_ASSERT(res[1] == input[1] + input2[1]);
        MY_ASSERT(res[2] == input[2] + input2[2]);
        MY_ASSERT(res[3] == input[3] + input2[3]);

        reg += reg2;
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0] + input2[0]);
        MY_ASSERT(res[1] == input[1] + input2[1]);
        MY_ASSERT(res[2] == input[2] + input2[2]);
        MY_ASSERT(res[3] == input[3] + input2[3]);

        reg = reg - reg2;
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        (reg * reg2).Store4Val(res);
        MY_ASSERT(res[0] == input[0] * input2[0]);
        MY_ASSERT(res[1] == input[1] * input2[1]);
        MY_ASSERT(res[2] == input[2] * input2[2]);
        MY_ASSERT(res[3] == input[3] * input2[3]);

        (reg / reg2).Store4Val(res);
        MY_ASSERT(res[0] == input[0] / input2[0]);
        MY_ASSERT(res[1] == input[1] / input2[1]);
        MY_ASSERT(res[2] == input[2] / input2[2]);
        MY_ASSERT(res[3] == input[3] / input2[3]);

        reg *= reg2;
        reg.Store4Val(res);
        MY_ASSERT(res[0] == input[0] * input2[0]);
        MY_ASSERT(res[1] == input[1] * input2[1]);
        MY_ASSERT(res[2] == input[2] * input2[2]);
        MY_ASSERT(res[3] == input[3] * input2[3]);

        reg = XMMReg4Double::Load4Val(input);
        reg2 = reg;
        reg2.Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == input[3]);

        unsigned char mask[32];

        XMMReg4Double::Equals(reg, reg).StoreMask(mask);
        MY_ASSERT(mask[0] == 0xFF);
        MY_ASSERT(mask[8] == 0xFF);
        MY_ASSERT(mask[16] == 0xFF);
        MY_ASSERT(mask[24] == 0xFF);

        XMMReg4Double::NotEquals(reg, reg).StoreMask(mask);
        MY_ASSERT(mask[0] == 0);
        MY_ASSERT(mask[8] == 0);
        MY_ASSERT(mask[16] == 0);
        MY_ASSERT(mask[24] == 0);

        XMMReg4Double::Greater(reg, reg).StoreMask(mask);
        MY_ASSERT(mask[0] == 0);
        MY_ASSERT(mask[8] == 0);
        MY_ASSERT(mask[16] == 0);
        MY_ASSERT(mask[24] == 0);

        double diff[] = { 1.5, -1.5, -0.5, 0.5 };
        XMMReg4Double::Greater(reg, reg + XMMReg4Double::Load4Val(diff)).StoreMask(mask);
        MY_ASSERT(mask[0] == 0);
        MY_ASSERT(mask[8] == 0xFF);
        MY_ASSERT(mask[16] == 0xFF);
        MY_ASSERT(mask[24] == 0);

        XMMReg4Double::Min(reg, reg + XMMReg4Double::Load4Val(diff)).Store4Val(res);
        MY_ASSERT(res[0] == input[0]);
        MY_ASSERT(res[1] == input[1] + diff[1]);
        MY_ASSERT(res[2] == input[2] + diff[2]);
        MY_ASSERT(res[3] == input[3]);

        reg = XMMReg4Double::Load4Val(input);
        XMMReg4Double reg_diff = XMMReg4Double::Load4Val(diff);
        XMMReg4Double::Ternary(XMMReg4Double::Greater(reg, reg + reg_diff), reg, reg_diff).Store4Val(res);
        MY_ASSERT(res[0] == diff[0]);
        MY_ASSERT(res[1] == input[1]);
        MY_ASSERT(res[2] == input[2]);
        MY_ASSERT(res[3] == diff[3]);
    }

#ifndef USE_SSE2_EMULATION
    {
        float input[] = { -1.3f, 1.5f, 40000.3f, 65537.0f };
        GUInt16 output[4];
        GDALCopy4Words(input, output);
        MY_ASSERT(output[0] == 0);
        MY_ASSERT(output[1] == 2);
        MY_ASSERT(output[2] == 40000);
        MY_ASSERT(output[3] == 65535);
    }
#endif


#ifndef USE_SSE2_EMULATION
    {
        float input[] = { -1.3f, 1.5f, 40000.3f, 65537.0f, 40000.3f, 1.3f, 65537.0f, -1.3f };
        GUInt16 output[8];
        GDALCopy8Words(input, output);
        MY_ASSERT(output[0] == 0);
        MY_ASSERT(output[1] == 2);
        MY_ASSERT(output[2] == 40000);
        MY_ASSERT(output[3] == 65535);
        MY_ASSERT(output[4] == 40000);
        MY_ASSERT(output[5] == 1);
        MY_ASSERT(output[6] == 65535);
        MY_ASSERT(output[7] == 0);
    }

    {
        float input[] = { -1.3f, 1.5f, 40000.3f, 65537.0f, 40000.3f, 1.3f, 65537.0f, -1.3f };
        unsigned char output[8];
        GDALCopy8Words<float, unsigned char>(input, output);
        MY_ASSERT(output[0] == 0);
        MY_ASSERT(output[1] == 2);
        MY_ASSERT(output[2] == 255);
        MY_ASSERT(output[3] == 255);
        MY_ASSERT(output[4] == 255);
        MY_ASSERT(output[5] == 1);
        MY_ASSERT(output[6] == 255);
        MY_ASSERT(output[7] == 0);
    }

#endif

    return 0;
}
