/*
 * This file is part of the swblocks-baselib library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

            final String word2 = JavaBridgeCommon.readString(localOutBuffer);
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
