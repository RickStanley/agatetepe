# agatetepe

Make requests from .http files.

![A image depicting agatetepe usage within a terminal emulator, the user made a GET request and received a response as expected](./example.png)

The goal is to choose a common subset of features between VS and Rider HTTP requests implementations:

- https://learn.microsoft.com/en-us/aspnet/core/test/http-files;
- https://www.jetbrains.com/help/rider/Http_client_in__product__code_editor.html;

The C++ 23 standard is chosen for exploration purposes.

The bare bones structure: initial parser, TUI-like features and variable substituition was coded 
with the help of a code generator, using the GLM 4.5 model.
