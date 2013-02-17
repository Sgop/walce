#ifndef DEBUG_H__
#define DEBUG_H__

#define SANITY_CHECKS 0


#if SANITY_CHECKS
#  define ASSERT_IF_FAIL(cond, message) \
     if (!(cond)) { printf("*** ASSERT *** %s(%d): %s\n", __FUNCTION__, __LINE__, message); }
#else
#  define ASSERT_IF_FAIL(cond, message)
#endif



#endif
