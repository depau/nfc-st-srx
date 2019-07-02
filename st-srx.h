//
// Created by depau on 7/2/19.
//

#ifndef NFC_ST_SRX_ST_SRX_H
#define NFC_ST_SRX_ST_SRX_H

#define SRIX4K_EEPROM_LEN 0x80
#define SRI512_EEPROM_LEN 0x10
#define DUMP_LEN 0x100

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint8_t eeprom[SRIX4K_EEPROM_LEN * 4];
    uint8_t padding[(DUMP_LEN - 1 - SRIX4K_EEPROM_LEN) * 4];
    uint8_t system_block[1 * 4];
} st_srix4k_tag_t;

typedef struct {
    uint8_t eeprom[SRI512_EEPROM_LEN * 4];
    uint8_t padding[(DUMP_LEN - 1 - SRI512_EEPROM_LEN) * 4];
    uint8_t system_block[1 * 4];
} st_sri512_tag_t;

typedef union {
    st_srix4k_tag_t srix4k;
    st_sri512_tag_t sri512;
    uint8_t raw[DUMP_LEN * 4];
} st_srx_tag_t;

size_t st_srx_get_uid(nfc_device *pnd, uint8_t *uidRx, bool verbose);
size_t st_srx_read_block(nfc_device *pnd, uint8_t *blockRx, uint8_t address, bool verbose);
size_t st_srx_write_block(nfc_device *pnd, uint8_t *blockRx, uint8_t address, uint8_t *data, bool verbose);

#endif //NFC_ST_SRX_ST_SRX_H
