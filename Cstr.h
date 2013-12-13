#pragma once
#include <sstream>
#include <memory>

////////////////////////////////////////////////////////////////////////////////////////
// This file defines two very similar macros    to_Cstr(...)    and    to_Cstr_s(...) 
// that might simplify generation of simple C-style text strings (see overview below).
// Note that the first macro has smaller runtime overhead (also no dependency on #include <memory>)
// but it is a non standard extension to C++. The second macro uses 's'tandard C++ and it may be 's'safer.
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
// The macro to_Cstr(...) formats its input and
// returns a C-style string (const char*) that can
// be passed to another function. The input should
// be a series of << concatenations, like for "cout".
//
// For example:
//     int err = GetErrorCode();
//     if (err)
//        PrintError( to_Cstr( "Error #" << err << " occurred" ) );
// Or:
//     double sec = timer.seconds();
//     string msg( to_Cstr( sec << " seconds have passed" ) );
//
//  The following is NOT okay! The C-style string was freed immediately
//  after 'msg' was given a pointer to it.
//     const char* msg = to_Cstr( seconds << " seconds have passed" );
//     cout << msg;
// 
#define to_Sstr_s(stuff) (((std::stringstream&)((*std::auto_ptr<std::stringstream>(new std::stringstream).get())<<stuff)).str().c_str()   )
 
#pragma warning (disable : 4239)
#define to_Cstr(stuff) ((std::ostringstream&)(((std::basic_ostringstream<char>&)std::ostringstream()) << stuff)).str().c_str()



