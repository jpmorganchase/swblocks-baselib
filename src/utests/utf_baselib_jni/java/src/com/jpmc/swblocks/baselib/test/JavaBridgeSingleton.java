package com.jpmc.swblocks.baselib.test;

import java.nio.ByteBuffer;

public class JavaBridgeSingleton {

    private static JavaBridgeSingleton instance;

    private int objectIndex;

    static {
        instance = new JavaBridgeSingleton(0);
    }

    public static JavaBridgeSingleton getInstance() {
        return instance;
    }

    private JavaBridgeSingleton(final int index) {
        objectIndex = index;
    }

    public void dispatch(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        final int testCase = inputBuffer.getInt();

        if (testCase == JavaBridgeCommon.PerfTest) {
            JavaBridgeCommon.perfTest(inputBuffer, outputBuffer);
        } else if(testCase == JavaBridgeCommon.ObjectInstanceTest) {
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
