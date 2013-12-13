/*******************************************************************************
  cs1037lib-time.h -- Time functions

  DESCRIPTION:
     Simple functions for querying/managing time.


*******************************************************************************/

#ifndef __CS1037LIB_TIME_H__
#define __CS1037LIB_TIME_H__

// GetMilliseconds:
//    Returns the number of milliseconds since the program started running.
//    Useful for benchmarking code, and for getting animations/simulations 
//    to run at a precise speed.
//
double GetMilliseconds();

// Pause:
//    Causes the program to wait ('freeze') a certain amount of time. 
//    Useful for making animations slow down.
//
void Pause(int milliseconds);

#endif // __CS1037LIB_TIME_H__
