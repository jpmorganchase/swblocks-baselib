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

        final String[] words = inString.toUpperCase().split(" ");

        final byte[] inArray = new byte[100];
        final ByteBuffer wrappedInBuffer = ByteBuffer.wrap(inArray);
        wrappedInBuffer.order(ByteOrder.nativeOrder());

        final byte[] outArray = new byte[100];
        final ByteBuffer wrappedOutBuffer = ByteBuffer.wrap(outArray);
        wrappedOutBuffer.order(ByteOrder.nativeOrder());

        ByteBuffer localInBuffer;
        ByteBuffer localOutBuffer;

        int counter = 0;
        for (final String word : words) {
            if (counter++ % 2 == 0) {
                localInBuffer = inBuffer;
                localOutBuffer = outBuffer;
            } else {
                localInBuffer = wrappedInBuffer;
                localOutBuffer = wrappedOutBuffer;
            }

            localInBuffer.clear();
            JavaBridgeCommon.writeString(localInBuffer, word);
            nativeCallback(localInBuffer, localOutBuffer, callbackId);

            final String word2 = JavaBridgeCommon.readString(localOutBuffer);
            if (!word2.equals(word + word)) {
                throw new RuntimeException("Unexpected word in the callback direct output buffer: " + word2);
            }
        }

        outBuffer.clear();
        JavaBridgeCommon.writeString(outBuffer, "Done");
    }
}
