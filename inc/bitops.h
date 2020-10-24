
#ifndef BITOPS_H 
#define BITOPS_H

#define BitSet(arg,posn)  arg = ((arg)|(1L<<(posn)))
#define BitClr(arg,posn)  arg = ((arg)&^(1L<<(posn)))
#define BitFlp(arg,posn)  arg = ((arg)^(1L<<(posn)))
#define BitStatus(arg,posn) ((arg)&(1L<<(posn)))

enum { bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7 };

#endif // !BITOPS_H 