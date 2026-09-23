#include <stdio.h>
#include <stdlib.h>

#include "TGA_lib.h"

//By default, fields are padded (e.g., char + 3B + int) 
//to align data with machine word boundaries for faster access.

//For the scope of the encoding/decoding process, compiler padding is prevented:
//the fields are saved contiguously, without the insertion of padding bytes(meaning that are "packed")
typedef struct __attribute__((__packed__)) tga_header_s //(if TGA Version 2.0 is considered) the header holds 18 bytes of information:
{
    uint8_t ID_lenght;       //Byte 0: Length of image ID field(usually contains date & time of creation)
    uint8_t color_map_type; //Byte 1: Flag defining if color map is used to describe image(1 = True, 0 = False)
    uint8_t img_type;      //Byte 2: Descriptor number containing compression and color type

    //The following 5 bytes describe the COLOR MAP
    uint16_t color_map_origin;  //Bytes 3 & 4: Index of first entry in the color map
    uint16_t color_map_length; //Bytes 5 & 6: Number of entries in the color map
    uint8_t color_map_depth;  //Byte 7: Number of bits used to describe an entry in the color map

    //The following 10 bytes describe the IMAGE DIMENSION & FORMAT
    uint16_t x_origin;           //Bytes 8 & 9: X-coordinate of image origin
    uint16_t y_origin;          //Bytes 10 & 11: Y-coordinate of image origin
    uint16_t img_width;        //Bytes 12 & 13: Image width in pixels
    uint16_t img_heigt;       //Bytes 14 & 15: Image height in pixels
    uint8_t bits_per_pixel;  //Byte 16: Number of bits used to describe a pixel
    uint8_t img_descriptor; //Byte 17:
                                 //*bits 0-3: describe alpha channel depth
                                //*bit 4: indicates right-to-left pixel ordering if set
                               //*bit 5: indicates top-to-bottom ordering if set
                              //*bit 6 & 7: indicated the way scanlines on CRT TVs were displayed(nowadays those bits are both set to 0 as the scanline modes are deprecated)
}TGA_Header;

struct TGA_image_s
{
    uint16_t width;
    uint16_t height;
    uint8_t bits_per_pixel;
    uint8_t* data;
};


//##### Creation & Distruction of TGA image ADT #####
TGAImage* TGA_CreateImage(int width, int height, int bits_per_pixel)
{
    if(width <= 0 || height <= 0){ fprintf(stderr, "[TGA_CreateImage]: invalid canvas dimensions (%dx%d), required strictly positive parameters.\n"); return NULL; }
    if(bits_per_pixel != 8 && bits_per_pixel != 16 && bits_per_pixel != 24 && bits_per_pixel != 32){ fprintf(stderr, "[TGA_CreateImage]: invalid pixel depth (%d bpp): expected 8(grayscale), 16(High Color), 24(True Color RGB) or 32(True Color with Alpha (RGBA))\n"); return NULL; }

    TGAImage* img = (TGAImage*)malloc(sizeof(TGAImage));
    if(!img){ fprintf(stderr, "[TGA_CreateImage]: unable to service a memory request, failed to allocate TGAImage type.\n"); return NULL; }

    size_t num_bytes = (size_t)width * height * (bits_per_pixel >> 3);
    img->data = malloc(num_bytes);
    if(!img->data){ fprintf(stderr, "[TGA_CreateImage]: out of memory, image dimensions (%dx%d @ %d bpp) exceed maximum adressable size.\n"); free(img); return NULL; }

    img->width = width; img->height = height;
    img->bits_per_pixel = bits_per_pixel;

    return img;
}
void TGA_DestroyImage(TGAImage* img)
{
    if(!img){ fprintf(stderr, "[TGA_CreateImage]: No TGAImage type allocated.\n"); return; }
    if(img->data != NULL) free(img->data);

    free(img);
}
//###################################################

