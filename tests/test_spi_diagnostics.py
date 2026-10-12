#!/usr/bin/env python3
"""Parser tests and optional patch/mock-C tests. Never load a kernel module.

SPI_IMX_SOURCE=/path/to/reference/spi-imx.c enables patch application and C smoke
checks against the pinned source. Mocks are NOT a kernel build or hardware test.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('spi_analysis', ROOT/'tools/display-driver/analyze_spi_diagnostics.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)
REFERENCE_SHA256 = '9a4d63562483b007bfff353bd58fc048bfae0a238cf54be38c2accc9e50739a3'


def sample(epoch=0, wall=1000):
    raw = dict(bus=2, cs=0, epoch=epoch, wall_ms=wall, payload_us=100000,
               payload_n=5, payload_bytes=153600, ideal_payload_us=25600,
               dma_prepare_us=1000, tx_wait_us=90000, rx_wait_us=1000,
               dma_n=5, payload_gap_max_us=2000)
    detail = dict(bus=2, cs=0, epoch=epoch, wall_ms=wall, clk_min=40000000)
    return ('[1.0] spi_imx test: spi-diag ' + ' '.join(f'{k}={v}' for k, v in raw.items()) + '\n' +
            '[1.0] spi_imx test: spi-diag-detail ' + ' '.join(f'{k}={v}' for k, v in detail.items()) + '\n')


class ParserTests(unittest.TestCase):
    def test_latest_complete_pair_per_epoch(self):
        text = sample(wall=1000) + sample(wall=2000) + sample(epoch=1)
        text += '[3.0] spi-diag bus=2 cs=0 epoch=1 wall_ms=3000 n=99\n'
        records = analysis.parse(text)
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0]['wall_ms'], 2000)
        self.assertEqual(records[1]['wall_ms'], 1000)

    def test_math(self):
        report = analysis.summarize(analysis.parse(sample())[0])
        self.assertEqual(report['payload_transfer_mean_ms'], 20)
        self.assertEqual(report['payload_effective_MB_s'], 1.536)
        self.assertEqual(report['ideal_wire_time_fraction'], 0.256)
        self.assertEqual(report['full_frame_payload_time_estimate_ms'], 100)
        self.assertEqual(report['dma_tx_wait_mean_ms'], 18)

    def test_empty_and_cli_failure(self):
        self.assertEqual(analysis.parse('ordinary dmesg line'), [])
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp)/'empty.log'; log.write_text('no diagnostic samples')
            result = subprocess.run(['python3', str(ROOT/'tools/display-driver/analyze_spi_diagnostics.py'), str(log)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('no complete', result.stderr)


@unittest.skipUnless(os.environ.get('SPI_IMX_SOURCE'), 'set SPI_IMX_SOURCE for reference patch/C smoke checks')
class PatchTests(unittest.TestCase):
    def test_patch_and_mocked_driver(self):
        source = Path(os.environ['SPI_IMX_SOURCE']).read_bytes()
        self.assertEqual(hashlib.sha256(source).hexdigest(), REFERENCE_SHA256)
        with tempfile.TemporaryDirectory() as temp:
            tree = Path(temp); target = tree/'drivers/spi/spi-imx.c'
            target.parent.mkdir(parents=True); target.write_bytes(source)
            patch = ROOT/'linux/files/spi-imx-display-diagnostics.patch'
            subprocess.run(['git', 'apply', '--check', str(patch)], cwd=tree, check=True)
            subprocess.run(['git', 'apply', str(patch)], cwd=tree, check=True)
            modified = target.read_text()
            struct = re.search(r'struct spi_imx_diag \{.*?\n\};', modified, re.S).group()

            def function(name):
                start = re.search(r'^static [^\n]*\b' + name + r'\(', modified, re.M).start()
                brace = modified.index('{', start)
                depth = 1; end = brace + 1
                while depth:
                    depth += (modified[end] == '{') - (modified[end] == '}')
                    end += 1
                return modified[start:end]

            functions = '\n'.join(function(name) for name in (
                'spi_imx_diag_selected', 'spi_imx_diag_start', 'spi_imx_diag_report',
                'spi_imx_dma_transfer', 'spi_imx_transfer'))
            harness = tree/'smoke.c'
            harness.write_text(MOCK_PREFIX + '\n' + struct + '\n' + MOCK_TYPES + '\n' + functions + '\n' + MOCK_MAIN)
            program = tree/'smoke'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-parameter', '-fsanitize=address,undefined',
                            '-g', str(harness), '-o', str(program)], check=True)
            result = subprocess.run([str(program)], check=True, capture_output=True, text=True)
            records = analysis.parse(result.stdout)
            self.assertTrue(records)
            report = analysis.summarize(records[0])
            self.assertEqual(report['raw']['clk_min'], 40000000)
            self.assertEqual(report['raw']['payload_pio_n'], 0)
            self.assertGreater(report['dma_tx_wait_mean_ms'], 0)
            json.dumps(report)


MOCK_PREFIX = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
typedef unsigned long long u64;
static int diag_bus = 2, diag_cs = 0;
static unsigned int diag_epoch;
static u64 now_ns = 1000000000;
#define READ_ONCE(x) (x)
#define NSEC_PER_SEC 1000000000ULL
#define NSEC_PER_MSEC 1000000ULL
#define max(a,b) ((a) > (b) ? (a) : (b))
#define div_u64(a,b) ((a) / (b))
#define dev_info(dev, ...) printf(__VA_ARGS__)
#define dev_err(dev, ...) fprintf(stderr, __VA_ARGS__)
#define MX51_ECSPI_CTRL 8
#define MX51_ECSPI_CTRL_BL_OFFSET 20
#define DMA_DEV_TO_MEM 0
#define DMA_MEM_TO_DEV 1
#define DMA_PREP_INTERRUPT 1
#define DMA_CTRL_ACK 2
static u64 ktime_get_ns(void) { return now_ns; }
'''
MOCK_TYPES = r'''
struct completion { int kind; };
struct scatterlist { unsigned int len; };
struct sg_table { struct scatterlist *sgl; unsigned int nents; };
struct dma_async_tx_descriptor { void (*callback)(void *); void *callback_param; };
struct spi_imx_data;
struct device { int unused; };
struct spi_master { int bus_num; void *dma_rx, *dma_tx; struct spi_imx_data *data; struct device dev; };
struct spi_device { struct spi_master *master; unsigned int chip_select; struct device dev; };
struct spi_transfer { unsigned int len, bits_per_word; struct sg_table tx_sg, rx_sg; };
struct devtype { unsigned int fifo_size; int (*rx_available)(struct spi_imx_data *);
    void (*setup_wml)(struct spi_imx_data *), (*trigger)(struct spi_imx_data *), (*reset)(struct spi_imx_data *); };
struct spi_imx_data { struct spi_imx_diag diag; bool diag_active, slave_mode, usedma;
    struct device *dev; unsigned long spi_clk;
    unsigned int speed_hz, spi_bus_clk, bits_per_word, wml;
    struct completion dma_rx_completion, dma_tx_completion;
    struct { struct spi_master *master; } bitbang;
    struct devtype *devtype_data; void (*rx)(struct spi_imx_data *); void *base; };
static struct spi_imx_data *spi_master_get_devdata(struct spi_master *m) { return m->data; }
static int is_imx51_ecspi(struct spi_imx_data *d) { return 1; }
static int is_imx53_ecspi(struct spi_imx_data *d) { return 0; }
static unsigned int readl(void *p) { return 7 << MX51_ECSPI_CTRL_BL_OFFSET; }
static int spi_imx_bytes_per_word(unsigned int bits) { return (bits + 7) / 8; }
static struct scatterlist *sg_last(struct scatterlist *sg, unsigned int n) { return sg + n - 1; }
static unsigned int sg_dma_len(struct scatterlist *sg) { return sg->len; }
static int spi_imx_dma_configure(struct spi_master *m) { now_ns += 1000000; return 0; }
static unsigned long spi_imx_calculate_timeout(struct spi_imx_data *d, unsigned int len) { return 100; }
static void spi_imx_dma_rx_callback(void *p) {}
static void spi_imx_dma_tx_callback(void *p) {}
static struct dma_async_tx_descriptor *dmaengine_prep_slave_sg(void *c,
    struct scatterlist *sg, unsigned int n, int dir, int flags) {
    static struct dma_async_tx_descriptor desc; now_ns += 500000; return &desc;
}
static void dmaengine_submit(struct dma_async_tx_descriptor *d) {}
static void reinit_completion(struct completion *c) {}
static void dma_async_issue_pending(void *c) { now_ns += 100000; }
static void dmaengine_terminate_all(void *c) {}
static int timeout_tx;
static unsigned long wait_for_completion_timeout(struct completion *c, unsigned long timeout) {
    now_ns += c->kind == 1 ? 20000000 : 4000000;
    return c->kind == 1 && timeout_tx ? 0 : 1;
}
static int spi_imx_pio_transfer(struct spi_device *s, struct spi_transfer *t) { now_ns += 100000000; return t->len; }
static int spi_imx_pio_transfer_slave(struct spi_device *s, struct spi_transfer *t) { return spi_imx_pio_transfer(s,t); }
static int empty(struct spi_imx_data *d) { return 0; }
static void trigger(struct spi_imx_data *d) { now_ns += 100000; }
static void noop(struct spi_imx_data *d) {}
'''
MOCK_MAIN = r'''
int main(void) {
    struct devtype type = {64, empty, noop, trigger, noop};
    struct spi_imx_data d = { .usedma = true, .devtype_data = &type,
        .spi_clk = 80000000, .speed_hz = 48000000, .spi_bus_clk = 40000000,
        .bits_per_word = 8, .dma_tx_completion = {1}, .dma_rx_completion = {2} };
    struct spi_master m = {.bus_num = 2, .data = &d, .dma_tx = &d, .dma_rx = &d};
    struct spi_device s = {.master = &m, .chip_select = 0};
    struct scatterlist sg = {.len = 32768};
    struct spi_transfer t = {.len = 32768, .bits_per_word = 8,
        .tx_sg = {&sg, 1}, .rx_sg = {&sg, 1}};
    d.bitbang.master = &m;
    for (int i = 0; i < 60; ++i) {
        now_ns += 1000000;
        assert(spi_imx_transfer(&s, &t) == 32768);
    }
    assert(d.diag.transfers == 60 && d.diag.dma_n == 60 && d.diag.payload_dma_n == 60);
    assert(d.diag.errors == 0 && d.diag.wml_min == 32 && d.diag.tx_sg_max == 1);
    assert(d.diag.tx_wait_ns == 60ULL * 20000000 && d.diag.rx_wait_ns == 60ULL * 4000000);
    assert(d.diag.payload_gap_ns == 59ULL * 1000000 && d.diag.last_burst_bits == 8);
    d.usedma = false; t.len = 1;
    assert(spi_imx_transfer(&s, &t) == 1);
    assert(d.diag.payload_n == 60 && d.diag.pio_n == 1);
    t.len = 32768;
    assert(spi_imx_transfer(&s, &t) == 32768);
    assert(d.diag.payload_pio_n == 1);
    diag_bus = -1;
    assert(spi_imx_transfer(&s, &t) == 32768);
    assert(d.diag.transfers == 62);
    diag_bus = 2; diag_epoch = 7;
    assert(spi_imx_transfer(&s, &t) == 32768);
    assert(d.diag.transfers == 1 && d.diag.payload_pio_n == 1 && d.diag.epoch == 7);
    d.usedma = true; timeout_tx = 1;
    assert(spi_imx_transfer(&s, &t) == -ETIMEDOUT);
    assert(d.diag.errors == 1 && d.diag.transfers == 2);
    return 0;
}
'''

if __name__ == '__main__':
    unittest.main()
