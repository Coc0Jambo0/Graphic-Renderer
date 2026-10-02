#include "TGA_lib/TGA_lib.h"

const TGAColor white = {.BGR_A = {255, 255, 255, 255}, .Bytes_pp = 3};
const TGAColor blue = {.BGR_A = {255, 0, 0, 255}, .Bytes_pp = 3};
const TGAColor green = {.BGR_A = {0, 255, 0, 255}, .Bytes_pp = 3};
const TGAColor red = {.BGR_A = {0, 0, 255, 255}, .Bytes_pp = 3};

int main(int argc, char* argv[])
{
    const int width = 64;
    const int height = 64;
    
    TGAImage* img = TGA_CreateImage(width, height, 24);
    if(!img) return -1;

    int ax =  7, ay =  3;
    int bx = 12, by = 37;
    int cx = 62, cy = 53;

    // Draw a 20x20 white square in the center
    TGA_SetPixel(img, ax, ay, white);
    TGA_SetPixel(img, bx, by, white);
    TGA_SetPixel(img, cx, cy, white);

    flip_vertically(img);
    if(!TGA_WriteFile(img, "bresenham_test_image.tga", 1)) return -1;

    return 0;
}