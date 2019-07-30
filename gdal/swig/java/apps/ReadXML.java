/******************************************************************************
 * $Id$
 *
 * Name:     ReadXML.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app for demonstrating the usage of the XMLNode class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 * Port from ReadXML.cs by Tamas Szekeres
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 * Copyright (c) 2007, Tamas Szekeres
 *
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

import org.gdal.gdal.XMLNode;
import org.gdal.gdal.XMLNodeType;
/**

 * <p>Title: GDAL Java readxml example.</p>
 * <p>Description: A sample app for demonstrating the usage of the XMLNode class.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the usage of the XMLNode class.
/// </summary>

public class ReadXML {

    public static void usage()

    {
            System.out.println("usage example: readxml {xml string}");
            System.exit(-1);
    }

    public static void main(String[] args)
    {

        if (args.length != 1) usage();

        XMLNode node = new XMLNode(args[0]);
        PrintNode(0, node);
    }

    public static void PrintNode(int recnum, XMLNode node)
    {
        for(int i = 0; i < recnum; i++)
            System.out.print(" ");
        System.out.println("Type: " + node.getType() + "  Value: " + node.getValue());
        if (node.getChild() != null) PrintNode(recnum + 1, node.getChild());
        if (node.getNext() != null) PrintNode(recnum, node.getNext());
    }
}
