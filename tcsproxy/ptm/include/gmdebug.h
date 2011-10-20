#ifndef __GMDEBUG_H__
#define __GMDEBUG_H__


#define dbgError 'e'
#define dbgTmp   't'
#include <fstream.h>


class Debug_ {
public:
  Debug_() : codes(0), prefix(""), foutPtr(NULL) { };
  ~Debug_() { if (codes!=0) delete [] codes; };
  void Initialize(char *codes_, char *prefix_, char *fileName=NULL) {
    if (codes!=0) delete [] codes;
    codes = new char[strlen(codes_)+1];
    strcpy(codes, codes_);
    prefix = prefix_; 
    if (fileName!=NULL) {
      fout.open(fileName, ios::out | ios::app);
      if (fout.good()) foutPtr = &fout;
    }
    if (foutPtr==NULL) foutPtr = &cerr;
  };
  gm_Bool ShouldPrint(char code) {
    if (codes==0) return gm_False;
    return (strchr(codes, code)!=NULL) ? gm_True : gm_False; 
  }
  char *getPrefix() { return prefix; };
  ostream &getStream() { return *foutPtr; }
  static Debug_* getInstance() { return &instance_; };
protected:
  char *codes;
  char *prefix;
  ofstream fout;
  ostream *foutPtr;
  static Debug_ instance_;
};


#ifdef gm_No_Debug
#define gm_Debug(code, x)
#else
#define gm_Debug(code, x) \
  if (Debug_::getInstance()->ShouldPrint(code)==gm_True) \
    Debug_::getInstance()->getStream()<<Debug_::getInstance()->getPrefix()<< x;
#endif



#endif // __GMDEBUG_H__
