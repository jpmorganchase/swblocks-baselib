# swblocks-baselib

'swblocks-baselib' is a modern C++11 library that provides a number unique and interesting capabilities, idiomatic blocks and wrappers. One of its main purpose and core capability is to provide blocks of functionality which address some common development needs and concerns in idiomatic, compose-able and generic fashion so they can be used in a generic context for both application development and for system level development. The library was originally written in C++11 and has taken advantage of the new core language capabilities and standard libraries of the new C++11 standard. It also has very controlled and minimalistic dependencies, with the main ones been a sub-set of Boost and also OpenSSL.

We believe this library to provide some interesting and unique design and capabilities above other well known C/C++ libraries such as Boost, POCO and others.

More details for the library, its architecture, examples, etc, will come soon, but for now this is a summary of the blocks of functionality and core capabilities it provides.

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
* A generic scalable async messaging server with pluggable authorization cache (the authorization cache is data driven and configurable and can work against arbitrary REST authorization service)
* Scalable async executor code and related wrappers to support converting sync interfaces to async (executing on thread pool)
* Async code also supports queueing and managing resources and memory which is critical for servers
* Very minimalistic and simple reactive sub-library (Rx) implementation (Observer / Observable interfaces)
* High performance generic parallel data transfer component based on the client messaging & Rx framework (based on tasks sub-library and the blob server protocol)
* High quality filesystem wrappers and utility code which provides important capabilities above Boost.Filesystem
* Minimalistic wrappers and utility code aimed at building command line pluming code (based on Boost.ProgramOptions)
* Crypto wrappers to facilitate core crypto functionality such as hashing, encryption, signing, certificates, etc (based on OpenSSL)
* Data models and wrappers for JSON crypto implementation (JOSE standard)
* Various other miscellaneous utility code such  (helpers for dealing with strings, string templates, UUDs, TLS, Base64, file encoding, time zone data, etc)

