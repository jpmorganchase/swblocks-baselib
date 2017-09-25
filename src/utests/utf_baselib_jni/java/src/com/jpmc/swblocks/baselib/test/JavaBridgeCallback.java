package com.jpmc.swblocks.baselib.test;

import java.nio.ByteBuffer;

public class JavaBridgeCallback {

    public static JavaBridgeCallback getInstance() {
        return new JavaBridgeCallback();
    }

    private static native void nativeCallback(ByteBuffer outputBuffer);

    public void dispatch(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        syncCallbackTest(inputBuffer, outputBuffer);
    }

    private void syncCallbackTest(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        final String inString = JavaBridgeCommon.readString(inputBuffer);

        String[] words = inString.toUpperCase().split(" ");

        for(String word : words) {
            JavaBridgeCommon.writeString(outputBuffer, word);
            nativeCallback(outputBuffer);
        }

        JavaBridgeCommon.writeString(outputBuffer, "Done");
    }
}
