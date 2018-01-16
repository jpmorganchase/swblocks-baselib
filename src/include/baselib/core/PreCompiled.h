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

#ifndef __BL_CORE_PRECOMPILED_H_
#define __BL_CORE_PRECOMPILED_H_

#include <build/PluginBuildId.h>

#include <baselib/core/Annotations.h>
#include <baselib/core/AppInitDone.h>
#include <baselib/core/BaseDefs.h>
#include <baselib/core/BaseIncludes.h>
#include <baselib/core/BoxedObjects.h>
#include <baselib/core/Checksum.h>
#include <baselib/core/Compiler.h>
#include <baselib/core/ConfigurationUtils.h>
#include <baselib/core/CPP.h>
#include <baselib/core/EcUtils.h>
#include <baselib/core/EndpointSelector.h>
#include <baselib/core/EndpointSelectorImpl.h>
#include <baselib/core/EnumUtils.h>
#include <baselib/core/ErrorDispatcher.h>
#include <baselib/core/ErrorHandling.h>
#include <baselib/core/FileEncoding.h>
#include <baselib/core/FsUtils.h>
#include <baselib/core/Intrusive.h>
#include <baselib/core/LoggableCounter.h>
#include <baselib/core/Logging.h>
#include <baselib/core/MessageBuffer.h>
#include <baselib/core/NetUtils.h>
#include <baselib/core/NumberUtils.h>
#include <baselib/core/ObjModelDefs.h>
#include <baselib/core/ObjModel.h>
#include <baselib/core/OS.h>
#include <baselib/core/PathUtils.h>
#include <baselib/core/PluginDefs.h>
#include <baselib/core/PoolAllocatorDefault.h>
#include <baselib/core/Pool.h>
#include <baselib/core/ProgramOptions.h>
#include <baselib/core/RangeUtils.h>
#include <baselib/core/Random.h>
#include <baselib/core/RefCountedBase.h>
#include <baselib/core/SerializationUtils.h>
#include <baselib/core/StringUtils.h>
#include <baselib/core/ThreadPool.h>
#include <baselib/core/ThreadPoolImpl.h>
#include <baselib/core/TimeUtils.h>
#include <baselib/core/TlsState.h>
#include <baselib/core/Utils.h>
#include <baselib/core/Uuid.h>
#include <baselib/core/UuidIterator.h>
#include <baselib/core/UuidIteratorImpl.h>

/*
 * Optional dependencies like OpenSSL should not be added to the
 * pre-compiled headers
 *
 * #include <baselib/core/AsioSSL.h>
 * #include <baselib/core/JsonUtils.h>
 * #include <baselib/core/XmlUtils.h>
 */

#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#endif /* __BL_CORE_PRECOMPILED_H_ */
