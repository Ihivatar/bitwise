// Preamble
#include <stdio.h>

typedef unsigned char uchar;
typedef signed char schar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long llong;
typedef unsigned long long ullong;

// Forward declarations
typedef union IntOrPtr IntOrPtr;
typedef struct Vector Vector;
typedef struct T T;

// Sorted declarations
#line 2 "../test1.ion"

#line 5

#line 8

#line 10
char c = 1;

#line 11
uchar uc = 1;

#line 12
schar sc = 1;

enum { N = (((char)(42)) + (8)) != (0) };

#line 18
uchar h(void);

#line 16
typedef int (A[(1) + ((2) * (sizeof(h())))]);

#line 84
struct Vector {
    int x;
    #line 85
    int y;
};

#line 43
typedef IntOrPtr U;

#line 27
int g(U u);

#line 49
union IntOrPtr {
    int i;
    int (*p);
};

#line 31
void k(void (*vp), int (*ip));

#line 37
void f1(void);

#line 45
int example_test(void);

#line 101
int fact_rec(int n);

#line 93
int fact_iter(int n);

#line 54
int (escape_to_char[256]) = {[110] = 10, [114] = 13, [116] = 9, [118] = 11, [98] = 8, [97] = 7, [48] = 0};

#line 64
int (array[11]) = {1, 2, 3, [10] = 4};

int is_even(int digit);

#line 82
int i;

#line 88
void f2(Vector v);

#line 111
T (*p);

#line 109
enum { n = (1) + (sizeof(p)) };

#line 113
struct T {
    int (a[n]);
};

void benchmark(int n);

#line 124
int va_test(int x, ...);

#line 129
typedef int (*F)(int, ...);

void test_ops(void);

#line 162
int main(int argc, char (*(*argv)));

// Function definitions
#line 18
uchar h(void) {
    #line 20
    (Vector){.x = 1, .y = 2}.x = 42;
    int (*p) = &((int){0});
    ulong x = ((uint){1}) + ((long){2});
    int y = +(c);
    return x;
}

int g(U u) {
    return u.i;
}

void k(void (*vp), int (*ip)) {
    #line 33
    vp = ip;
    ip = vp;
}

void f1(void) {
    #line 39
    int (*p) = &((int){0});
    *(p) = 42;
}

#line 45
int example_test(void) {
    return (fact_rec(10)) == (fact_iter(10));
}

#line 66
int is_even(int digit) {
    int b = 0;
    switch (digit) {
    case 0:
    case 2:
    case 4:
    case 6:
    case 8: {
        #line 70
        b = 1;
        break;
    }
    }
    #line 72
    return b;
}

#line 88
void f2(Vector v) {
    #line 90
    v = (Vector){0};
}

int fact_iter(int n) {
    int r = 1;
    for (int i = 0; (i) <= (n); i++) {
        r *= i;
    }
    return r;
}

int fact_rec(int n) {
    if ((n) == (0)) {
        return 1;
    } else {
        return (n) * (fact_rec((n) - (1)));
    }
}

#line 117
void benchmark(int n) {
    int r = 1;
    for (int i = 1; (i) <= (n); i++) {
        r *= i;
    }
}

int va_test(int x, ...) {
    #line 126
    return 0;
}

#line 131
void test_ops(void) {
    #line 133
    float pi = 3.140000;
    float f = 0.000000;
    f = +(pi);
    f = -(pi);
    int n = -(1);
    n = ~(n);
    f = (f) * (pi);
    f = (pi) / (pi);
    n = (3) % (2);
    n = (n) + ((uchar)(1));
    int (*p) = &(n);
    p = (p) + (1);
    n = ((p) + (1)) - (p);
    n = (n) << (1);
    n = (n) >> (1);
    int b = ((p) + (1)) > (p);
    b = ((p) + (1)) >= (p);
    b = ((p) + (1)) < (p);
    b = ((p) + (1)) <= (p);
    b = ((p) + (1)) == (p);
    b = (1) > (2);
    b = (1.230000) <= (pi);
    n = 255;
    b = (n) & (~(1));
    b = (n) & (1);
    b = ((n) & (~(1))) ^ (1);
    b = (p) && (pi);
}

int main(int argc, char (*(*argv))) {
    #line 164
    test_ops();
    int b = example_test();
    puts("Hello, world!");
    int c = getchar();
    printf("You wrote \'%c\'\n", c);
    va_test(1);
    va_test(1, 2);
    return 0;
}
