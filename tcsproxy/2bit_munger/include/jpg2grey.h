#ifndef __JPG2GREY_H__
#define __JPG2GREY_H__

DistillerStatus
jpg2grey_init(C_DistillerType distType, 
		       int argc, const char * const *argv);

void
jpg2grey_clean(void);

DistillerStatus
jpg2grey_main(unsigned char *injpg, unsigned int inlen,
		unsigned char **outgrey, int *owidth, int *oheight);

#endif
