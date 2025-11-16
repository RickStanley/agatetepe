# agatetepe

Make requests from .http files.

![A image depicting agatetepe usage within a terminal emulator, the user made a GET request and received a response as expected](./example.png)

The goal is to choose a common subset of features between VS and Rider HTTP requests implementations:

- https://learn.microsoft.com/en-us/aspnet/core/test/http-files;
- https://www.jetbrains.com/help/rider/Http_client_in__product__code_editor.html;

The C++ 23 standard is chosen for exploration purposes.

The bare bones structure: initial parser, TUI-like features and variable substituition was coded 
with the help of a code generator, using the GLM 4.5 model.

Build it using:

Linux:

```bash
# assuming libcurl is installed in your system
g++ -std=c++23 $(pkg-config --cflags libcurl) http_5.cc $(pkg-config --libs libcurl) -o reqs
```

_Why `http_5.cc`?_
This is my **fifth** time, in a span of 3 years, attempting to create a HTTP file parser without much, or any, help of AI.
Understanding "modern" C++ (>=23) is a personal goal, one that can only be achieved by trial and error.

The following are non-exhaustive list of goals and non-goals. Permissive to changes.

Goals:

- be Rider and VS http file syntax compilant;
- create a small subset of HTTP request utilities;
- laboratory for C++ 23+ semantics study;
- portable (Windows and MacOS are included and should work with little to no effort);

Non-goals:

- replace well-known, battle-tested tools like https://hurl.dev/;
- fully-fledge/feature-complete tool (but a [lua](https://www.lua.org/) integration won't hurt);
    - no (or rather, not yet) authentication flow;
    - cookies storage;

Dependencies are not allowed. There must be a good reason to introduce one.
Macros are not allowed, we have better tools in C++ 23+.
Make system is wanting; I'll add later for cross compilation.