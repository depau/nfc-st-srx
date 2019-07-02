/*-
 * Free/Libre Near Field Communication (NFC) library
 *
 * Libnfc historical contributors:
 * Copyright (C) 2009      Roel Verdult
 * Copyright (C) 2009-2013 Romuald Conty
 * Copyright (C) 2010-2012 Romain Tartière
 * Copyright (C) 2010-2013 Philippe Teuwen
 * Copyright (C) 2012-2013 Ludovic Rousseau
 * See AUTHORS file for a more comprehensive list of contributors.
 * Additional contributors of this file:
 * Davide Depau - Adapted for use with ST SRx reader
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1) Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2 )Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Note that this license only applies on the examples, NFC library itself is under LGPL
 *
 */
/**
 * @file nfc-utils.c
 * @brief Provide some examples shared functions.
 */
#include <nfc/nfc.h>
#include <err.h>
#include <ctype.h>

#include "nfc-utils.h"

void
print_hex(const uint8_t *pbtData, const size_t szBytes) {
    size_t szPos;

    for (szPos = 0; szPos < szBytes; szPos++) {
        fprintf(stderr, "%02x  ", pbtData[szPos]);
    }

    fprintf(stderr, "\t\t|");
    for (szPos = 0; szPos < szBytes; szPos++) {
        fprintf(stderr, "%c", isprint(pbtData[szPos]) ? pbtData[szPos] : '.');
    }

    fprintf(stderr, "|\n");
}

void
print_nfc_target(const nfc_target *pnt, bool verbose) {
    char *s;
    str_nfc_target(&s, pnt, verbose);
    fprintf(stderr, "%s", s);
    nfc_free(s);
}

size_t
transceive_bytes(nfc_device *pnd, const uint8_t *pbtTx, uint8_t *pbtRx, const size_t szTx, bool verbose) {
    if (verbose) {
        // Show transmitted command
        fprintf(stderr, "Sent bits:     ");
        print_hex(pbtTx, szTx);
    }

    // Transmit the command bytes
    size_t res;

    if ((res = nfc_initiator_transceive_bytes(pnd, pbtTx, szTx, pbtRx, sizeof(pbtRx), 0)) < 0)
        return false;

    if (verbose) {
        // Show received answer
        fprintf(stderr, "Received bits: ");
        print_hex(pbtRx, res);
    }

    // Successful transfer
    return res;
}