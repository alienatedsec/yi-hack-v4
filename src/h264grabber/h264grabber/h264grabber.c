/*
 * Copyright (c) 2021 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Dump h264 content from /tmp/view to fifo or stdout
 */

#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <getopt.h>
#include <signal.h>

#define TABLE_HIGH_OFFSET 0x10
#define TABLE_LOW_OFFSET 0x25A0
#define TABLE_LENGTH 9600
#define TABLE_RECORD_SIZE 32
#define TABLE_RECORD_NUM 300
#define BUF_SIZE 1586752
#define STREAM_HIGH_OFFSET 0x9640
#define STREAM_LOW_OFFSET 0x109640

#define FRAME_COUNTER_OFFSET 18
#define FRAME_OFFSET_OFFSET 4
#define FRAME_LENGTH_OFFSET 8

#define MILLIS_10 10000

#define RESOLUTION_NONE 0
#define RESOLUTION_LOW  360
#define RESOLUTION_HIGH 1080

#define BUFFER_FILE "/tmp/view"

#define FIFO_NAME_LOW "/tmp/h264_low_fifo"
#define FIFO_NAME_HIGH "/tmp/h264_high_fifo"

// Unused vars
unsigned char IDR[]               = {0x65, 0xB8};
unsigned char NAL_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65};
unsigned char PFR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x61};
unsigned char SPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char PPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};

//unsigned char *addr;                      /* Pointer to shared memory region (header) */
//int resolution;
//int debug;
//int fifo;

long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds

    return milliseconds;
}

void sigpipe_handler(int unused)
{
    // Do nothing
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES, --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: LOW or HIGH (default HIGH)\n");
    fprintf(stderr, "\t-f, --fifo\n");
    fprintf(stderr, "\t\tenable fifo output\n");
    fprintf(stderr, "\t-d, --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv) {
    unsigned char *frame_ptr;
    unsigned int frame_offset;
    unsigned int frame_length;
    unsigned char *record_ptr, *next_record_ptr;

    FILE *fFid = NULL;
    FILE *fOut = NULL;

    int current_frame, frame_counter, frame_counter_tmp, next_frame_counter;
    int table_offset, stream_offset;

    int i, c;
    mode_t mode = 0755;

    table_offset = TABLE_HIGH_OFFSET;
    stream_offset = STREAM_HIGH_OFFSET;

    unsigned char *addr;
    int resolution = RESOLUTION_HIGH;
    int debug = 0;
    int fifo = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"fifo",  no_argument, 0, 'f'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:fdh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            }
            break;

        case 'f':
            fprintf (stderr, "Using fifo as output\n");
            fifo = 1;
            break;

        case 'd':
            fprintf(stderr, "Debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if (strcmp("h264grabber_l", basename(argv[0])) == 0) {
        resolution = RESOLUTION_LOW;
    } else if (strcmp("h264grabber_h", basename(argv[0])) == 0) {
        resolution = RESOLUTION_HIGH;
    }

    if (resolution == RESOLUTION_LOW) {
        table_offset = TABLE_LOW_OFFSET;
        stream_offset = STREAM_LOW_OFFSET;
        fprintf(stderr, "Resolution low\n");
    } else if (resolution == RESOLUTION_HIGH) {
        table_offset = TABLE_HIGH_OFFSET;
        stream_offset = STREAM_HIGH_OFFSET;
        fprintf(stderr, "Resolution high\n");
    }

    // Opening an existing file
    fFid = fopen(BUFFER_FILE, "r") ;
    if ( fFid == NULL ) {
        fprintf(stderr, "Could not open file %s\n", BUFFER_FILE) ;
        return -1;
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, BUF_SIZE, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "Error mapping file %s\n", BUFFER_FILE);
        fclose(fFid);
        return -2;
    }
    if (debug) fprintf(stderr, "%lld - mapping file %s, size %d, to %08x\n", current_timestamp(), BUFFER_FILE, BUF_SIZE, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "%lld - closing the file %s\n", current_timestamp(), BUFFER_FILE) ;
    fclose(fFid) ;

    if (fifo == 0) {
        char stdoutbuf[262144];

        if (setvbuf(stdout, stdoutbuf, _IOFBF, sizeof(stdoutbuf)) != 0) {
            fprintf(stderr, "Error setting stdout buffer\n");
        }
        fOut = stdout;
    } else {
        sigaction(SIGPIPE, &(struct sigaction){{sigpipe_handler}}, NULL);

        if (resolution == RESOLUTION_LOW) {
            unlink(FIFO_NAME_LOW);
            if (mkfifo(FIFO_NAME_LOW, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_LOW);
                return -1;
            }
            fOut = fopen(FIFO_NAME_LOW, "w");
            if (fOut == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_LOW);
                return -1;
            }
        } else if (resolution == RESOLUTION_HIGH) {
            unlink(FIFO_NAME_HIGH);
            if (mkfifo(FIFO_NAME_HIGH, mode) < 0) {
                fprintf(stderr, "mkfifo failed for file %s\n", FIFO_NAME_HIGH);
                return -1;
            }
            fOut = fopen(FIFO_NAME_HIGH, "w");
            if (fOut == NULL) {
                fprintf(stderr, "Error opening fifo %s\n", FIFO_NAME_HIGH);
                return -1;
            }
        }
    }

    // Find the record with the largest frame_counter
    current_frame = -1;
    frame_counter = -1;
    for (i = 0; i < TABLE_RECORD_NUM; i++) {
        // Get pointer to the record
        record_ptr = addr + table_offset + (i * TABLE_RECORD_SIZE);
        // Get the frame counter
        frame_counter_tmp = (((int) *(record_ptr + FRAME_COUNTER_OFFSET + 1)) << 8) +
                    ((int) *(record_ptr + FRAME_COUNTER_OFFSET));
        // Check if the is the largest frame_counter
        if (frame_counter_tmp > frame_counter) {
            frame_counter = frame_counter_tmp;
        } else {
            current_frame = i;
            break;
        }
    }
    if (debug) fprintf(stderr, "%lld - found latest frame: id %d, frame_counter %d\n", current_timestamp(), current_frame, frame_counter);

    // Wait for the next record to arrive and read the frame
    for (;;) {
        // Get pointer to the record
        record_ptr = addr + table_offset + (current_frame * TABLE_RECORD_SIZE);
        if (debug) fprintf(stderr, "%lld - processing frame %d\n", current_timestamp(), current_frame);
        // Check if we are at the end of the table
        if (current_frame == TABLE_RECORD_NUM - 1) {
            next_record_ptr = addr + table_offset;
            if (debug) fprintf(stderr, "%lld - rewinding circular table\n", current_timestamp());
        } else {
            next_record_ptr = record_ptr + TABLE_RECORD_SIZE;
        }
        // Get the frame counter of the next record
        next_frame_counter = ((int) *(next_record_ptr + FRAME_COUNTER_OFFSET + 1)) * 256 + ((int) *(next_record_ptr + FRAME_COUNTER_OFFSET));
        // Check if the frame counter is valid
        if (next_frame_counter >= frame_counter + 1) {
            // Get the offset of the stream
            frame_offset = (((int) *(record_ptr + FRAME_OFFSET_OFFSET + 3)) << 24) +
                        (((int) *(record_ptr + FRAME_OFFSET_OFFSET + 2)) << 16) +
                        (((int) *(record_ptr + FRAME_OFFSET_OFFSET + 1)) << 8) +
                        ((int) *(record_ptr + FRAME_OFFSET_OFFSET));
//            next_frame_offset = ((((((next_*record_ptr + FRAME_OFFSET_OFFSET + 3) * 256) + (next_*record_ptr + FRAME_OFFSET_OFFSET + 2)) * 256) +
//                        (next_*record_ptr + FRAME_OFFSET_OFFSET + 1)) * 256) + (next_*record_ptr + FRAME_OFFSET_OFFSET);
            // Get the pointer to the frame address
            frame_ptr = addr + stream_offset + frame_offset;
//            next_frame_ptr = STREAM_HIGH_OFFSET + next_frame_offset;
            // Get the length of the frame
            frame_length = (((int) *(record_ptr + FRAME_LENGTH_OFFSET + 3)) << 24) +
                        (((int) *(record_ptr + FRAME_LENGTH_OFFSET + 2)) << 16) +
                        (((int) *(record_ptr + FRAME_LENGTH_OFFSET + 1)) << 8) +
                        ((int) *(record_ptr + FRAME_LENGTH_OFFSET));
            if (debug) fprintf(stderr, "%lld - writing frame: frame_offset %d, frame_ptr %08x, frame_length %d\n", current_timestamp(), frame_offset, (unsigned int) frame_ptr, frame_length);
            // Write the frame
            fwrite(frame_ptr, 1, frame_length, fOut);

            // Check if we are at the end of the table
            if (current_frame == TABLE_RECORD_NUM - 1) {
                current_frame = 0;
            } else {
                current_frame++;
            }
        }
        fflush(fOut);

        // Wait 10 milliseconds
        usleep(MILLIS_10);
    }

    // Unreacheable path

    if (fifo == 1) {
        if (resolution == RESOLUTION_LOW) {
            fclose(fOut);
            unlink(FIFO_NAME_LOW);
        } else if (resolution == RESOLUTION_HIGH) {
            fclose(fOut);
            unlink(FIFO_NAME_HIGH);
        }
    }

    // Unmap file from memory
    if (munmap(addr, BUF_SIZE) == -1) {
        if (debug) fprintf(stderr, "Error munmapping file\n");
    } else {
        if (debug) fprintf(stderr, "Unmapping file %s, size %d, from %08x\n", BUFFER_FILE, BUF_SIZE, (unsigned int) addr);
    }

    return 0;
}
