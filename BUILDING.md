# BUILDING VIA DOCKER VIRTUALISATION

RefindPlus can be built on any operating system environment that supports Docker virtualisation. A Docker image has been created by a third party developer and is available on the DockerHub website (https://hub.docker.com/r/xaionaro2/edk2-builder).
Please refer to that project's repository (https://github.com/xaionaro/edk2-builder-docker) for details and support on this option.

<br><br>

---

<br><br>

# BUILDING NATIVELY ON MAC OS

## Python

The build process requires Python 2 but Python was essentially removed from macOS in 12.x Monterey.
If running this version of macOS or newer, download and install Python 2.7.18 from the Python website (https://www.python.org/downloads).

**NB:** Python 2 is available by default on macOS 11.x Big Sur and older.

## Xcode

### Base Installation

Download the version of Xcode for your macOS version from the Mac App Store and install.
The third-party maintained XcodeReleases website (https://xcodereleases.com) provides convenient links to Xcode packages on Apple's servers.

### Commandline Tools Installation

After installing Xcode, you will need to additionally install its commandline tools.

To do this, at a Terminal prompt, enter:

```
$ xcode-select --install
```

## HomeBrew

### Background

While Xcode provides a full development environment as well as a suite of different utilities, it does not provide all the tools required for TianoCore EDK II development as required to build RefindPlus on macOS natively.

This guide focuses on using HomeBrew to provide the required tools but equivalent steps can be taken in MacPorts and Fink; which may offer better support for older versions of macOS.
Substitute equivalent commands in as required.

You will find installation instructions on the HomeBrew website (https://brew.sh)

Tools installed using HomeBrew are placed in `/usr/local/bin`.
This non-standard location avoids conflicts with pre-installed tools but the `PATH` environment variable must be updated, if required, for installed tools to be found.

To do this, at a Terminal prompt, enter:

```
$ if [[ "${PATH}" != *"/usr/local/bin"* ]] ; then export PATH="/usr/local/bin:${PATH}"; fi
```

### Install Build Assembler

The assembler used for TianoCore EDK II is the Netwide Assembler (NASM).

```
$ brew install nasm && brew upgrade nasm
```

### Install ACPI Compiler

ACPICA is required to compile code in ACPI Source Language for TianoCore EDK II firmware builds.

```
$ brew install acpica && brew upgrade acpica
```

### Install Image Converter

The mtoc/ocmtoc utilities convert the macOS Mach-O image format to the PE/COFF format required by the UEFI specifications.

On macOS 10.15 Catalina/Older, use...
```
$ brew uninstall ocmtoc && brew install mtoc && brew upgrade mtoc
```

On macOS 11.x Big Sur/Newer or if MTOC fails on Catalina/Older, use...
```
$ brew uninstall mtoc && brew install ocmtoc && brew upgrade ocmtoc
```

**NB:** Only install one of `mtoc` or `ocmtoc` at a time.

`mtoc` and `ocmtoc` are only available as packages on HomeBrew.
Prebuilt versions of `ocmtoc` can be found here: https://github.com/acidanthera/ocmtoc/releases

## Prepare RefindPlus Environment

### Fork the RefindPlus Repository

Navigate to `https://github.com/dakanji/RefindPlus` and fork the repository.

### Clone the Forked RefindPlus Repository

In Terminal, clone the forked `RefindPlus` repository into a `RefindPlus/Working` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlus.git Working
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ git remote add upstream https://github.com/dakanji/RefindPlus.git
```

**NB:** Replace `YOUR_GITHUB_USERNAME_GOES_HERE` above with your actual GitHub User Name.

Your local RefindPlus repository will be under `Documents/RefindPlus/Working`.

## Prepare UDK2018 Environment

### Fork the RefindPlusUDK Repository

Navigate to `https://github.com/dakanji/RefindPlusUDK` and fork the repository

### Clone the Forked RefindPlusUDK Repository

In Terminal, clone the forked `RefindPlusUDK` repository into a `RefindPlus/edk2` folder under your `Documents` directory as follows:

```
$ mkdir -p ~/Documents/RefindPlus && cd ~/Documents/RefindPlus
$ git clone https://github.com/YOUR_GITHUB_USERNAME_GOES_HERE/RefindPlusUDK.git edk2
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ git remote add upstream https://github.com/dakanji/RefindPlusUDK.git
```

**NB:** Replace `YOUR_GITHUB_USERNAME_GOES_HERE` above with your actual GitHub User Name.

Your local RefindPlusUDK repository will be under `Documents/RefindPlus/edk2`.

## Run Build Script

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - Always use a new Terminal window when building.
- Type `chmod +x` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file onto the Terminal window and press `Enter`.
- Type `sh` in Terminal, add a space, then drag the `RefindPlusBuilder.sh` file onto the Terminal window again and press `Enter`.
  - Enter a space followed by a branch name to the end of the line (if you want to build on that branch) before pressing `Enter`.
  - If nothing is entered, the script will build on the default `GOPFix` branch.
  - The "chmod +x" step is typically only required the first time the script file is ever run.

## Source Repository Alignment

If some time has passed since your last build or since you initially created your repositories, you will need to ensure your repositories are aligned with the source repositories in order to incorporate updates added in the intervening period.

### OPTION 1: Scripted Sync (Recommended)

- Navigate to your `/Documents/RefindPlus/edk2/000-BuildScript` folder in the Finder.
- Separately, open a new Terminal window.
  - A new Terminal window is best for syncing.
- Type `chmod +x`, add a space, then drag the `RepoUpdater.sh` file onto the Terminal window and press `Enter`.
- Type `sh`, add a space, then drag the `RepoUpdater.sh` file onto the Terminal window again and press `Enter`.
  - The "chmod +x" step is typically only required the first time the script file is ever run

**NB:** If you get an error after running the script, try running it again as subsequent runs should realign things.
If the script still fails after a third attempt, try the manual sync steps outlined below instead.

### OPTION 2: Manual Sync

#### Sync RefindPlus Manually

```
$ cd ~/Documents/RefindPlus/Working && git checkout GOPFix
$ git reset --hard a2cc87f019c4de3a1237e2dc23f432c27cec5ec6
$ git push origin HEAD -f && git pull upstream GOPFix
$ git push
```

#### Sync RefindPlusUDK Manually

```
$ cd ~/Documents/RefindPlus/edk2 && git checkout rudk
$ git reset --hard a94082b4e5e42a1cfdcbab0516f9ecdbb596d201
$ git push origin HEAD -f && git pull upstream rudk
$ git push
```

### OPTION 3: GitHub Sync

GitHub includes an interface for syncing forks (which will need to be pulled to your local machine).
While, unlike Option 3, Options 1 and 2 will leave your fork with a clean history consistent with the source repositories, some may find the GitHub interface easier to use.
