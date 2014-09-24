/******************************************************************************
 * $Id$
 *
 * Name:     ReadXML.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the usage of the XMLNode class.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
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

using System;

using OSGeo.GDAL;
/**

 * <p>Title: GDAL C# readxml example.</p>
 * <p>Description: A sample app for demonstrating the usage of the XMLNode class.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the usage of the XMLNode class.
/// </summary> 

class ReadXML {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage example: readxml {xml string}");
		System.Environment.Exit(-1);
	}
 
	public static void Main(string[] args) {

		if (args.Length != 1) usage();

        XMLNode node = new XMLNode(args[0]);
        
        PrintNode(0, node);
	}

    public static void PrintNode(int recnum, XMLNode node)
    {
        Console.WriteLine(new String(' ', recnum) + "Type: " + node.Type +
            "  Value: " + node.Value);
        if (node.Child != null) PrintNode(recnum + 1, node.Child);
        if (node.Next != null) PrintNode(recnum, node.Next);
    }
}