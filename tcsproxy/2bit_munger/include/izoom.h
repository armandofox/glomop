#ifndef IZOOMDEF
#define IZOOMDEF

#define IMPULSE		1
#define BOX		2
#define TRIANGLE	3
#define QUADRATIC	4
#define MITCHELL	5
#define GAUSSIAN	6
#define LANCZOS2	7
#define LANCZOS3	8

typedef struct FILTER {
    int n, totw, halftotw;
    short *dat;
    short *w;
} FILTER;

typedef struct zoom {
    int (*getfunc)();
    short *abuf;
    short *bbuf;
    int anx, any;
    int bnx, bny;
    short **xmap;
    int type;
    int curay;
    int y;
    FILTER *xfilt, *yfilt;	/* stuff for fitered zoom */
    short *tbuf;
    int nrows, clamp, ay;
    short **filtrows;
    int *accrow;
} zoom;

zoom *newzoom();
void getzoomrow(zoom*, short*, int);
void freezoom(zoom*);

#endif
