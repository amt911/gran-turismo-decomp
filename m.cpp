#include "h.h"
int f(int a, int b) { int r = 0; CHK(a); CHK(b); return r; }
int main(){ return f(5,1) - 1; }
