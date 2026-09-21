#include "asm.h"
#include "../libpcsxcore/cdrom_bits.h"

#ifndef NULL
#define NULL (void *)0
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

int main()
{
    struct tstate st = { (u8 *)0x1f800000, 0, };
    register u32 ra asm("ra");
    u8 rirq, buf[16];
    int have_cd;
    int len;

    printf("started, ra=%x\n", ra);
    printf("irq stat/mask %08x/%08x\n", HW16(st.hw, 0x1070), HW16(st.hw, 0x1074));
    HW32(st.hw, 0x1074) = 0;
    HW16(st.hw, 0x1124) = 0x200; // start counter2, clk/8

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
