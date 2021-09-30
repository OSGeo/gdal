/*
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

// Adapted from FcUcs4ToUtf8 and FcUtf8ToUcs4() from https://github.com/freedesktop/fontconfig/blob/master/src/fcstr.c

namespace {

inline int
FcUtf8ToUcs4 (const uint8_t *src_orig,
              uint32_t      *dst,
              size_t         len)
{
    const uint8_t   *src = src_orig;
    uint8_t          s;
    size_t           extra;
    uint32_t         result;

    if (len == 0)
        return 0;

    s = *src++;
    len--;

    if (!(s & 0x80))
    {
        result = s;
        extra = 0;
    }
    else if (!(s & 0x40))
    {
        return -1;
    }
    else if (!(s & 0x20))
    {
        result = s & 0x1f;
        extra = 1;
    }
    else if (!(s & 0x10))
    {
        result = s & 0xf;
        extra = 2;
    }
    else if (!(s & 0x08))
    {
        result = s & 0x07;
        extra = 3;
    }
    else if (!(s & 0x04))
    {
        result = s & 0x03;
        extra = 4;
    }
    else if ( ! (s & 0x02))
    {
        result = s & 0x01;
        extra = 5;
    }
    else
    {
        return -1;
    }
    if (extra > len)
        return -1;

    while (extra)
    {
        --extra;
        result <<= 6;
        s = *src++;

        if ((s & 0xc0) != 0x80)
            return -1;

        result |= s & 0x3f;
    }
    *dst = result;
    return static_cast<int>(src - src_orig);
}

inline int
FcUcs4ToUtf8 (uint32_t ucs4,
              uint8_t* dest)
{
    int      bits;
    uint8_t *d = dest;

    if      (ucs4 <       0x80) {  *d++= static_cast<uint8_t>(ucs4);                          bits= -6; }
    else if (ucs4 <      0x800) {  *d++= static_cast<uint8_t>(((ucs4 >>  6) & 0x1F) | 0xC0);  bits=  0; }
    else if (ucs4 <    0x10000) {  *d++= static_cast<uint8_t>(((ucs4 >> 12) & 0x0F) | 0xE0);  bits=  6; }
    else if (ucs4 <   0x200000) {  *d++= static_cast<uint8_t>(((ucs4 >> 18) & 0x07) | 0xF0);  bits= 12; }
    else if (ucs4 <  0x4000000) {  *d++= static_cast<uint8_t>(((ucs4 >> 24) & 0x03) | 0xF8);  bits= 18; }
    else if (ucs4 < 0x80000000) {  *d++= static_cast<uint8_t>(((ucs4 >> 30) & 0x01) | 0xFC);  bits= 24; }
    else return 0;

    for ( ; bits >= 0; bits-= 6) {
        *d++= static_cast<uint8_t>(((ucs4 >> bits) & 0x3F) | 0x80);
    }
    return static_cast<int>(d - dest);
}

} // namespace
