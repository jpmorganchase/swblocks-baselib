package org.swblocks.baselib.test;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;

public class JavaBridge {

    private static AtomicInteger objectIndexCounter = new AtomicInteger(-1);

    private int objectIndex;

    private JavaBridge(final int index) {
        objectIndex = index;
    }

    public static JavaBridge getInstance() {
        final int index = objectIndexCounter.incrementAndGet();
        return new JavaBridge(index);
    }

    public void dispatch(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        final int testCase = inputBuffer.getInt();

        if (testCase == JavaBridgeCommon.PerfTest) {
            JavaBridgeCommon.perfTest(inputBuffer, outputBuffer);
        } else if (testCase == JavaBridgeCommon.ObjectInstanceTest) {
            writeObjectDetails(outputBuffer);
        } else {
            throw new RuntimeException("Invalid test case: " + testCase);
        }
    }

    private void writeObjectDetails(final ByteBuffer outputBuffer) {
        final String className = this.getClass().getName();
        JavaBridgeCommon.writeString(outputBuffer, className);
        outputBuffer.putInt(objectIndex);
    }
}
