package com.jpmc.swblocks.baselib.test;

import java.nio.ByteBuffer;

public class JavaBridge {

    static public JavaBridge newInstance() {
        return new JavaBridge();
    }

    public void dispatch(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        /*
         * Read from input buffer
         */

        final byte int8 = inputBuffer.get();
        final short int16 = inputBuffer.getShort();
        final int int32 = inputBuffer.getInt();
        final long int64 = inputBuffer.getLong();

        final int inStringSize = inputBuffer.getInt();
        final StringBuilder sb = new StringBuilder(inStringSize);
        for(int i=0; i < inStringSize; ++i ) {
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
        outputBuffer.putInt(outString.length());
        for(int i=0; i < outString.length(); ++i ) {
            outputBuffer.put((byte)outString.charAt(i));
        }
    }
}
