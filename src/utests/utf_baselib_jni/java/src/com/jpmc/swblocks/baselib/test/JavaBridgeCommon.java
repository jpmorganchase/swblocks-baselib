package com.jpmc.swblocks.baselib.test;

import java.nio.ByteBuffer;

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

        final int inStringSize = inputBuffer.getInt();
        final StringBuilder sb = new StringBuilder(inStringSize);
        for (int i=0; i < inStringSize; ++i ) {
            sb.insert(i, (char)inputBuffer.get());
        }
        final String inString = sb.toString();

        /*
         * Write into output buffer
         */

        outputBuffer.put(int8);
        outputBuffer.putShort(int16);
        outputBuffer.putInt(int32);
        outputBuffer.putLong(int64);

        final String outString = inString.toUpperCase();
        writeString(outputBuffer, outString);
    }

    static void writeString(final ByteBuffer outputBuffer, final String text) {
        outputBuffer.putInt(text.length());
        for (int i=0; i < text.length(); ++i ) {
            outputBuffer.put((byte)text.charAt(i));
        }
    }
}
