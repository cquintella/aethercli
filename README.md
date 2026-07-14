# aethercli
This is a shortcut manager that takes the form of a famous CLI.

## Compilation and Installation

The project uses CMake and requires a C++17 compliant compiler.

```bash
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc)
sudo make install
```

For advanced instructions, configuration, security options, and user management, please refer to [RTFM.md](RTFM.md).
