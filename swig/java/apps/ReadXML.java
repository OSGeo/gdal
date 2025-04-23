/******************************************************************************
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
 * SPDX-License-Identifier: MIT
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
