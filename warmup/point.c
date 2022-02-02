#include <assert.h>
#include "common.h"
#include "point.h"
#include <math.h>

void point_translate(struct point *p, double x, double y) {
	p -> x += x;
	p -> y += y;
}

double point_distance(const struct point *p1, const struct point *p2) {
	return sqrt(pow(p2 -> x - p1 -> x, 2) + pow(p2 -> y - p1 -> y, 2));
}

int point_compare(const struct point *p1, const struct point *p2) {
	double l1 = pow(p1 -> x, 2) + pow(p1 -> y, 2);
	double l2 = pow(p2 -> x, 2) + pow(p2 -> y, 2);
	if (l1 < l2) return -1;
	else if (l1 == l2) return 0;
	else return 1;
}
