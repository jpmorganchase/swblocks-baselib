package com.jpmc.swblocks.baselib.test;

import java.nio.ByteBuffer;

public class JavaBridge {

    static public void dispatch(ByteBuffer inputBuffer, ByteBuffer outputBuffer) throws Exception {
        /*
         * Read from input buffer
         */

        byte int8 = inputBuffer.get();
        short int16 = inputBuffer.getShort();
        int int32 = inputBuffer.getInt();
        long int64 = inputBuffer.getLong();

        int inStringSize = inputBuffer.getInt();
        byte[] inStringBytes = new byte[inStringSize];
        inputBuffer.get(inStringBytes);
        String inString = new String(inStringBytes, "UTF-8");

        /*
         * Write into output buffer
         */

        outputBuffer.put(int8);
        outputBuffer.putShort(int16);
        outputBuffer.putInt(int32);
        outputBuffer.putLong(int64);

        String outString = inString.toUpperCase();
        outputBuffer.putInt(outString.length());
        byte[] outStringBytes = outString.getBytes("UTF-8");
        outputBuffer.put(outStringBytes);
    }
}
