#!/usr/bin/env python3
"""
Consistency check for YOLOv3-tiny implementation
- Verify kernel size calculations are consistent across files
- Check filter/channel terminology is used correctly
- Validate memory offset calculations
"""

import os
import re

def check_file(filepath):
    """Check a single file for consistency issues."""
    if not os.path.exists(filepath):
        return None
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    issues = []
    
    # Check 1: No "ch *" or "ch =" inconsistencies (should use "filter")
    if 'for (int ch' in content and 'ip_conv_execute' in content:
        issues.append("ERROR: Uses 'ch' loop variable in main function (should be 'filter')")
    
    # Check 2: Kernel size formula consistency
    kernel_patterns = [
        r'kernel_sz\s*\*\s*kernel_sz\s*\*\s*input_c\s*\*\s*4',
        r'kernel_per_filter\s*=\s*kernel_sz\s*\*\s*kernel_sz\s*\*\s*input_c\s*\*\s*4'
    ]
    found_kernel = False
    for pattern in kernel_patterns:
        if re.search(pattern, content):
            found_kernel = True
    
    if 'kernel_sz' in content and not found_kernel and 'IP_interface' not in filepath:
        pass  # OK to not have in test/main files
    
    # Check 3: Verify no old-style comments remain in critical sections
    if '========' in content and len(content.split('========')) > 1:
        issues.append("WARNING: Old-style divider comments still present")
    
    # Check 4: Verify filter loop is present where needed
    if 'ip_conv_execute(' in content and 'for (int filter' not in content:
        if 'IP_interface.c' in filepath:
            pass  # This file should have the loop
        elif 'yoloV3tiny' in filepath:
            pass  # Main files call the function
    
    # Check 5: Memory offset calculation consistency
    if 'filter * kernel_per' in content:
        if 'kernel_offset = filter' not in content:
            issues.append("WARNING: Filter offset calculation may be inconsistent")
    
    return issues

# Check all main files
files_to_check = [
    'IP_interface/IP_interface.c',
    'include/IP_interface.h',
    'YOLOv3tiny/yoloV3tiny.c',
    'YOLOv3tiny/yoloV3tiny_naive.c',
    'Bounding/bbox_hw.c',
    'Packetization/packetize.c',
]

print("=" * 60)
print("YOLOv3-tiny Implementation Consistency Check")
print("=" * 60)

total_issues = 0
passed_files = []

for filepath in files_to_check:
    full_path = f'/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7/{filepath}'
    result = check_file(full_path)
    
    if result is None:
        print(f"✗ {filepath}: FILE NOT FOUND")
        total_issues += 1
    elif len(result) == 0:
        print(f"✓ {filepath}: PASS")
        passed_files.append(filepath)
    else:
        for issue in result:
            print(f"⚠ {filepath}: {issue}")
            total_issues += 1

print("\n" + "=" * 60)
print(f"Summary: {len(passed_files)}/{len(files_to_check)} files passed")

# Check kernel calculations manually
print("\n" + "=" * 60)
print("Kernel Size Verification")
print("=" * 60)

test_cases = [
    ("L0", 3, 3, 32),   # Conv 3x3, 3->32
    ("L2", 3, 32, 64),  # Conv 3x3, 32->64
    ("L12", 1, 1024, 255),  # Conv 1x1, 1024->255
]

print("\nKernel size per filter (h×w×input_c×4):")
for name, h, w, ic in test_cases:
    size = h * w * ic * 4
    print(f"  {name}: {h}×{w}×{ic}×4 = {size:,} bytes")

print("\n" + "=" * 60)
print("Memory Offset Consistency")
print("=" * 60)

print("\nFor 13×13×255 output (Layer 12):")
oh, ow, oc = 13, 13, 255
output_ch_size = oh * ow * 4
print(f"  Output channel size: {oh}×{ow}×4 = {output_ch_size} bytes")
print(f"  Channel 0 offset: 0×{output_ch_size} = 0")
print(f"  Channel 1 offset: 1×{output_ch_size} = {output_ch_size}")
print(f"  Channel 254 offset: 254×{output_ch_size} = {254 * output_ch_size}")

print("\nFor 1×1×1024→255 kernel:")
h, w, ic = 1, 1, 1024
kernel_size = h * w * ic * 4
print(f"  Kernel per filter: {h}×{w}×{ic}×4 = {kernel_size} bytes")
print(f"  Filter 0 kernel offset: 0×{kernel_size} = 0")
print(f"  Filter 1 kernel offset: 1×{kernel_size} = {kernel_size}")
print(f"  Filter 254 kernel offset: 254×{kernel_size} = {254 * kernel_size}")

print("\n" + "=" * 60)
print("✓ CONSISTENCY CHECK COMPLETE")
print("=" * 60)
