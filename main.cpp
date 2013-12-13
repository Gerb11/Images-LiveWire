#include "cs1037lib-window.h" // for basic drawing and keyboard/mouse input operations 
#include "cs1037lib-button.h" // for basic buttons and other GUI controls 
#include "Cstr.h"       // convenient macros for generating C-style text strings
#include <iostream>     // for cout
#include <vector>
#include <stack>
#include "Basics2D.h"
#include "Table2D.h"
#include "Math2D.h"
#include "Image2D.h"
#include "wire.h" // declarations of global variables and functions shared with wire.cpp


// declarations of global variables used for GUI controls/buttons/dropLists
const char* image_names[] = { "logo" , "uwocampus", "canada", "original" , "model" , "liver" }; // an array of image file names
int im_index = 1;    // index of currently opened image (inital value)
int save_counter[] = {0,0,0,0,0,0}; // counter of saved results for each image above 
const char* mode_names[]  = { "CONTOUR", "REGION"}; // an array of mode names 
enum Mode  {DRAW_CONTOUR=0, DRAW_REGION=1}; // see main.cpp
Mode mode = DRAW_CONTOUR; // index of the current mode (in the array 'mode_names')
bool view = false; // flag set by the check box
const int cp_height = 34; // height of "control panel" (area for buttons)
const int pad = 10; // width of extra "padding" around image (inside window)
int T = 10; // region growing threshold
int T_box; // handle for T-box (for setting threshold parameter T)
int check_box_view; // handle for 'view' check-box

// declarations of global functions used in GUI (see code below "main")
void left_click(Point click);   // call-back function for left mouse-clicks
void right_click(Point click);  // call-back function for right mouse-clicks
void image_load(int index); // call-back function for dropList selecting image file
void image_save();  // call-back function for button "Save Result"
void mode_set(int index);   // call-back function for dropList selecting mode 
void clear();  // call-back function for button "Clear"
void view_set(bool v);  // call-back function for check box for "view" 
void T_set(const char* T_string);  // call-back function for setting parameter "T" (threshold for region growing) 

                           

int main()
{
 	cout << " left/right clicks - enter contour points or region seeds " << endl; 
 	cout << " click 'Clear' or press 'd'-key to delete current 'contour' or 'region' " << endl; 
	cout << " click 'X'-box to close the application \n\n " << endl; 

	  // initializing buttons/dropLists and the window (using cs1037utils methods)
	int blank = CreateTextLabel(""); // adds grey "control panel" for buttons/dropLists, see "cs1037utils.h"
    SetControlPosition(blank,0,0); SetControlSize(blank,1280,cp_height); // see "cs1037utils.h"
	int dropList_files = CreateDropList(6, image_names, im_index, image_load); // the last argument specifies the call-back function, see "cs1037utils.h"
	int label = CreateTextLabel("Mode:"); // see "cs1037utils.h"
	int dropList_modes = CreateDropList(2, mode_names, mode, mode_set); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_clear = CreateButton("Clear",clear); // the last argument specifies the call-back function, see "cs1037utils.h"
	int button_save = CreateButton("Save",image_save); // the last argument specifies the call-back function, see "cs1037utils.h"
	T_box = CreateTextBox(to_Cstr("T=" << T), T_set); 
	check_box_view = CreateCheckBox("view" , view, view_set); // see "cs1037utils.h"
	SetWindowTitle("Live-Wire");      // see "cs1037utils.h"
    SetDrawAxis(pad,cp_height+pad,false); // sets window's "coordinate center" for GetMouseInput(x,y) and for all DrawXXX(x,y) functions in "cs1037utils" 
	                                      // we set it in the left corner below the "control panel" with buttons

	  // initializing the application
	image_load(im_index);
	SetWindowVisible(true); // see "cs1037utils.h"

	  // while-loop processing keys/mouse interactions 
	while (!WasWindowClosed()) // WasWindowClosed() returns true when 'X'-box is clicked
	{
		char c;
		if (GetKeyboardInput(&c)) // check keyboard
		{ 
			if (c == 'd') clear(); // 
		}

		int x, y, button;
		if (GetMouseInput(&x, &y, &button)) // checks if there are mouse clicks or mouse moves
		{
			Point mouse(x,y); // STORES PIXEL OF MOUSE CLICK
			if      (button == 1) left_click(mouse);   // button 1 means the left mouse button
			else if (button == 2) right_click(mouse);  // button 2 means the right mouse button
			draw(mouse);
		}
	}

	  // deleting the controls
	DeleteControl(button_clear);    // see "cs1037utils.h"
	DeleteControl(dropList_files);
	DeleteControl(dropList_modes);     
	DeleteControl(label);
	DeleteControl(check_box_view);
	DeleteControl(button_save);
	DeleteControl(T_box);
	return 0;
}

// call-back function for the 'mode' selection dropList 
// 'int' argument specifies the index of the 'mode' selected by the user among 'mode_names'
void mode_set(int index)
{
	mode = (Mode) index;
	cout << "drawing mode is set to " << mode_names[index] << endl;
	reset_segm();
	draw();
}

// call-back function for the 'image file' selection dropList
// 'int' argument specifies the index of the 'file' selected by the user among 'image_names'
void image_load(int index) 
{
	im_index = index;
	cout << "loading image file " << image_names[index] << ".bmp" << endl;
	image = loadImage<RGB>(to_Cstr(image_names[index] << ".bmp")); // global function defined in Image2D.h
	int width  = max(400,(int)image.getWidth()) + 2*pad + 80;
	int height = max(100,(int)image.getHeight())+ 2*pad + cp_height;
	SetWindowSize(width,height); // window height includes control panel ("cp")
    SetControlPosition(    T_box,     image.getWidth()+pad+5, cp_height+pad);
    SetControlPosition(check_box_view,image.getWidth()+pad+5, cp_height+pad+25);
	reset_segm();  // clears current "contour" and "region" objects - function in a4.cpp
	draw();
}

// call-back function for button "Clear"
void clear() { 
	reset_segm(); // clears current "contour" and "region" objects - function in wire.cpp
	draw();
}

// call-back function for check box for "view" check box 
void view_set(bool v) {view=v; draw();}  

// call-back function for setting parameter "T"  
void T_set(const char* T_string) {
	sscanf_s(T_string,"T=%d",&T);
	cout << "parameter T is set to " << T << endl;
}

// call-back function for left mouse-click
void left_click(Point click)
{
	if (!image.pointIn(click)) return;
	if      (mode==DRAW_CONTOUR) addToContour(click); // function in wire.cpp
	else if (mode==DRAW_REGION && region[click]==0) regionGrow(click,T); // function in wire.cpp
}

// call-back function for right mouse-click
void right_click(Point click)
{
	if (!image.pointIn(click)) return;
	if      (mode==DRAW_CONTOUR) addToContourLast(click); // function in wire.cpp
	else if (mode==DRAW_REGION  && region[click]==0)  regionGrow(click,T); // function in wire.cpp
}

// window drawing function
void draw(Point mouse)
{ 
	unsigned i;
	// Clear the window to white
	SetDrawColour(255,255,255); DrawRectangleFilled(-pad,-pad,1280,1024);
	
	if (!image.isEmpty()) drawImage(image); // draws image (object defined in wire.cpp) using global function in Image2D.h (1st draw method there)
	else {SetDrawColour(255, 0, 0); DrawText(2,2,"image was not found"); return;}

	// look-up tables specifying colors and transparancy for each possible integer value (0,1,2,3) in "region"
	RGB    colors[4]       = { black,  blue,  red,  white};
	double transparancy[4] = { 0,      0.2,   0.2,  1.0};
	if ((mode==DRAW_REGION || view || closedContour) && !region.isEmpty()) drawImage(region,colors,transparancy); // 4th draw() function in Image2D.h

	if (mode==DRAW_CONTOUR && !contour.empty() && !closedContour) 
	{	// Draws "contour" - object declared in wire.cpp
		SetDrawColour(255,0,0); 
		for (i = 0; i<(contour.size()-1); i++)  
			DrawLine(contour[i].x,   contour[i].y, 
					 contour[i+1].x, contour[i+1].y);

		// If contour is 'open' and mouse is over the image, draw extra 'preview' line segment to current mouse pos.
		if (image.pointIn(mouse) && !closedContour) 
		{
			stack<Point> * path = liveWire(mouse);
			if (path) {
				Point cur, prev = contour[i]; // start from the last contour point
				while (!path->empty()) {cur=path->top(); path->pop(); DrawLine(prev.x,prev.y,cur.x,cur.y); prev=cur;}
				delete path;
			}
		}
	}
}

// call-back function for button "Save"
void image_save() 
{
	// FEEL FREE TO MODIFY THE CODE BELOW TO SAVE WHATEVER IMAGE YOU WOULD LIKE!!!
	cout << "saving the results into image file... ";
	Table2D<RGB> tmp;

	if (mode==DRAW_REGION || view) {    // SAVES REGION MASK OVER THE ORIGINAL IMAGE (similar to what is drawn on the window)
		RGB    colors[4]       = { black,  blue,  red,  white};
		double transparancy[4] = { 0,      0.2,   0.2,  1.0};
		Table2D<RGB>    cMat = convert<RGB>(region,Palette(colors));   //3rd convert method in Table2D.h
		Table2D<double> aMat = convert<double>(region,Palette(transparancy)); 
		tmp = cMat%aMat + image%(1-aMat);
	}
	else if (mode==DRAW_CONTOUR && !contour.empty()) {  // SAVES RED CONTOUR (LIVE-WIRE) OVER THE ORIGINAL IMAGE 
		tmp = image;           // (ALTERNATIVELY, COPY THE CODE ABOVE (FOR mode==DRAW_REGION) TO SAVE THE CROPPED-OUT SEGMENT)
		for (unsigned i=0; i<contour.size(); i++) tmp[contour[i]] = red;
	}
	
	save_counter[im_index]++;
	string name(to_Cstr("Results/result_" << image_names[im_index] << save_counter[im_index] << ".bmp"));
	saveImage(tmp, to_Cstr(name)); // saves to BMP file
	cout << name << endl; // prints the name of the saved .bmp file on the console
}
