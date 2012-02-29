/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2012 Joseph Gaeddert
 * Copyright (c) 2007, 2008, 2009, 2010, 2012 Virginia Polytechnic
 *                                      Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// Test Rader's algorithm for FFT of prime number
//
// References:
//  [Rader:1968] Charles M. Rader, "Discrete Fourier Transforms When
//      the Number of Data Samples Is Prime," Proceedings of the IEEE,
//      vol. 56, number 6, pp. 1107--1108, June 1968
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <getopt.h>
#include <complex.h>

#define DEBUG 0
#define DFT_FORWARD (-1)
#define DFT_REVERSE ( 1)
#define MAX_FACTORS (40)

// print usage/help message
void usage()
{
    printf("fft_rader_prime_test -- test Rader's prime FFT algorithm, compare to slow DFT method\n");
    printf("options (default values in []):\n");
    printf("  h     : print usage/help\n");
    printf("  n     : fft size (must be prime and greater than 2)\n");
}

// super slow DFT, but functionally correct
void dft_run(unsigned int    _nfft,
             float complex * _x,
             float complex * _y,
             int             _dir,
             int             _flags);

// determine if number is prime (slow, simple method)
int is_prime(unsigned int _n);

// find smallest primitive root of _n (assuming _n is prime)
unsigned int primitive_root(unsigned int _n);

// compute c = base^exp (mod n)
unsigned int modpow(unsigned int _base,
                    unsigned int _exp,
                    unsigned int _n);

int main(int argc, char*argv[]) {
    // transform size (must be prime)
    unsigned int nfft = 17;

    int dopt;
    while ((dopt = getopt(argc,argv,"uhn:")) != EOF) {
        switch (dopt) {
        case 'h':   usage();                return 0;
        case 'n':   nfft = atoi(optarg);    break;
        default:
            exit(1);
        }
    }

    // validate input
    if ( nfft <= 2 || !is_prime(nfft)) {
        fprintf(stderr,"error: %s, input transform size must be prime and greater than two\n", argv[0]);
        exit(1);
    }

#if 1
    // compute primitive root of nfft
    unsigned int g = primitive_root(nfft);
    printf("computed primitive root of %u as %u\n", nfft, g);
#else
    {
    unsigned int g = 3;
    unsigned int i;
    printf("s = [");
    for (i=1; i<nfft; i++)
        printf("%4u", modpow(g,i,nfft));
    printf("]\n");
    }
#endif
    //return 0;

    unsigned int i;

    // create and initialize data arrays
    float complex * x      = (float complex *) malloc(nfft * sizeof(float complex));
    float complex * y      = (float complex *) malloc(nfft * sizeof(float complex));
    float complex * y_test = (float complex *) malloc(nfft * sizeof(float complex));
    if (x == NULL || y == NULL || y_test == NULL) {
        fprintf(stderr,"error: %s, not enough memory for allocation\n", argv[0]);
        exit(1);
    }
    for (i=0; i<nfft; i++) {
        //x[i] = randnf() + _Complex_I*randnf();
        x[i] = (float)i + _Complex_I*(3 - (float)i);
        y[i] = 0.0f;
    }

    // compute output for testing
    dft_run(nfft, x, y_test, DFT_FORWARD, 0);

    // 
    // run Rader's algorithm
    //

    // 
    // print results
    //
    for (i=0; i<nfft; i++) {
        printf("  y[%3u] = %12.6f + j*%12.6f (expected %12.6f + j%12.6f)\n",
            i,
            crealf(y[i]),      cimagf(y[i]),
            crealf(y_test[i]), cimagf(y_test[i]));
    }

    // compute error
    float rmse = 0.0f;
    for (i=0; i<nfft; i++) {
        float e = y[i] - y_test[i];
        rmse += e*conjf(e);
    }
    rmse = sqrtf(rmse / (float)nfft);
    printf("RMS error : %12.4e (%s)\n", rmse, rmse < 1e-3 ? "pass" : "FAIL");

    // free allocated memory
    free(x);
    free(y);
    free(y_test);

    return 0;
}

// super slow DFT, but functionally correct
void dft_run(unsigned int    _nfft,
             float complex * _x,
             float complex * _y,
             int             _dir,
             int             _flags)
{
    unsigned int i;
    unsigned int k;

    int d = (_dir == DFT_FORWARD) ? -1 : 1;

    for (i=0; i<_nfft; i++) {
        _y[i] = 0.0f;
        for (k=0; k<_nfft; k++) {
            float phi = 2*M_PI*d*i*k / (float)_nfft;
            _y[i] += _x[k] * cexpf(_Complex_I*phi);
        }
    }
}

// determine if number is prime (slow, simple method)
int is_prime(unsigned int _n)
{
    if (_n < 4) return 1;

    unsigned int i;
    for (i=2; i<_n; i++) {
        if ( (_n % i) == 0)
            return 0;
    }

    return 1;
}

// find smallest primitive root of _n (assuming _n is prime)
unsigned int primitive_root(unsigned int _n)
{
    // find unique factors of _n-1
    unsigned int unique_factors[MAX_FACTORS];
    unsigned int num_unique_factors = 0;
    unsigned int n = _n-1;
    unsigned int k;
    do {
        for (k=2; k<=n; k++) {
            if ( (n%k)==0 ) {
                // k is a factor of (_n-1)
                n /= k;

                // add element to end of table
                unique_factors[num_unique_factors] = k;

                // increment counter only if element is unique
                if (num_unique_factors == 0)
                    num_unique_factors++;
                else if (unique_factors[num_unique_factors-1] != k)
                    num_unique_factors++;
                break;
            }
        }
    } while (n > 1 && num_unique_factors < MAX_FACTORS);

#if 0
    // print unique factors
    printf("found %u unique factors of n-1 = %u\n", num_unique_factors, _n-1);
    for (k=0; k<num_unique_factors; k++)
        printf("  %3u\n", unique_factors[k]);
#endif

    // search for minimum integer for which
    //   g^( (_n-1)/m ) != 1 (mod _n)
    // for all unique roots 'm'
    unsigned int g;
    int root_found = 0;
    for (g=2; g<_n; g++) {
        int is_root = 1;
        for (k=0; k<num_unique_factors; k++) {
            unsigned int e = (_n-1) / unique_factors[k];
            if ( modpow(g,e,_n) == 1 ) {
                // not a root
                is_root = 0;
                break;
            }
        }

        if (is_root) {
            //printf("  %u is a primitive root!\n", g);
            root_found = 1;
            break;
        }
    }

    return g;
}

// compute c = base^exp (mod n)
unsigned int modpow(unsigned int _base,
                    unsigned int _exp,
                    unsigned int _n)
{
    unsigned int c = 1;
    unsigned int i;
    for (i=0; i<_exp; i++)
        c = (c * _base) % _n;

    return c;
}

