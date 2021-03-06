#include "fixed_point.h"

#define F 16384 /* 2**14 */


int
fixed_point_fp_to_int_nearest (int17_14t x)
{
	if (x >= 0)
		return (x + F/2) / F ;
	else
		return (x - F/2) / F ;

}

int
fixed_point_fp_to_int_zero (int17_14t x)
{
	return x / F;
}

int17_14t
fixed_point_int_to_fp (int n)
{
	return n*F;
}

int17_14t
fixed_point_add_fp_fp (int17_14t x, int17_14t y)
{
	return x + y;
}

int17_14t
fixed_point_add_fp_int (int17_14t x, int n)
{
	return x + n * F;
}


int17_14t
fixed_point_multiply_fp_fp (int17_14t x, int17_14t y)
{
	return ((int64_t) x) * y / F;
}

int17_14t
fixed_point_multiply_fp_int (int17_14t x, int n)
{
	return x * n;
}

int17_14t
fixed_point_divide_fp_fp (int17_14t x, int17_14t y)
{
	return ((int64_t) x) * F / y;
}

int17_14t
fixed_point_divide_fp_int (int17_14t x, int n)
{
	return x / n;
}
