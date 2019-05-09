# Setting Up the nRF5 SDK for Mesh Environment

This section describes how to set up the nRF5 SDK for Mesh development environment on your host operating system.


## Set up the toolchain

To build the Mesh applications, a toolchain based on `CMake` is required. Follow one of the following guides for your host operating system:

* [macOS](#macos)
* [Windows](#windows)
* [Linux](#linux)

### macOS

1. Install *Homebrew* by following instructions on the [Homebrew site](https://brew.sh/). 


2. Install [CMake](https://cmake.org/) and [Ninja](https://ninja-build.org/) using `brew`:
	
	``` sh
	brew install cmake ninja
	```

3. Download and install the [GNU ARM Embedded Toolchain](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads). The `6-2017-q2-update` version is recommended. Then ensure the path is added to your OS PATH environment variable.

    ``` sh
    # in ~/.bash_profile, add the following script
    export PATH="<path to install directory>/gcc-arm-none-eabi-6-2017-q2-update/bin:${PATH}"
    ```
    Type the following in your terminal to verify if the path is set correctly:

    ``` sh
    arm-none-eabi-gcc --version
    ```

4. Download the [nRF5x-Command-Line-Tools-OSX](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF5-Command-Line-Tools/Download#infotabs), then extract the `.tar` archive anywhere on your filesystem. Ensure the extracted directory is added to your OS PATH environment variable.

    ``` sh
    # in ~/.bash_profile, add the following script
    export PATH="<the path to the extracted directory>:${PATH}"
    ```

    Type the following in your terminal to verify if `mergehex` works:
    ``` sh
    mergehex --version
    ```

5. Install the latest stable version of [pyOCD](https://github.com/mbedmicro/pyOCD) via `pip` as follows:

	``` sh
	pip install -U pyocd
	```

	Type the following in your terminal to verify if `pyocd` works:
	``` sh
	pyocd --version
	```

### Windows

The easiest way to install the native Windows dependencies is to first install [Chocolatey](https://chocolatey.org/), a package manager for Windows. If you prefer to install dependencies manually, you can also download the required programs from their respective websites.

1. Install **Chocolatey** by following the instructions on the [Chocolatey install](https://chocolatey.org/install) page.

2. Open a command prompt (`cmd.exe`) as an **Administrator**

3. Optionally disable global confirmation to avoid having to confirm installation of individual programs:

    ``` sh
    choco feature enable -n allowGlobalConfirmation
    ```

4. Install *CMake*:

    ``` sh
    choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System'
    ```

5. Install the rest of the tools, and close the Administrator command prompt window when finished.

    ``` sh
    choco install git python ninja
    ```

6. Download and install the [GNU ARM Embedded Toolchain](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads). The `6-2017-q2-update` version is recommended. Run the installer and follow the given instructions. Upon completion, check the *Add path to environment variable* option. Then verify if the compiler works:

    ``` sh
    arm-none-eabi-gcc --version
    ```

7. Download the [nRF5x-Command-Line-Tools for Win32](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF5-Command-Line-Tools/Download#infotabs). Run the installer and follow the given instructions. Then verify if `mergehex` works:

    ``` sh
    mergehex --version
    ```

8. Install the latest stable version of [pyOCD](https://github.com/mbedmicro/pyOCD) via `pip` as follows:

	``` sh
	pip install -U pyocd
	```
	Type the following in your terminal to verify if `pyocd` works:
	``` sh
	pyocd --version
	```

### Linux

This section describes how to set up the development environment on Ubuntu. The steps should be similar for other Linux distributions.

1. Ensure your host system is up to date before proceeding.

    ``` sh
    sudo apt-get update
    ```
    ``` sh
    sudo apt-get upgrade
    ```

2. Install the following packages using your system’s package manager.

    ``` sh
    sudo apt-get install --no-install-recommends git cmake ninja-build python3-pip
    ```

3. Download and install the [GNU ARM Embedded Toolchain](https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads). The `6-2017-q2-update` version is recommended. Then ensure the path is added to your OS PATH environment variable.

    ``` sh
    # in ~/.bash_profile, add the following script
    export PATH="<path to install directory>/gcc-arm-none-eabi-6-2017-q2-update/bin:${PATH}"
    ```
    Type the following in your terminal to verify if the path is set correctly:

    ``` sh
    arm-none-eabi-gcc --version
    ```

4. Download the [nRF5x-Command-Line-Tools-Linux-xxx](https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF5-Command-Line-Tools/Download#infotabs), then extract the `.tar` archive anywhere on your filesystem. Ensure the extracted directory is added to your OS PATH environment variable.

    ``` sh
    # in ~/.bash_profile, add the following script
    export PATH="<the path to the extracted directory>:${PATH}"
    ```

    Type the following in your terminal to verify if `mergehex` works:
    ``` sh
    mergehex --version
    ```

5. Install the latest stable version of [pyOCD](https://github.com/mbedmicro/pyOCD) via `pip` as follows:

	``` sh
	pip install -U pyocd
	```

	Type the following in your terminal to verify if `pyocd` works:
	``` sh
	pyocd --version
	```


## Clone the repository

Clone the `grove-mesh-kit` repository from GitHub:

``` sh
git clone --recursive https://github.com/makerdiary/grove-mesh-kit
```

Or if you have already cloned the project, you may update the submodule:

``` sh
git submodule update --init
```

## Install the nRF5 SDK

The nRF5 SDK for Mesh now requires the nRF5 SDK to compile.

Download the SDK file `nRF5_SDK_v15.2.0_9412b96` from [www.nordicsemi.com](https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK/).

<a href="https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK/Download#infotabs"><button data-md-color-primary="marsala">Download</button></a>

Extract the zip file to the `grove-mesh-kit` repository. This should give you the following folder structure:

``` sh
./grove-mesh-kit/
├── LICENSE
├── README.md
├── config
├── docs
├── examples
├── firmware
├── mkdocs.yml
└── nrf_sdks
    ├── README.md
    ├── nRF5-SDK-for-Mesh
    └── nRF5_SDK_v15.2.0_9412b96
```

To use the nRF5 SDK you first need to set the toolchain path in `makefile.windows` or `makefile.posix` depending on platform you are using. That is, the `.posix` should be edited if your are working on either Linux or macOS. These files are located in:

``` sh
<nRF5 SDK>/components/toolchain/gcc
```

Open the file in a text editor ([Sublime](https://www.sublimetext.com/) is recommended), and make sure that the `GNU_INSTALL_ROOT` variable is pointing to your GNU Arm Embedded Toolchain install directory.

``` sh
GNU_INSTALL_ROOT ?= $(HOME)/gcc-arm-none-eabi/gcc-arm-none-eabi-6-2017-q2-update/bin/
GNU_VERSION ?= 6.3.1
GNU_PREFIX ?= arm-none-eabi
```

## Next Steps
Congratulations! Now you can try to build and run the mesh examples. Head to [Building & Running the examples](../building-n-running-the-examples) section for more details.


## Create an Issue

Interested in contributing to this project? Want to report a bug? Feel free to click here:

<a href="https://github.com/makerdiary/grove-mesh-kit/issues/new"><button data-md-color-primary="marsala"><i class="fa fa-github"></i> Create an Issue</button></a>


