/******************************************************************************
 * $Id$
 *
 * Name:     multireadtest.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to stress-test thread-safety
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault
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
 *****************************************************************************/

import org.gdal.gdal.gdal;
import org.gdal.gdal.Dataset;

public class multireadtest implements Runnable
{
    private String _filename;

    public multireadtest(String filename)
    {
        _filename = filename;
    }

    public void run()
    {
        for(int i=0;i<100;i++)
        {
            Dataset ds = gdal.Open(_filename);
            ds.GetRasterBand(1).Checksum();
            //ds.delete();
        }
    }

    public static void main(String[] args) throws InterruptedException
    {
        String filename = args[0];

        gdal.AllRegister();

        Thread t[] = new Thread[4];
        for(int i=0;i<4;i++)
        {
            t[i] = new Thread(new multireadtest(filename));
            t[i].start();
        }
        for(int i=0;i<4;i++)
        {
            t[i].join();
        }
        //gdal.GDALDestroyDriverManager();
    }
}
