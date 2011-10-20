/*
 *  ppmpad.c: take a small image, pad it into the top left corner of a
 *  bbox of a larger size
 */

#include "ppm.h"
#include "pnm.h"
#include <string.h>

pixel **ppmpad_main(int smallx, int smally,
                    int bigx, int bigy, pixel bgcolor, pixel **array)
{
    int i;
    pixel **origarray = array;
    pixel **newarray = ppm_allocarray(bigx,bigy);
    int red = (int)PPM_GETR(bgcolor);
    int optimize = (red == PPM_GETG(bgcolor)
                    && red == PPM_GETB(bgcolor));

    for (i=0; i<smally; i++) {
        /* copy first part of row, from orig image */
        memcpy(newarray[i], *origarray++,
               (size_t)(smallx * sizeof(pixel)));
        /* blast remainder of row.  if r==g==b, we can optimize and use */
        /* memset, otherwise we have to use  */
        if (optimize)
          memset(newarray[i] + smallx, red,
                 (size_t)((bigx-smallx) * sizeof(pixel)));
        else
          perror("GACK!") ; /* nothing here yet */
    }

    if (optimize)
      for (i=smally; i<bigy; i++) 
        memset(newarray[i], red, (size_t)(bigx * sizeof(pixel)));
    else
      perror("GACK!");                         /* nothing here yet */

    return newarray;
}
    /* for remainder of array, just blast empty rows. */
    
        
