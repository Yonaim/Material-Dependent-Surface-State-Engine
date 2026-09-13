# MDSS Engine Code Style

This document defines the code style used throughout **MDSS Engine**.

The project follows an Unreal Engine-inspired naming and formatting style, adapted for a standalone C++ rendering engine.

# Naming

## Namespace

The root namespace is `MDSS`.

```cpp
namespace MDSS
{
}
```

## Types

Classes, structs, enum types, and enum values use **PascalCase**.

## Functions

Functions use **PascalCase**.

## Variables

Member variables, local variables, and function parameters use **PascalCase**.

Boolean variables use `b` followed by PascalCase.

## Constants

Constants and `constexpr` values use **PascalCase**.

## Acronyms

Established acronyms remain uppercase inside identifiers.

Common project acronyms include:

```text
MDSS
GPU
UV
SR
OBJ
MTL
```

# Files

## File Naming

Source files use the name of their primary class or type.

C++ source files use `.h` and `.cpp`.

Shaders use stage-specific extensions such as `.vert`, `.frag`, and `.comp`.

## Header Structure

Headers use `#pragma once`.

Forward declarations are used when a complete type definition is not required.

# Includes

Includes are grouped in the following order:

1. Matching header
2. MDSS project headers
3. Third-party library headers
4. Standard library headers

Include groups are separated by one blank line.

# Formatting

Formatting is controlled by the project's `.clang-format` file.

## Indentation

Indentation uses **4 spaces**.

## Braces

Braces use **Allman style**.

## Pointer and Reference Alignment

Pointer and reference symbols are attached to the type.

## Line Length

The preferred maximum line length is **120 columns**.

# Types and Initialization

## Integer Types

Fixed-width standard integer types are used.

```cpp
std::int32_t
std::uint32_t
std::uint64_t
```

## Member Initialization

Members are initialized at declaration when a meaningful default value exists.

# Placeholder Files

Files reserved for future implementation contain:

```cpp
// dummy
```
