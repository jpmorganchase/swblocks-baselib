package org.swblocks.baselib.test;

import org.junit.Test;
import static org.junit.Assert.*;

public class JavaBridgeTest {
    @Test public void testJavaBridge() {
        JavaBridge javaBridge = JavaBridge.getInstance();
        assertTrue("JavaBridge.getInstance()", javaBridge != null);
    }
}
