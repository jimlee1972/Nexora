"""Run the native quick benchmark and validate its machine-readable evidence contract."""
import json
import math
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    reuse = len(sys.argv) == 3 and sys.argv[2] == '--reuse'
    flag = '--benchmark-reuse' if reuse else '--benchmark'
    result = subprocess.run([sys.argv[1], flag, '3', '2'],
                            text=True, capture_output=True, timeout=110)
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode == 77:
        return 77
    require(result.returncode == 0, 'native benchmark/output oracle failed')
    rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    require(len(rows) == 2, 'benchmark must report exactly two modes')
    require([r['mode'] for r in rows] == ['serial', 'parallel'], 'benchmark mode order')
    for row in rows:
        require(row['recording_resource_reuse_enabled'] == reuse, 'reuse must be explicit')
        require(0 <= row['cached_command_pools'] <= 8, 'bounded allocator cache')
        if reuse:
            require(row['backend'] == 'Vulkan' and row['command_pool_creations'] == 0 and
                    row['command_pool_reuses'] == 12 and row['cached_command_pools'] == 4,
                    'all measured warmed lists must use exclusively retired allocators')
        else:
            require(row['command_pool_reuses'] == 0 and row['cached_command_pools'] == 0,
                    'default benchmark must not reuse allocators')
            if row['backend'] == 'Vulkan':
                require(row['command_pool_creations'] == 12, 'default native pool allocation count')
        require(row['schema'] == 'nexora.recording-benchmark.v1', 'unknown report schema')
        require(row['samples'] == 3 and row['warmup_pairs'] == 2, 'sample contract')
        require(row['lists_per_sample'] == 4 and row['dispatches_per_list'] == 2,
                'workload contract')
        require(row['oracle_comparisons'] == 24, 'each measured output must be verified')
        require(row['workers'] == (1 if row['mode'] == 'serial' else 4), 'worker contract')
        require(row['physical_hardware_verified'] is False and
                row['physical_acceptance'] is False and row['target_budget'] is None,
                'CI benchmark cannot certify physical performance')
        for field in ('cpu_setup_median_ms', 'cpu_record_schedule_join_median_ms',
                      'cpu_setup_record_median_ms', 'cpu_setup_record_p95_ms',
                      'cpu_submit_wait_median_ms', 'cpu_submit_wait_p95_ms'):
            require(isinstance(row[field], (int, float)) and math.isfinite(row[field]) and
                    row[field] >= 0, f'invalid CPU metric {field}')
        for prefix in ('cpu_setup_record', 'cpu_submit_wait'):
            require(row[prefix + '_median_ms'] <= row[prefix + '_p95_ms'], 'percentile order')
        require(0 <= row['gpu_samples'] <= row['samples'], 'GPU sample count')
        if row['gpu_samples'] == 0:
            require(row['gpu_sum_median_ms'] is None and row['gpu_sum_p95_ms'] is None and
                    row['gpu_source'] == 0, 'unavailable GPU timing must stay explicit')
        else:
            require(row['gpu_source'] in (1, 2, 3), 'GPU timing must identify a native source')
            require(all(isinstance(row[k], (float, int)) and math.isfinite(row[k]) and row[k] >= 0
                        for k in ('gpu_sum_median_ms', 'gpu_sum_p95_ms')), 'invalid GPU interval')
            require(row['gpu_sum_median_ms'] <= row['gpu_sum_p95_ms'], 'GPU percentile order')
        baseline, maximum = row['rss_after_warmup_bytes'], row['max_observed_rss_bytes']
        require(all(value is None or (isinstance(value, int) and value > 0)
                    for value in (baseline, maximum)), 'RSS observation contract')
        if baseline is not None:
            require(maximum is not None and maximum >= baseline, 'RSS maximum contract')
    for field in ('backend', 'vendor_id', 'device_id', 'driver_version', 'compiler',
                  'compiler_version', 'software_device_flag', 'samples', 'dispatches_per_list'):
        require(rows[0][field] == rows[1][field], f'modes must share {field}')
    # Invalid CLI sizes must reject before allocating the benchmark workload.
    for args in (('0', '2'), ('102', '2'), ('3', '65'), ('3x', '2')):
        invalid = subprocess.run([sys.argv[1], flag, *args],
                                 text=True, capture_output=True, timeout=10)
        require(invalid.returncode == 1 and 'bounded positive integers' in invalid.stderr,
                'unbounded/invalid benchmark argument was accepted')
    return 0


if __name__ == '__main__':
    sys.exit(main())
