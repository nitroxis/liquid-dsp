/*
 * Copyright (c) 2007 - 2022 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Frame detector and synchronizer; uses a novel correlation method to
// detect a synchronization pattern, estimate carrier frequency and
// phase offsets as well as timing phase, then correct for these
// impairments in a simple interface suitable for custom frame recovery.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "liquid.internal.h"

// push samples through detection stage
int qdsync_cccf_execute_detect(qdsync_cccf   _q,
                               float complex _x);

// step receiver mixer, matched filter, decimator
//  _q      :   frame synchronizer
//  _x      :   input sample
int qdsync_cccf_step(qdsync_cccf   _q,
                     float complex _x);

// append sample to output buffer
int qdsync_cccf_buf_append(qdsync_cccf   _q,
                           float complex _x);

// main object definition
struct qdsync_cccf_s {
    unsigned int    seq_len;    // preamble sequence length
    int             ftype;      // filter type
    unsigned int    k;          // samples per symbol
    unsigned int    m;          // filter semi-length
    float           beta;       // excess bandwidth factor

    qdsync_callback callback;   // user-defined callback function
    void *          context;    // user-defined context object
    qdetector_cccf  detector;   // detector

    // status variables
    enum {
        QDSYNC_STATE_DETECT=0,  // detect frame
        QDSYNC_STATE_SYNC,      // apply carrier offset correction and matched filter
    }               state;      // frame synchronization state
    unsigned int symbol_counter;// counter: total number of symbols received including preamble sequence

    nco_crcf        mixer;      // coarse carrier frequency recovery

    // timing recovery objects, states
    firpfb_crcf     mf;         // matched filter/decimator
    unsigned int    npfb;       // number of filters in symsync
    int             mf_counter; // matched filter output timer
    unsigned int    pfb_index;  // filterbank index

    // symbol buffer
    unsigned int    buf_out_len;// output buffer length
    float complex * buf_out;    // output buffer
    unsigned int    buf_out_counter; // output counter
};

// create detector with generic sequence
qdsync_cccf qdsync_cccf_create_linear(liquid_float_complex * _seq,
                                      unsigned int           _seq_len,
                                      int                    _ftype,
                                      unsigned int           _k,
                                      unsigned int           _m,
                                      float                  _beta,
                                      qdsync_callback        _callback,
                                      void *                 _context)
{
    // validate input
    if (_seq_len == 0)
        return liquid_error_config("qdsync_cccf_create(), sequence length cannot be zero");

    // allocate memory for main object and set internal properties
    qdsync_cccf q = (qdsync_cccf) malloc(sizeof(struct qdsync_cccf_s));
    q->seq_len = _seq_len;
    q->ftype   = _ftype;
    q->k       = _k;
    q->m       = _m;
    q->beta    = _beta;

    // create detector
    q->detector = qdetector_cccf_create_linear(_seq, _seq_len, _ftype, _k, _m, _beta);

    // create down-coverters for carrier phase tracking
    q->mixer = nco_crcf_create(LIQUID_NCO);

    // create symbol timing recovery filters
    q->npfb = 256;   // number of filters in the bank
    q->mf   = firpfb_crcf_create_rnyquist(q->ftype, q->npfb, q->k, q->m, q->beta);

    // allocate buffer for storing output samples
    q->buf_out_len = 64; // user can re-size this later
    q->buf_out     = (float complex*) malloc(q->buf_out_len*sizeof(float complex));

    // set callback and context values
    qdsync_cccf_set_callback(q, _callback);
    qdsync_cccf_set_context (q, _context );

    // reset and return object
    qdsync_cccf_reset(q);
    return q;
}

// copy object
qdsync_cccf qdsync_cccf_copy(qdsync_cccf q_orig)
{
    // validate input
    if (q_orig == NULL)
        return liquid_error_config("qdetector_%s_copy(), object cannot be NULL", "cccf");

    // create new object and copy base parameters
    qdsync_cccf q_copy = (qdsync_cccf) malloc(sizeof(struct qdsync_cccf_s));
    memmove(q_copy, q_orig, sizeof(struct qdsync_cccf_s));

    // set callback and userdata fields
    q_copy->callback = q_orig->callback;
    q_copy->context  = q_orig->context;

    // copy sub-objects
    q_copy->detector = qdetector_cccf_copy(q_orig->detector);
    q_copy->mixer    = nco_crcf_copy      (q_orig->mixer);
    q_copy->mf       = firpfb_crcf_copy   (q_orig->mf);

    // copy memory in new allocation
    q_copy->buf_out = (float complex*)liquid_malloc_copy(q_orig->buf_out, q_orig->buf_out_len, sizeof(float complex));

    // return new object
    return q_copy;
}

int qdsync_cccf_destroy(qdsync_cccf _q)
{
    // destroy internal objects
    qdetector_cccf_destroy(_q->detector);
    nco_crcf_destroy(_q->mixer);
    firpfb_crcf_destroy(_q->mf);

    // free output buffer
    free(_q->buf_out);

    // free main object memory
    free(_q);
    return LIQUID_OK;
}

int qdsync_cccf_print(qdsync_cccf _q)
{
    printf("<liquid.qdsync_cccf>\n");
    return LIQUID_OK;
}

int qdsync_cccf_reset(qdsync_cccf _q)
{
    qdetector_cccf_reset(_q->detector);
    _q->state = QDSYNC_STATE_DETECT;
    _q->symbol_counter = 0;
    _q->buf_out_counter = 0;
    firpfb_crcf_reset(_q->mf);
    return LIQUID_OK;
}

// get detection threshold
float qdsync_cccf_get_threshold(qdsync_cccf _q)
{
    return qdetector_cccf_get_threshold(_q->detector);
}

// set detection threshold
int qdsync_cccf_set_threshold(qdsync_cccf _q,
                              float       _threshold)
{
    return qdetector_cccf_set_threshold(_q->detector, _threshold);
}

// set carrier offset search range
int qdsync_cccf_set_range(qdsync_cccf _q,
                          float       _dphi_max)
{
    return qdetector_cccf_set_range(_q->detector, _dphi_max);
}

// set callback method
int qdsync_cccf_set_callback(qdsync_cccf _q, qdsync_callback _callback)
{
    _q->callback = _callback;
    return LIQUID_OK;
}

// set context value
int qdsync_cccf_set_context (qdsync_cccf _q, void * _context)
{
    _q->context = _context;
    return LIQUID_OK;
}

// Set callback buffer size (the number of symbol provided to the callback
// whenever it is invoked).
int qdsync_cccf_set_buf_len (qdsync_cccf _q, unsigned int _buf_len)
{
    if (_buf_len == 0)
        return liquid_error(LIQUID_EICONFIG,"qdsync_cccf_set_buf_len(), buffer length must be greater than 0");

    // check current state
    if (_q->buf_out_counter < _buf_len) {
        // buffer might not be empty, but we aren't resizing within this space;
        // ok to resize so long as old samples are copied
        _q->buf_out_len = _buf_len;
        float complex * buf_new = (float complex*)realloc(_q->buf_out, _q->buf_out_len*sizeof(float complex));
        if (buf_new == NULL)
            return liquid_error(LIQUID_EIMEM,"qdsync_cccf_set_buf_len(), could not allocate %u samples", _buf_len);
        _q->buf_out = buf_new;
    } else {
        // we are shrinking the buffer below the number of samples it currently
        // holds; invoke the callback as many times as needed to reduce its size
        unsigned int index = 0;
        while (_q->buf_out_counter >= _buf_len) {
            if (_q->callback != NULL)
                _q->callback(_q->buf_out + index, _buf_len, _q->context);

            // adjust counters
            index += _buf_len;
            _q->buf_out_counter -= _buf_len;
        }

        // copy old values to front of buffer
        memmove(_q->buf_out, _q->buf_out + index, _q->buf_out_counter*sizeof(float complex));

        // now resize the buffer appropriately
        _q->buf_out_len = _buf_len;
        float complex * buf_new = (float complex*)realloc(_q->buf_out, _q->buf_out_len*sizeof(float complex));
        if (buf_new == NULL)
            return liquid_error(LIQUID_EIMEM,"qdsync_cccf_set_buf_len(), could not allocate %u samples", _buf_len);
        _q->buf_out = buf_new;
    }
    return LIQUID_OK;
}

int qdsync_cccf_execute(qdsync_cccf            _q,
                        liquid_float_complex * _buf,
                        unsigned int           _buf_len)
{
    unsigned int i;
    for (i=0; i<_buf_len; i++) {
        switch (_q->state) {
        case QDSYNC_STATE_DETECT:
            // detect frame (look for p/n sequence)
            qdsync_cccf_execute_detect(_q, _buf[i]);
            break;
        case QDSYNC_STATE_SYNC:
            // receive preamble sequence symbols
            qdsync_cccf_step(_q, _buf[i]);
            break;
        default:
            return liquid_error(LIQUID_EINT,"qdsync_cccf_exeucte(), unknown/unsupported state");
        }
    }
    return LIQUID_OK;
}

int qdsync_cccf_is_open(qdsync_cccf _q)
{
    return _q->state == QDSYNC_STATE_DETECT ? 0 : 1;
}

// correlator output
float qdsync_cccf_get_rxy(qdsync_cccf _q)
{
    return qdetector_cccf_get_rxy(_q->detector);
}

// fractional timing offset estimate
float qdsync_cccf_get_tau(qdsync_cccf _q)
{
    return qdetector_cccf_get_tau(_q->detector);
}

// channel gain
float qdsync_cccf_get_gamma(qdsync_cccf _q)
{
    return qdetector_cccf_get_gamma(_q->detector);
}

// carrier frequency offset estimate
float qdsync_cccf_get_dphi(qdsync_cccf _q)
{
    return qdetector_cccf_get_dphi(_q->detector);
}

// carrier phase offset estimate
float qdsync_cccf_get_phi(qdsync_cccf _q)
{
    return qdetector_cccf_get_phi(_q->detector);
}

//
// internal methods
//

// execute synchronizer, seeking preamble sequence
//  _q     :   frame synchronizer object
//  _x      :   input sample
//  _sym    :   demodulated symbol
int qdsync_cccf_execute_detect(qdsync_cccf   _q,
                               float complex _x)
{
    // push through pre-demod synchronizer
    float complex * v = qdetector_cccf_execute(_q->detector, _x);

    // check if frame has been detected
    if (v != NULL) {
        // get estimates
        float tau_hat   = qdetector_cccf_get_tau  (_q->detector);
        float gamma_hat = qdetector_cccf_get_gamma(_q->detector);
        float dphi_hat  = qdetector_cccf_get_dphi (_q->detector);
        float phi_hat   = qdetector_cccf_get_phi  (_q->detector);

        // set appropriate filterbank index
        _q->mf_counter = _q->k - 2;
        _q->pfb_index =  0;
        int index = (int)(tau_hat * _q->npfb);
        if (index < 0) {
            _q->mf_counter++;
            index += _q->npfb;
        }
        _q->pfb_index = index;
        //printf("* qdsync detected! tau:%6.3f, dphi:%12.4e, phi:%6.3f, gamma:%6.2f dB, mf:%u, pfb idx:%u\n",
        //        tau_hat, dphi_hat, phi_hat, 20*log10f(gamma_hat), _q->mf_counter, _q->pfb_index);

        // output filter scale
        firpfb_crcf_set_scale(_q->mf, 1.0f / (_q->k * gamma_hat));

        // set frequency/phase of mixer
        nco_crcf_set_frequency(_q->mixer, dphi_hat);
        nco_crcf_set_phase    (_q->mixer, phi_hat );

        // update state
        _q->state = QDSYNC_STATE_SYNC;

        // run buffered samples through synchronizer
        unsigned int buf_len = qdetector_cccf_get_buf_len(_q->detector);
        qdsync_cccf_execute(_q, v, buf_len);
    }
    return LIQUID_OK;
}

// step receiver mixer, matched filter, decimator
//  _q      :   frame synchronizer
//  _x      :   input sample
//  _y      :   output symbol
int qdsync_cccf_step(qdsync_cccf   _q,
                     float complex _x)
{
    // mix sample down
    float complex v;
    nco_crcf_mix_down(_q->mixer, _x, &v);
    nco_crcf_step    (_q->mixer);

    // push sample into filterbank
    firpfb_crcf_push   (_q->mf, v);
    firpfb_crcf_execute(_q->mf, _q->pfb_index, &v);

    // increment counter to determine if sample is available
    _q->mf_counter++;
    int sample_available = (_q->mf_counter >= _q->k-1) ? 1 : 0;

    // set output sample if available
    if (sample_available) {
        // decrement counter by k=2 samples/symbol
        _q->mf_counter -= _q->k;

        // append to output
        qdsync_cccf_buf_append(_q, v);
    }

    // return flag
    return LIQUID_OK;
}

// append sample to output buffer
int qdsync_cccf_buf_append(qdsync_cccf   _q,
                           float complex _x)
{
    // account for filter delay
    _q->symbol_counter++;
    if (_q->symbol_counter <= 2*_q->m)
        return LIQUID_OK;

    // append sample to end of buffer
    _q->buf_out[_q->buf_out_counter] = _x;
    _q->buf_out_counter++;

    // check if buffer is full
    if (_q->buf_out_counter == _q->buf_out_len) {
        // reset counter
        _q->buf_out_counter = 0;

        // invoke callback
        if (_q->callback != NULL) {
            int rc = _q->callback(_q->buf_out, _q->buf_out_len, _q->context);
            if (rc)
                return qdsync_cccf_reset(_q);
        }
    }
    return LIQUID_OK;
}

