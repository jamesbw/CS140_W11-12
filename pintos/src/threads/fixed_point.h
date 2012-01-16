#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* Fixed point integer representation for real numbers */
typedef int int17_14t;

int fixed_point_fp_to_int_nearest (int17_14t x);
int fixed_point_fp_to_int_zero (int17_14t x);
int17_14t fixed_point_int_to_fp (int n);
int17_14t fixed_point_add_fp_fp (int17_14t x, int17_14t y);
int17_14t fixed_point_add_fp_int (int17_14t x, int n);
int17_14t fixed_point_multiply_fp_fp (int17_14t x, int17_14t y);
int17_14t fixed_point_multiply_fp_int (int17_14t x, int n);
int17_14t fixed_point_divide_fp_fp (int17_14t x, int17_14t y);
int17_14t fixed_point_divide_fp_int (int17_14t x, int n);


#endif /* threads/fixed_point.h */
