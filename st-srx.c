//
// Created by depau on 7/2/19.
//

#include <stdio.h>
#include <nfc/nfc.h>
#include "nfc-utils.h"
#include "st-srx.h"


size_t
st_srx_get_uid(nfc_device *pnd, uint8_t *uidRx, const size_t szUid, bool verbose) {
    uint8_t cmd[] = {0x0b};
    return transceive_bytes(pnd, (const uint8_t *) &cmd, uidRx, sizeof(cmd), szUid, verbose);
}


size_t
st_srx_read_block(nfc_device *pnd, uint8_t *blockRx, const size_t szRead, uint8_t address, bool verbose) {
    uint8_t cmd[2] = {0x08};
    memcpy(cmd + 1, &address, 1);
    return transceive_bytes(pnd, (const uint8_t *) &cmd, blockRx, sizeof(cmd), szRead, verbose);
}

size_t
st_srx_write_block(nfc_device *pnd, uint8_t *blockRx, const size_t szWrite, uint8_t address, uint8_t *data, bool verbose) {
    uint8_t cmd[6] = {0x09};
    memcpy(cmd + 1, &address, 1);
    memcpy(cmd + 2, data, 4);
    return transceive_bytes(pnd, (const uint8_t *) &cmd, blockRx, sizeof(cmd), szWrite, verbose);
}