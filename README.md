[![License](http://img.shields.io/badge/license-Apache_2.0-blue.svg?style=flat)](https://www.apache.org/licenses/LICENSE-2.0.html)

# swblocks-baselib

## Overview

'swblocks-baselib' is a modern C++11 library that provides a number of unique and interesting capabilities, idiomatic blocks and wrappers. One of its main goals and core capability is to provide blocks of functionality which address common development needs and concerns in idiomatic, compose-able and generic fashion, so they can be used in a generic context for both application development and for system level development. The library was originally written in C++11 and it has taken full advantage of the C++11 core language capabilities and its standard library.

## License

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this library except in compliance with the License. You may obtain a copy of the License at:

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

See the [LICENSE](LICENSE) file for additional license information

## Important design choices and principles

There are a few deliberate design choices which as a combination we believe differentiate swblocks-baselib from most other typical C++ libraries. One of the important goals of the library is to promote and facilitate these same important design choices for the applications, system components or other libraries that will be built on top of swblocks-baselib. These design choices are the following:

1. It should be very easy to use and to integrate with other libraries, projects and generally bodies of existing C++ code (see the next design choices and principles)
1. It should be very easy to create new applications and components based on swblocks-baselib without having to deal with and resolve basic development concerns and aspects such as error handling, logging, concurrency, platform abstraction, task-based programming, security, async I/O, etc
1. Minimalistic approach to both the design and implementation (i.e. [YAGNI](https://en.wikipedia.org/wiki/You_aren't_gonna_need_it)).
1. High level of internal reusability and structure (i.e. [DRY Principle](https://en.wikipedia.org/wiki/Don't_repeat_yourself))
1. The library was written in C++11 from the very beginning with a deliberate goal of not supporting pre-C++11 compilers and toolchains and in general not supporting any backward compatibility with pre-C++11 code. This provided many advantages and enabled us to have more elegant designs, simplicity, readability and generally much better code structure. The library has been used and tested extensively with representative cross-section of C++11 compliant compilers and toolchains including Visual Studio, Clang and GCC.
1. Other than the main C++11 dependency (i.e. dependency on the C++11 standard libraries and runtime) the library has very minimal and very deliberately chosen external dependencies on other C/C++ libraries and code (mainly Boost and OpenSSL).
1. Internal encapsulation of all the external dependencies within the library itself to allow us to easily switch them if necessary. E.g. to be able to switch functionality from 'boost' namespace to 'std' namespace if in the future some functionality of Boost becomes part of the C++ standard library in subsequent C++14 and C++17 standards or e.g. in the case where the Boost functionality is preferable to the functionality in the standard library for some reason. It also allows us to rewrite and / or replace the dependency entirely if necessary. The power of C++, and C++11 specifically, allows us to write such costless encapsulation layers without extra pluming or writing unnecessary wrappers and / or unnecessary abstractions.
1. Header files only physical design and code structure to enable most flexible packaging of the code and ease of use. Most importantly this physical design and organization of the code does not impose any special requirements for packaging, projects structure, build systems or compiler toolchain(s), etc - you just include the header files you need in your project and that's it.
1. The library is platform and architecture agnostic and one of its main deliberate goals is to enable and promote the implementation of new such platform and architecture agnostic libraries, system components and applications. It has been tested and used extensively on a number of platforms and architectures, including Windows, Ubuntu and RedHat, but should be very easy to make work on any other platforms. Most of the library code itself, except for a very small fraction (which is isolated in a couple of header files), is also fully platform and architecture agnostic.
1. Forced upgrade model wrt to versioning and backward compatibility (similar to Boost e.g.)

## How to use

As mentioned above in the design choices and principles section the library organization and physical design is header files only and it should be very easy to use, it does not need to be built, it does not impose any special requirements for build systems, toolchains, project structure, etc. It also has very minimal external dependencies which are either header files only wrt to physical design or they are very common and well known such and typically already part of most C++ projects (e.g. Boost and OpenSSL).

In order to use the library all you need is the following:

1. A C++11 compliant compiler toolchain. It has been extensively tested with selection of recent versions of popular modern C++ complier toolchains, but it should generally work or made easily to work with any subsequent versions. For details the currently supported compiler toolchains and their versions see [CONTRIBUTING.md](CONTRIBUTING.md) file.
1. Recent builds of Boost and OpenSSL. It has been extensively tested with recent versions of Boost and OpenSSL, but it should generally work or easily made to work with any subsequent versions of these libraries. You need to make sure that both Boost and OpenSSL libraries are on the include path in your projects / build environment and that you are also linking to them. For details the currently supported versions of Boost and OpenSSL see [CONTRIBUTING.md](CONTRIBUTING.md) file.
1. [json-spirit library](https://www.codeproject.com/Articles/20027/JSON-Spirit-A-C-JSON-Parser-Generator-Implemented). This is a header files only JSON parser implementation base on Boost.Spirit. It has been tested with version 4.08, but it should probably work fine with any subsequent versions. You need to make sure that json-spirit library is on the include path in your projects / build environment. Note that in order to down,oad the JSON Spirit library from he code project link above you need to register and login (free), but if you don't want to do that you can get it from one of the GitHub forks - e.g. one possible such fork is  [here](https://github.com/png85/json_spirit)
1. An existing build environment, IDE and / or project structure in general - whatever you currently use for your C++ projects in general, could be Visual Studio, Eclipse, XCode, Code Blocks, CLion, etc, for IDE and / or make, cmake, gradle or something else for command line build system, etc
1. Just add src/include path to your header includes in your projects / build environment.

That's it! You can now just include the code that you need and use it.

If you don't have your own C++ build / development environment and / or you want to use the one that is been used for the development of swblocks-baselib itself then check out the see the [CONTRIBUTING.md](CONTRIBUTING.md) file for more information.

## How to modify the library code, validate your changes and contribute back

See the [CONTRIBUTING.md](CONTRIBUTING.md) file for more information on how to modify the code and contribute back

## Summary of the main blocks of functionality and core capabilities

More details for its capabilities, design and architecture, implementation, examples, etc, will be provided in the coming days and weeks, but for now this is a quick high level summary of the blocks of functionality and core capabilities it provides.

* Core idiomatic error handling wrappers and utility code (based on Boost.Exception)
* Core idiomatic logging capabilities and related utility code
* Object model to support reference counted objects and interfaces (similar to IUnknown)
* Simple plugin model and related reference implementation of a file based loader (based on the object model above)
* Flexible JSON wrappers + JSON data modelling code for ORM and objects serialization
* First class unique OS specific abstractions and wrappers to uniformly isolate OS specific code (above what Boost and C++11 standard libraries provide)
* Thread pools and tasks sub-library for support async and task based concurrent designs and programming as well as for mixing sync and async execution (based on Boost.ASIO)
* Generic TCP async tasks for building scalable and secure (SSL enabled) TCP clients and TCP servers (based on tasks sub-library)
* Generic async tasks for scalable HTTP client and HTTP server code (based on tasks sub-library)
* Minimalistic async HTTP server implementation (based on tasks sub-library)
* Simple messaging protocol and related async tasks for fast transmission of data blocks (blob server protocol)
* Async tasks for building scalable messaging client and messaging servers based on the above simple messaging protocol (also based on tasks sub-library)
* A generic scalable async messaging server with plug-able authorization cache (the authorization cache is data driven and configurable and can work against arbitrary REST authorization service)
* Scalable async executor code and related wrappers to support converting sync interfaces to async (executing on thread pool)
* Async code also supports queueing and managing resources and memory which is critical for servers
* Very minimalistic and simple reactive sub-library (Rx) implementation (Observer / Observable interfaces)
* High performance generic parallel data transfer component based on the client messaging & Rx framework (based on tasks sub-library and the blob server protocol)
* High quality filesystem wrappers and utility code which provides important capabilities above Boost.Filesystem
* Minimalistic wrappers and utility code aimed at building command line pluming code (based on Boost.ProgramOptions)
* Crypto wrappers to facilitate core crypto functionality such as hashing, encryption, signing, certificates, etc (based on OpenSSL)
* Data models and wrappers for JSON crypto implementation (JOSE standard)
* Various other miscellaneous utility code such  (helpers for dealing with strings, string templates, UUDs, TLS, Base64, file encoding, time zone data, etc)
