
## Contributing

Thank you for your interest in contributing to swblocks-baselib!

swblocks-baselib is built on and intended for open source and we fully intend to accept public contributions in the near future. Until then, feel free to file issues and open pull requests, but note that we won't be merging them until the necessary processes are in place.

If you are currently J.P. Morgan Chase employee and interested in contributing you can contact [Lazar Ivanov](https://github.com/lazar-ivanov) about more details on the process and governance of contributions from current J.P. Morgan Chase employees.

The swblocks-baselib library comes with a comprehensive unit tests suite and virtually all parts of it are very extensively tested. It also comes currently with a couple of application binaries - 'bl-tool' and 'bl-messaging-broker'.

In order to be able to make changes to the library code and contribute these back you will need to be able to modify and add to the tests code suite and also of course be able to build and run the unit tests suite, etc. The way you can do this is by using the swblocks-baselib development environment and its own build system. The library comes also with its own flexible make files and build + test system which allows you to build these binaries and unit tests and to run the unit tests. To learn how to use the swblocks-baselib library development environment and build system look at the next section.

## Development environment and build system

The library development environment is based on GNU make, but it has somewhat sofisticated structure and implementation that is [declarative and build-by-convention](https://docs.gradle.org/current/userguide/overview.html) and of course also avoids the [pitfalls of recursive make](https://www.google.com/#q=recursive+make+considered+harmful). The development environment and the build system assumes certain structure of dependencies and how they are built. This structure and its assumptions are embedded in the make files and the steps to build and prepare the external dependencies are described in the various notes files (under notes folder in the root). As mentioned in the [README.md](README.md) file the swblocks-baselib library and its development environment and build system has very limited dependencies, namely C++11 compliant compiler and toolchain (e.g. recent versions of Clang, GCC or VC++), Boost, OpenSSL and lastly a small JSON parser library which is header files only and does not need to be built (JSON Spirit 4.08), so preparing the dependencies for the development environment is relatively easy, but it is not automated (you have to follow the instructions in the notes files).

The development environment and build system has a minimal notion of versioning to facilitate evolution without having to break the world, but it generally requires "forced upgrade" model where older versions are quickly depreceiated and the users / clients of the library are expected to upgrade and move forward. That applies to both the development environment and the code itsef. The code of course is changed carefully with backward compatibility in mind and older versions of the developemt environment or dependencies are not explicitly broken or not supported, but they might be, so if you are a user of the library and you can't upgrade easily at some point you might not be able to pick up the latest version of swblocks-baselib library and might have to stay on older version - btw, the model is very similar to the way Boost is versioned and how backward compatibility is maintained there.

A development environment version is a notion of collection of specific versions of compiler toolchains + a collection of compatible versions of the 3rd party dependencies. Currently swblocks-baselib library officially supports two developmnent environment versions - devenv3 (latest) and devenv2 (old) with the devenv2 environment of course to be eventually depreciated in the somwhat near future (probably within a year or so). Here are the collections of the compiler toolchains versions and the 3rd party dependencies versions for each development environment:

* **devenv2**
  * Operating Systems
    * Linux (RedHat rhel5, rhel6, rhel7, Ubuntu 12.04, 14.04, 16.04)
    * Windows (7, 8.x, 10 - both x86 and x64)
  * Compilers
    * GCC 4.9
    * Clang 3.5
    * vc12 (Visual C++ 2013)
  * C++11 standard library implementations
    * libstdc++ for GCC and Clang
    * msvcrt for vc12
  * Boost 1.58
  * OpenSSL 1.0.2d
  * JSON Spirit 4.08
* **devenv3**
  * Operating Systems
    * Darwin (15.x, 16.x - i.e. El Capitan and macOS Sierra)
    * Linux (RedHat rhel5, rhel6, rhel7, Ubuntu 12.04, 14.04, 16.04)
    * Windows (7, 8.x, 10 - both x86 and x64)
  * Compilers
    * GCC 6.3
    * Clang 3.9
    * Clang 7.3 (for Darwin)
    * vc14 (Visual C++ 2015)
  * C++11 standard library implementations
    * libc++ (for Darwin)
    * libstdc++ for GCC and Clang
    * msvcrt for vc14
  * Boost 1.63
  * OpenSSL 1.1.0d
  * JSON Spirit 4.08

## Development environment distributions and links

In addition to the code dependencies of the library itself (compiler toolchain, boost, openssl, JSON spirit) the development environment also has few aditional dependencies such as GNU make (e.g. via msys on Windows), git, python, etc, plus some optional such as Eclipse CDT (to use C++ indexer and IDE). The "binary blob" that contains the pre-built versions of the development environment with all code dependencies plus the additional and optional such is called "devenv distribution". Some poluplar combinations of devenv distributions were prebuilt and uploaded on Google drive, so they can be downloaded directly and facilitate the development and ease of use of the swblocks-baselib library. Here is the list of the currently supported devenv distributions with the link of where to download them from (more will be coming in the future):

* **devenv3** for Windows (Win7 - Win10, including latest Win10 creator's edition, SDK 8.1 and 10), vc14 (VS 2015) can be downloaded from [here](https://drive.google.com/open?id=0B7R8Vjb6OGTHcmNOUjJOTHRWLWs)
* **devenv3** for Darwin (d156 - 15.x - 16.x, El Capitan and macOS Sierra), clang 7.3, libc++ can be downloaded [here](https://drive.google.com/open?id=0B7R8Vjb6OGTHTzB3WG1RTjNfWE0)
* **devenv3** for Ubuntu 16.04 LTS with GCC 6.3 and Clang 3.9 can be downloaded [here](https://drive.google.com/open?id=0B7R8Vjb6OGTHVWpXRnR3LVZYbUk)

All links above are either .zip file (for Windows) or a .gz tar file for Darwin and Linux. Once they are downloaded they can be extracted into some location (e.g. **c:\\swblocks** for Windows or **/Users/userid/swblocks** for Darwin and Linux) and then once you clone the swblocks-baselib library from github in order to use it you simply need to create a small .mk file in **projects/make** folder called **ci-init-env.mk** and point the 3 _DIST_ roots to the place where you have extracted the development environment blob.

E.g. if you have extracted the development environment into **c:\\swblocks** then the **projects/make/ci-init-env.mk** can look the following way for Windows:

```make
# initialize the important env roots

DIST_ROOT_DEPS1 = /c/swblocks/dist-devenv3-windows
DIST_ROOT_DEPS2 = /c/swblocks/dist-devenv3-windows
DIST_ROOT_DEPS3 = /c/swblocks/dist-devenv3-windows
```

Note that on Windows you should use UNIX style paths since we use GNU make (via msys)

E.g. **projects/make/ci-init-env.mk** can look the following way for Darwin (or you can adjust for Linux):

```make
# initialize the important env roots

DIST_ROOT_DEPS1 = /Users/userid/swblocks/dist-devenv3-darwin-15.6
DIST_ROOT_DEPS2 = /Users/userid/swblocks/dist-devenv3-darwin-15.6
DIST_ROOT_DEPS3 = /Users/userid/swblocks/dist-devenv3-darwin-15.6
```

Once you create unpack the development environment distribution and create the **projects/make/ci-init-env.mk** as per above you can now build the code run tests by specifying the targets, etc. The unit test targets start with **utf_baselib** prefix (e.g. utf_baselib, utf_baselib_data) and the respective test targets start with **test_utf_baselib**. The other targets are the respective binary names and there are special targets too, just type make help for more information.

Here are some examples e.g.:

```make
make utf_baselib
make test_utf_baselib_data
make -k -j6
make -k -j6 && make -k -j8 test
make help
make -k -j6 && make -k -j8 test && make install
```
