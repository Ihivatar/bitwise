// Preamble
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

typedef unsigned char uchar;
typedef signed char schar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long llong;
typedef unsigned long long ullong;

typedef uchar uint8;
typedef schar int8;
typedef ushort uint16;
typedef short int16;
typedef uint uint32;
typedef int int32;
typedef ullong uint64;
typedef llong int64;

// Forward declarations
typedef struct vec2 vec2;

// Sorted declarations
#line 10 "../test2.ion"
struct vec2 {
    float x;
    #line 11
    float y;
};

vec2 add2(vec2 a, vec2 b);

#line 18
vec2 sub2(vec2 a, vec2 b);

#line 22
vec2 neg2(vec2 a);

#line 26
vec2 mul2(float a, vec2 b);

#line 30
vec2 addmul2(vec2 a, float b, vec2 c);

#line 34
float dot2(vec2 a, vec2 b);

#line 38
float len2(vec2 a);

#line 42
vec2 unit2(vec2 a);

#line 46
vec2 perp2(vec2 a);

#line 50
vec2 dir2(vec2 a, vec2 b);

#line 54
vec2 rot2(float a, vec2 b);

#line 60
int main(int argc, char (*(*argv)));

// Function definitions
#line 14
vec2 add2(vec2 a, vec2 b) {
    return (vec2){(a.x) + (b.x), (a.y) + (b.y)};
}

vec2 sub2(vec2 a, vec2 b) {
    return (vec2){(a.x) - (b.x), (a.y) - (b.y)};
}

vec2 neg2(vec2 a) {
    return (vec2){-(a.x), -(a.y)};
}

vec2 mul2(float a, vec2 b) {
    return (vec2){(a) * (b.x), (a) * (b.y)};
}

vec2 addmul2(vec2 a, float b, vec2 c) {
    return (add2)(a, (mul2)(b, c));
}

float dot2(vec2 a, vec2 b) {
    return ((a.x) * (b.x)) + ((a.y) * (b.y));
}

float len2(vec2 a) {
    return (sqrtf)((dot2)(a, a));
}

vec2 unit2(vec2 a) {
    return (mul2)((1) / ((len2)(a)), a);
}

vec2 perp2(vec2 a) {
    return (vec2){-(a.y), a.x};
}

vec2 dir2(vec2 a, vec2 b) {
    return (unit2)((sub2)(b, a));
}

vec2 rot2(float a, vec2 b) {
    float c = (cosf)(a);
    float s = (sinf)(a);
    return (vec2){((c) * (b.x)) - ((s) * (b.y)), ((s) * (b.x)) + ((c) * (b.y))};
}

int main(int argc, char (*(*argv))) {
    vec2 v = {1, 0};
    vec2 w = {0, 1};
    v = (rot2)((3.140000f) / (4), v);
    w = (add2)(w, (vec2){0.100000f, 0.200000f});
    return 0;
}
