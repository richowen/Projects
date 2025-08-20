#!/usr/bin/env python3
"""
Simple test client for the Milk Machine remote monitor
Tests both TCP and HTTP interfaces
"""

import socket
import time
import requests
import threading
import sys

# Configuration
MILK_MACHINE_IP = "192.168.1.16"  # Your ESP32 IP
TCP_PORT = 23
WEB_PORT = 80

def test_tcp_connection():
    """Test TCP telnet-style connection"""
    print("Testing TCP connection...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((MILK_MACHINE_IP, TCP_PORT))
        
        print(f"✓ Connected to TCP port {TCP_PORT}")
        
        # Read welcome message
        data = sock.recv(1024).decode('utf-8')
        print(f"Welcome: {data.strip()}")
        
        # Listen for real-time data for 30 seconds
        print("Listening for real-time data (30 seconds)...")
        start_time = time.time()
        while time.time() - start_time < 30:
            sock.settimeout(1)
            try:
                data = sock.recv(1024).decode('utf-8')
                if data:
                    print(f"[TCP] {data.strip()}")
            except socket.timeout:
                continue
            except:
                break
        
        sock.close()
        print("✓ TCP test completed")
        
    except Exception as e:
        print(f"✗ TCP test failed: {e}")

def test_web_interface():
    """Test web interface"""
    print("\nTesting Web interface...")
    try:
        # Test main page
        response = requests.get(f"http://{MILK_MACHINE_IP}:{WEB_PORT}/", 
                               auth=('admin', 'Gliders1!'), timeout=10)
        
        if response.status_code == 200:
            print("✓ Web interface accessible")
            print(f"Content length: {len(response.text)} bytes")
        else:
            print(f"✗ Web interface returned status {response.status_code}")
            
        # Test logs endpoint
        response = requests.get(f"http://{MILK_MACHINE_IP}:{WEB_PORT}/logs", 
                               auth=('admin', 'Gliders1!'), timeout=10)
        
        if response.status_code == 200:
            print("✓ Logs endpoint accessible")
            print(f"Recent logs preview:\n{response.text[:500]}...")
        else:
            print(f"✗ Logs endpoint returned status {response.status_code}")
            
    except Exception as e:
        print(f"✗ Web test failed: {e}")

def continuous_monitor():
    """Continuously monitor via TCP"""
    print("\nStarting continuous monitor (press Ctrl+C to stop)...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((MILK_MACHINE_IP, TCP_PORT))
        print("✓ Connected for continuous monitoring")
        
        while True:
            data = sock.recv(1024).decode('utf-8')
            if data:
                timestamp = time.strftime("%H:%M:%S")
                print(f"[{timestamp}] {data.strip()}")
                
    except KeyboardInterrupt:
        print("\n✓ Monitoring stopped by user")
    except Exception as e:
        print(f"✗ Monitoring failed: {e}")
    finally:
        if 'sock' in locals():
            sock.close()

if __name__ == "__main__":
    print("=== Milk Machine Remote Monitor Test ===")
    print(f"Target: {MILK_MACHINE_IP}")
    
    if len(sys.argv) > 1 and sys.argv[1] == "monitor":
        continuous_monitor()
    else:
        test_tcp_connection()
        test_web_interface()
        
        print("\n=== Test Summary ===")
        print("Both TCP and Web interfaces tested.")
        print(f"Run 'python {sys.argv[0]} monitor' for continuous monitoring")
        print("Access web interface at: http://{MILK_MACHINE_IP}/")