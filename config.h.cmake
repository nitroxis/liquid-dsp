#ifndef __LIQUID_CONFIG_H__
#define __LIQUID_CONFIG_H__

/* Support AVX (Advanced Vector Extensions) instructions */
#cmakedefine01 HAVE_AVX

/* Define to 1 if you have the <complex.h> header file. */
#cmakedefine01 HAVE_COMPLEX_H

/* Define to 1 if you have the <emmintrin.h> header file. */
#cmakedefine01 HAVE_EMMINTRIN_H

/* Define to 1 if you have the <fec.h> header file. */
#cmakedefine HAVE_FEC_H

/* Define to 1 if you have the <fftw3.h> header file. */
#cmakedefine01 HAVE_FFTW3_H

/* Define to 1 if you have the <float.h> header file. */
#cmakedefine01 HAVE_FLOAT_H

/* Define to 1 if you have the <getopt.h> header file. */
#cmakedefine01 HAVE_GETOPT_H

/* Define to 1 if you have the <immintrin.h> header file. */
#cmakedefine01 HAVE_IMMINTRIN_H

/* Define to 1 if you have the <inttypes.h> header file. */
#cmakedefine01 HAVE_INTTYPES_H

/* Define to 1 if you have the `fec' library (-lfec). */
#cmakedefine HAVE_LIBFEC

/* Define to 1 if you have the <memory.h> header file. */
#cmakedefine01 HAVE_MEMORY_H

/* Define to 1 if you have the <mmintrin.h> header file. */
#cmakedefine01 HAVE_MMINTRIN_H

/* Support MMX instructions */
#cmakedefine01 HAVE_MMX

/* Define to 1 if you have the <pmmintrin.h> header file. */
#cmakedefine01 HAVE_PMMINTRIN_H

/* Define to 1 if you have the <smmintrin.h> header file. */
#cmakedefine01 HAVE_SMMINTRIN_H

/* Support SSE (Streaming SIMD Extensions) instructions */
#cmakedefine01 HAVE_SSE

/* Support SSE2 (Streaming SIMD Extensions 2) instructions */
#cmakedefine01 HAVE_SSE2

/* Support SSE3 (Streaming SIMD Extensions 3) instructions */
#cmakedefine01 HAVE_SSE3

/* Support SSE4.1 (Streaming SIMD Extensions 4.1) instructions */
#cmakedefine01 HAVE_SSE41

/* Support SSE4.2 (Streaming SIMD Extensions 4.2) instructions */
#cmakedefine01 HAVE_SSE42

/* Support SSSE3 (Supplemental Streaming SIMD Extensions 3) instructions */
#cmakedefine01 HAVE_SSSE3

/* Define to 1 if you have the <stdint.h> header file. */
#cmakedefine01 HAVE_STDINT_H

/* Define to 1 if you have the <sys/resource.h> header file. */
#cmakedefine01 HAVE_SYS_RESOURCE_H

/* Define to 1 if you have the <tmmintrin.h> header file. */
#cmakedefine01 HAVE_TMMINTRIN_H

/* Define to 1 if you have the <unistd.h> header file. */
#cmakedefine01 HAVE_UNISTD_H

/* Define to 1 if you have the <xmmintrin.h> header file. */
#cmakedefine01 HAVE_XMMINTRIN_H

/* Force internal FFT even if libfftw is available */
#cmakedefine01 LIQUID_FFTOVERRIDE

/* Force overriding of SIMD (use portable C code) */
#cmakedefine01 LIQUID_SIMDOVERRIDE

/* The size of `int', as computed by sizeof. */
#cmakedefine SIZEOF_INT @SIZEOF_INT@

/* The size of `unsigned int', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_INT @SIZEOF_UNSIGNED_INT@

#endif // __LIQUID_CONFIG_H__
