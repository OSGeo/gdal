/*
Copyright 2015 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
A local copy of the license and additional notices are located with the
source distribution at:
http://github.com/Esri/lerc/
Contributors:  Thomas Maurer
    Lucian Plesea 2014

Despite the file name, only the RLE codec of a bitmask is in this file
The LERC RLE encoding is the one designed by Thomas Maurer
In the encoded stream there are two types of sequences and an end marker
- A repeating byte sequence, starts with a negative, low endian 16bit count,
    between 5 and 32767, followed by the repeated byte value
- A non-repeating sequence, starts with a positive, low endian 16bit count,
    between 1 and 32767, followed by count bytes
- Count of -32768 is the end marker
0 to 4 byte repeats are not encoded as repeated byte sequence
*/

#include "BitMask.h"
#include <cassert>

NAMESPACE_LERC_START

#define MAX_RUN 32767
#define MIN_RUN 5
// End of Transmission
#define EOT -(MAX_RUN + 1)

// Decode a RLE bitmask, size should be already set
// Returns false if input seems wrong
// Not safe if fed garbage !!!
// Zero size mask is fine, only checks the end marker
bool BitMask::RLEdecompress(const Byte* src) {
    Byte *dst = m_pBits;
    int sz = Size();
    short int count;
    assert(src);

// Read a two bye short int
#define READ_COUNT if (true) { count = *src++; count += (*src++ << 8); }

    while (sz) { // One sequence per loop
	READ_COUNT;
	if (count < 0) { // negative count for repeats
	    Byte b = *src++;
	    sz += count;
	    while (0 != count++)
		*dst++ = b;
	} else { // No repeats
	    sz -= count;
	    while (0 != count--)
		*dst++ = *src++;
	}
    }
    READ_COUNT;
    return (count == EOT);
}

// Encode helper function
// It returns how many times the byte at *s is repeated
// a value between 1 and min(max_count, MAX_RUN)
inline static int run_length(const Byte *s, int max_count)
{
    assert(max_count && s);
    if (max_count > MAX_RUN)
	max_count = MAX_RUN;
    const Byte c = *s++;
    for (int count = 1; count < max_count; count++)
	if (c != *s++)
	    return count;
    return max_count;
}

//
// RLE compressed size is bound by n + 4 + 2 * (n - 1) / 32767
//
int BitMask::RLEcompress(Byte *dst) const {
    assert(dst);
    // Next input byte
    Byte *src = m_pBits;
    Byte *start = dst;
    // left to process
    int sz = Size();

    // Pointer to current sequence count
    Byte *pCnt = dst;
    // non-repeated byte count
    int oddrun = 0;

// Store a two byte count in low endian
#define WRITE_COUNT(val) if (true) { *pCnt++ = Byte(val & 0xff); *pCnt++ = Byte(val >> 8); }
// Flush an existing odd run
#define FLUSH if (oddrun) { WRITE_COUNT(oddrun); pCnt += oddrun; dst = pCnt + 2; oddrun = 0; }

    dst += 2; // Skip the space for the first count
    while (sz) {
	int run = run_length(src, sz);
	if (run < MIN_RUN) { // Use one byte
	    *dst++ = *src++;
	    sz--;
	    if (MAX_RUN == ++oddrun)
		FLUSH;
	} else { // Found a run
	    FLUSH;
	    WRITE_COUNT(-run);
	    *pCnt++ = *src;
	    src += run;
	    sz -= run;
	    dst = pCnt + 2; // after the next marker
	}
    }
    FLUSH;
    WRITE_COUNT(EOT); // End marker
    // return compressed output size
    return int(pCnt - start);
}

// calculate encoded size
int BitMask::RLEsize() const {
    // Next input byte
    Byte *src = m_pBits;
    // left to process
    int sz = Size();
    // current non-repeated byte count
    int oddrun = 0;
    // Simulate an odd run flush
#define SIMFLUSH if (oddrun) { osz += oddrun + 2; oddrun = 0; }
    // output size, start with size of end marker
    int osz = 2;
    while (sz) {
	int run = run_length(src, sz);
	if (run < MIN_RUN) {
	    src++;
	    sz--;
	    if (MAX_RUN == ++oddrun)
		SIMFLUSH;
	} else {
	    SIMFLUSH;
	    src += run;
	    sz -= run;
	    osz += 3; // Any run is 3 bytes
	}
    }
    SIMFLUSH;
    return osz;
}

NAMESPACE_LERC_END
