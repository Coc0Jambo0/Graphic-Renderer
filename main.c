#include "TGA_lib/TGA_lib.h"

int main(int argc, char* argv[])
{
    const TGAColor color1 = {.BGR_A = {20, 209, 239, 0}, .Bytes_pp = 4};
    const TGAColor color2 = {.BGR_A = {0, 0, 0, 255}, .Bytes_pp = 4};


    const int width = 64; const int height = 64;
    
    TGAImage* img = TGA_CreateImage(width, height, 32);
    if(!img) return -1;

    int stripe_height = 8;
    for (int y = 0; y < height; y++) 
    {
        //Determine color based on which stripe band the current row falls into

        TGAColor current_color = ((y / stripe_height) % 2 == 0) ? color1 : color2;

        for (int x = 0; x < width; x++) {
            TGA_SetPixel(img, x, y, current_color);
        }
    }

    TGA_WriteFile(img, "test_image.tga", 1);
    return 0;
}