#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "TGA_lib.h"

//By default, fields are padded (e.g., char + 3B + int) 
//to align data with machine word boundaries for faster access.

//For the scope of the encoding/decoding process, compiler padding is prevented:
//the fields are saved contiguously, without the insertion of padding bytes(meaning that are "packed")
typedef struct __attribute__((__packed__)) tga_header_s //(if TGA Version 2.0 is considered) the header holds 18 bytes of information:
{
    uint8_t ID_lenght;       //Byte 0: Length of image ID field(usually contains date & time of creation)
    uint8_t color_map_type; //Byte 1: Flag defining if color map is used to describe image(1 = True, 0 = False)
    uint8_t img_type;      //Byte 2: Descriptor number containing color format and compression type:
                                  //*bits 0-2: indicates the color format utilized:
                                    //* 0: no data
                                   //*1: color mapped
                                  //*2: True-Color(aka RGB/RGBA)
                                 //*3: Grayscale(aka Black & white shades)
                               //*bit 3: RLE compression flag(RLE applied if flag == 1, raw pixels otherwise)

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
}TGAHeader;

struct TGA_image_s
{
    uint16_t width;
    uint16_t height;
    uint8_t Bytes_per_pixel;
    uint8_t* data;
};


//##### Creation & Distruction of TGA image ADT #####
TGAImage* TGA_CreateImage(int width, int height, int bits_per_pixel)
{
    //Check for parameter validity
    if(width <= 0 || height <= 0){ fprintf(stderr, "[TGA_CreateImage]: invalid canvas dimensions (%dx%d), required strictly positive parameters.\n", width, height); return NULL; }
    if(bits_per_pixel != 8 && bits_per_pixel != 16 && bits_per_pixel != 24 && bits_per_pixel != 32){ fprintf(stderr, "[TGA_CreateImage]: invalid pixel depth (%d bpp): expected 8, 16, 24, or 32\n", bits_per_pixel); return NULL; }

    //Check for memory availability
    TGAImage* img = (TGAImage*)malloc(sizeof(TGAImage));
    if(!img){ fprintf(stderr, "[TGA_CreateImage]: unable to service a memory request, failed to allocate TGAImage type.\n"); return NULL; }

    size_t num_bytes = (size_t)width * height * (bits_per_pixel >> 3);
    img->data = malloc(num_bytes);
    if(!img->data){ fprintf(stderr, "[TGA_CreateImage]: out of memory, image dimensions (%dx%d @ %d bpp) exceed maximum addressable size.\n", width, height, bits_per_pixel); free(img); return NULL; }

    img->width = (uint16_t)width; img->height = (uint16_t)height;
    img->Bytes_per_pixel = (uint8_t)bits_per_pixel >> 3;

    return img;
}
void TGA_DestroyImage(TGAImage* img)
{
    if(!img){ fprintf(stderr, "[TGA_CreateImage]: No TGAImage type allocated.\n"); return; }
    if(img->data) free(img->data);

    free(img);
}
//###################################################

//##### Reduced Lenght Encoding(RLE) pack & unpack functions #####
static int pack_RLE_data(TGAImage* img, FILE* file_out)
{
    //*7th bit defines if RLE is applied
    //*bits 0-6: count the amount of pixel considered ==> beacuse "0 pixels read" holds no meaning, 
                                                        //the actual count is (count + 1), meaning the maximum count value is 128
    const uint8_t max_pixel_read = 128;
    const size_t total_pixel_count = (img->width) * (img->height);
    
    const uint8_t Bytes_per_pixel = img->Bytes_per_pixel;
    const uint8_t* data = img->data;

    size_t current_pixel = 0;//index to cycle through pixels
    while(current_pixel < total_pixel_count)
    {
        size_t pixel_seq_start = current_pixel * Bytes_per_pixel;
        size_t current_byte_in_seq = pixel_seq_start;

        uint8_t seq_len = 1, is_raw = 1;

        while(current_pixel + seq_len < total_pixel_count && seq_len < max_pixel_read)
        {
            //Check if current pixel and next pixel have same color
            uint8_t next_is_equal = memcmp(&data[current_byte_in_seq], &data[current_byte_in_seq + Bytes_per_pixel], Bytes_per_pixel) == 0;
            //At the beginning of the sequence, recognize if its managing RLE(same color pixels) or raw pixels(different color pixels)
            if(seq_len == 1) is_raw =! next_is_equal;

            //if its dealing with raw pixels and the next read pixel has same color compared to the last one in the sequence, interrupt the raw pixels sequence
            if(is_raw && next_is_equal)
            { 
                //sequence length of raw pixels counts only different color pixels:
                //thus the reason for decrementing of 1 the length
                seq_len--; 
                break; 
            }
            //if its dealing with RLE and the next read pixel has different color compared to the last one in the sequence, interrupt the RLE sequence
            if(!is_raw && !next_is_equal) break;

            //increment size of current sequence
            seq_len++;
        }
        //update index position after terminating current sequence
        current_pixel += seq_len;

        //*if analyzed sequence contains raw pixels, save the lenght of sequence(aka 7th bit is 0)
        //*if analyzed sequence contais RLE, save the length of the sequence + 127(aka 7th bit is 1)
        uint8_t img_type_byte = is_raw ? (seq_len - 1) : (seq_len + 127);
        //Write the Byte 2 in the file
        if(fputc(img_type_byte, file_out) == EOF){ fprintf(stderr, "[TGA] Write error: failed to write RLE packet header byte (%s), EOF reached\n", strerror(errno)); return 0; }
        //Write pixel data:
        //*if sequence contains raw pixels, write each pixel(sequence length * Bytes per pixel)
        //*if sequence contains RLE, write only the single repeating pixel
        size_t Bytes_to_write = is_raw ? ((size_t)seq_len * Bytes_per_pixel) : (size_t)Bytes_per_pixel;
        if(fwrite(&data[pixel_seq_start], 1, Bytes_to_write, file_out) != Bytes_to_write){ fprintf(stderr, "[TGA] Write error: failed to write %zu bytes of pixel payload (%s)\n", Bytes_to_write, strerror(errno)); return 0; }
    }

    return 1;
}

static int unpack_RLE_data(TGAImage* img, FILE* file_in)
{
    const size_t total_pixel_count = (img->width) * (img->height);
    const uint8_t Bytes_per_pixel = img->Bytes_per_pixel;
    uint8_t* data = img->data;

    size_t current_pixel = 0;
    size_t current_byte = 0;

    uint8_t num_pixel = 0;
    while(current_pixel < total_pixel_count)
    {
        uint8_t num_pixeL_read = (uint8_t)fgetc(file_in);
        if(num_pixeL_read == EOF){ fprintf(stderr, "[TGA] Read error: failed to read RLE packet, EOF reached.\n"); return 0; }

        //A sequence of raw pixels begins if number of read pixels < 128
        if(num_pixeL_read < 128)
        {
            //Remembering that "0 pixels read" has no meaning, add 1 to number of read pixels to obtain actual count
            num_pixel = num_pixeL_read + 1;

            if(current_pixel + num_pixel > total_pixel_count) { fprintf(stderr, "[TGA] Read error: raw pixel payload exceeds canvas dimensions.\n"); return 0; }
        
            size_t Bytes_to_read = (size_t)num_pixel * Bytes_per_pixel;
            if(fread(&data[current_byte], 1, Bytes_to_read, file_in) != Bytes_to_read){ fprintf(stderr, "[TGA] Read error: failed to read raw pixel payload (%s).\n", strerror(errno)); return 0; }

            current_byte += Bytes_to_read;
            current_pixel += num_pixel;
        }
        //A sequence of RLE pixels begins if number of read pixels >= 128
        else
        {
            //Remembering that "0 pixels read" has no meaning, add 1 to number of read pixels to obtain actual count
            num_pixel = num_pixeL_read - 127;

            if(current_pixel + num_pixel > total_pixel_count) { fprintf(stderr, "[TGA] Read error: RLE payload exceeds canvas dimensions.\n"); return 0; }
            
            //Saving the repeating color in a buffer
            uint8_t color_buffer[4] = {0, 0, 0, 0};
            if(fread(color_buffer, 1, Bytes_per_pixel, file_in) != (size_t)Bytes_per_pixel){ fprintf(stderr, "[TGA] Read error: failed to read RLA payload (%s).\n", strerror(errno)); return 0; }
            
            //Writing repating color pixels
            for(uint8_t i = 0; i < num_pixel; i++)
            {
                memcpy(&data[current_byte], color_buffer, Bytes_per_pixel);
                current_byte += Bytes_per_pixel;
            }

            current_pixel +- num_pixel;
        }
    }
    return 1;
}
//################################################################

