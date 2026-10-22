// tools/check_crash_report.sh: twice() is defined here and in
// crash_report_inline_b.cpp (an inline function: a weak definition in each
// object); the program uses this one, and b's own calls reach its copy.
inline int twice(int *p) { return *p * 2; }
int fromA(int *p) { return twice(p) + 1; }
