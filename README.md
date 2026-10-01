# TGA Image Library in C

A lightweight, zero-dependency C library for reading, writing, and manipulating TGA image files.

[![Status](https://img.shields.io/badge/Status-Work_in_Progress-yellow.svg)](#)
[![License](https://img.shields.io/badge/License-zlib%2Fpng-blue.svg)](LICENSE.txt)
[![Standard](https://img.shields.io/badge/Standard-C99-informational.svg)](#)


## Preface: Legal Notice & Academic Attribution

This project was created **for educational purposes only**, exploring computer graphics fundamentals and taking inspiration from **Professor Dmitry V. Sokolov's** [*tinyrenderer*](https://github.com/ssloy/tinyrenderer) projects.

Before proceeding, please **read the `LICENSE.txt` file**, which contains the zlib/png license terms and full academic attributions.


## 1. What is this Library?

This library provides a few set of tools to handle **Truevision TGA (.tga)** images. 
By parsing the binary data, **header metadata and pixel payloads** can be manipulated in memory to generate, modify, and export images.

### Key Features

* **Totally independent library:** Built from scratch using only the C standard library.

* **Color Support:** Handles **RGB** (24-bit), **RGBA** (32-bit), and **Grayscale** (8-bit) pixel formats.

* **Run-Length Encoding (RLE):** Supports both reading and writing RLE-compressed streams to minimize file dimensions.

* **Cache-Friendly Memory Layout:** Data is organized in contiguous row-major order to optimize CPU prefetching and cache hit rates.


## 2. Main components

Image manipulation is structured around three data types:

* **`TGAHeader` (Internal):** Handles the 18-byte TGA file header.
* **`TGAImage` (ADT):** Represents the canvas using a flat, contiguous **1D vector buffer** organized in **row-major order**.

  > **Why a flat 1D buffer?**  
  > Storing pixel data in a single allocation guarantees that rows are contiguous in RAM. On modern hardware with standard 64-Byte cache lines, requesting the first 3-byte RGB pixel incurs a single cache miss; the following ~21 pixels are loaded into L1 cache simultaneously, resulting into consecutive cache hits. A classic 2D array of pointers, scatters rows across the memory, resulting in slower fetches.

* **`TGAColor` (non-ADT):** A struct containing a value foreach channel to **represent the desired color**.

*(To learn more about the TGA format, consult the [Truevision TGA Wikipedia page](https://en.wikipedia.org/wiki/Truevision_TGA).)*

## 3. API Reference

### Initialization & Cleanup

+ TGAImage* TGA_CreateImage(int width, int height, int bits_per_pixel);
 > Allocates a new empty canvas in memory

+ void TGA_DestroyImage(TGAImage* img);
 > Safely frees the image's dedicated memory


### I/O with files

+ TGAImage* TGA_ReadFile(const char* filename);

> Fetches the TGA file from memory and parses it into binary data to be handled

+ int TGA_WriteFile(TGAImage* img, const char* filename, int rle_flag);

> Given a file location, converts the binary data into an actual image(return 0 if an error occured, otherwise return 1)


### Pixel manipulation

+ TGAColor TGA_GetPixel(const TGAImage* img, int x, int y);

> Returns the color value of the pixel at the coordinates (x, y)

+ void TGA_SetPixel(TGAImage* img, int x, int y, TGAColor color);

> Sets the given color to the pixel at the coordinates (x, y)
