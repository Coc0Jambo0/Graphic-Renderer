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

            current_pixel += num_pixel;
        }
    }
    return 1;
}
//################################################################

//##### Flip functions for image(vertically & horizontally) #####
static int flip_vertically(TGAImage* img)
{
    const size_t width = img->width;
    const size_t height = img->height;
    const size_t Bytes_pp = img->Bytes_per_pixel;

    if(!img || !img->data || width == 0 || height == 0){ fprintf(stderr, "[TGA] Error: Failed to flip vertically the image(invalid image data).\n"); return 0; }
    if(Bytes_pp != 1 || Bytes_pp != 2 || Bytes_pp != 3 || Bytes_pp != 4){ fprintf(stderr, "[TGA] Error: Failed to flip vertically the image(invalid image data).\n"); return 0; }
    
    //x-coordinates
    for(size_t c = 0; c < width; c++)
    {
        //y-coordinates
        for(size_t r = 0; r < height / 2; r++)
        {
            //Bth color channel
            for(int B = 0; B < Bytes_pp; B++)
            {
                //foreach row r:
                //traverse r * width pixels and move of c columns, convert the position in bytes and access the Bth channel
                size_t pixel_channel = (((r * width) + c) * Bytes_pp) + B;
                //repeat the exact same calculation but starting from the bottom(height - 1) and rising up of r rows(height - 1 - r)
                size_t mirrored_pixel_channel = ((((height - 1 - r) * width) + c) * Bytes_pp) + B;
                
                //swap position
                uint8_t temp = img->data[pixel_channel];
                img->data[pixel_channel] = img->data[mirrored_pixel_channel];
                img->data[mirrored_pixel_channel] = temp;
            }
        }
    }

    return 1;
}

static int flip_horizontally(TGAImage* img)
{
    const size_t width = img->width;
    const size_t height = img->height;
    const size_t Bytes_pp = img->Bytes_per_pixel;

    if(!img || !img->data || width == 0 || height == 0){ fprintf(stderr, "[TGA] Error: Failed to flip vertically the image(invalid image data).\n"); return 0; }
    if(Bytes_pp != 1 || Bytes_pp != 2 || Bytes_pp != 3 || Bytes_pp != 4){ fprintf(stderr, "[TGA] Error: Failed to flip vertically the image(invalid image data).\n"); return 0; }
    
    //x-coordinates
    for(size_t c = 0; c < width / 2; c++)
    {
        //y-coordinates
        for(size_t r = 0; r < height; r++)
        {
            //Bth color channel
            for(int B = 0; B < Bytes_pp; B++)
            {
                //foreach row r:
                //traverse r * width pixels and move of c columns, convert the position in bytes and access the Bth channel
                size_t pixel_channel = (((r * width) + c) * Bytes_pp) + B;
                //repeat the exact same calculation but starting from the right(width - 1) and proceeding to the left of c columns(width - 1 - c)
                size_t mirrored_pixel_channel = (((r * width) + (width - 1 - c)) * (Bytes_pp)) + B;
                
                //swap position
                uint8_t temp = img->data[pixel_channel];
                img->data[pixel_channel] = img->data[mirrored_pixel_channel];
                img->data[mirrored_pixel_channel] = temp;
            }
        }
    }

    return 1;
}
//###############################################################

//##### Read/Write TGA image data from/in file #####
TGAImage* TGA_ReadFile(const char* filename)
{
    //Check if filename is proper and exists
    if(!filename){fprintf(stderr, "[TGA_ReadFile]: Invalid filename parameter (NULL pointer).\n"); return NULL; }

    FILE* file_in = fopen(filename, "rb");
    if(!file_in){ fprintf(stderr, "[TGA_ReadFile]: Cannot open file '%s': %s\n", filename, strerror(errno)); return NULL; }

    //Read the 18 Byte file header
    TGAHeader header;
    if(fread(&header, sizeof(TGAHeader), 1, file_in) != 1){ fprintf(stderr, "[TGA_ReadFile]: File '%s' truncated: failed to read complete 18-byte header.\n", filename); fclose(file_in); return NULL; }

    //##### HEADER INSPECTION #####
    //Byte 0: handled in the data payload(file cursor skipping over it)

    //Byte 1: color_map_type(0 if no palette is present, 1 otherwise)
    //Although color-mapped images are unsupported, RGB/RGBA TGAs might include a color palette:
    //the tool reads the color map metadata solely to calculate how many bytes to skip before the pixel payload.
    if (header.color_map_type != 0 && header.color_map_type != 1) { fprintf(stderr, "[TGA_ReadFile]: Byte no. 1 error (invalid color map specification %u).\n", header.color_map_type); fclose(file_in); return NULL; }
    
    //Byte 2: img_type
    //2: Uncompressed RGB/RGBA
    //3: Uncompressed Grayscale
    //10: RLE RGB/RGBA
    //11: RLE Grayscale
    if (header.img_type != 2 && header.img_type != 3 && header.img_type != 10 && header.img_type != 11) 
    { fprintf(stderr, "[TGA_ReadFile]: Byte no. 2 error (unsupported image type %u).\n", header.img_type); fclose(file_in); return NULL; }
    
    //Bytes 3 & 4: color_map_origin(ignored)

    //Bytes 5 & 6: color_map_length
    if(header.color_map_type == 0 && header.color_map_length != 0) fprintf(stderr, "[TGA_ReadFile]: Bytes 5 & 6 warning(defined length %u for non-existing color map.)\n", header.color_map_length);

    //Byte 7: color_map_depth(number of bits used to represent color map entries)
    //Header guard: palette entry size cannot be 0 bits if a color map is present
    if(header.color_map_type == 0 && header.color_map_length != 0){ fprintf(stderr, "[TGA_ReadFile]: Byte 7 error (entries depth of 0 bits for defined color map.)\n"); fclose(file_in); return NULL; }

    //Bytes 8 & 9: x_origin

    //Bytes 10 && 11: y_origin

    //Bytes 12 & 13: img_width
    if(header.img_width == 0){ fprintf(stderr, "[TGA_ReadFile]: Bytes 12 & 13 error (invalid image width).\n"); fclose(file_in); return NULL; }
    
    //Bytes 14 & 15: img_height
    if(header.img_heigt == 0){ fprintf(stderr, "[TGA_ReadFile]: Bytes 14 & 15 error (invalid image height).\n"); fclose(file_in); return NULL; }
    
    //Byte 16: bits_per_pixel
    if(header.bits_per_pixel != 8 && header.bits_per_pixel != 16 && header.bits_per_pixel != 24 && header.bits_per_pixel != 32)
    { fprintf(stderr, "[TGA_ReadFile]: Byte 16 error (unsupported pixel depth %u bpp).\n", header.bits_per_pixel); fclose(file_in); return NULL; }
    
    //Note: Grayscale image accepts only 8 bpp
    if((header.img_type == 3 || header.img_type == 11) && header.bits_per_pixel != 8)
    { fprintf(stderr, "[TGA_ReadFile]: Byte 16 error (grayscale image type does not support %u bpp).\n", header.bits_per_pixel); fclose(file_in); return NULL; }

    //Byte 17: 
    //Extract last 2 bits of Byte 17(representing how scanlines were displayed on old CRT TVs ==> value could be either 0, 1, 2 or 3)
    //In the modern era, CRT display modes are no longer relevant
    uint8_t scanline_display_mode = (header.img_descriptor >> 6) & 0x03;
    if(scanline_display_mode != 0){ fprintf(stderr, "[TGA_ReadFile]: Byte 17 error (Interleaved format is unsupported).\n", scanline_display_mode); fclose(file_in); return NULL; }
    //extract top_to_bottom flag value(aka 5th bit)
    int is_top_to_bottom = (header.img_descriptor & (1 << 5)) != 0;
    //extract left_to_right flag value(aka 4th bit)
    int is_right_to_left = (header.img_descriptor & (1 << 4)) != 0;
    //#############################

    //##### POST-HEADER(ID & Color map data)#####
    //Skip ID info
    if(header.ID_lenght > 0)
    {
        if(fseek(file_in, header.ID_lenght, SEEK_CUR) != 0)
        { 
            fprintf(stderr, "[TGA_ReadFile]: Failed skipping %u bytes Image ID field.\n", header.ID_lenght); 
            fclose(file_in); return NULL;
        }
    }

    //Even if this library manages only RGB/RGBA and Grayscale images, some image exporters still include a color map(needs to be skipped), here is why:
    //*Some softwares produce a thumbnail/icon preview of the image, utilizing a small color map(saved inside the TGA file) to render it
    //*Some image editors convert a indexed-color mapped image to a RGB/RGBA color image, without clearing the color map data

    if(header.color_map_type != 0 && header.color_map_length > 0)
    {
        //This formula, calculates the number of Bytes that represent the color map:
        //normally length * depth / 8 is enough, but some TGA color map use 15 bits color entries.
        //( Suppose that there is only 1 entry which depth is 15b, 
        //  it needs to occupy 2 Bytes in memory(15b + 1b padding) but the calculation  1 * 15 / 8 = 1(meaning the TGA file is misaligned of 1 Byte).
        //  To resolve the issue, instead of using functions like .ceil(), 
        //  the calculation ⌈A/B⌉ = (A + (B - 1))/B can be used, which in our case is ((length * depth) + 7)/8 )
        
        size_t map_bytes = ((size_t)header.color_map_length * header.color_map_depth + 7) / 8;
        if(fseek(file_in, (long)map_bytes, SEEK_CUR) != 0){ fprintf(stderr, "[TGA_ReadFile]: Failed skipping color map of %zu Bytes.\n", map_bytes); fclose(file_in); return NULL; }
    }
    //###########################################

    //##### Image PAYLOAD #####
    //Allocate TGA pointer(ADT)
    TGAImage* img = TGA_CreateImage(header.img_width, header.img_heigt, header.bits_per_pixel);
    if(!img){ fclose(file_in); return 0; }
    
    //Uncompressed RGB/RGBA or Grayscale image type
    if(header.img_type == 2 || header.img_type == 3)
    {
        //direct sequential read
        size_t data_bytes = (size_t)(img->width) * (img->height) * (img->Bytes_per_pixel);
        if(fread(img->data, 1, data_bytes, file_in) != data_bytes){ fprintf(stderr, "[TGA_ReadFile]: Unexpected EOF reading %zu raw pixel bytes.\n", data_bytes); TGA_DestroyImage(img); fclose(file_in); return NULL; }
    }
    //RLE encoded RGB/RGBA or Grayscale image type
    else if(header.img_type == 10 || header.img_type == 11)
    {
        if(!unpack_RLE_data(img, file_in))
        { 
            TGA_DestroyImage(img);
            return NULL; 
        }
    }
    //#########################

    fclose(file_in);

    //if image was generated from bottom to top, flip it vertically
    if(!is_top_to_bottom)
    {
        if(!flip_vertically(img))
        { TGA_DestroyImage(img); return NULL; }
    }
    //if image was generated from right to left, flip it horizontally
    if(is_right_to_left)
    {
        if(!flip_horizontally(img))
        { TGA_DestroyImage(img); return NULL; }
    }

    return img;
}
int TGA_WriteFile(TGAImage* img, const char* filename, int rle_flag)
{
    if(!filename){fprintf(stderr, "[TGA_ReadFile]: Invalid filename parameter (NULL pointer).\n"); return 0; }
    
    if(!img || !img->data){ fprintf(stderr, "[TGA_WriteFile]: Invalid image data.\n"); return 0; }
    if(img->width == 0){ fprintf(stderr, "[TGA_WriteFile]: Invalid image width.\n"); return 0; }
    if(img->height == 0){ fprintf(stderr, "[TGA_WriteFile]: Invalid image height.\n"); return 0; }

    FILE* file_out = fopen(filename, "wb");
    if(!file_out){ fprintf(stderr, "[TGA_WriteFile]: Failed opening destination(%s): %s", filename, strerror(errno)); return 0; }

    //##### HEADER #####
    //Initialize 18-Byte TGA Header
    TGAHeader header; memset(&header, 0, sizeof(TGAHeader));

    //All data related to the color map is set to 0(as the encoder/decoder does not deal with color mapped images)

    //Byte 0: ID_Lenght
    header.ID_lenght = 0;

    //Byte 1: color_map_type
    header.color_map_type = 0;

    //Byte 2: img_type
    //Grayscale(8 bpp) or RGB(24 bpp)/RGBA(32 bpp)
    
    //*img_type = 11(Grayscale with applied RLE)
    //*img_type = 3(Grayscale with raw pixels)
    if(img->Bytes_per_pixel == 1) header.img_type = rle_flag ? 11 : 3;
    //*img type = 10(RGB/RGBA with applied RLE)
    //*img tyoe = 2(RGB/RGBA with raw pixels)
    else if(img->Bytes_per_pixel == 3 || img->Bytes_per_pixel == 4) header.img_type = rle_flag ? 10 : 2;
    else{ fprintf(stderr, "[TGA_WriteFile]: Invalid pixel depth value(%u).\n"); fclose(file_out); return 0; }

    //Bytes 3 & 4: color_map_origin
    header.color_map_origin = 0;

    //Bytes 5 & 6: color_map_length
    header.color_map_length = 0;

    //Byte 7: color_map_depth
    header.color_map_depth = 0;

    //Bytes 8 & 9: x_origin
    header.x_origin = 0;

    //Bytes 10 & 11: y_origin
    header.y_origin = 0;

    //Bytes 12 & 13: img_width
    header.img_width = img->width;

    //Bytes 14 & 15: img_height
    header.img_heigt = img->height;

    //Byte 16: bits_per_pixel
    header.bits_per_pixel = (uint8_t)(img->Bytes_per_pixel * 8);

    //Byte 17: img_descriptor
    //* bits 0-3: alpha channel depth
    uint8_t alpha_depth = (img->Bytes_per_pixel == 4) ? 8 : 0; //only if RGBA encoding is utilized
    
    //* bit 4: left to right display flag(0 = left_to_right)
    //* bit 5: top to bottom display flag(1 = top_to_bottom) ==> 0x20
    //* bits 6 & 7: CRT TVs scanlines display modes(set to 00)

    //the generated image has a byte 17 = 
    //* 00| 1 | 0 | 1000 if RGBA is utilized
    //* 00| 1 | 0 | 0000 if other color representations are utilized
    header.img_descriptor = (uint8_t)(0x20 | (alpha_depth & 0x0F));

    //Attempt to write TGA header in the file
    if(fwrite(&header, sizeof(TGAHeader),1 , file_out) != 1){ fprintf(stderr, "[TGA_WriteFile]: Failed to write complete 18-byte header.\n"); fclose(file_out); return 0; }
    //##################

    //##### Image PAYLOAD #####
    //Uncompressed RGB/RGBA or Grayscale image type
    if(!rle_flag)
    {
        size_t data_bytes = (size_t)(img->width) * (img->height) * (img->Bytes_per_pixel);
        if(fwrite(img->data, 1, data_bytes, file_out) != data_bytes){ fprintf(stderr, "[TGA_ReadFile]: Unexpected error while writing %zu raw pixel bytes.\n", data_bytes); fclose(file_out); return 0; }
    }
    //RLE encoded RGB/RGBA or Grayscale image type
    else
    {
        if(pack_RLE_data(img, file_out))
        {
            TGA_DestroyImage(img);
            return 0;
        }
    }
    //#########################
    fclose(file_out);
    return 1;
}
//###################################################

//##### Getters & Setters #####
int TGA_GetWidth(const TGAImage* img){ return img->width; }
int TGA_GetHeight(const TGAImage* img){ return img->height; }

TGAColor TGA_GetPixel(const TGAImage* img, int x, int y)
{
    //Instatiate a "void" color
    TGAColor color = {0};

    //Check image boundaries
    if(!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height){ fprintf(stderr, "[TGA_GetPixel]: Boundary error, pixel position (%d, %d) out of range!.\n", x, y); return color; }

    color.Bytes_pp = img->Bytes_per_pixel;
    //Find pixel in the image and use it as starting position to read its RGBA values
    size_t pixel_position = ((size_t)y * img->width + (size_t)x) * color.Bytes_pp;
    for(uint8_t B = 0; B < color.Bytes_pp; B++) color.BGR_A[B] = img->data[pixel_position + B];

    return color;
}
void TGA_SetPixel(TGAImage* img, int x, int y, TGAColor color)
{
    //Check image boundaries
    if(!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height) { fprintf(stderr, "[TGA_GetPixel]: Boundary error, pixel position (%d, %d) out of range!.\n", x, y); return; }
    
    if(color.Bytes_pp != img->Bytes_per_pixel){ fprintf(stderr, "[TGA_SetPixel]: Color format mismatch, using %u bpp but current image expects %u bpp.\n", color.Bytes_pp >> 3, img->Bytes_per_pixel >> 3); return ;}
    //Find pixel in the image and use it as starting position to write its RGBA values
    size_t pixel_position = ((size_t)y * img->width + (size_t)x) * (img->Bytes_per_pixel);
    for(uint8_t B = 0; B < img->Bytes_per_pixel; B++) img->data[pixel_position + B] = color.BGR_A[B];

    return;
}
//#############################
