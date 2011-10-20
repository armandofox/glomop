#ifndef PROPDITDEF
#define PROPDITDEF

typedef struct propdit {
    int (*ditfunc)();
    int xsize, cury;
    int *noise;
    int *err0;
    int *err1;
} propdit;

propdit *newpropdit(int xsize);
void propditrow(propdit *pd, short *buf, short *obuf);
void freepropdit(propdit *pd);

#endif
