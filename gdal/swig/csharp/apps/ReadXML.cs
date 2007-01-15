using System;

using GDAL;
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