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

#ifndef __BL_TASKS_PRECOMPILED_H_
#define __BL_TASKS_PRECOMPILED_H_

#include <baselib/tasks/Algorithms.h>
#include <baselib/tasks/utils/DirectoryScannerControlToken.h>
#include <baselib/tasks/ExecutionQueue.h>
#include <baselib/tasks/ExecutionQueueImpl.h>
#include <baselib/tasks/ExecutionQueueNotifyBase.h>
#include <baselib/tasks/ExecutionQueueNotify.h>
#include <baselib/tasks/utils/ScanDirectoryTask.h>
#include <baselib/tasks/SimpleTaskControlToken.h>
#include <baselib/tasks/TaskBase.h>
#include <baselib/tasks/TaskControlToken.h>
#include <baselib/tasks/Task.h>
#include <baselib/tasks/TasksIncludes.h>
#include <baselib/tasks/TasksUtils.h>
#include <baselib/tasks/TcpBaseTasks.h>
#include <baselib/tasks/utils/Pinger.h>
#include <baselib/tasks/utils/ShutdownTask.h>
#include <baselib/tasks/utils/ExcludedPathsControlToken.h>

/*
 * Optional dependencies like OpenSSL should not be added to the
 * pre-compiled headers
 *
 * #include <baselib/http/SimpleHttpSslTask.h>
 * #include <baselib/tasks/TcpSslBaseTasks.h>
 */

#endif /* __BL_TASKS_PRECOMPILED_H_ */
