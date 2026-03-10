zako (alongside libzako) is a minimal but performant Cangjie input method (library) implementation, targeting Wayland compositors that support the input method v2 protocol.

## Installation

The core library, libzako, has no dependencies besides the C standard library. The reference client implementation, zako (also functioning as a Wayland frontend), requires xkbcommon.

```sh
# generate C headers and glue code
wayland-scanner client-header wayland/input-method-unstable-v2.xml input-method-unstable-v2.h
wayland-scanner private-code wayland/input-method-unstable-v2.xml input-method-unstable-v2.c
wayland-scanner client-header wayland/virtual-keyboard-unstable-v1.xml virtual-keyboard-unstable-v1.h
wayland-scanner private-code wayland/virtual-keyboard-unstable-v1.xml virtual-keyboard-unstable-v1.c

# compile executable with optimizations
clang *.c libzako/*.c -o zako -lwayland-client -lxkbcommon -O3 -flto
```

## Usage

Currently zako does not support many configuration options. A Cangjie dictionary, like [this one](https://github.com/LdBeth/cim/blob/main/UnihanCangjieTable.txt), is required. Consult `zako -h` for more information on supported options.

It operates in two states, active and inactive (default). When zako is active, it translates a set of keystrokes for input method purposes. <kbd>C-\\</kbd> toggles between states, <kbd>left</kbd> and <kbd>right</kbd> traverse between candidates, and <kbd>enter</kbd> commits the current pre-edit string.

Client-side key repeat is not implemented. Instead, zako relies on the repeated pseudo-state introduced in the seat v10 protocol, which is not yet widely supported by Wayland compositors.

## Library

This repository also hosts libzako, the library providing core input method functionalities. An optimized double-array trie (named the double decker) allows for O(1) input processing and prefix matching. For practical integration details, refer to zako, the reference implementation.
