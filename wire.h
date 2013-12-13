#pragma once

// THIS FILE CONTAINS DECLARATIONS OF SOME GLOBAL VARIABLES AND FUNCTIONS DEFINED EITHER IN "wire.cpp" OR IN "main.cpp"
// THIS FILE MUST BE INCLUDED IN "main.cpp" and "wire.cpp" AS BOTH USE THESE GLOBAL VARIABLES AND FUNCTIONS 
using namespace std;

extern Table2D<RGB> image;  // loaded by GUI in main.cpp
extern bool closedContour;    // a flag indicating if contour was closed - set in wire.cpp
extern vector<Point> contour;   // a list of 'contour' points - set in wire.cpp
extern Table2D<int> region;   // a binary 2D mask of 'region' points - set in wire.cpp
const double INFTY=1.e20;

void addToContour(Point click);
void addToContourLast(Point click);
void regionGrow(Point seed, double T);
void reset_segm();
void computeEdgePaths();
void computeEdgeWeight(Point p, Point q, int k);
stack<Point>* liveWire(Point p);
void computePaths(Point seed);
int contourInterior();

extern bool view; // defined in main.cpp (boolean flag set by a check box)
void draw(Point mouse = Point(-1,-1)); // defined in main.cpp, but it is also called in wire.cpp for visualisation 


////////////////////////////////////////////////////////////
// MyPoint objects extend Points with extra variable "m_priority". 
// MyPoint have an overloaded comparison operators based on their priority.
class MyPoint : public Point {
	bool done;
	double m_priority;
	double topLeftEdge;
	double topRightEdge;
	double topEdge;
	double leftEdge;
	double rightEdge;
	double bottomLeftEdge;
	double bottomRightEdge;
	double bottomEdge;
public:
	MyPoint(){
		topLeftEdge = INFTY;
		topRightEdge = INFTY;
		topEdge = INFTY;
		leftEdge = INFTY;
		rightEdge = INFTY;
		bottomLeftEdge = INFTY;
		bottomRightEdge = INFTY;
		bottomEdge = INFTY;
	};//default constructor
	MyPoint(Point p, double priority) : Point(p), m_priority(priority) {
		topLeftEdge = INFTY;
		topRightEdge = INFTY;
		topEdge = INFTY;
		leftEdge = INFTY;
		rightEdge = INFTY;
		bottomLeftEdge = INFTY;
		bottomRightEdge = INFTY;
		bottomEdge = INFTY;
	}
	double getPriority() const {return m_priority;}	
	void setPriority(double priority) {m_priority = priority;}
    bool operator<(const MyPoint& b) const   {return getPriority() > b.getPriority();}
	void setTopLeftEdge(double value) {topLeftEdge = value;}
	double getTopLeftEdge() {return topLeftEdge;}
	void setTopRightEdge(double value) {topRightEdge = value;}
	double getTopRightEdge() {return topRightEdge;}
	void setTopEdge(double value) {topEdge = value;}
	double getTopEdge() {return topEdge;}
	void setLeftEdge(double value) {leftEdge = value;}
	double getLeftEdge() {return leftEdge;}
	void setRightEdge(double value) {rightEdge = value;}
	double getRightEdge() {return rightEdge;}
	void setBottomLeftEdge(double value) {bottomLeftEdge = value;}
	double getBottomLeftEdge() {return bottomLeftEdge;}
	void setBottomRightEdge(double value) {bottomRightEdge = value;}
	double getBottomRightEdge() {return bottomRightEdge;}
	void setBottomEdge(double value) {bottomEdge = value;}
	double getBottomEdge() {return bottomEdge;}
};
