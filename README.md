# Cyber Secure Six | SSIX

![enter image description here](https://uploads-ssl.webflow.com/605a69054a6f3bc14b61c508/609532f31223b84ac6db9ecc_Cyber%20Secure%20Six.png)

The Secure Six Currency and community is our attempt at creating a global movement behind privacy and security and to provide the means to achieve our goal for the common good.

# SSIX Developer Usage Guide

All you need to know to build the Cyber Secure Six (SSIX) cryptocurrency on various supported platforms.

## Linux | Ubuntu 20.04

   You will need the following packages: boost (1.67), cmake, git, gcc (7.x), g++ (7.x), make, and python3.
   **Environment Setup**
   
-   `sudo apt-get update`
-   `sudo apt-get -y install build-essential python3-dev gcc-7 g++-7 git cmake libboost-all-dev libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev`
-   `sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70`
-   `sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 70`

**One liner environment setup:** 

    sudo apt-get update && sudo apt-get -y install build-essential python3-dev gcc-7 g++-7 git cmake libboost-all-dev libgflags-dev libsnappy-dev zlib1g-dev libbz2-dev liblz4-dev libzstd-dev && sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70 && sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 70 

**Building:**

-   `git clone https://github.com/Cyber-Secure-Six-Inc/SSIX.git`
-   `cd SSIX`
-   `rm -rf build; mkdir -p build/release; cd build/release`
-   `cmake -D STATIC=ON -D ARCH="default" -D CMAKE_BUILD_TYPE=Release ../..`
-   `cmake --build . -j 4`

**Tip:** the `--j 4` in the last command `cmake --build . -j 4` is the numbers of CPU threads to be used by the compiler to build the SSIX binaries. Adapt to your system configuration as needed. 

**One liner build command to run compilation of binaries with 4 threads:**

    git clone https://github.com/Cyber-Secure-Six-Inc/SSIX.git  && cd SSIX && rm -rf build; mkdir -p build/release; cd build/release && cmake -D STATIC=ON -D ARCH="default" -D CMAKE_BUILD_TYPE=Release ../.. &&  cmake --build . -j 4


## Windows
**Environment Setup:**
-   Install [Visual Studio 2017 Community Edition](https://my.visualstudio.com/Downloads?q=Visual%20Studio%202017)
-   Along with Visual Studio 15.9, it is **required** that you install **Desktop development with C++** and the **VC++ v141 toolchain** when selecting features.
- [Boost 1.71.0](https://sourceforge.net/projects/boost/files/boost-binaries/1.71.0/) for MSVC 14.1 x64

**DO NOT CHANGE BOOST INSTALL DIRECTORIES!**

**Building:**
Adapt commands as required. A build usually goes as follows:
- Run `x64 Native Tools Command Prompt for VS2017` from Start Menu
- `cd` into your clone of the repository i.e: `C:/Users/%USERPROFILE%/Documents/GitHub/SSIX`
- `md build && cd build`
- `set PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin";%PATH%`
- `cmake -DBOOST_ROOT=C:\local\boost_1_71_0 -DBOOST_LIBRARYDIR=C:\local\boost_1_71_0\lib64-msvc-14.1 -G "Visual Studio 14 Win64" ..`
- Open the `SSIX.sln` in Visual Studio 2017 in `build` directory, ensure the Solution Configuration is `Release x64` at the top and hit `Ctrl+Shift+B` to build the binaries. 
- Compiled binaries can be found in `build/src/Release`



## Binaries

The SSIX binaries created by the compiler configuration present in this repositories are as follows:

|                |Command|Usage|
|----------------|-------------------------------|-----------------------------|
|SSIXd|`SSIXd`|Daemon used for connection to network. Required to use anything relating to the SSIX network.|
|Simplewallet|`simplewallet`|Simple command line wallet to transact over the SSIX network.|
|Wallet Daemon|`walletd`|Wallet daemon used for anything that needs the Wallet RPC API such as running a mining pool.|



## Credits
Cyber Secure Six aims to build a community around the development of a Cryptonote protocol currency where the code is maintained and kept updated so everyone can benefit and build on top of what has already been done. It was very challenging to make the original 2012 era Cryptonote protocol workable in 2021. The SSIX master branch is a fork of  the Talleo Project by 
 Mika Lindqvist. Improvements to Talleo's upstream code are candidates for potential improvements to the SSIX network. We hope to contribute back into the Talleo Project as we would not be as far along as we currently are without it.

The SSIX cryptocurrency would not be possible without the contributions of the Cryptonote Developers, Bytecoin Developers, Monero Developers, TurtleCoin Developers, Forknote Project, PinkstarcoinV2 Developers, Bittorium Developers, Talleo developers and of course, our own development team!

