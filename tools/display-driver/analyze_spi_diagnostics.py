#!/usr/bin/env python3
"""Summarize cumulative spi-imx diagnostic logs; no target/device access."""
import argparse
import json
from pathlib import Path
import re


def parse(text):
    samples = {}
    for line in text.splitlines():
        match = re.search(r'\b(spi-diag(?:-detail)?)\s+(.*)', line)
        if not match:
            continue
        fields = {key: int(value) for key, value in
                  re.findall(r'\b([a-z][a-z0-9_]*)=(\d+)\b', match[2])}
        required = {'bus', 'cs', 'epoch', 'wall_ms'}
        if not required <= fields.keys():
            continue
        key = tuple(fields[name] for name in ('bus', 'cs', 'epoch', 'wall_ms'))
        samples.setdefault(key, {})[match[1]] = fields
    latest = {}
    for key, records in samples.items():
        if {'spi-diag', 'spi-diag-detail'} <= records.keys():
            # Last complete pair in file order, not highest epoch (it can wrap).
            latest[key[:3]] = {**records['spi-diag'], **records['spi-diag-detail']}
    return list(latest.values())


def summarize(raw):
    def ratio(numerator, denominator, scale=1):
        return round(numerator / denominator * scale, 4) if denominator else None

    return {
        'bus': raw['bus'], 'cs': raw['cs'], 'epoch': raw['epoch'],
        'measurement': 'driver_software_timing_not_panel_fps',
        'raw': raw,
        'payload_transfer_mean_ms': ratio(raw['payload_us'], raw['payload_n'], 0.001),
        'payload_effective_MB_s': ratio(raw['payload_bytes'], raw['payload_us']),
        'ideal_wire_time_fraction': ratio(raw['ideal_payload_us'], raw['payload_us']),
        'dma_prepare_mean_ms': ratio(raw['dma_prepare_us'], raw['dma_n'], 0.001),
        'dma_tx_wait_mean_ms': ratio(raw['tx_wait_us'], raw['dma_n'], 0.001),
        'dma_rx_wait_mean_ms': ratio(raw['rx_wait_us'], raw['dma_n'], 0.001),
        'consecutive_payload_gap_max_ms': round(raw['payload_gap_max_us'] / 1000, 4),
        'full_frame_payload_time_estimate_ms': ratio(153600, raw['payload_bytes'], raw['payload_us'] / 1000),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path, help='dmesg capture containing complete two-line reports')
    args = parser.parse_args()
    records = parse(args.log.read_text())
    if not records:
        parser.error('no complete spi-diag / spi-diag-detail report pairs found')
    try:
        for record in records:
            print(json.dumps(summarize(record), sort_keys=True))
    except KeyError as error:
        parser.error(f'incomplete/unsupported diagnostic fields: {error}')


if __name__ == '__main__':
    main()
