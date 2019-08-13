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
    fprintf(stderr, "usage: %s [-h] [-v] [-w | -d] [-t x4k|512] [-f FILE]\n", progname);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -h         Show this help message\n");
    fprintf(stderr, "  -v         Verbose - print transceived messages\n");
    fprintf(stderr, "  -w         Write dump instead of reading\n");
    fprintf(stderr, "  -d         Dry run - check for potential irreversible changes instead of writing\n");
    fprintf(stderr, "  -f FILE    Dump (write) memory content to (from) FILE\n");
    fprintf(stderr, "  -f -       Dump (write) memory content to stdout (from stdin) (default)\n");
    fprintf(stderr, "  -t x4k|512 Select SRIX4K or SRI512 tag type. Default is SRIX4K\n");
}


static int
read_dump_file(st_srx_tag_t *dest, FILE *dump_fd) {
    for (size_t i = 0; i < sizeof(dest->raw_bytes); i++) {
        int byte = fgetc(dump_fd);
        if (ferror(dump_fd)) {
            perror("porcodio");
            return EXIT_FAILURE;
        }

        dest->raw_bytes[i] = byte;
    }
    if (fgetc(dump_fd) != EOF) {
        fprintf(stderr, "Dump file is longer than expected. Refusing to write to avoid damage.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int
write_dump_file(st_srx_tag_t *src, FILE *dump_fd) {
    for (size_t i = 0; i < sizeof(src->raw_bytes); i++) {
        fputc(src->raw_bytes[i], dump_fd);
    }
    return EXIT_SUCCESS;
}

static int
dump_eeprom(st_srx_tag_t *dest, bool verbose) {
    // Dump EEPROM to RAM
    fprintf(stderr, "Reading %d blocks\n|", tag_length);
    for (uint8_t i = 0; i < tag_length; i++) {
        uint8_t *block_dest = dest->raw_bytes + i * 4;
        if (st_srx_read_block(pnd, block_dest, i, verbose) <= 0) {
            nfc_perror(pnd, "nfc_initiator_transceive_bytes");
            nfc_close(pnd);
            nfc_exit(context);
            return EXIT_FAILURE;
        }
        if (!verbose)
            fputc('.', stderr);
    }
    fprintf(stderr, "|\n");

    fprintf(stderr, "Reading system area block (0xFF)\n");
    uint8_t *block_dest = dest->raw_bytes + 0xff * 4;
    if (st_srx_read_block(pnd, block_dest, 0xff, verbose) <= 0) {
        nfc_perror(pnd, "nfc_initiator_transceive_bytes");
        nfc_close(pnd);
        nfc_exit(context);
        return EXIT_FAILURE;
    }
    if (!verbose)
        fprintf(stderr, "|.|\n");

    // Store 1s in all empty blocks. This is an extra  allows a 512 dump to be written on a X4K without
    // accidentally write protecting anything
    if (tag_length == SRIX4K_EEPROM_LEN) {
        memset(dest->srix4k.padding, 0xff, sizeof(dest->srix4k.padding));
    } else {
        memset(dest->sri512.padding, 0xff, sizeof(dest->sri512.padding));
    }

    return EXIT_SUCCESS;
}

static void
write_dry_run(st_srx_tag_t *file_dump) {
    st_srx_tag_t tag_dump;
    fprintf(stderr, "Reading tag...\n");
    dump_eeprom(&tag_dump, false);

    fprintf(stderr, "\nChecking system area\n");
    uint32_t tag_sys = tag_dump.srix4k.system_block[0] << 24 | tag_dump.srix4k.system_block[1] << 16 |
                       tag_dump.srix4k.system_block[2] << 8 | tag_dump.srix4k.system_block[3];
    uint32_t file_sys = file_dump->srix4k.system_block[0] << 24 | file_dump->srix4k.system_block[1] << 16 |
                        file_dump->srix4k.system_block[2] << 8 | file_dump->srix4k.system_block[3];

    if ((file_sys & tag_sys) != tag_sys) {
        fprintf(stderr, "Tag system area would irreversibly be updated. In particular:\n");

        // Check lockable OTP area
        for (int i = 24; i < 32; i++) {
            if ((file_sys >> i & 1) == 0) {
                if (i == 24) {
                    fprintf(stderr, "- Block %d would be locked\n", 7);
                }
                fprintf(stderr, "- Block %d would be locked\n", i - 16);
            }
        }

        for (int i = 8; i < 24; i++) {
            if ((file_sys >> i & 1) == 0) {
                fprintf(stderr, "- ST reserved area would be changed with unknown results\n");
                break;
            }
        }

        if ((file_sys & 0xFF) != 0xFF) {
            fprintf(stderr, "- Fixed chip ID would be set to %02X\n", file_sys & 0xFF);
        }
    }

    bool autoerase = false;
    fprintf(stderr, "\nChecking 32-bit binary counters\n");
    for (int i = 5; i <= 6; i++) {
        uint32_t tag_val = tag_dump.raw_blocks[i][0] << 24 | tag_dump.raw_blocks[i][1] << 16 |
                           tag_dump.raw_blocks[i][2] << 8 | tag_dump.raw_blocks[i][3];
        uint32_t file_val = file_dump->raw_blocks[i][0] << 24 | file_dump->raw_blocks[i][1] << 16 |
                            file_dump->raw_blocks[i][2] << 8 | file_dump->raw_blocks[i][3];

        if (file_val < tag_val) {
            fprintf(stderr, "Counter at block %d would be updated", i);

            if (i == 6 && ((tag_val ^ file_val) >> (32 - 11) > 0)) {
                fprintf(stderr, " (OTP area auto-erase cycle triggered)");
                autoerase = true;
            }

            fputc('\n', stderr);
        }
    }

    fprintf(stderr, "\nChecking resettable OTP area\n");
    for (int i = 0; i <= 4; i++) {
        uint8_t *tag = tag_dump.raw_blocks[i];
        uint8_t *file = file_dump->raw_blocks[i];
        for (int j = 0; j < 4; j++) {
            if (autoerase && tag[j] != file[j]) {
                fprintf(stderr, "Block %d would be changed (due to auto-erase)\n", i);
                break;
            }
            if ((tag[j] & file[j]) != tag[j]) {
                fprintf(stderr, "Block %d would be updated\n", i);
                break;
            }
        }
    }
}

static int
write_eeprom(st_srx_tag_t *src, bool verbose) {
    fprintf(stderr, "Writing %d blocks\n|", tag_length);
    for (uint8_t i = 0; i < tag_length; i++) {
        uint8_t *block_src = src->raw_blocks[i];

        if (st_srx_read_block(pnd, abtRx, i, verbose) <= 0) {
            nfc_perror(pnd, "nfc_initiator_transceive_bytes");
            nfc_close(pnd);
            nfc_exit(context);
            return EXIT_FAILURE;
        }

        if (memcmp(block_src, abtRx, 4) != 0) {
            if (st_srx_write_block(pnd, abtRx, i, block_src, verbose) <= 0) {
                nfc_perror(pnd, "nfc_initiator_transceive_bytes");
                nfc_close(pnd);
                nfc_exit(context);
                return EXIT_FAILURE;
            }
            if (!verbose)
                fputc('.', stderr);
        } else if (!verbose) {
            fputc(' ', stderr);
        }
    }
    fprintf(stderr, "|\n");

    fprintf(stderr, "Writing system area block (0xFF)\n");
    uint8_t *block_src = src->srix4k.system_block;
    if (st_srx_read_block(pnd, abtRx, 0xFF, verbose) <= 0) {
        nfc_perror(pnd, "nfc_initiator_transceive_bytes");
        nfc_close(pnd);
        nfc_exit(context);
        return EXIT_FAILURE;
    }
    if (memcmp(block_src, abtRx, 4) != 0) {
        if (st_srx_write_block(pnd, abtRx, 0xFF, block_src, verbose) <= 0) {
            nfc_perror(pnd, "nfc_initiator_transceive_bytes");
            nfc_close(pnd);
            nfc_exit(context);
            return EXIT_FAILURE;
        }
        if (!verbose)
            fprintf(stderr, "|.|\n");
    } else if (!verbose) {
        fprintf(stderr, "| |\n");
    }

    return EXIT_SUCCESS;
}


int
main(int argc, const char *argv[]) {

    int ch;
    char *dump_file = NULL;
    char *tag_type = NULL;
    bool verbose = false;
    bool write = false;
    bool dry_run = false;

    FILE *dump_fd;

    // Parse arguments
    while ((ch = getopt(argc, (char *const *) argv, "hvwdt:f:")) != -1) {
        switch (ch) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'v':
                verbose = true;
                break;
            case 'f':
                dump_file = optarg;
                break;
            case 'w':
                write = true;
                break;
            case 'd':
                dry_run = true;
                break;
            case 't':
                tag_type = optarg;
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
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
    if (dump_file == NULL || (strlen(dump_file) == 1 && dump_file[0] == '-')) {
        if (!write) {
            printf("stdout %s\n", dump_file);
            dump_fd = stdout;
        } else {
            printf("stdin %s\n", dump_file);
            dump_fd = stdin;
        }
    } else {
        if (!write && !dry_run) {
            printf("wb %s\n", dump_file);
            dump_fd = fopen(dump_file, "wb");
        } else {
            printf("rb %s\n", dump_file);
            dump_fd = fopen(dump_file, "rb");
        }
        if (!dump_fd) {
            ERR("Could not open file %s.\n", dump_file);
            exit(EXIT_FAILURE);
        }
    }

    if (dump_fd == NULL) {
        perror("Error opening dump file");
        exit(EXIT_FAILURE);
    }

    if (dry_run) {
        fprintf(stderr, "===== DRY RUN =====\n");
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

    int ret = EXIT_SUCCESS;
    if (dry_run) {
        ret = read_dump_file(&dump, dump_fd);
        if (ret == EXIT_SUCCESS) {
            write_dry_run(&dump);
        }
    } else if (!write) {
        ret = dump_eeprom(&dump, verbose);
        if (ret == EXIT_SUCCESS) {
            ret = write_dump_file(&dump, dump_fd);
        }
    } else {
        ret = read_dump_file(&dump, dump_fd);
        if (ret == EXIT_SUCCESS) {
            write_eeprom(&dump, verbose);
        }
    }

    if (ret != EXIT_SUCCESS) {
        fprintf(stderr, "Operation error\n");
        exit(ret);
    }

    fclose(dump_fd);

    nfc_close(pnd);
    nfc_exit(context);

    return 0;
}