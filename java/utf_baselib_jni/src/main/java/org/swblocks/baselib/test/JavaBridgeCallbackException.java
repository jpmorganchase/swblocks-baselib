package org.swblocks.baselib.test;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class JavaBridgeCallbackException {

    public static JavaBridgeCallbackException getInstance() {
        return new JavaBridgeCallbackException();
    }

    private native void nativeCallback(ByteBuffer inBuffer, ByteBuffer outBuffer, long callbackId) throws JniException;

    public void dispatch(final ByteBuffer inBuffer, final ByteBuffer outBuffer, final long callbackId) {
        syncCallbackTest(inBuffer, outBuffer, callbackId);
    }

    private void syncCallbackTest(final ByteBuffer inBuffer, final ByteBuffer outBuffer, final long callbackId) {
        final String inString = JavaBridgeCommon.readString(inBuffer);

        String[] words = inString.toUpperCase().split(" ");

        byte[] inArray = new byte[100];
        ByteBuffer wrappedInBuffer = ByteBuffer.wrap(inArray);
        wrappedInBuffer.order(ByteOrder.nativeOrder());

        byte[] outArray = new byte[100];
        ByteBuffer wrappedOutBuffer = ByteBuffer.wrap(outArray);
        wrappedOutBuffer.order(ByteOrder.nativeOrder());

        ByteBuffer localInBuffer;
        ByteBuffer localOutBuffer;

        int counter = 0;
        for (String word : words) {
            if (counter++ % 2 == 0) {
                localInBuffer = inBuffer;
                localOutBuffer = outBuffer;
            } else {
                localInBuffer = wrappedInBuffer;
                localOutBuffer = wrappedOutBuffer;
            }

            localInBuffer.clear();
            JavaBridgeCommon.writeString(localInBuffer, word);

            String exceptionString = null;
            try {
                nativeCallback(localInBuffer, localOutBuffer, callbackId);
            } catch (JniException exc) {
                exceptionString = exc.getMessage();
            }

            if (word.equals("STD::RUNTIME_ERROR")) {
                if (exceptionString == null || !exceptionString.equals("Exception from JNI native callback")) {
                    throw new RuntimeException("Expected exception from native callback");
                }
            } else {
                if (exceptionString != null) {
                    throw new RuntimeException("Unexpected exception from native callback: " + exceptionString);
                }
            }

            if (exceptionString != null) {
                continue;
            }

            String word2 = JavaBridgeCommon.readString(localOutBuffer);
            if (!word2.equals(word + word)) {
                throw new RuntimeException("Unexpected word in the callback direct output buffer: " + word2);
            }
        }

        outBuffer.clear();
        JavaBridgeCommon.writeString(outBuffer, "Done");
    }

    static class JniException extends Exception {

        public JniException(String message) {
            super(message);
        }
    }
}
