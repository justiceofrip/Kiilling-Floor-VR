#pragma once
#include <cmath>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(false)
inline bool Near(float a,float b,float epsilon=.0001f){return std::abs(a-b)<epsilon;}