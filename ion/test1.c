// Forward declarations
int example_test(void);
void f(void);
typedef union IntOrPtr IntOrPtr;
int fact_iter(int n);
int fact_rec(int n);
typedef struct T T;
typedef struct Vector Vector;

// Ordered declarations
int example_test(void) {
    return (fact_rec(10)) == (fact_iter(10));
}
int fact_rec(int n) {
    if ((n) == (0)) {
        return 1;
    } else {
        return (n) * (fact_rec((n) - (1)));
    }
}
int fact_iter(int n) {
    int r = 1;
    for (int i = 2; (i) <= (n); i++) {
        r *= i;
    }
    return r;
}
void f(void) {
    IntOrPtr u1 = (IntOrPtr){.i = 42};
    IntOrPtr u2 = (IntOrPtr){.p = (int *)(42)};
    u1.i = 0;
    u2.p = (int *)(0);
}
union IntOrPtr {
    int i;
    int (*p);
};
T (*p);
enum { n = (1) + (sizeof(p)) };
struct T {
    int (a[n]);
};
struct Vector {
    int x;
    int y;
};
int i;