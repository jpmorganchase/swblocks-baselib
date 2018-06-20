package org.swblocks.baselib.test;

import java.nio.ByteBuffer;

/**
 * A test rest server helper to allow for testing of JavaBridgeRestHelper.h
 */

public class JavaBridgeRestTestServer {
    private static final JavaBridgeRestTestServer instance = new JavaBridgeRestTestServer();

    private JavaBridgeRestTestServer() {
    }

    /**
     * This well-known static method must be implemented so that the class can be instantiated in native code,
     * see bl::jni::JavaBridge C++ class.
     */

    public static JavaBridgeRestTestServer getInstance() {
        return instance;
    }

    /**
     * This well-known method must be implemented so that the provided functionality can be invoked from native code,
     * see bl::jni::JavaBridge C++ class.
     */

    public void dispatch(final ByteBuffer input, final ByteBuffer output, final long callbackId) {
        /*
         * Native code calls ByteBuffer.flip() method before reading it hence
         * the position should be set to its size to avoid size becoming 0
         */

        input.position(input.limit());

        /**
         * Just pass it back to the native side as is
         */

        nativeCallback(input, output, callbackId);

        /**
         * Copy the input to the output buffer
         */

        output.limit(input.limit());
        input.rewind();
        output.rewind();
        output.put(input);
    }

    /**
     * This well-known method must be declared so that JVM can call directly a native function,
     * see bl::jni::JavaBridge C++ class.
     */

    public native void nativeCallback(final ByteBuffer input, final ByteBuffer output, final long callbackId);

    /**
     * Note: do not remove this exception, it is used from the native side and it is expected to be defined this way.
     */

    private static class JniException extends RuntimeException {
        public JniException(String message) {
            super(message);
        }
    }
}
