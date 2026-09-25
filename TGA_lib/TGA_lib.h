#ifndef TGA_H
#define TGA_H

#include <stdint.h>

typedef struct TGA_image_s TGAImage;
typedef struct TGA_color_s
{
    uint8_t blue, green, red, alpha;
    uint8_t Bytes_pp;
} TGAColor;

//##### Creation & Distruction of TGA image ADT #####
TGAImage* TGA_CreateImage(int width, int height, int bit_pp);
void TGA_DestroyImage(TGAImage* img);
//###################################################

//##### Read/Write TGA image data from/in file #####
int TGA_ReadFile(TGAImage* img, const char* filename);
int TGA_WriteFile(const TGAImage* img, const char* filename, int vflip_flag, int rle_flag);
//###################################################

//##### Getters & Setters #####
int TGA_GetWidth(const TGAImage* img);
int TGA_GetHeight(const TGAImage* img);

TGAColor TGA_GetPixel(const TGAImage* img, int x, int y);
void TGA_SetPixel(TGAImage* img, int x, int y, TGAColor color);
//#############################
#endif