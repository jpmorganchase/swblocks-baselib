package org.swblocks.baselib.test;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class JavaBridgeCallback {

    public static JavaBridgeCallback getInstance() {
        return new JavaBridgeCallback();
    }

    private native void nativeCallback(ByteBuffer inBuffer, ByteBuffer outBuffer, long callbackId);

    public void dispatch(final ByteBuffer inBuffer, final ByteBuffer outBuffer, final long callbackId) {
        syncCallbackTest(inBuffer, outBuffer, callbackId);
    }

    private void syncCallbackTest(final ByteBuffer inBuffer, final ByteBuffer outBuffer, final long callbackId) {
        final String inString = JavaBridgeCommon.readString(inBuffer);

        String[] words = inString.toUpperCase().split(" ");

        byte[] inArray = new byte[100];
        ByteBuffer localInBuffer = ByteBuffer.wrap(inArray);
        localInBuffer.order(ByteOrder.nativeOrder());

        byte[] outArray = new byte[100];
        ByteBuffer localOutBuffer = ByteBuffer.wrap(outArray);
        localOutBuffer.order(ByteOrder.nativeOrder());

        int counter = 0;
        for (String word : words) {
            if (counter++ % 2 == 0) {
                inBuffer.clear();
                JavaBridgeCommon.writeString(inBuffer, word);
                nativeCallback(inBuffer, outBuffer, callbackId);

                String word2 = JavaBridgeCommon.readString(outBuffer);
                if (!word2.equals(word + word)) {
                    throw new RuntimeException("Unexpected word in the callback direct output buffer: " + word2);
                }
            } else {
                localInBuffer.clear();
                JavaBridgeCommon.writeString(localInBuffer, word);
                nativeCallback(localInBuffer, localOutBuffer, callbackId);

                String word2 = JavaBridgeCommon.readString(localOutBuffer);
                if (!word2.equals(word + word)) {
                    throw new RuntimeException("Unexpected word in the callback indirect output buffer: " + word2);
                }
            }
        }

        outBuffer.clear();
        JavaBridgeCommon.writeString(outBuffer, "Done");
    }
}
