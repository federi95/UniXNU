/* Owned deterministic fixtures; public source implementations remain external. */
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccder.h>
#include <libkern/libkern.h>
#include <pexpert/pexpert.h>
#include "public-crypto-expected.h"
/* Explicit asm binding exercises the existing kernel entry, outside namespace macros. */
extern void unixnu_native_digest(const struct ccdigest_info *, size_t, const void *, void *) __asm__("_ccdigest");
volatile uint32_t unixnu_public_crypto_fixture_result[12] = {1};
static const size_t lengths[] = {0,1,3,55,56,63,64,65,111,112,127,128,129,255,256,257,385};
static const char *names[] = {"sha1","sha224","sha256","sha384","sha512"};
static const struct ccdigest_info *infos[] = {&ccsha1_ltc_di,&ccsha224_ltc_di,&ccsha256_ltc_di,&ccsha384_ltc_di,&ccsha512_ltc_di};

static bool filled(const unsigned char *p, size_t n, unsigned char value)
{
    for (size_t i=0;i<n;i++) if (p[i]!=value) return false;
    return true;
}


static void fixture_digest(int negative)
{
    size_t id=0;
    for (size_t a=0;a<5;a++) for (size_t n=0;n<sizeof(lengths)/sizeof(lengths[0]);n++) for (unsigned mode=0;mode<4;mode++) {
        const struct ccdigest_info *di=infos[a]; size_t len=lengths[n];
        unsigned char message[385], output[80], arena[288] __attribute__((aligned(16)));
        for (size_t i=0;i<sizeof(message);i++) message[i]=(unsigned char)(17*i+3);
        if (len==3) memcpy(message,"abc",3);
        memset(output,0xa5,sizeof(output)); memset(arena,0xa5,sizeof(arena));
        size_t logical=ccdigest_di_size(di);
        size_t storage=sizeof(struct ccdigest_ctx)*cc_ctx_n(struct ccdigest_ctx,logical);
        if (storage+16>sizeof(arena) || di->output_size>64) { unixnu_public_crypto_fixture_result[4]++; return; }
        ccdigest_ctx_t ctx=(ccdigest_ctx_t)(arena+16); unsigned char *digest=output+8;
        bool clear_ok=true;
        if (mode==3) {
            unixnu_native_digest(di,len,len ? message : NULL,digest);
        } else if (mode==0) {
            ccdigest(di,len,len ? message : NULL,digest);
        } else {
            ccdigest_init(di,ctx); ccdigest_update(di,ctx,0,NULL);
            size_t offset=0, step=0;
            static const size_t chunks[]={1,13,31,7,64,127};
            while (offset<len) {
                size_t count=mode==1 ? 1 : chunks[step++%6];
                if (count>len-offset) count=len-offset;
                ccdigest_update(di,ctx,count,message+offset); offset+=count;
                if (mode==2) ccdigest_update(di,ctx,0,NULL);
            }
            ccdigest_final(di,ctx,digest);
            ccdigest_di_clear(di,ctx); clear_ok=filled(arena+16,logical,0);
        }
        bool guard_ok=filled(arena,16,0xa5) && filled(arena+16+storage,sizeof(arena)-16-storage,0xa5)
                   && filled(output,8,0xa5) && filled(output+8+di->output_size,sizeof(output)-8-di->output_size,0xa5);
        bool match=true;
        for (size_t i=0;i<di->output_size;i++) {
            unsigned char expected=unixnu_digest_expected[a][n][i];
            if (negative && id==0 && i==0) expected^=1;
            if (digest[i]!=expected) match=false;
        }
        if (!guard_ok || !clear_ok || !match) unixnu_public_crypto_fixture_result[4]++;
        unixnu_public_crypto_fixture_result[2]++;
        unixnu_public_crypto_fixture_result[5]++;
        if (mode==1 || mode==2) unixnu_public_crypto_fixture_result[6]++;
        if (mode==3) unixnu_public_crypto_fixture_result[7]++;
        if (!guard_ok) unixnu_public_crypto_fixture_result[10]++;
        if (!clear_ok) unixnu_public_crypto_fixture_result[11]++;
        id++;
    }
}

struct der_fixture {
    const char *name;
    unsigned char data[264];
    size_t n;
    ccder_tag tag;
    bool loose_ok, strict_ok;
    size_t loose_cursor, strict_cursor, length;
};

static const struct der_fixture der_cases[] = {
    {"zero", {4,0}, 2, 4, true,true, 2,2,0},
    {"short", {4,1,42}, 3, 4, true,true, 2,2,1},
    {"trailing", {4,1,42,99}, 4, 4, true,true, 2,2,1},
    {"canonical128", {4,0x81,0x80}, 131, 4, true,true, 3,3,128},
    {"canonical256", {4,0x82,1,0}, 260, 4, true,true, 4,4,256},
    {"noncanonical81", {4,0x81,1,42}, 4, 4, true,false, 3,1,1},
    {"noncanonical82", {4,0x82,0,1,42}, 5, 4, true,false, 4,1,1},
    {"noncanonical83", {4,0x83,0,0,1,42}, 6, 4, true,false, 5,1,1},
    {"noncanonical84", {4,0x84,0,0,0,0}, 6, 4, true,false, 6,1,0},
    {"empty", {0}, 0, 4, false,false, 0,0,0},
    {"wrongtag", {5,1,42}, 3, 4, false,false, 1,1,0},
    {"missinglen", {4}, 1, 4, false,false, 1,1,0},
    {"indefinite", {4,0x80}, 2, 4, false,false, 1,1,0},
    {"unsupportedlen", {4,0x85,0,0,0,0,0}, 7, 4, false,false, 1,1,0},
    {"truncated81", {4,0x81}, 2, 4, false,false, 1,1,0},
    {"truncated82", {4,0x82,0}, 3, 4, false,false, 1,1,0},
    {"truncatedbody", {4,2,42}, 3, 4, false,false, 1,1,0},
    {"multitag", {0x1f,0x20,1,42}, 4, 0x20, true,true, 3,3,1},
    {"truncatedtag", {0x1f,0x80}, 2, 4, false,false, 0,0,0},
    {"highclass", {0xa0,1,42}, 3, ((ccder_tag)0xa0 << ((sizeof(ccder_tag)-1)*8)), true,true, 2,2,1},
};

static ptrdiff_t position(const unsigned char *p, const struct der_fixture *c)
{
    return p ? p - c->data : -1;
}


static void fixture_der(void)
{
    for (size_t i = 0; i < sizeof(der_cases)/sizeof(der_cases[0]); i++) {
        const struct der_fixture *c = &der_cases[i];
        for (unsigned mode = 0; mode < 6; mode++) {
            bool strict = mode & 1, alias = mode >= 4;
            bool expected_ok = strict ? c->strict_ok : c->loose_ok;
            size_t cursor = strict ? c->strict_cursor : c->loose_cursor;
            ccder_read_blob from = {c->data, c->data + c->n};
            ccder_read_blob range = {c->data, c->data};
            size_t length = 99;
            bool ok, passed;
            if (mode < 2) {
                ok = strict ? ccder_blob_decode_tl_strict(&from, c->tag, &length)
                            : ccder_blob_decode_tl(&from, c->tag, &length);
                passed = ok == expected_ok && position(from.der,c) == (ptrdiff_t)cursor
                      && from.der_end == c->data + c->n && length == (ok ? c->length : 0);
            } else {
                ccder_read_blob *out = alias ? &from : &range;
                ok = strict ? ccder_blob_decode_range_strict(&from, c->tag, out)
                            : ccder_blob_decode_range(&from, c->tag, out);
                passed = ok == expected_ok
                      && position(out->der,c) == (ok ? (ptrdiff_t)cursor : -1)
                      && position(out->der_end,c) == (ok ? (ptrdiff_t)(cursor+c->length) : -1);
                if (!alias) {
                    passed = passed && position(from.der,c) == (ptrdiff_t)(cursor + (ok ? c->length : 0))
                                    && from.der_end == c->data + c->n;
                }
                length = ok ? c->length : 0;
            }
            if (!passed) unixnu_public_crypto_fixture_result[4]++;
            unixnu_public_crypto_fixture_result[3]++;
            unixnu_public_crypto_fixture_result[ok ? 8 : 9]++;
        }
    }
}

__attribute__((noinline,used)) void unixnu_public_crypto_fixture_checkpoint(void)
{
    __asm__ __volatile__("" ::: "memory");
}
int unixnu_public_crypto_fixture_run(int negative)
{
    for (size_t i=1;i<12;i++) unixnu_public_crypto_fixture_result[i]=0;
    unixnu_public_crypto_fixture_result[1]=1;
    fixture_digest(negative);
    fixture_der();
    unixnu_public_crypto_fixture_result[1]=unixnu_public_crypto_fixture_result[4] ? 3:2;
    kprintf("PUBLIC_CRYPTO_FIXTURE: digest=%u der=%u failures=%u guards=%u clears=%u bridge=%u\n",
      unixnu_public_crypto_fixture_result[2],unixnu_public_crypto_fixture_result[3],unixnu_public_crypto_fixture_result[4],
      unixnu_public_crypto_fixture_result[5],unixnu_public_crypto_fixture_result[6],unixnu_public_crypto_fixture_result[7]);
    return (int)unixnu_public_crypto_fixture_result[4];
}
