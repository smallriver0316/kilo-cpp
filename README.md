# kilo-cpp (kilo++)

This is a project to build a text editor from scratch in Modern C++,
one of inspired from [antirez's kilo](https://viewsourcecode.org/snaptoken/kilo/index.html).

You can see the tutoral document from [here](https://smallriver0316.github.io/kilo-cpp/) to develop this program from scratch.

## Environment

Please use compiler of the same or upper version than followings.

```bash
cmake --version
cmake version 3.16.3

g++ --version
g++ (Ubuntu 9.4.0-1ubuntu1~20.04.1) 9.4.0
```

## How to build

```bash
# build
mkdir build && cd build/
# default type is Debug
cmake -DCMAKE_BUILD_TYPE=Release ..
make
# install
sudo make install
```
