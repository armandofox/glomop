#ifndef __GIF2GREY_H__
#define __GIF2GREY_H__

DistillerStatus
gif2grey_init(C_DistillerType distType, 
		       int argc, const char * const *argv);

void
gif2grey_clean(void);

DistillerStatus
gif2grey_main(unsigned char *ingif, unsigned int inlen,
		unsigned char **outgrey, int *owidth, int *oheight);

#endif
