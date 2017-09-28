package org.swblocks.baselib.test;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class JavaBridgeCommon {

    final static int PerfTest = 0;
    final static int ObjectInstanceTest = 1;

    static void perfTest(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        /*
         * Read from input buffer
         */

        final byte int8 = inputBuffer.get();
        final short int16 = inputBuffer.getShort();
        final int int32 = inputBuffer.getInt();
        final long int64 = inputBuffer.getLong();
        final String inString = readString(inputBuffer);

        /*
         * Write into output buffer
         */

        outputBuffer.put(int8);
        outputBuffer.putShort(int16);
        outputBuffer.putInt(int32);
        outputBuffer.putLong(int64);
        writeString(outputBuffer, inString.toUpperCase());
    }

    static String readString(final ByteBuffer buffer) {
        final int textSize = buffer.getInt();
        final byte[] bytes = new byte[textSize];
        buffer.get(bytes);
        return new String(bytes, StandardCharsets.UTF_8);
    }

    static void writeString(final ByteBuffer buffer, final String text) {
        buffer.putInt(text.length());
        byte[] bytes = text.getBytes(StandardCharsets.UTF_8);
        buffer.put(bytes);
    }
}
