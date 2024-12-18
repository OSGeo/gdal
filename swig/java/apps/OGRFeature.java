/******************************************************************************
 *
 * Name:     OGRFeature.java
 * Project:  OGR Java Interface
 * Purpose:  A sample app for demonstrating the caveats with JNI and garbage collecting...
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import org.gdal.ogr.Feature;
import org.gdal.ogr.FeatureDefn;

/* The nasty things below are necessary if the Feature class has a finalize() */
/* method, which is no longer the case. See OGRTestGC.java */

public class OGRFeature
{
    static int i;
    static boolean doCleanup = false;
    static boolean bHasFinished = false;
    static FeatureDefn featureDefn;
    static Object mutex = new Object();

    public static void main(String[] args)
    {
        featureDefn = new FeatureDefn();

        new Thread() {
            public void run()
            {
                while(!bHasFinished)
                {
                    if (doCleanup)
                    {
                        int refBefore, refAfter;
                        while(true)
                        {
                            refBefore = featureDefn.GetReferenceCount();
                            System.out.print("i=" + i + " ref(before)="+ refBefore );
                            System.runFinalization();
                            refAfter = featureDefn.GetReferenceCount();
                            System.out.println(" ref(after)="+ refAfter );
                            if (refBefore == refAfter)
                                System.gc();
                            else
                                break;
                        }
                        synchronized (mutex) {
                            doCleanup = false;
                            mutex.notify();
                        }
                    }

                    synchronized (mutex) {
                        while (doCleanup == false && bHasFinished == false)
                        {
                            //System.out.println("thread wakeup");
                            try
                            {
                                mutex.wait();
                            }
                            catch(InterruptedException ie)
                            {
                                System.out.println("InterruptedException");
                            }
                        }
                    }
                }
            }
        }.start();

        // Add features
        for (i = 0; i < 5000000; i++)
        {
            new Feature(featureDefn);

            if ((i % 100000) == 0)
            {
                /* Due to the fact that the Feature class has a finalize() method */
                /* the garbage collector will differ finalization. So we have to do */
                /* wait that the cleanup thread has forced finalizations */
                while(featureDefn.GetReferenceCount() > 100000)
                {
                    //System.out.println("waiting for cleanup");
                    synchronized (mutex) {
                        doCleanup = true;
                        mutex.notify();
                        while (doCleanup)
                        {
                            //System.out.println("main thread wakeup");
                            try
                            {
                                mutex.wait();
                            }
                            catch(InterruptedException ie)
                            {
                                System.out.println("InterruptedException");
                            }
                        }
                    }
                }
            }
        }

        synchronized (mutex) {
            bHasFinished = true;
            mutex.notify();
        }

        System.out.print("i=" + i + " ref(before)="+ featureDefn.GetReferenceCount() );
        System.gc();
        System.runFinalization();
        System.out.println(" ref(after)="+ featureDefn.GetReferenceCount() );
    }
}
