
# Kilo Text Editor

## Overview

**Kilo** is a lightweight implementation of the Kilo text editor, based on the [Kilo tutorial](https://viewsourcecode.org/snaptoken/kilo/index.html). Currently, it includes:

- A basic text editor.
- Config File: an ini file that holds some of the editor configuration.
- Auto Indent: Automatically indent lines for better code formatting it only indents with the same amout as previous line.
- Copy and Paste: Support for clipboard operations.

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

## Features Details

### Configuration File

The configuration file for the application is written in the INI format and is located at:

`$HOME/.kilorc.ini` (Linux)

#### Sections and Options

##### `[editor]`
This section defines settings related to the editor behavior. Below are the configurable options:

| **Option**       | **Description**                               | **Type**        | **Values**                   |
|-------------------|-----------------------------------------------|-----------------|------------------------------|
| `indent`         | Defines the type of indentation to use.       | `"space"` or `"tab"` | `"space"`, `"tab"`          |
| `indent_amount`  | Sets the number of spaces equivalent to a tab. | Integer         | Any positive integer         |
| `line_numbers`   | Enables or disables line numbers in the editor. | `"true"` or `"false"` | `"true"`, `"false"`         |
| `syntax`         | Enables or disables syntax highlighting.     | `"true"` or `"false"` | `"true"`, `"false"`         |
| `quit_times`     | Specifies the number of attempts required to quit the editor when there are unsaved changes. | Integer         | Any positive integer         |

### Copy and paste

first enter visual mode by pressing v from normal mode, then highlight the text to copy with y   
and paste it with p

## Coming Features

Planned features for future versions include:

- **Modal Editing**: Add modal editing similar to Vim.
- **Multiple Buffers**: Allow editing multiple files simultaneously.

---

Feel free to contribute or provide feedback!

