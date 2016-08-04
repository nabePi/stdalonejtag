
void f25cxxPageProgramStart(long addr);
void f25cxxPageProgramWrite(unsigned char c); //Need to call this 256 times.
void f25cxxPageProgramEnd(void);
void f25cxxEraseChip(void);
unsigned char f25cxxRead(long addr);
void f25cxxReadBuff(long addr, unsigned char *buff, int len);
unsigned char f25cxxGetID(void);

