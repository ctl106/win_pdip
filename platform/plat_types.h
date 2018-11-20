// define format specifier for size_t
#if defined _WIN32  // for those who are unaware, this encompasses 32 AND 64
#define PRISIZE  "Iu"
#else
#define PRISIZE  "zu"
#endif // _WIN32

// define format specifier for pid_t to reconcile different sizes
#ifdef _WIN64   // 64 bit Windows is a special snowflake
#define PRIPID  "I64d"
#else
#define PRIPID  "d"
#endif // _WIN64
