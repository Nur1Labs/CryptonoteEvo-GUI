# cryptonote-gui

## How to build binaries from source code

### Windows
To build the gui you must have built cryptonote core, so please do all steps from [here](https://github.com/Nur1Labs/CryptonoteEvo-GUI#building-on-windows) before proceed. Install [QtCreator](https://www.qt.io/download-thank-you?os=windows), open the project file cryptonote-gui/src/cryptonote-gui.pro in QtCreator and build it using MSVS kit (you must have MSVS installed already to build cryptonote core).

### MacOS

To build the gui you must have built cryptonote core, so please do all steps from [here](https://github.com/Nur1Labs/CryptonoteEvo-GUI#building-on-mac-osx) before proceed. Install [QtCreator](https://www.qt.io/download-thank-you?os=macos), open the project file cryptonote-gui/src/cryptonote-gui.pro in QtCreator and build it using clang kit (you must have XCode installed already to build cryptonote core).

### Linux
```
# To install all required packages on Ubuntu use the following command:
$ sudo apt install qt5-qmake qtbase5-dev qtbase5-dev-tools

$ git clone https://github.com/Nur1Labs/CryptonoteEvo-CLI.git
$ cd CryptonoteEvo-CLI
$ mkdir -p build
$ cd build
$ cmake ..
$ make -j4 evonote-crypto
$ cd ../..
$ git clone https://github.com/Nur1Labs/CryptonoteEvo-GUI.git
$ cd CryptonoteEvo-GUI
$ mkdir -p build
$ cd build
$ cmake ..
$ make -j4
```
Alternative way:
```
# Install QtCreator:
$ sudo apt install qtcreator

$ git clone https://github.com/Nur1Labs/CryptonoteEvo-CLI.git
$ cd CryptonoteEvo-CLI
$ mkdir -p build
$ cd build
$ cmake ..
$ make -j4 evonote-crypto
$ cd ../..
$ git clone https://github.com/Nur1Labs/CryptonoteEvo-GUI.git
```
Now open the project file CryptonoteEvo-GUI/src/cryptonote-gui.pro in QtCreator and build it.

## Donation Support

https://github.com/FndNur1Labs/CryptonoteEvo
