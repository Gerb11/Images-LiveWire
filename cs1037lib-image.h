/*******************************************************************************
  cs1037lib-image.h -- Working with bitmap images

  DESCRIPTION:
     Provides functions to both read/write bitmap image files, as well as
     to draw bitmaps anywhere within the main window.
     Can read PNG/BMP images from disk. (Transparency only supported in PNG.)
     Can write BMP images to disk.

  EXAMPLES:
     Load an image and draw it in the main window:
        
        #include "cs1037lib-window.h"
        #include "cs1037lib-image.h"
        
        void main() {
            int uwologo = LoadImage("uwologo.png"); // Load image from file
            DrawImage(uwologo,0,0);                 // Draw image at top-left corner

            SetWindowVisible(true);                 // Show main window 
            while (!WasWindowClosed()) { }          // Wait for user to close it

            DeleteImage(uwologo);                   // Free the memory used by the image
        }

*******************************************************************************/

#ifndef __CS1037LIB_IMAGE_H__
#define __CS1037LIB_IMAGE_H__

// LoadImage
//    Reads a PNG or BMP image from disk.
//    Returns a positive integer (a 'handle') that uniquely identifies 
//    the image. Use this integer handle when calling DrawImage etc.
//
// Example:
//    int uwologo = LoadImage("uwologo.png"); // Load image from file
//    DrawImage(uwologo,0,0);                 // Draw image at top-left corner
//
int LoadImage(const char* fileName);

// SaveImage
//    Saves an image to disk with the given file name.
//    Can only write to BMP files (.bmp).
//  
void SaveImage(int handle, const char* fileName);

// CreateImage
//    Allocates pixels for an image of a specific size.
//    Returns an integer (a 'handle') that identifies the image.
//
//    A bitmap must use one of several pixel formats, specified
//    by passing a string to 'format'
//       "bgra" -- use BGRA pixel format (4 bytes per pixel)
//       "bgr"  -- use BGR pixel format (3 bytes per pixel)
//       "gray" -- use I pixel format (gray intensity, 1 byte per pixel)
//
// Example:
//    See example code for GetImagePixelPtr below.
//
int CreateImage(int sizeX, int sizeY, const char* format);

// GetImageSizeX, GetImageSizeY
//    Returns the internal dimensions of the image, in pixels.
//
int GetImageSizeX(int handle);
int GetImageSizeY(int handle);

// GetImageBytesPerPixel
//    Returns how many bytes are used per pixel. Useful when
//    you've loaded an image file but need different code to handle
//    grayscale versus colour images.
//     * 4 bytes corresponds to BGRA format (A = alpha transparency)
//     * 3 bytes corresponds to BGR format
//     * 1 byte  corresponds to I format (I = gray intensity)
//
int GetImageBytesPerPixel(int handle);

// GetImagePixelPtr
//    Returns a pointer to the raw bytes that define the
//    image's colour data. Only needed if you want to 
//    inspect/modify specific pixels. 
//
// Example:
//    int h = CreateImage(9, 9, "bgr");
//    for (int x = 0; x < 9; ++x) {
//        unsigned char* p = GetPixelPtr(h, x, 4);
//        p[0] = 0; p[1] = 0; p[2] = 255;  // set pixel at (x,4) to red
//    }
//    DrawImage(h, 20, 10); // draw a 9x9 black box with horizontal red line inside
//
// Note for advanced users:
//    RGB and gray images with odd width will have padding bytes at end of row.
//
unsigned char* GetPixelPtr(int handle, int x, int y);

// DrawImage, DrawImageStretched:
//    The top-left corner will be at (x0,y0). If you stretch the image,
//    the bottom-right corner will be at (x1,y1).
//
void DrawImage(int handle, int x0, int y0);
void DrawImageStretched(int handle, int x0, int y0, int x1, int y1);

// DeleteImage
//    Be sure to call this to release memory when you're done with
//    an image. (images can take a LOT of memory!)
//
void DeleteImage(int handle);

#endif // __CS1037LIB_IMAGE_H__
