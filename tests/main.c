#include "asm.h"
#include "../libpcsxcore/cdrom_bits.h"

#ifndef NULL
#define NULL (void *)0
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define u8      unsigned char
#define u16     unsigned short
#define u32     unsigned int
#define s16     signed short

#define HW8(v,  ofs) *((volatile u8  *)&v[ofs])
#define HW16(v, ofs) *((volatile u16 *)&v[ofs])
#define HW32(v, ofs) *((volatile u32 *)&v[ofs])

#define CDHC 8
#define CDHM (CDHC - 1)

struct tstate
{
    u8 *hw;
    u32 tt;     // tests total
    u32 tp;     // tests passed
    u32 cnt0w;  // counter0 (clk) updated during wait
    u32 cnt2w;  // counter2 (clk/8) same
    u16 tf_grp; // failed tests in this group
    u8 cd_cmd;  // last cd cmd
    u8 cd_hist_pos;
    struct {
      u8 cmd, irq, stat, unused;
      u32 cycles;  // from cmd issue, in cpu/system cycles
      u32 gcycles; // global counter
    } cd_hist[CDHC];
};

void delay(struct tstate *st, int loops)
{
    u8 *hw = st->hw;
    u16 cnt0 = st->cnt0w;
    u16 cnt2 = st->cnt2w >> 3;
    u32 cnt0u = 0, cnt2u = 0;
    while (loops--) {
        // could use bit12 of stat reg, but that would race against cnt reg,
        // also as long as the counter is at 0xffff the stat bit12 is set again
        u16 cnt0n = HW16(hw, 0x1100);
        u16 cnt2n = HW16(hw, 0x1120);
        cnt0u += (cnt0n < cnt0);
        cnt2u += (cnt2n < cnt2);
        cnt0 = cnt0n;
        cnt2 = cnt2n;
        asm volatile("nop;nop;nop;nop");
    }
    st->cnt0w = ((st->cnt0w & 0xffff0000) + (cnt0u << 16)) | cnt0;
    st->cnt2w = ((st->cnt2w & 0xfff80000) + (cnt2u << 19)) | ((u32)cnt2 << 3);
}

struct cd_cprm
{
    u8 l;
    u8 p[16];
};

static struct cd_cprm prm0(void) { struct cd_cprm p; p.l = 0; return p; }
static struct cd_cprm prm3(u8 a0, u8 a1, u8 a2) {
    struct cd_cprm p; p.l = 3; p.p[0] = a0; p.p[1] = a1; p.p[2] = a2;
    return p;
}

static void cdr_w_cmd(struct tstate *st, u8 cmd, const struct cd_cprm prm)
{
    u8 i;
    HW8(st->hw, 0x1800) = 0; // index 0
    for (i = 0; i < prm.l; i++)
        HW8(st->hw, 0x1802) = prm.p[i];
    st->cnt0w = 0;
    st->cd_cmd = cmd;
    HW16(st->hw, 0x1104) = 0; // start counter0, sys/cpu clk
    HW8(st->hw, 0x1801) = cmd;
}

static void cdr_w_irqf(u8 *hw, u8 val)
{
    HW8(hw, 0x1800) = 1;
    HW8(hw, 0x1803) = val;
}

static u8 cdr_r_irqf(u8 *hw)
{
    HW8(hw, 0x1800) = 0;
    return HW8(hw, 0x1803);
}

static u8 cdr_poll_irq(struct tstate *st, int do_ack, int log_timeout)
{
    u8 r, pos;
    int i;

    HW8(st->hw, 0x1800) = 1;
    for (i = 0x20000; i > 0; i--) {
        r = HW8(st->hw, 0x1803);
        delay(st, 1); // also updates counters
        if (r & 0x1f)
            break;
    }
    r = HW8(st->hw, 0x1803); // re-read
    // printf is too slow, so log
    pos = ++st->cd_hist_pos & CDHM;
    st->cd_hist[pos].cmd = st->cd_cmd;
    st->cd_hist[pos].irq = r;
    st->cd_hist[pos].stat = 0xff;
    st->cd_hist[pos].cycles = st->cnt0w;
    st->cd_hist[pos].gcycles = st->cnt2w;
    if (i == 0) {
        if (log_timeout)
            printf("cd irq timeout: %02x\n", r);
    }
    else if (do_ack)
        cdr_w_irqf(st->hw, 0x1f);
    return r;
}

static int cdr_read_response(struct tstate *st, u8 *r)
{
    int i;
    HW8(st->hw, 0x1800) = 1;
    for (i = 0; i < 16; i++) {
        if (!(HW8(st->hw, 0x1800) & (1 << 5)))
            break;
        r[i] = HW8(st->hw, 0x1801);
    }
    if (i == 16 && (HW8(st->hw, 0x1800) & (1 << 5)))
        printf("wtf1\n");
    if (i)
        st->cd_hist[st->cd_hist_pos & CDHM].stat = r[0];
    return i;
}

static int cdr_do_cmd(struct tstate *st, u8 cmd, const struct cd_cprm prm,
    u8 *rirq, u8 *rbuf)
{
    u8 r;
    HW8(st->hw, 0x1800) = 1;
    r = HW8(st->hw, 0x1803);
    if ((r & 0x1f) != 0) {
        printf("cd cmd %02x: pre irq %02x\n", cmd, r);
        HW8(st->hw, 0x1803) = 0x1f;
        return -1;
    }
    r = HW8(st->hw, 0x1800);
    if (!(r & 8) || (r & 0x80u)) {
        printf("cd cmd %02x: unexpected status %02x\n", cmd, r);
        return -1;
    }
    HW8(st->hw, 0x1800) = 0;
    cdr_w_cmd(st, cmd, prm);
    r = cdr_poll_irq(st, 1, 1);
    if (rirq)
        *rirq = r & 0x1f;
    else if ((r & 0x1f) != Acknowledge) {
        printf("cd cmd %02x: bad irq (%02x)\n", cmd, r);
        return -1;
    }
    return cdr_read_response(st, rbuf);
}

static int cdr_poll_read_2nd(struct tstate *st, u8 irq, u8 *rbuf)
{
    u8 r;
    r = cdr_poll_irq(st, 1, 0);
    if ((r & 0x1f) != irq) {
        printf("cd cmd %02x: bad irq (%02x)\n", st->cd_cmd, r);
        return -1;
    }
    return cdr_read_response(st, rbuf);
}

static void cdrom_dump_hist(struct tstate *st)
{
    int i, pos, count = st->cd_hist_pos + 1;
    if (count > CDHC)
        count = CDHC;
    pos = st->cd_hist_pos - (count - 1);
    for (i = 0; i < count; i++, pos++) {
        pos &= CDHM;
        printf("%9u: cmd %2d: irq %02x st %02x cyc %u\n",
               st->cd_hist[pos].gcycles,
               st->cd_hist[pos].cmd, st->cd_hist[pos].irq,
               st->cd_hist[pos].stat, st->cd_hist[pos].cycles);
    }
}

#define assert_eq(st_, expr_, val_) do { \
    u32 e2_ = expr_; \
    (st_)->tt++; \
    if (e2_ == (u32)(val_)) \
        (st_)->tp++; \
    else { \
        (st_)->tf_grp++; \
        printf("%s:%d: %x != %x\n", __FILE__, __LINE__, e2_, val_); \
    } \
} while (0)

static void test_cdrom_irqen(struct tstate *st)
{
    assert_eq(st, cdr_r_irqf(st->hw), 0xff);
}

static int test_cdrom_start_read(struct tstate *st)
{
    u16 tf_grp_old = st->tf_grp;
    u8 rx[16];
    int ret;
    st->tf_grp = 0;
    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlReadN, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    assert_eq(st, cdr_poll_read_2nd(st, DataReady, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | STATUS_ROTATING);
    ret = st->tf_grp ? -1 : 0;
    st->tf_grp += tf_grp_old;
    return ret;
}

static void test_cdrom_nocd(struct tstate *st, u8 shell_bit)
{
    u8 rirq, rx[16];
    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), &rirq, rx), 2);
    assert_eq(st, rirq, DiskError);
    assert_eq(st, rx[0], shell_bit | STATUS_ERROR);
    assert_eq(st, rx[1], ERROR_NOTREADY);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], shell_bit);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], 0);
}

static void test_cdrom_read_pause(struct tstate *st, u8 shell_bit)
{
    u8 rx[16];
    st->cd_hist_pos = 0xff;
    st->tf_grp = 0;
    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), NULL, rx), 1);
    assert_eq(st, rx[0], shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlReadN, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_poll_read_2nd(st, DataReady, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_poll_read_2nd(st, Complete, rx), 1);
    assert_eq(st, rx[0], shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], shell_bit | STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    if (st->tf_grp)
        cdrom_dump_hist(st);
}

static void test_cdrom_read_premature_pause(struct tstate *st)
{
    u8 rirq, rx[16];
    st->cd_hist_pos = 0xff;
    st->tf_grp = 0;
    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlReadN, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), &rirq, rx), 2);
    assert_eq(st, rirq, DiskError);
    assert_eq(st, rx[0] & ~STATUS_SEEK, STATUS_ROTATING | STATUS_ERROR); // todo: wrong seek
    assert_eq(st, rx[1], ERROR_NOTREADY);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0] & ~STATUS_SEEK, STATUS_ROTATING); // todo: seek, error clearing
    assert_eq(st, cdr_poll_read_2nd(st, DataReady, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | STATUS_ROTATING);
    // now pause properly
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | STATUS_ROTATING);
    assert_eq(st, cdr_poll_read_2nd(st, Complete, rx), 1);
    assert_eq(st, rx[0], STATUS_ROTATING);
    if (st->tf_grp)
        cdrom_dump_hist(st);
}

// commands while pause is pending
static void test_cdrom_pause_resp2_cancel(struct tstate *st)
{
    u8 rirq, rx[16];
    st->cd_hist_pos = 0xff;
    st->tf_grp = 0;

    // invalid command
    if (test_cdrom_start_read(st) != 0) return;
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), NULL, rx), 1);
    assert_eq(st, rx[0], STATUS_READ | STATUS_ROTATING);
    assert_eq(st, cdr_do_cmd(st, 0, prm0(), &rirq, rx), 2);
    assert_eq(st, rirq, DiskError);
    assert_eq(st, rx[0] & STATUS_ERROR, STATUS_ERROR);
    assert_eq(st, rx[1], ERROR_INVALIDCMD);
    assert_eq(st, cdr_poll_irq(st, 1, 0) & 0x1f, 0);

    // Nop/Getstat
    if (test_cdrom_start_read(st) != 0) return;
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), NULL, rx), 1);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    // depending on what cd-rom is doing, we may be able to see READ bit or not
    if (st->cd_hist[st->cd_hist_pos & CDHM].cycles < 100000) // 100k?
        assert_eq(st, rx[0], STATUS_READ | STATUS_ROTATING);
    else
        assert_eq(st, rx[0], STATUS_ROTATING);
    assert_eq(st, cdr_poll_irq(st, 1, 0) & 0x1f, 0);

    // Setloc
    if (test_cdrom_start_read(st) != 0) return;
    assert_eq(st, cdr_do_cmd(st, CdlPause, prm0(), NULL, rx), 1);
    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), NULL, rx), 1);
    assert_eq(st, cdr_poll_irq(st, 1, 0) & 0x1f, 0);
    if (st->tf_grp)
        cdrom_dump_hist(st);
}

static void test_cdrom_seek_resp2_cancel(struct tstate *st)
{
    u8 rx[16];
    st->cd_hist_pos = 0xff;
    st->tf_grp = 0;

    assert_eq(st, cdr_do_cmd(st, CdlSetloc, prm3(0, 2, 0), NULL, rx), 1);
    assert_eq(st, cdr_do_cmd(st, CdlSeekL, prm0(), NULL, rx), 1);
    assert_eq(st, cdr_do_cmd(st, CdlNop, prm0(), NULL, rx), 1);
    //assert_eq(st, rx[0], STATUS_ROTATING); // todo?
    assert_eq(st, cdr_poll_irq(st, 1, 0) & 0x1f, 0);
    if (st->tf_grp)
        cdrom_dump_hist(st);
}

#define ctc2(v, r) asm volatile("ctc2 %0, $" #r :: "r"(v))
#define mtc2(v, r) asm volatile("mtc2 %0, $" #r :: "r"(v))

struct gte_rtps_case
{
    u32 vxy0, vz0;
    u32 r[5];         // R11R12 R13R21 R22R23 R31R32 R33
    u32 tr[3];
    u32 ofx, ofy;
    u32 h, dqa, dqb;
    u32 sz[3];        // SZ1-3 before
    u32 sxy[2];       // SXY1-2 before
    u32 res[2][17];   // gte_rtps_sf1lm0*() output, with and without NCLIP
};

static const struct gte_rtps_case gte_rtps_cases[] = {
    // no flags
    { 0xffce0064, 1000, { 0x1000, 0, 0x1000, 0, 0x1000 }, { 10, 20, 3000 },
      160 << 16, 120 << 16, 200, 0xff9c, 0x100000,
      { 1, 2, 3 }, { 0x00100020, 0x00300040 },
      {{
        0x000000af, 0x0000006e, 0xffffffe2, 0x00000fa0, 0x00100020, 0x00300040,
        0x007600a5, 0x007600a5, 0x00000001, 0x00000002, 0x00000003, 0x00000fa0,
        0x000affec, 0x0000006e, 0xffffffe2, 0x00000fa0, 0x00000000
      }, {
        0x000000af, 0x0000006e, 0xffffffe2, 0x00000fa0, 0x00100020, 0x00300040,
        0x007600a5, 0x007600a5, 0x00000001, 0x00000002, 0x00000003, 0x00000fa0,
        0xfffffc20, 0x0000006e, 0xffffffe2, 0x00000fa0, 0x00000000
      }}},
    // non-trivial rotation, divider lut
    { 0x0123fedc, 0x0555, { 0x0e3a0521, 0xfb07fc80, 0x0cf1093e, 0x0a0bf6aa, 0x0bc1 },
      { -300, 777, 1234 }, 0x012345, -0x6789a, 0x155, 0x1234, 0x10000,
      { 0xffff, 0x8000, 0x7fff }, { 0xffff8000, 0x7fff0001 },
      {{
        0x00001000, 0xfffffe4e, 0x0000085b, 0x00000a1d, 0xffff8000, 0x7fff0001,
        0x0113ffc7, 0x0113ffc7, 0x0000ffff, 0x00008000, 0x00007fff, 0x00000a1d,
        0x0266c960, 0xfffffe4e, 0x0000085b, 0x00000a1d, 0x00001000
      }, {
        0x00001000, 0xfffffe4e, 0x0000085b, 0x00000a1d, 0xffff8000, 0x7fff0001,
        0x0113ffc7, 0x0113ffc7, 0x0000ffff, 0x00008000, 0x00007fff, 0x00000a1d,
        0xc0a68114, 0xfffffe4e, 0x0000085b, 0x00000a1d, 0x00000000
      }}},
    // IR1+/IR2- sat, SZ3<0, H>=SZ3*2, SX/SY sat, MAC0+ overflow, IR0 sat
    { 0x80017fff, 0x7fff, { 0x00007fff, 0x00000000, 0x00007fff, 0x7fff7fff, 0x7fff },
      { 0, 0, -0x100000 }, 0, 0, 1, 0x7fff, 0x7fffffff,
      { 0, 0, 0 }, { 0, 0 },
      {{
        0x00001000, 0x00007fff, 0xffff8000, 0xffff8000, 0x00000000, 0x00000000,
        0xfc0003ff, 0xfc0003ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x7ffd8000, 0x0003fff0, 0xfffc000f, 0xfff3fff0, 0x81c7f000
      }, {
        0x00001000, 0x00007fff, 0xffff8000, 0xffff8000, 0x00000000, 0x00000000,
        0xfc0003ff, 0xfc0003ff, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x0003fff0, 0xfffc000f, 0xfff3fff0, 0x00000000
      }}},
    // SZ3>0xffff, IR3 sat, MAC0- overflow, IR0<0
    { 0x00000000, 0x7fff, { 0, 0, 0, 0, 0x7fff },
      { -5, 5, 0x100000 }, -1, 1, 0xffff, 0x8000, -0x40000000,
      { 7, 8, 9 }, { 1, 2 },
      {{
        0x00000000, 0xfffffffb, 0x00000005, 0x00007fff, 0x00000001, 0x00000002,
        0x0004fffb, 0x0004fffb, 0x00000007, 0x00000008, 0x00000009, 0x0000ffff,
        0x40008000, 0xfffffffb, 0x00000005, 0x0013fff0, 0x80449000
      }, {
        0x00000000, 0xfffffffb, 0x00000005, 0x00007fff, 0x00000001, 0x00000002,
        0x0004fffb, 0x0004fffb, 0x00000007, 0x00000008, 0x00000009, 0x0000ffff,
        0x00000004, 0xfffffffb, 0x00000005, 0x0013fff0, 0x00000000
      }}},
    // H just below SZ3*2, SX/SY-
    { 0x00000000, 0x0000, { 0, 0, 0, 0, 0 },
      { -0x7fff, -0x7fff, 0x8000 }, -0x2000000, 0x1000000, 0xffff, 0x0100, 0,
      { 0, 0, 0 }, { 0, 0 },
      {{
        0x00001000, 0xffff8001, 0xffff8001, 0x00007fff, 0x00000000, 0x00000000,
        0xfc00fc00, 0xfc00fc00, 0x00000000, 0x00000000, 0x00000000, 0x00008000,
        0x01fffe00, 0xffff8001, 0xffff8001, 0x00008000, 0x8040f000
      }, {
        0x00001000, 0xffff8001, 0xffff8001, 0x00007fff, 0x00000000, 0x00000000,
        0xfc00fc00, 0xfc00fc00, 0x00000000, 0x00000000, 0x00000000, 0x00008000,
        0x00000000, 0xffff8001, 0xffff8001, 0x00008000, 0x00000000
      }}},
};

static void gte_rtps_setup(const struct gte_rtps_case *c)
{
    ctc2(c->r[0], 0); ctc2(c->r[1], 1); ctc2(c->r[2], 2);
    ctc2(c->r[3], 3); ctc2(c->r[4], 4);
    ctc2(c->tr[0], 5); ctc2(c->tr[1], 6); ctc2(c->tr[2], 7);
    ctc2(c->ofx, 24); ctc2(c->ofy, 25); ctc2(c->h, 26);
    ctc2(c->dqa, 27); ctc2(c->dqb, 28);
    mtc2(c->vxy0, 0); mtc2(c->vz0, 1);
    mtc2(c->sz[0], 17); mtc2(c->sz[1], 18); mtc2(c->sz[2], 19);
    mtc2(c->sxy[0], 13); mtc2(c->sxy[1], 14);
}

static void test_gte_rtps(struct tstate *st)
{
    static const char * const names[17] = {
        "IR0", "IR1", "IR2", "IR3", "SXY0", "SXY1", "SXY2", "SXYP",
        "SZ0", "SZ1", "SZ2", "SZ3", "MAC0", "MAC1", "MAC2", "MAC3", "FLAG"
    };
    u32 i, j, v, out[17];

    for (i = 0; i < sizeof(gte_rtps_cases) / sizeof(gte_rtps_cases[0]); i++) {
        const struct gte_rtps_case *c = &gte_rtps_cases[i];
        for (v = 0; v < 2; v++) {
            gte_rtps_setup(c);
            if (v == 0)
                gte_rtps_sf1lm0(out);
            else
                gte_rtps_sf1lm0_nclip(out);
            st->tt++;
            for (j = 0; j < 17; j++)
                if (out[j] != c->res[v][j])
                    break;
            if (j == 17) {
                st->tp++;
                continue;
            }
            printf("gte rtps case %d/%d:", i, v);
            for (j = 0; j < 17; j++)
                if (out[j] != c->res[v][j])
                    printf(" %s %x!=%x", names[j], out[j], c->res[v][j]);
            printf("\n");
        }
    }
}

static const struct {
    const char *name;
    int (*test)(int *array_012); // returns 1 if failed
} func_tests[] = {
    { "cpu_delay0",   cpu_delay0 },
    { "cpu_delay1",   cpu_delay1 },
    { "cpu_delay_j0", cpu_delay_j0 },
    { "cpu_delay_j1", cpu_delay_j1 },
};

int main()
{
    struct tstate st = { (u8 *)0x1f800000, 0, };
    int array_012[] = { 0, 1, 2 };
    register u32 ra asm("ra");
    u8 rirq, buf[16];
    int have_cd;
    int len;
    u32 i;

    printf("started, ra=%x\n", ra);
    printf("irq stat/mask %08x/%08x\n", HW16(st.hw, 0x1070), HW16(st.hw, 0x1074));
    HW32(st.hw, 0x1074) = 0;
    HW16(st.hw, 0x1124) = 0x200; // start counter2, clk/8

    // enable cop2
    asm volatile("mfc0 %0, $12; nop; or %0, %1; mtc0 %0, $12; nop"
        : "=&r"(len) : "r"(1u << 30));
    test_gte_rtps(&st);

    // func_tests
    for (i = 0; i < ARRAY_SIZE(func_tests); i++) {
        int ret = func_tests[i].test(array_012);
        if (ret)
            printf("%s:%d: %s: %d\n", __FILE__, __LINE__, func_tests[i].name, ret);
        else
            st.tp++;
        st.tt++;
    }

    // cdrom
    test_cdrom_irqen(&st);
    cdr_w_irqf(st.hw, 0x5f); // ack irq, clear param fifo
    // query with an invalid cmd to not lose the valuable STATUS_SHELLOPEN bit
    len = cdr_do_cmd(&st, 0, prm0(), &rirq, buf);
    have_cd = len == 2 && (buf[0] & STATUS_ROTATING);
    printf("cd: %s (%02x)\n", have_cd ? "yes" : "no", buf[0]);
    if (have_cd) {
        test_cdrom_read_pause(&st, buf[0] & STATUS_SHELLOPEN);
        test_cdrom_read_premature_pause(&st);
        test_cdrom_pause_resp2_cancel(&st);
        test_cdrom_seek_resp2_cancel(&st);
    }
    else {
        test_cdrom_nocd(&st, buf[0] & STATUS_SHELLOPEN);
    }

    printf("%s (%d/%d)\n", st.tt == st.tp ? "pass" : "fail", st.tp, st.tt);
    return st.tt == st.tp ? 0 : 1;
}

// vim:ts=4:sw=4:expandtab
