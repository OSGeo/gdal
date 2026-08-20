import org.gdal.osr.CoordinateTransformation;
import org.gdal.osr.SpatialReference;
import org.gdal.osr.osrConstants;

/**
 * <p>Title: OSRTransform2Test</p>
 * <p>Description: Quick test for the new GetSource()/GetTarget() bindings on
 * CoordinateTransformation</p>
 * @author Tom Moore
 * @version 1.0
 */
public class OSRTransform2Test {

    public static void main(String[] args) {
        System.out.println("running GetSource/GetTarget test...\n");

        // 1. Instantiate 2 SpatialReference objects.
        SpatialReference srcSrs = new SpatialReference();
        srcSrs.ImportFromEPSG(4326); // WGS 84
        srcSrs.SetAxisMappingStrategy(osrConstants.OAMS_TRADITIONAL_GIS_ORDER);

        SpatialReference dstSrs = new SpatialReference();
        dstSrs.ImportFromEPSG(26911); // NAD83 / UTM zone 11N
        dstSrs.SetAxisMappingStrategy(osrConstants.OAMS_TRADITIONAL_GIS_ORDER);

        // 2. Build the transformation from them.
        CoordinateTransformation transform =
                CoordinateTransformation.CreateCoordinateTransformation(srcSrs, dstSrs);
        if (transform == null) {
            throw new RuntimeException("Could not create coordinate transformation");
        }

        // 3. Exercise the new bindings.
        SpatialReference gotSource = transform.GetSource();
        SpatialReference gotTarget = transform.GetTarget();

        String sourceCode = gotSource.GetAuthorityCode(null);
        String targetCode = gotTarget.GetAuthorityCode(null);

        System.out.println("GetSource() -> EPSG:" + sourceCode
                + "  (expected 4326)  " + (("4326".equals(sourceCode)) ? "PASS" : "FAIL"));
        System.out.println("GetTarget() -> EPSG:" + targetCode
                + "  (expected 26911) " + (("26911".equals(targetCode)) ? "PASS" : "FAIL"));

        // 4. Confirm these are cloned, independent objects
        boolean sourceIsIndependent = (gotSource != srcSrs);
        boolean targetIsIndependent = (gotTarget != dstSrs);

        System.out.println("GetSource() returns a distinct object: "
                + (sourceIsIndependent ? "PASS" : "FAIL"));
        System.out.println("GetTarget() returns a distinct object: "
                + (targetIsIndependent ? "PASS" : "FAIL"));

        // Content equality, via IsSame()
        int sourceIsSame = gotSource.IsSame(srcSrs);
        int targetIsSame = gotTarget.IsSame(dstSrs);

        System.out.println("GetSource() is the SAME CRS as srcSrs (IsSame): "
                + (sourceIsSame == 1 ? "PASS" : "FAIL") + "  (IsSame=" + sourceIsSame + ")");
        System.out.println("GetTarget() is the SAME CRS as dstSrs (IsSame): "
                + (targetIsSame == 1 ? "PASS" : "FAIL") + "  (IsSame=" + targetIsSame + ")");

        // Negative control: two genuinely different CRS should NOT be "same".
        int sourceVsTargetSame = srcSrs.IsSame(dstSrs);
        System.out.println("srcSrs vs dstSrs correctly NOT the same CRS: "
                + (sourceVsTargetSame == 0 ? "PASS" : "FAIL") + "  (IsSame=" + sourceVsTargetSame + ")");

        // Mutate the returned clone and confirm the original is untouched.
        gotSource.ImportFromEPSG(3857); // deliberately corrupt the clone
        String srcCodeAfterMutation = srcSrs.GetAuthorityCode(null);
        System.out.println("Original srcSrs unaffected by mutating the clone: "
                + ("4326".equals(srcCodeAfterMutation) ? "PASS" : "FAIL")
                + "  (srcSrs is still EPSG:" + srcCodeAfterMutation + ")");

        // 5. exercise GetInverse() too, since it follows the same
        //    %newobject/Clone-vs-borrowed pattern.
        CoordinateTransformation inverse = transform.GetInverse();
        System.out.println("GetInverse() returned non-null: "
                + (inverse != null ? "PASS" : "FAIL"));

        System.out.println("\nDone.");
    }
}