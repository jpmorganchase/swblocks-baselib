package org.swblocks.baselib.test;

import java.nio.ByteBuffer;

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

        int counter = 0;
        for (String word : words) {
            inBuffer.clear();
            JavaBridgeCommon.writeString(inBuffer, word);

            String exceptionString = null;
            try {
                nativeCallback(inBuffer, outBuffer, callbackId);
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

            String word2 = JavaBridgeCommon.readString(outBuffer);
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
