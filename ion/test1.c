#include <stdio.h>

// Forward declarations
#line 1"../test1.ion"
int example_test(void);
typedef union IntOrPtr IntOrPtr;
typedef struct Vector Vector;
#line 23"../test1.ion"
int fact_iter(int n);
#line 31"../test1.ion"
int fact_rec(int n);
typedef struct T T;
#line 47"../test1.ion"
int main(int argc, char (*(*argv)));

// Ordered declarations
#line 1"../test1.ion"
int example_test(void) {
    return (fact_rec(10)) == (fact_iter(10));
}
#line 31"../test1.ion"
int fact_rec(int n) {
    if ((n) == (0)) {
        return 1;
    } else {
        return (n) * (fact_rec((n) - (1)));
    }
}
#line 23"../test1.ion"
int fact_iter(int n) {
    int r = 1;
    for (int i = 2; (i) <= (n); i++) {
        r *= i;
    }
    return r;
}
#line 5"../test1.ion"
union IntOrPtr {
    int i;
    int (*p);
};
#line 17"../test1.ion"
int i;
#line 19"../test1.ion"
struct Vector {
    int x;
    int y;
};
#line 41"../test1.ion"
T (*p);
#line 39"../test1.ion"
enum { n = (1) + (sizeof(p)) };
#line 43"../test1.ion"
struct T {
    int (a[n]);
};
#line 47"../test1.ion"
int main(int argc, char (*(*argv))) {
    #line 49"../test1.ion"
    int b = example_test();
    puts("Hello, world!");
    return 0;
}