// Preamble
#include <stdio.h>

// Forward declarations
typedef union IntOrPtr IntOrPtr;
typedef struct Vector Vector;
typedef struct T T;

// Sorted declarations
#line 1 "../test1.ion"
enum { N = ((char)(42)) + (8) };

#line 5
int h(void);

#line 3
typedef int (A[(1) + ((2) * (sizeof(h())))]);

#line 13
typedef IntOrPtr U;

#line 9
int g(U u);

#line 19
union IntOrPtr {
    int i;
    int (*p);
};

#line 15
int example_test(void);

#line 66
int fact_rec(int n);

#line 58
int fact_iter(int n);

#line 24
int (escape_to_char[256]) = {[110] = 10, [114] = 13, [116] = 9, [118] = 11, [98] = 8, [97] = 7, [48] = 0};

#line 34
int (array[11]) = {1, 2, 3, [10] = 4};

int is_even(int digit);

#line 52
int i;

struct Vector {
    int x;
    #line 55
    int y;
};

#line 76
T (*p);

#line 74
enum { n = (1) + (sizeof(p)) };

#line 78
struct T {
    int (a[n]);
};

void benchmark(int n);

#line 89
int main(int argc, char (*(*argv)));

// Function definitions
#line 5
int h(void) {
    return 42;
}

int g(U u) {
    return u.i;
}

#line 15
int example_test(void) {
    return (fact_rec(10)) == (fact_iter(10));
}

#line 36
int is_even(int digit) {
    int b = 0;
    switch (digit) {
    case 0:
    case 2:
    case 4:
    case 6:
    case 8: {
        #line 40
        b = 1;
        break;
    }
    }
    #line 42
    return b;
}

#line 58
int fact_iter(int n) {
    int r = 1;
    for (int i = 2; (i) <= (n); i++) {
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

#line 82
void benchmark(int n) {
    int r = 1;
    for (int i = 1; (i) < (n); i++) {
        r *= i;
    }
}

int main(int argc, char (*(*argv))) {
    int b = example_test();
    puts("Hello, world!");
    int c = getchar();
    return 0;
}
