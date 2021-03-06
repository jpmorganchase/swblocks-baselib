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
import java.util.concurrent.atomic.AtomicInteger;

public class JavaBridge {

    private static AtomicInteger objectIndexCounter = new AtomicInteger(-1);

    private int objectIndex;

    private JavaBridge(final int index) {
        objectIndex = index;
    }

    public static JavaBridge getInstance() {
        final int index = objectIndexCounter.incrementAndGet();
        return new JavaBridge(index);
    }

    public void dispatch(final ByteBuffer inputBuffer, final ByteBuffer outputBuffer) {
        final int testCase = inputBuffer.getInt();

        if (testCase == JavaBridgeCommon.PerfTest) {
            JavaBridgeCommon.perfTest(inputBuffer, outputBuffer);
        } else if (testCase == JavaBridgeCommon.ObjectInstanceTest) {
            writeObjectDetails(outputBuffer);
        } else {
            throw new RuntimeException("Invalid test case: " + testCase);
        }
    }

    private void writeObjectDetails(final ByteBuffer outputBuffer) {
        final String className = this.getClass().getName();
        JavaBridgeCommon.writeString(outputBuffer, className);
        outputBuffer.putInt(objectIndex);
    }
}
