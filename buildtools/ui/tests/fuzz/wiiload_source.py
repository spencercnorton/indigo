#!/usr/bin/env python3
"""Emit the real upload decoder/image checks and a bounded wire fuzzer."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from wiiload_source import stream_source

PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
typedef uint8_t u8;typedef uint32_t u32;typedef int uiDrawObj_t;
typedef int wiiload_read_function_t(int,void *,size_t);
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ELFMAG "\177ELF"
#define SELFMAG 4
static const u8 *input;static size_t available,cursor;
static int readInput(int fd,void *out,size_t size) {
    (void)fd;if(size>available-cursor)size=available-cursor;
    memcpy(out,input+cursor,size);cursor+=size;return (int)size;
}
static void DrawUpdateProgressBar(uiDrawObj_t *p,int value) {
    (void)p;assert(value>=0&&value<=100);
}
'''

DRIVER = r'''
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if(size<8||size>1024*1024)return 0;
    size_t output=wiiload_be32(data),declared=wiiload_be32(data+4);
    if(output>65536)return 0; /* harness work bound, not the protocol limit */
    input=data+8;available=size-8;cursor=0;
    void *decoded=wiiload_read(0,declared,output,readInput,NULL);
    if(decoded) {
        uLongf length=output+1;uLong compressed=available;
        u8 *check=malloc(output+1);assert(check);
        assert(uncompress2(check,&length,input,&compressed)==Z_OK);
        assert(length==output&&compressed==declared);
        assert(memcmp(check,decoded,output)==0);
        (void)wiiload_image_valid(decoded,output,false);
        free(check);free(decoded);
    }
    /* Directly cover hostile executable tables even without a valid zlib. */
    (void)wiiload_image_valid(data,size,false);
    return 0;
}
'''

Path(sys.argv[1]).write_text(PREFIX + stream_source() + DRIVER)
