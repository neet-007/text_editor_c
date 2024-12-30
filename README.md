
# Kilo Text Editor

## Overview

**Kilo** is a lightweight implementation of the Kilo text editor, based on the [Kilo tutorial](https://viewsourcecode.org/snaptoken/kilo/index.html). Currently, it includes:

- A basic text editor.

## Build

The project is contained in a single file and uses only C core libraries. To build it, you can simply run:

```bash
make kilo.c
```

## Usage

### Open an Empty Buffer

To open an empty editor buffer, run:

```bash
./kilo
```

### Open a File

To edit an existing file, use:

```bash
./kilo filename
```

## Coming Features

Planned features for future versions include:

- **Auto Indent**: Automatically indent lines for better code formatting.
- **Copy and Paste**: Support for clipboard operations.
- **Config File**: Customize settings through a configuration file.
- **Modal Editing**: Add modal editing similar to Vim.
- **Multiple Buffers**: Allow editing multiple files simultaneously.

---

Feel free to contribute or provide feedback!

