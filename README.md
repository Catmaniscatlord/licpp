# Requirements

- `gcc 13.2.0`
- `cmake 3.22`
- build-essential
- doctest-dev
- zlib1g-dev
- libavcodec-dev
- libavformat-dev
- libavutil-dev
- libncurses-dev
- libswscale-dev
- libunistring-dev
- pkg-config

# Building

In the source directory run
`cmake -S . -B build`
`cmake --build build/`

# Running

Once you have built the project, the executable will be at `build/src/Main`

When you run program, it will output to `output.txt` in the cwd
