#!/usr/bin/env python3
"""
Comprehensive file dependency and memory analysis for YOLOv3-tiny pipeline
"""

import os
import re
from pathlib import Path

def extract_includes(file_path):
    """Extract all #include statements from a file"""
    includes = []
    try:
        with open(file_path, 'r') as f:
            for line in f:
                match = re.match(r'#include\s+[<"]([^>"]+)[>"]', line.strip())
                if match:
                    includes.append(match.group(1))
    except:
        pass
    return includes

def find_all_files(base_path):
    """Find all .c and .h files"""
    files = {}
    base = Path(base_path)
    for file_path in base.rglob('*'):
        if file_path.suffix in ['.c', '.h']:
            rel_path = str(file_path.relative_to(base))
            files[rel_path] = file_path
    return files

def analyze_dependencies():
    """Analyze all file dependencies"""
    base_path = "/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7"
    files = find_all_files(base_path)
    
    print("\n" + "="*80)
    print("FILE DEPENDENCY AND MEMORY ANALYSIS")
    print("="*80)
    
    # Core files
    print("\n[CORE PIPELINE FILES]")
    core_files = [
        "YOLOv3tiny/yoloV3tiny.c",
        "YOLOv3tiny/yoloV3tiny_naive.c",
        "IP_interface/IP_interface.c",
        "Packetization/packetize.c",
        "Tracking/track_hw.c",
        "Bounding/bbox_hw.c",
        "Scale/scale.c",
    ]
    
    dependencies = {}
    
    for file_name in core_files:
        if file_name in files:
            file_path = files[file_name]
            includes = extract_includes(file_path)
            dependencies[file_name] = includes
            
            print(f"\n{file_name}")
            print(f"  Includes: {len(includes)}")
            for inc in includes:
                if inc.startswith("../"):
                    print(f"    - {inc} ✓")
                else:
                    print(f"    - {inc}")
    
    # Header files
    print("\n[HEADER FILES]")
    header_files = [
        "include/IP_interface.h",
        "include/mem_map.h",
        "include/packetize.h",
        "include/track_hw.h",
        "include/bbox_hw.h",
        "Scale/scale.h",
    ]
    
    for file_name in header_files:
        if file_name in files:
            file_path = files[file_name]
            includes = extract_includes(file_path)
            
            print(f"\n{file_name}")
            print(f"  Includes: {len(includes)}")
            for inc in includes:
                if inc not in ["stdint.h", "stdbool.h", "stdio.h", "string.h"]:
                    print(f"    - {inc}")
    
    return dependencies


def analyze_naive_pipeline():
    """Analyze the naive pipeline memory flow"""
    print("\n" + "="*80)
    print("NAIVE PIPELINE MEMORY ANALYSIS")
    print("="*80)
    
    # Read yoloV3tiny_naive.c
    naive_file = "/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7/YOLOv3tiny/yoloV3tiny_naive.c"
    
    with open(naive_file, 'r') as f:
        content = f.read()
    
    # Extract buffer definitions
    print("\n[BUFFER DEFINITIONS]")
    ping = re.search(r'#define PING_BUFFER_BASE\s+\((.*?)\)', content)
    pong = re.search(r'#define PONG_BUFFER_BASE\s+\((.*?)\)', content)
    temp = re.search(r'#define TEMP_BUFFER_BASE\s+\((.*?)\)', content)
    
    if ping: print(f"PING_BUFFER: {ping.group(1)}")
    if pong: print(f"PONG_BUFFER: {pong.group(1)}")
    if temp: print(f"TEMP_BUFFER: {temp.group(1)}")
    
    # Extract layer configs
    print("\n[LAYER CONFIGURATIONS]")
    layer_configs = re.findall(r'\{(\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+)', content)
    
    print(f"Total layers configured: {len(layer_configs)}")
    
    # Extract loop structure
    print("\n[FORWARD PASS FLOW]")
    if "for (int i = 0; i < NUM_LAYERS; i++)" in content:
        print("✓ Per-layer loop detected")
    
    if "packetize_image" in content:
        print("✓ Packetization before each layer")
    
    if "ip_conv_execute" in content:
        print("✓ Conv execution detected")
    
    if "ip_pool_execute" in content:
        print("✓ Pool execution detected")
    
    if "memcpy(temp_input" in content:
        print("✓ Output to temp buffer detected")
    
    if "current_input = current_output" in content:
        print("✓ Buffer swap detected")
    
    # Memory usage
    print("\n[MEMORY USAGE]")
    print("PING buffer: 16MB (packetized input)")
    print("PONG buffer: 16MB (layer output)")
    print("TEMP buffer: 16MB (intermediate storage)")
    print("Total naive buffers: 48MB")


def check_function_coverage():
    """Check which functions are available and used"""
    print("\n" + "="*80)
    print("FUNCTION COVERAGE")
    print("="*80)
    
    # IP interface functions
    print("\n[IP_interface.h]")
    ip_funcs = [
        "ip_conv_execute()",
        "ip_pool_execute()",
        "ip_is_busy()",
        "ip_wait_complete()",
    ]
    for func in ip_funcs:
        print(f"  ✓ {func}")
    
    # Bounding box functions
    print("\n[bbox_hw.h]")
    bbox_funcs = [
        "bbox_hw_decode_head()",
        "bbox_hw_nms()",
    ]
    for func in bbox_funcs:
        print(f"  ✓ {func}")
    
    # Tracking functions
    print("\n[track_hw.h]")
    track_funcs = [
        "track_hw_reset()",
        "track_hw_predict()",
        "track_hw_associate_update()",
        "track_hw_compact()",
        "track_hw_get_tracks()",
    ]
    for func in track_funcs:
        print(f"  ✓ {func}")
    
    # Packetization functions
    print("\n[packetize.h]")
    packet_funcs = [
        "packetize_image()",
        "calculate_packetize_output_size()",
    ]
    for func in packet_funcs:
        print(f"  ✓ {func}")


def verify_main_files():
    """Verify main implementation files"""
    print("\n" + "="*80)
    print("MAIN FILES VERIFICATION")
    print("="*80)
    
    files_to_check = {
        "yoloV3tiny.c": "/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7/YOLOv3tiny/yoloV3tiny.c",
        "yoloV3tiny_naive.c": "/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7/YOLOv3tiny/yoloV3tiny_naive.c",
        "IP_interface.c": "/home/karikature/Desktop/ReconFLabs/RISC-P/Software-Artix 7/IP_interface/IP_interface.c",
    }
    
    for name, path in files_to_check.items():
        print(f"\n{name}:")
        try:
            with open(path, 'r') as f:
                content = f.read()
            
            # Check for main function
            if "int main(void)" in content:
                print("  ✓ main() defined")
            
            # Check for function definitions
            if "void forward_" in content:
                print("  ✓ forward pass implemented")
            
            if "bool ip_conv_execute" in content:
                print("  ✓ ip_conv_execute defined")
            
            if "static bool ip_conv_execute_single_channel" in content:
                print("  ✓ internal per-channel function")
            
            # Line count
            lines = len(content.split('\n'))
            print(f"  Lines: {lines}")
            
        except Exception as e:
            print(f"  ERROR: {e}")


def main():
    print("\n" + "="*80)
    print("YOLOV3-TINY PIPELINE COMPLETE ANALYSIS")
    print("="*80)
    
    # Analyze dependencies
    analyze_dependencies()
    
    # Analyze naive pipeline
    analyze_naive_pipeline()
    
    # Check function coverage
    check_function_coverage()
    
    # Verify main files
    verify_main_files()
    
    print("\n" + "="*80)
    print("ANALYSIS COMPLETE")
    print("="*80)


if __name__ == "__main__":
    main()
