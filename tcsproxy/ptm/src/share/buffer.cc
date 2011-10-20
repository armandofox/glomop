#include "distinterface.h"

#ifdef __cplusplus
extern "C" {
#endif

void  
DistillerBufferClone(DistillerBuffer *dst, DistillerBuffer *src)
{
  assert(src != NULL);
  src->Clone(dst);
}

gm_Bool
DistillerBufferAlloc(DistillerBuffer *dBuf, size_t size)
{
  return dBuf->Alloc(size);
}

gm_Bool
DistillerBufferRealloc(DistillerBuffer *dBuf, size_t size)
{
  return dBuf->Realloc(size);
}

void
DistillerBufferFree(DistillerBuffer *dBuf)
{
  dBuf->Free();
}

void
DistillerBufferSetLength(DistillerBuffer *dBuf, UINT32 len)
{
  dBuf->SetLength(len);
}

void
DistillerBufferSetString(DistillerBuffer *dBuf, char *str, size_t len)
{
  dBuf->StringBuffer(str, len);
}

void
DistillerBufferSetStatic(DistillerBuffer *dBuf, void *buf_, size_t len)
{
  dBuf->StaticBuffer(buf_, len);
}

#ifdef __cplusplus
}
#endif

