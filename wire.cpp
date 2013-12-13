#include <iostream>     // for cout, rand
#include <vector>       // STL data structures
#include <stack>
#include <queue> 
#include "Basics2D.h"
#include "Table2D.h"
#include "Math2D.h"
#include "Image2D.h"
#include "cs1037lib-time.h"  // for "pausing"  
#include "wire.h"


// GLOBAL PARAMETERS AND SPECIALISED DATA TYPES
static const Point shift[8]={Point(-1, -1), Point(-1, 0), Point(-1, 1), Point(0, 1), Point(1, 1), Point(1, 0), Point(1, -1), Point(0, -1)};
enum Direction {BOTTOMLEFT = 0, LEFT = 1, TOPLEFT = 2, TOP = 3, TOPRIGHT = 4, RIGHT = 5, BOTTOMRIGHT = 6, BOTTOM = 7, NONE=10};
const Direction Reverse[8]={TOPRIGHT, RIGHT, BOTTOMRIGHT, BOTTOM, BOTTOMLEFT, LEFT, TOPLEFT, TOP}; 
double fn(const double t) {double w=100.0; return 1.0/(1.0+w*t);}  // function used for setting "pixel penalties" ("Sensitivity" w is a tuning parameter)

// declarations of global variables 
Table2D<RGB> image; // image is "loaded" from a BMP file by function "image_load" in "main.cpp" 
vector<Point> contour; // list of "contour" points
bool closedContour=false; // a flag indicating if contour was closed 
Table2D<int> region;  // 2D array storing binary mask for pixels (1 = "in", 0 = "out", 2 = "active front")
Table2D<double> penalty; // 2d array of pixels' penalties computed in reset_segm() 
// Low penalty[x][y] implies high image "contrast" at image pixel p=(x,y).

Table2D<MyPoint> edgeWeights;

Table2D<double>    dist;     // 2D table of "distances" for current paths to the last seed
Table2D<Direction> toParent; // 2D table of "directions" along current paths to the last seed. 


// GUI calls this function when button "Clear" is pressed, or when new image is loaded
// THIS FUNCTION IS FULLY IMPLEMENTED, YOU DO NOT NEED TO CHANGE THE CODE IN IT
void reset_segm()
{
	cout << "resetting 'contour' and 'region'" << endl;

	// removing all region markings
	region.reset(image.getWidth(),image.getHeight(),0);

	// remove all points from the "contour"
	while (!contour.empty()) contour.pop_back();
	closedContour=false;

	// resetting 2D tables "dist" and "toParent" (erazing paths) 
	dist.reset(image.getWidth(),image.getHeight(),INFTY);
	toParent.reset(image.getWidth(),image.getHeight(),NONE);

	// recomputing "penalties" from an estimate of image contrast at each pixel p=(x,y)
	if (image.isEmpty()) {penalty.resize(0,0); return;} 
	Table2D<double> contrast = grad2(image);  //(implicit conversion of RGB "image" to "Table2D<double>") 
	// NOTE: function grad2() (see Math2D.h) computes (at each pixel) expression  Ix*Ix+Iy*Iy  where Ix and Iy
	// are "horizontal" and "vertical" derivatives of image intensity I(x,y) - the average of RGB values.
	// This expression describes the "rate of change" of intensity, or local image "contrast". 
	penalty = convert(contrast,&fn); // "&fn" - address of function "fn" (defined at the top of this file) 
	// "convert" (see Math2D.h) sets penalty at each pixel according to formula "penalty[x][y] = fn (contrast[x][y])" 
	computeEdgePaths();
}


/////////////////////////////////////////////////////////////////////////////////////////
// GUI calls this function as "mouse" moves over the image to any new pixel p. It is also 
// called inside "addToContour" for each mouse click in order to add the current live-wire
// to the contour. Function should allocate, initialize, and return a path (stack) of 
// adjacent points from p to the current "seed" (last contour point) but excluding the seeed. 
// The path should follow directions in 2D table "toParent" computed in "computePaths()".
stack<Point>* liveWire(Point p)
{
	if (contour.empty()) return NULL;  

	// Create a new stack of Points dynamically.
	stack<Point> * path = new stack<Point>();

	// Add point 'p' into it. Then (iteratively) add its "parent" according to
	// 2D Table "toParent", then the parent of the parent, e.t.c....    until
	// you get a pixel that has parent "NONE". In case "toParent" table is 
	// computed correctly inside "computePaths(seed)", this pixel should be 
	// the current "seed", so you do not need to add it into the "path".
	// ....
	Direction n = toParent[p];
	while(n!=NONE) {path->push(p); p=p+shift[n]; n = toParent[p];}

	// Return the pointer to the stack.
	return path;
}

// GUI calls this function when a user left-clicks on an image pixel while in "Contour" mode
void addToContour(Point p) 
{
	if (closedContour) return;

	// if contour is empty, append point p to the "contour", 
	// else (if contour is not empty) append to the "contour" 
	// all points returned by "live-wire(p)" (a path from p to last seed). 
	if (contour.empty()) contour.push_back(p);
	else 
	{   
		stack<Point> *path = liveWire(p);
		if (path) while (!path->empty()) {contour.push_back(path->top()); path->pop();}
		delete path;
	}
	// The call to computePaths(p) below should precompute "optimal" paths 
	// to the new seed p and store them in 2D table "toParent".
	computePaths(p);
}

// GUI calls this function when a user right-clicks on an image pixel while in "Contour" mode
void addToContourLast(Point p)
{
	if (closedContour || contour.empty()) return;

	// add your code below to "close" the contour using "live-wire"
	addToContour(p);
	// extracting live-wire from the first contour point to point p
	stack<Point> *path = liveWire(contour[0]);
	if (path) { // note: last point in "path" is already in "contour"
		while (path->size()>1) {contour.push_back(path->top()); path->pop();}
		delete path;
	}
	closedContour = true;
	draw();

	cout << "interior region has volume of " << contourInterior() << " pixels" << endl; 

}

// GUI calls this function when a user left-clicks on an image pixel while in "Region" mode
// The method computes a 'region' of pixels connected to 'seed'. The region is grown by adding
// to each pixel p (starting at 'seed') all neighbors q such that intensity difference is small,
// that is, |Ip-Iq|<T where T is some fixed threshold. Use Queue for active front. 
// To compute intensity difference Ip-Iq, use function dI(RGB,RGB) defined in "Image2D.h"   
void regionGrow(Point seed, double T)
{
	if (!image.pointIn(seed)) return;
	int counter = 0;                  
	queue<Point> active;        
	active.push(seed);

	// use BREADTH-FIRST_SEARCH (FIFO order) traversal - "Queue"
	while (!active.empty())
	{
		Point p = active.front();
		active.pop();
		region[p]=1;  // pixel p is extracted from the "active_front" and added to "region", but
		counter++;    // then, all "appropriate" neighbors of p are added to "active_front" (below)
		for (int i=0; i<8; i++) // goes over 8 neighbors of pixel p because it includes the diagonals
		{   // uses overloaded operator "+" in Point.h and array of 'shifts' (see the top of file)
			Point q = p + shift[i]; // to compute a "neighbor" of pixel p
			if (image.pointIn(q) && region[q]==0 && abs(dI(image[p],image[q]))<T) 
			{ // we checked if q is inside image range and that the difference in intensity
				// between p and q  is sufficiently small 	 
				active.push(q); 
				region[q]=2; // "region" value 2 corresponds to pixels in "active front".
			}                 // Eventually, all pixels in "active front" are extracted 
		}                     // and their "region" value is set to 1.
		if (view && counter%60==0) {draw(); Pause(20);} // visualization, skipped if checkbox "view" is off
	}
	cout << "grown region of volume " << counter << " pixels    (threshold " << T << ", BFS-Queue)" << endl;
}

///////////////////////////////////////////////////////////////////////////////////
// computePaths() is a function for "precomputing" live-wire (shortest) paths from 
// any image pixel to the given "seed". This method is called inside "addToContour" 
// for each new mouse click. Optimal path from any pixel to the "seed" should accumulate 
// the least amount of "penalties" along the path (each pixel p=(x,y) on a path contributes 
// penalty[p] precomputed in "reset_segm()"). In this function you should use 2D tables 
// "toParent" and "dist" that store for each pixel its "path-towards-seed" direction and, 
// correspondingly, the sum of penalties on that path (a.k.a. "distance" to the seed). 
// The function iteratively improves paths by traversing pixels from the seed until 
// all nodes have direction "toParent" giving the shortest (cheapest) path to the "seed".
void computePaths(Point seed)
{
	if (!image.pointIn(seed)) return;
	region.reset(0); // resets 2D table "region" for visualization

	// Reset 2D arrays "dist" and "toParent"  (erazing all current paths)
	dist.reset(INFTY);
	toParent.reset(NONE);
	dist[seed] = 0;
	int counter = 0; // just for info printed at the bottom of the function

	// Create a queue (or priority_queue) for "active" points/pixels and 
	// traverse pixels to improve paths stored in "toParent" (and "dist") 
	priority_queue<MyPoint> active;
	
	MyPoint startSeed = edgeWeights[seed];
	startSeed.setPriority(0);
	active.push(startSeed); 
	while(!active.empty())
	{
		MyPoint p = active.top(); 
		active.pop();  
		counter++;
		if (region[p]==1) continue; // left-over copy of p
		region[p]=1; 

		for(int i = 0; i < 8; i++) {
			Point q = p + shift[i];
			if(!image.pointIn(q)) {
				continue;
			}
			double edgeWeight;
			if(i == BOTTOMLEFT) {
				edgeWeight = p.getBottomLeftEdge();
			} else if(i == LEFT) {
				edgeWeight = p.getLeftEdge();
			}  else if(i == TOPLEFT) {
				edgeWeight = p.getTopLeftEdge();
			}  else if(i == TOP) {
				edgeWeight = p.getTopEdge();
			}  else if(i == TOPRIGHT) {
				edgeWeight = p.getTopRightEdge();
			}  else if(i == RIGHT) {
				edgeWeight = p.getRightEdge();
			}  else if(i == BOTTOMRIGHT) {
				edgeWeight = p.getBottomRightEdge();
			}  else if(i == BOTTOM) {
				edgeWeight = p.getBottomEdge();
			}
			double newDistance = edgeWeight + dist[p];

			if(image.pointIn(q) && region[q] != 1 && newDistance < dist[q]) {
				// better path is found for "q" 
				toParent[q] = Reverse[i];// direction to a new "parent" on a better path to "seed"
				dist[q] = newDistance;
				MyPoint newPoint = edgeWeights[q]; //grabs the new point with the edge weights calculated for. 
				newPoint.setPriority(newDistance); //set priority for sorting purposes
				active.push(newPoint);// better new "distance" to seed along that path
				region[q] = 2;// value for points inside active front
			}
			
		}

		if (view && counter%60==0) {draw(); Pause(20);} // visualization, skipped if checkbox "view" is off
	} 
	cout << "paths computed,  number of 'pops' = " << counter 
		<<  ",  number of pixels = " << (region.getWidth()*region.getHeight()) << endl; 
}

void computeEdgePaths()	{

	edgeWeights.reset(image.getWidth(), image.getHeight(), MyPoint());
	double sqrtOfTwo = sqrt(2);
	for(int i = 0; i < image.getWidth(); i++) {
		for(int j = 0; j < image.getHeight(); j++) {
			Point p = Point(i , j);
			MyPoint addingPoint = MyPoint(p, 0);
			for(int k = 0; k < 8; k++) {
				Point q = p + shift[k];
				if(!image.pointIn(q)) {
					continue;
				}

				/*double bottomLeftPixel = image[p.x - 1][p.y - 1];
				double leftPixel = image[p.x - 1][p.y];
				double topLeftPixel = image[p.x - 1][p.y + 1];
				double topPixel = image[p.x][p.y + 1];
				double topRightPixel = image[p.x + 1][p.y + 1];
				double rightPixel = image[p.x + 1][p.y];
				double bottomRightPixel = image[p.x + 1][p.y - 1];
				double bottomPixel = image[p.x][p.y - 1];*/
				double difference = 0;
				double edgeCost = 0;
				if(k == BOTTOMLEFT) {
					difference = (I(image[p.x - 1][p.y]) - I(image[p.x][p.y - 1])) / sqrtOfTwo;
					edgeCost = abs(fn(difference));
					addingPoint.setBottomLeftEdge(edgeCost);
				} else if(k == LEFT && p.y - 1 >= 0 && p.y + 1 < image.getHeight()) {
					difference = ((I(image[p.x - 1][p.y + 1]) - I(image[p.x][p.y + 1])) - (I(image[p.x - 1][p.y - 1]) - I(image[p.x][p.y - 1])));
					edgeCost = abs(fn(difference));
					addingPoint.setLeftEdge(edgeCost);
				} else if(k == TOPLEFT) {
					difference = (I(image[p.x - 1][p.y]) - I(image[p.x][p.y + 1])) / sqrtOfTwo;
					edgeCost = abs(fn(difference));
					addingPoint.setTopLeftEdge(edgeCost);
				} else if(k == TOP && p.x - 1 >= 0 && p.x + 1 < image.getWidth()) {
					difference = ((I(image[p.x - 1][p.y + 1]) - I(image[p.x - 1][p.y])) - (I(image[p.x + 1][p.y + 1]) - I(image[p.x + 1][p.y]))) / 4;
					edgeCost = abs(fn(difference));
					addingPoint.setTopEdge(edgeCost);
				} else if(k == TOPRIGHT) {
					difference = (I(image[p.x + 1][p.y]) - I(image[p.x][p.y + 1])) / sqrtOfTwo;
					edgeCost = abs(fn(difference));
					addingPoint.setTopRightEdge(edgeCost);
				} else if(k == RIGHT && p.y - 1 >= 0 && p.y + 1 < image.getHeight()) {
					difference = ((I(image[p.x + 1][p.y + 1]) - I(image[p.x][p.y + 1])) - (I(image[p.x][p.y - 1]) - I(image[p.x + 1][p.y - 1]))) / 4;
					edgeCost = abs(fn(difference));
					addingPoint.setRightEdge(edgeCost);
				} else if(k == BOTTOMRIGHT) {
					difference = (I(image[p.x][p.y - 1]) - I(image[p.x + 1][p.y])) / sqrtOfTwo;
					edgeCost = abs(fn(difference));
					addingPoint.setBottomRightEdge(edgeCost);
				} else if(k == BOTTOM && p.x - 1 >= 0 && p.x + 1 < image.getWidth()) {
					difference = ((I(image[p.x - 1][p.y - 1]) - I(image[p.x - 1][p.y]))  - (I(image[p.x + 1][p.y - 1]) - I(image[p.x + 1][p.y])))/ 4;
					edgeCost = abs(fn(difference));
					addingPoint.setBottomEdge(edgeCost);
				}
			}
			edgeWeights[p] = addingPoint;
		}
	}

}

// This function is called at the end of "addToContourLast()". Function 
// "contourInterior()" returns volume of the closed contour's interior region 
// (including points on the contor itself), or -1 if contour is not closed yet.
int contourInterior()
{
	if (!closedContour || image.isEmpty()) return -1;
	int counter = 0;

	region.reset(0);
	for (unsigned i = 0; i < contour.size(); ++i) region[contour[i]] = 3;

	queue<Point> active;
	active.push(Point(0,0)); // note: non-empty image guarantees that pixel p=(0,0) is inside image
	while(!active.empty())
	{
		Point p=active.front();
		active.pop();
		region[p]=3;
		counter++;
		for (int i=0; i<4; i++)
		{
			Point q = p+shift[i]; //using overloaded operator + (see "Point.h")
			if ( region.pointIn(q) && region[q]==0) 
			{
				region[q]=2; // active front
				active.push(q);
			}
		}
		if (view && counter%60==0) {draw(); Pause(20);} // visualization, skipped if checkbox "view" is off
	}
	return region.getWidth() * region.getHeight() - counter;
}

