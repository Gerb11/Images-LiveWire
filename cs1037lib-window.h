/*******************************************************************************
  cs1037lib-window.h -- basic window for drawing and keyboard/mouse polling

  DESCRIPTION:
     Provides functions to make the following things easy:
      - draw shapes in a window,
      - poll keyboard and mouse for input.

  EXAMPLES:
     The most basic program possible is to just pop up an empty window:
        
        #include "cs1037lib-window.h"
        
        void main() {
            SetWindowVisible(true);          // Show the main window.
            while (!WasWindowClosed()) { }   // Wait for user to close it.
        }


     You could also draw something in the window:

        #include "cs1037lib-window.h"
        
        void main() {
            SetDrawColour(0,0,0);                           // Set drawing colour to black
            DrawRectangleFilled(0,0,2000,2000);             // Fill the whole window with black
            SetDrawColour(0,255,0);                         // Set drawing colour to bright green
            DrawText("Why so green and lonely?",100,150);   // Draw text at x,y position 100,150

            SetWindowVisible(true);          // Show the main window.
            while (!WasWindowClosed()) { }   // Wait for user to close it.
        }

     
     You can check to see if the keyboard was pressed recently:
     
        #include "cs1037lib-window.h"
        #include <iostream>
        using namespace std;
        
        void main() {
            SetWindowVisible(true);
            while (!WasWindowClosed()) { 
                char key;
                if (GetKeyboardInput(&key))
                    cout << "Key pressed was " << key << endl;
            }
        }


     You can also track where the mouse is moving, and if any buttons were pressed:

        #include "cs1037lib-window.h"
        #include <iostream>
        using namespace std;
        
        void main() {
            SetWindowVisible(true);
            while (!WasWindowClosed()) { 
                int x,y,button;
                if (GetMouseInput(&x,&y,&button)) {
                    if (button > 0)
                        cout << "button " <<  button << " pressed at "  << x << "," << y << endl;
                    else if (button < 0)
                        cout << "button " << -button << " released at " << x << "," << y << endl;
                    else
                        cout << "mouse moved to " << x << "," << y << endl;
                }
            }
        }


*******************************************************************************/

#ifndef __CS1037LIB_WINDOW_H__
#define __CS1037LIB_WINDOW_H__

// SetWindowVisible:
//    Call SetWindowVisible(true) to make the main window show up on screen.
//
void SetWindowVisible(bool visible);

// SetWindowTitle:
//    Sets the text in the title bar of the window
//
void SetWindowTitle(const char* title);

// SetWindowPosition, SetWindowSize:
//    Repositions the top-left corner of the window, and resizes the window.
//
void SetWindowPosition(int x, int y);
void SetWindowSize(int sizeX, int sizeY);

// WasWindowClosed:
//    Returns true if the main window was closed, via the little X button or otherwise.
//
bool WasWindowClosed();

// GetKeyboardInput:
//    Returns true if a key has been pressed since the last time this was called.
//    After returning true, 'key' will contain the (lower-case) ASCII character 
//    corresponding to the key that was pressed. The ENTER key returns '\r', by the way.
//    Keys that do not correspond to ASCII characters (pgup, arrows, etc) are ignored!
//
// Example:
//    char key;
//    if (GetKeyboardInput(&key))
//        cout << "Key pressed was " << key << endl;
//
bool GetKeyboardInput(char* key);

// GetMouseInput:
//    Returns true if the mouse has moved or if a button has changed state (up/down).
//    After returning, mouseX and mouseY will contain the position of the mouse
//    relative to the top-left corner of the window. 
//    If a mouse button transitions from the up to down state, mouseButton will contain
//    the number of that button (1, 2, or 3 for left, right or middle). If the 
//    button transitions from down to up, then the number will be negative.
//
// Example:
//    int x,y,button;
//    if (GetMouseInput(&x,&y,&button)) {
//        if (button > 0)
//           cout << "mouse button number " <<  button << " was pressed at "  << x << "," << y << endl;
//        else if (button < 0)
//           cout << "mouse button number " << -button << " was released at " << x << "," << y << endl;
//        else
//           cout << "mouse moved to " << x << "," << y << endl;
//    }
//
bool GetMouseInput(int* mouseX, int* mouseY, int* mouseButton);

// SetDrawColour:
//    All drawing functions will draw using the RGB colour specified here.
//    Each colour component should be from 0.0 to 1.0.
//
// Example:
//    SetDrawColour(255,0,0);                // pure red
//    DrawText("red text",0,0);
//    DrawText("more red",0,16);
//    SetDrawColour(255,255,0);              // yellow
//    DrawText("yellow text",0,32);
//
void SetDrawColour(unsigned char r, unsigned char g, unsigned char b);

// DrawText:
//    Coordinate (x0,y0) will be the top-left corner of the
//    text that is drawn, which is given by 'str'.
//
void DrawText(int x0, int y0, const char* str);

// DrawRectangleOutline, DrawRectangleFilled:
//    Coordinate (x0,y0) will be the top-left corner of the
//    rectangle and (x1-1, y1-1) will be the bottom left corner
//    (i.e. if x0==x1 or y0==y1 no drawing will take place).
//    Outlines are drawn 'inside' the shape, not outside.
//
void DrawRectangleOutline(int x0, int y0, int x1, int y1);
void DrawRectangleFilled(int x0, int y0, int x1, int y1);

// DrawEllipseOutline, DrawEllipseFilled:
//    The ellipse will be centered at (x,y) with a separate radius for
//    each axis (radiusX, radiusY).
//    Outlines are drawn 'inside' the shape, not outside.
//
void DrawEllipseOutline(int x, int y, int radiusX, int radiusY);
void DrawEllipseFilled(int x, int y, int radiusX, int radiusY);

// DrawLine:
//    Draws a line segment from (x0,y0) to (x1,y1).
//
void DrawLine(int x0, int y0, int x1, int y1);

// DrawPolygonOutline, DrawPolygonFilled:
//    DrawPolygonOutline draws 'count-1' line segments connecting 
//    the points listed in 'xy'. Integers xy[0],xy[1] define the first 
//    point (x0,y0), while xy[2],xy[3] define the second point etc.
//    DrawPolygonFilled works the same way except it fills the
//    interior region defined by those lines.
//
// Example:
//    int xy[] = {  50, 100,   // (x0,y0)
//                 100,  50,   // (x1,y1)
//                 150, 100,   // (x2,y2)
//                 100, 150 }; // (x3,y3) defines a diamond shape
//    SetDrawColour(0,255,0);
//    DrawPolygonFilled(xy,4); // green diamond!
//
void DrawPolygonOutline(int* xy, int count);
void DrawPolygonFilled(int* xy, int count);

// SetDrawAxis:
//    GetMouseInput() and all DrawXXX() functions place the origin (0,0) 
//    at the top-left of the window by default. SetDrawAxis lets you 
//    draw/receive all coordinates with respect to a different axis origin.
//
// Example:
//    SetWindowSize(512,512);
//    SetDrawAxis(256,256,true);  // Origin at center of window, Y increases upwards.
//
void SetDrawAxis(int originX, int originY, bool flipY);

#endif // __CS1037LIB_WINDOW_H__
