using SkiaSharp;
using System.IO;
using System.Drawing;
using System;

enum PixelFormat
{
    Alpha,
    Canonical,
    DontCare,
    Extended,
    Format16bppArgb1555,
    Format16bppGrayScale,
    Format16bppRgb555,
    Format16bppRgb565,
    Format1bppIndexed,
    Format24bppRgb,
    Format32bppArgb,
    Format32bppPArgb,
    Format32bppRgb,
    Format48bppRgb,
    Format4bppIndexed,
    Format64bppPArgb,
    Format64bppArgb,
    Format8bppIndexed,
    Gdi,
    Indexed,
    Max,
    PAlpha,
    Undefined
}

enum ImageLockMode
{
    ReadOnly,
    ReadWrite,
    UserInputBuffer,
    WriteOnly
}

class BitmapData {
    public IntPtr Scan0;
    public Int32 Stride;
}

class ColorPalette
{
    public Color[] Entries = new Color[256];
}

class Bitmap : SKBitmap
{
    public ColorPalette Palette
    {
        get { return new ColorPalette(); } 
        set { }
    }

    public Bitmap(Int32 width, Int32 height, PixelFormat pxformat) : base(width, height) { }

    public Bitmap Clone()
    {
        return (Bitmap)Copy();
    }

    public void SetPixel(int x, int y, Color color)
    {
        base.SetPixel(x, y, new SKColor((uint)color.ToArgb()));
    }

    public BitmapData LockBits(System.Drawing.Rectangle rect, ImageLockMode flags, PixelFormat format)
    {
        return new BitmapData() { Scan0 = this.GetPixels(), Stride = this.RowBytes };
    }

    public void UnlockBits(BitmapData data) { }

    public void Save(string filename)
    {
        using (MemoryStream memStream = new MemoryStream())
        using (SKManagedWStream wstream = new SKManagedWStream(memStream))
        {
            Encode(wstream, SKEncodedImageFormat.Bmp, 1);
            byte[] data = memStream.ToArray();

            File.WriteAllBytes(filename, data);
        }
    }
}

