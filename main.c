#include <stdio.h>
#include <stdlib.h>
#include <nfc/nfc.h>
#include <getopt.h>
#include "nfc-utils.h"
#include "st-srx.h"

#define MAX_TARGET_COUNT 16
#define MAX_FRAME_LEN 264

static nfc_context *context;
static nfc_device *pnd;
static nfc_target *nt;
static nfc_target ant[MAX_TARGET_COUNT];
static uint8_t abtRx[MAX_FRAME_LEN];
static uint8_t tag_length;
static st_srx_tag_t dump;

static const nfc_modulation nmISO14443B = {
        .nmt = NMT_ISO14443B,
        .nbr = NBR_106,
};

static const nfc_modulation nmSTSRx = {
        .nmt = NMT_ISO14443B2SR,
        .nbr = NBR_106,
};


static void
print_usage(const char *progname) {
    fprintf(stderr, "usage: %s [-h] [-v] -o FILE [-t x4k|512]\n", progname);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -h         Show this help message\n");
    fprintf(stderr, "  -v         Verbose - print transceived messages\n");
    fprintf(stderr, "  -o FILE    Dump memory content to FILE\n");
    fprintf(stderr, "  -o -       Dump memory content to stdout\n");
    fprintf(stderr, "  -t x4k|512 Select SRIX4K or SRI512 tag type. Default is SRIX4K\n");
}

int
main(int argc, const char *argv[]) {

    int ch;
    char *dump_file = NULL;
    char *tag_type = NULL;
    bool verbose = false;

    FILE *dump_fd;

    // Parse arguments
    while ((ch = getopt(argc, (char *const *) argv, "hvo:t::")) != -1) {
        switch (ch) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'v':
                verbose = true;
                break;
            case 'o':
                dump_file = optarg;
                break;
            case 't':
                tag_type = optarg;
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (dump_file == NULL) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (tag_type == NULL || strcmp(tag_type, "x4k") == 0) {
        tag_length = SRIX4K_EEPROM_LEN;
    } else if (strcmp(tag_type, "512") == 0) {
        tag_length = SRI512_EEPROM_LEN;
    } else {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open output file
    if ((strlen(dump_file) == 1) && (dump_file[0] == '-')) {
        dump_fd = stdout;
    } else {
        dump_fd = fopen(dump_file, "wb");
        if (!dump_fd) {
            ERR("Could not open file %s.\n", dump_file);
            exit(EXIT_FAILURE);
        }
    }


    // Initialize libnfc
    nfc_init(&context);
    if (context == NULL) {
        ERR("Unable to init libnfc (malloc)");
        fclose(dump_fd);
        exit(EXIT_FAILURE);
    }

    const char *acLibnfcVersion = nfc_version();
    fprintf(stderr, "%s uses libnfc %s\n", argv[0], acLibnfcVersion);

    // Try to open the NFC reader
    pnd = nfc_open(context, NULL);
    if (pnd == NULL) {
        ERR("Error opening NFC reader");
        fclose(dump_fd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    if (nfc_initiator_init(pnd) < 0) {
        fclose(dump_fd);
        nfc_perror(pnd, "nfc_initiator_init");
        nfc_close(pnd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "NFC device: %s opened\n", nfc_device_get_name(pnd));

    // For some reason a ISO14443B-2 tag won't be detected if I don't scan for
    // ISO14443B tags first
    nfc_initiator_list_passive_targets(pnd, nmISO14443B, ant, MAX_TARGET_COUNT);

    fprintf(stderr, "Waiting for tag...\n");

    // Infinite select for tag
    if (nfc_initiator_select_passive_target(pnd, nmSTSRx, NULL, 0, nt) <= 0) {
        fclose(dump_fd);
        nfc_perror(pnd, "nfc_initiator_select_passive_target");
        nfc_close(pnd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    print_nfc_target(nt, false);

    // Try to retrieve the UID using the SRx protocol to confirm it's working
    fprintf(stderr, "Found ISO14443B-2 tag, UID:\n");
    size_t received_bytes = st_srx_get_uid(pnd, abtRx, true);
    if (received_bytes <= 0) {
        fclose(dump_fd);
        ERR("Failed to retrieve the UID");
        nfc_perror(pnd, "nfc_initiator_transceive_bytes");
        nfc_close(pnd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }

    // Dump EEPROM to RAM
    fprintf(stderr, "Reading %d blocks\n|", tag_length);
    for (uint8_t i = 0; i < tag_length; i++) {
        uint8_t *block_dest = dump.raw + i * 4;
        if (st_srx_read_block(pnd, block_dest, i, verbose) <= 0) {
            fclose(dump_fd);
            nfc_perror(pnd, "nfc_initiator_transceive_bytes");
            nfc_close(pnd);
            nfc_exit(context);
            exit(EXIT_FAILURE);
        }
        if (!verbose)
            fprintf(stderr, ".");
    }
    fprintf(stderr, "|\n");

    fprintf(stderr, "Reading system area block (0xFF)\n");
    uint8_t *block_dest = dump.raw + 0xff * 4;
    if (st_srx_read_block(pnd, block_dest, 0xff, verbose) <= 0) {
        fclose(dump_fd);
        nfc_perror(pnd, "nfc_initiator_transceive_bytes");
        nfc_close(pnd);
        nfc_exit(context);
        exit(EXIT_FAILURE);
    }
    if (!verbose)
        fprintf(stderr, "|.|\n");

    // Store 1s in all empty blocks. This is an extra  allows a 512 dump to be written on a X4K without
    // accidentally write protecting anything
    if (tag_length == SRIX4K_EEPROM_LEN) {
        memset(dump.srix4k.padding, 0xff, sizeof(dump.srix4k.padding));
    } else {
        memset(dump.sri512.padding, 0xff, sizeof(dump.sri512.padding));
    }

    for (size_t i = 0; i < sizeof(dump.raw); i++) {
        fputc(dump.raw[i], dump_fd);
    }
    fclose(dump_fd);

    nfc_close(pnd);
    nfc_exit(context);

    return 0;
}