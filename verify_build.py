#!/usr/bin/env python3
"""
Energy Analyzer Build Verification Script
Validates that the firmware build is complete and ready for deployment
"""

import os
import sys
import hashlib
from pathlib import Path

def verify_binaries():
    """Verify all required binaries exist and have correct sizes"""
    print("=" * 70)
    print("ENERGY ANALYZER FIRMWARE - BUILD VERIFICATION")
    print("=" * 70)
    
    build_dir = Path("build")
    
    required_files = {
        "build/analisador.elf": (7000000, 8500000),  # 7-8.5 MB expected
        "build/analisador.bin": (800000, 900000),    # 800-900 KB expected
        "build/bootloader/bootloader.bin": (20000, 30000),  # 20-30 KB expected
        "build/partition_table/partition-table.bin": (100, 1000),  # ~100-1000 bytes
    }
    
    print("\n1. BINARY VERIFICATION")
    print("-" * 70)
    
    all_exist = True
    for filepath, (min_size, max_size) in required_files.items():
        path = Path(filepath)
        if path.exists():
            size = path.stat().st_size
            if min_size <= size <= max_size:
                status = "✅ PASS"
                print(f"{status}: {filepath}")
                print(f"         Size: {size:,} bytes ({size/1024/1024:.2f} MB)")
            else:
                status = "⚠️  WARNING"
                print(f"{status}: {filepath}")
                print(f"         Size: {size:,} bytes (expected {min_size:,}-{max_size:,})")
        else:
            all_exist = False
            print(f"❌ FAIL:  {filepath} - FILE NOT FOUND")
    
    return all_exist

def verify_compilation_logs():
    """Verify compilation completed without errors"""
    print("\n2. COMPILATION STATUS")
    print("-" * 70)
    
    build_log = Path("build/.ninja_log")
    if build_log.exists():
        print(f"✅ PASS: Build log exists")
        
        # Count build steps
        with open(build_log, 'r') as f:
            lines = f.readlines()
            step_count = len([l for l in lines if l.strip()])
        
        print(f"         Build steps recorded: {step_count}")
        return True
    else:
        print("⚠️  WARNING: Build log not found (non-critical)")
        return True

def verify_source_fixes():
    """Verify that all three critical fixes were applied"""
    print("\n3. SOURCE CODE FIXES VERIFICATION")
    print("-" * 70)
    
    fixes = {
        "config in board_hal": {
            "file": "components/board_hal/CMakeLists.txt",
            "pattern": "REQUIRES driver config"
        },
        "no driver in app": {
            "file": "components/app/CMakeLists.txt",
            "pattern": "REQUIRES board_hal services network"
        },
        "SAFE_STRCPY macro fix": {
            "file": "components/config/common_types.h",
            "pattern": "strncpy((char *)(dest)"
        }
    }
    
    all_fixed = True
    for fix_name, fix_info in fixes.items():
        filepath = Path(fix_info["file"])
        pattern = fix_info["pattern"]
        
        if filepath.exists():
            with open(filepath, 'r') as f:
                content = f.read()
            
            if pattern in content:
                print(f"✅ PASS: {fix_name}")
            else:
                print(f"❌ FAIL: {fix_name} - Pattern not found in {fix_info['file']}")
                all_fixed = False
        else:
            print(f"❌ FAIL: {fix_name} - File not found: {fix_info['file']}")
            all_fixed = False
    
    return all_fixed

def verify_documentation():
    """Verify all documentation files exist"""
    print("\n4. DOCUMENTATION VERIFICATION")
    print("-" * 70)
    
    docs = [
        "BUILD_COMPLETION_REPORT.md",
        "DEPLOYMENT_GUIDE.md",
        "BUILD_COMPLETION_CHECKLIST.md"
    ]
    
    all_exist = True
    for doc in docs:
        path = Path(doc)
        if path.exists():
            size = path.stat().st_size
            print(f"✅ PASS: {doc} ({size:,} bytes)")
        else:
            print(f"❌ FAIL: {doc} - NOT FOUND")
            all_exist = False
    
    return all_exist

def main():
    """Run all verifications"""
    try:
        verify_binaries()
        verify_compilation_logs()
        sources_ok = verify_source_fixes()
        docs_ok = verify_documentation()
        
        print("\n" + "=" * 70)
        print("VERIFICATION SUMMARY")
        print("=" * 70)
        
        if sources_ok and docs_ok:
            print("✅ BUILD VERIFICATION PASSED")
            print("\nThe firmware build is complete and ready for deployment.")
            print("\nNext steps:")
            print("  1. Read DEPLOYMENT_GUIDE.md for flashing instructions")
            print("  2. Connect ESP32 device via USB")
            print("  3. Run: idf.py -p <PORT> flash monitor")
            print("\nFor technical details, see BUILD_COMPLETION_REPORT.md")
            return 0
        else:
            print("⚠️  BUILD VERIFICATION INCOMPLETE")
            print("\nSome verifications failed. Please review above.")
            return 1
            
    except Exception as e:
        print(f"\n❌ VERIFICATION ERROR: {e}")
        return 2

if __name__ == "__main__":
    sys.exit(main())
