#ifndef HIPASSDEF
#define HIPASSDEF

typedef struct hipass {
    int active, y;
    int xsize, ysize;
    int extrapval;
    short *blurrows[3];
    short *pastrows[2];
    short *acc;
    int (*getfunc)();
} hipass;

hipass *newhipass();
void freehipass(hipass*);
void gethipassrow(hipass*, short*, int);

#endif
