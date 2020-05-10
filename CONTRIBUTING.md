
## Contributing

Thank you for your interest in contributing to swblocks-baselib!

This page describes the details for the most recent versions of compiler toolchains and library dependencies (devenv4). You can see [CONTRIBUTING.DEVENV3.md](CONTRIBUTING.DEVENV3.md) file for details on the previous version of the supported development environments (devenv3).

swblocks-baselib is built on and intended for open source and we fully intend to accept public contributions in the near future. Until then, feel free to file issues and open pull requests, but note that we won't be merging them until the necessary processes are in place.

If you are currently J.P. Morgan Chase employee and interested in contributing you can contact [Lazar Ivanov](https://github.com/lazar-ivanov) about more details on the process and governance of contributions from current J.P. Morgan Chase employees.

The swblocks-baselib library comes with a comprehensive unit tests suite and virtually all parts of it are very extensively tested. It also comes currently with a couple of application binaries - 'bl-tool' and 'bl-messaging-broker'.

In order to be able to make changes to the library code and contribute these back you will need to be able to modify and add to the tests code suite and also of course be able to build and run the unit tests suite, etc. The way you can do this is by using the swblocks-baselib development environment and its own build system. The library comes also with its own flexible make files and build + test system which allows you to build these binaries and unit tests and to run the unit tests. To learn how to use the swblocks-baselib library development environment and build system look at the next section.

## Development environment and build system

The library development environment is based on GNU make, but it has somewhat sophisticated structure and implementation that is [declarative and build-by-convention](https://docs.gradle.org/current/userguide/overview.html) and of course also avoids the [pitfalls of recursive make](https://www.google.com/#q=recursive+make+considered+harmful). The development environment and the build system assumes certain structure of dependencies and how they are built. This structure and its assumptions are embedded in the make files and the steps to build and prepare the external dependencies are described in the various notes files (under notes folder in the root). As mentioned in the [README.md](README.md) file the swblocks-baselib library and its development environment and build system has very limited dependencies, namely C++11 compliant compiler and toolchain (e.g. recent versions of Clang, GCC or VC++), Boost, OpenSSL and lastly a small JSON parser library which is header files only and does not need to be built (JSON Spirit 4.08), so preparing the dependencies for the development environment is relatively easy, but it is not automated (you have to follow the instructions in the notes files).

The development environment and build system has a minimal notion of versioning to facilitate evolution without having to break the world, but it generally requires "forced upgrade" model where older versions are quickly depreciated and the users / clients of the library are expected to upgrade and move forward. That applies to both the development environment and the code itself. The code of course is changed carefully with backward compatibility in mind and older versions of the development environment or dependencies are not explicitly broken or not supported, but they might be, so if you are a user of the library and you can't upgrade easily at some point you might not be able to pick up the latest version of swblocks-baselib library and might have to stay on older version - btw, the model is very similar to the way Boost is versioned and how backward compatibility is maintained there.

A development environment version is a notion of collection of specific versions of compiler toolchains + a collection of compatible versions of the 3rd party dependencies. Currently swblocks-baselib library officially supports three development environment versions - devenv4 (latest) and [devenv3](CONTRIBUTING.DEVENV3.md) and devenv2 (older versions) with the devenv2 environment of course to be eventually depreciated in the future. Here are the collections of the compiler toolchains versions and the 3rd party dependencies versions in the most recent development environment (for older environment see [CONTRIBUTING.DEVENV3.md](CONTRIBUTING.DEVENV3.md) file):

* **devenv4**
  * Operating Systems
    * Darwin / macOS platforms
      * 17.x / macOS High Sierra (with Clang 10.00 or later)
      * 18.x / macOS Mojave (with Clang 10.00 or later)
      * 19.x / macOS Catalina (with Clang 10.00 or later)
    * Linux platforms
      * RedHat 6 (rhel6)
      * RedHat 7 (rhel7)
      * RedHat 8 (rhel8)
      * Ubuntu x64 18.04 (ub18)
      * Ubuntu x86 18.04 (ub18)
      * Ubuntu x64 20.04 (ub18) - temporarily enabled with the ub18 binaries
    * Windows platforms
      * Windows 7 both x86 and x64
      * Windows 8.x both x86 and x64
      * Windows 10 both x86 and x64
  * Compilers
    * GCC 8.3.0 and Clang 8.0.1 for all supported Linux platforms
    * Apple Clang 10.00 or later for Darwin / macOS platforms
    * Microsoft vc14.1 Visual C++ 2017 for Windows platforms
  * C++11 standard library implementations
    * libc++ with Clang for Darwin / macOS and Linux platforms
    * libstdc++ with GCC and Clang for Linux platforms
    * msvcrt for vc14.1 for Windows platforms
  * Boost 1.72
  * OpenSSL 1.1.1d
  * JSON Spirit 4.08

## Development environment distributions and links

In addition to the code dependencies of the library itself (compiler toolchain, Boost, OpenSSL, JSON spirit) the development environment also has few additional dependencies such as GNU make (e.g. via msys on Windows), git, python, etc, plus some optional such as Eclipse CDT (to use C++ indexer and IDE). The "binary blob" that contains the pre-built versions of the development environment with all code dependencies plus the additional and optional such is called "devenv distribution". Some popular combinations of devenv distributions were prebuilt and uploaded on Google drive, so they can be downloaded directly and facilitate the development and ease of use of the swblocks-baselib library. Here is the list of the currently supported devenv distributions with the link of where to download them from (more will be coming in the future):

* **devenv4** for Windows x64 only (Win7 - Win10, including latest Win10 updates, SDK 8.1 and 10), vc14.1 (VS 2017) can be downloaded from [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x64-only.zip); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x64-only.zip
* **devenv4** for Windows x86 only (Win7 - Win10, including latest Win10 updates, SDK 8.1 and 10), vc14.1 (VS 2017) can be downloaded from [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x86-only.zip); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x86-only.zip
* **devenv4** for Windows both x64 and x86 (Win7 - Win10, including latest Win10 updates, SDK 8.1 and 10), vc14.1 (VS 2017) can be downloaded from [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x64-and-x86.zip); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-windows-Hostx64-x64-and-x86.zip
* **devenv4** for Darwin / macOS (17.x or later / macOS High Sierra or later), clang 10.00, libc++ can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-darwin-17.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-darwin-17.tar.gz
* **devenv4** for Ubuntu x64 18.04 LTS with GCC 8.3.0 and Clang 8.0.1 can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-ub18-gcc830-clang801.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-ub18-gcc830-clang801.tar.gz
* **devenv4** for Ubuntu x86 18.04 LTS with GCC 8.3.0 and Clang 8.0.1 can be downloaded [here](hhttps://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-ub18-x86-gcc830-clang801.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-ub18-x86-gcc830-clang801.tar.gz
* **devenv4** for RedHat 6 / CentOS 6 with GCC 8.3.0 and Clang 8.0.1 can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel6-gcc830-clang801.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel6-gcc830-clang801.tar.gz
* **devenv4** for RedHat 7 / CentOS 6 with GCC 8.3.0 and Clang 8.0.1 can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel7-gcc830-clang801.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel7-gcc830-clang801.tar.gz
* **devenv4** for RedHat 8 / CentOS 8 with GCC 8.3.0 and Clang 8.0.1 can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel8-gcc830-clang801.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/dist-devenv4-rhel8-gcc830-clang801.tar.gz

Here are the links to the original source and binary dependencies needed to build new development environment images for new platforms. Instructions about how to do this are in the devenv4 notes files.

* **devenv4** source code for the libraries and compiler toolchains can be downloaded [here](hhttps://storage.googleapis.com/swblocks-dist/devenv/4/source-dependencies.tar.gz); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/source-dependencies.tar.gz
* **devenv4** binary dependencies for Windows can be downloaded [here](https://storage.googleapis.com/swblocks-dist/devenv/4/windows-binary-dependencies.zip); or from command line:  
wget https://storage.googleapis.com/swblocks-dist/devenv/4/windows-binary-dependencies.zip

All links above are either .zip file (for Windows) or a .gz tar file for Darwin / macOS and Linux. Once they are downloaded they can be extracted into some location (e.g. **c:\\swblocks** for Windows or **/Users/userid/swblocks** for Darwin and Linux) and then once you clone the swblocks-baselib library from github in order to use it you simply need to create a small .mk file in **projects/make** folder called **ci-init-env.mk** and point the 3 _DIST_ roots to the place where you have extracted the development environment blob.

E.g. if you have extracted the development environment into **c:\\swblocks** then the **projects/make/ci-init-env.mk** can look the following way for Windows:

```make
# initialize the important env roots

DIST_ROOT_DEPS1 = /c/swblocks/dist-devenv4-windows
DIST_ROOT_DEPS2 = /c/swblocks/dist-devenv4-windows
DIST_ROOT_DEPS3 = /c/swblocks/dist-devenv4-windows
```

Note that on Windows you should use UNIX style paths since we use GNU make (via msys)

E.g. **projects/make/ci-init-env.mk** can look the following way for Darwin (or you can adjust for Linux):

```make
# initialize the important env roots

DIST_ROOT_DEPS1 = /Users/userid/swblocks/dist-devenv4-darwin-17
DIST_ROOT_DEPS2 = /Users/userid/swblocks/dist-devenv4-darwin-17
DIST_ROOT_DEPS3 = /Users/userid/swblocks/dist-devenv4-darwin-17
```

An additional step which applies **only** to Windows is to add few entries to the Path environment variable associated with your account permanently as these tools are necessary to build, run tests and use the development environment in general, but by default are not available on Windows. On Linux / UNIX / Darwin these are typically available, or can be made available, in the OS, so this step is not necessary on these platforms. Here is the list of entries which need to be added to the path assuming the location where you have extracted the development environment distribution is **c:\\swblocks** (preferably in this order):

```
c:\swblocks\dist-devenv4-windows\msys2\latest\msys64\usr\bin
c:\swblocks\dist-devenv4-windows\git\latest\default\bin
c:\swblocks\dist-devenv4-windows\python\2.7-latest\default
```

If you are using WinDbg for debugging on Windows you can also optionally add the following entry to the Path associated with your account:

```
c:\swblocks\dist-devenv4-windows\winsdk\10\default\Debuggers\x64
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

## IDE support and Eclipse CDT indexer

The Eclipse CDT indexer is an excellent tool and probably one of the best such that exist to vastly improve productivity of C++ developers. swblocks-baselib comes with a small python script that generates eclipse project files for the various targets, so Eclipse can be used with the CDT indexer for development. The script doesn't take any arguments hence can be simply executed like this:

```
$ python scripts/generate-eclipse-project-config.py
```

At this point the eclipse projects will be generated in **bld/makeeclipse** location and you can simply copy/move them in more permanent location (e.g. if you use the **CI_ENV_ROOT** environment variable to control the development environment configuration then the expected location to move them to would be **$CI_ENV_ROOT/projects/makeeclipse**) and then of course start using them by importing them in an Eclipse workspace.

Note also that each time you import or open the projects for the first time in an Eclipse workspace you may get something like the following errors on Windows:

```
Program "g++" not found in PATH	<my-target> ...
Program "gcc" not found in PATH	<my-target> ...
```

These errors can simply be ignored (deleted once). They won't show up again until you re-open and / or re-import the project again.

Note also that it is highly recommended (and actually some parts actually required - e.g. style and formatting, line ending, etc) that configure your Eclipse workspace according to the instructions in **notes\\eclipse_workspace.txt**.

## Using GitHub and creating pull requests

This section of course will not cover general information about how to use Git and GitHub (there is plenty of information on the internet and GitHub site itself), but if you are new to Git it is highly recommended to read the following [link](https://www.sbf5.com/~cduan/technical/git) which is not the typical Git tutorial, but will help you understand Git conceptually. And of course for tutorials on the specifics, the likes of "getting started", "cheat sheets", etc, you can search on Google as there are plenty of those available on the internet.

Here are some details on how to use GitHub to open pull requests and contribute specifically to the swblocks-baselib library. First of course you will need to create an account on GitHub and then create your own private fork (on GitHub) of the repository off the master copy located in the JP Morgan Chase account area [here](https://github.com/jpmorganchase/swblocks-baselib). The way you can create a private fork of the repository is by first going to the [master copy link](https://github.com/jpmorganchase/swblocks-baselib) and then clicking the 'Fork' button in the top right corner. The reason you need to create a private fork is that the [master copy](https://github.com/jpmorganchase/swblocks-baselib) is locked down for write access and you can't directly make changes to it via push Git commands, but only via pull requests (from branches in your own private fork).

How do you want to organize the branches in your own private fork is up to your own preference, but it is recommended to pull directly from the master copy into your private branches in your fork and then when you have changes ready you can create pull requests from branches in your private for to master branch in the master copy fork.

In order to pull easily changes from the master copy repository it is recommended to add it as an additional remote where fetch is allowed, but push is disabled. You can accomplish this with the following Git commands:

```
git remote add --mirror=fetch public_origin https://github.com/jpmorganchase/swblocks-baselib.git
git remote set-url --push public_origin disabled
```

Now every time you want to pull changes from the master repository you can do this with the following Git command:

```
git pull public_origin master
```

If you want to do pull / push for your own private branches in your own private fork you of course can do this in the usual way:

```
git checkout -b mybranch
git push --set-upstream origin mybranch
git push
git pull origin master
```

Note also that it is recommended to not use pull requests in your own private fork (but use Git push / pull directly) as this will create extra 'pollution' with unnecessary commits for the private pull requests and these will eventually leak in the master repository as part of your official / public pull requests.
