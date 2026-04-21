#!/usr/bin/env python3
"""
Test client for the Milk Machine remote monitor.
Tests both TCP (port 23, password-protected) and HTTP (port 80, Basic auth) interfaces.

Usage:
    python test_monitor.py           -- run TCP + HTTP smoke tests
    python test_monitor.py monitor   -- continuous TCP log tail (Ctrl+C to stop)
"""

import socket
import time
import sys

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False
    print("Note: 'requests' not installed — HTTP tests will be skipped.")
    print("      pip install requests")

# ------------------------------------------------------------------ #
# Configuration — update to match src/secrets.h                       #
# ------------------------------------------------------------------ #
MILK_MACHINE_IP = "192.168.1.16"
TCP_PORT        = 23
WEB_PORT        = 80
USERNAME        = "admin"
PASSWORD        = "Gliders1!"   # must match SECRET_MONITOR_PASS in secrets.h
# ------------------------------------------------------------------ #


def _recv_line(sock, timeout=10.0):
    """Read bytes from sock until newline or timeout. Returns decoded string."""
    sock.settimeout(timeout)
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            chunk = sock.recv(256)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                break
        except socket.timeout:
            break
    # [Fix #19] errors='replace' handles garbled bytes on ESP32 reset
    return buf.decode("utf-8", errors="replace").strip()


def test_tcp_connection():
    """TCP smoke test: connect, authenticate, listen for 30 s."""
    print("Testing TCP connection...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((MILK_MACHINE_IP, TCP_PORT))
        print(f"  Connected to {MILK_MACHINE_IP}:{TCP_PORT}")

        # Read the password prompt
        prompt = _recv_line(sock, timeout=5.0)
        print(f"  Prompt : {prompt!r}")

        # Send password
        sock.sendall((PASSWORD + "\n").encode("utf-8"))

        # Read welcome / denial
        response = _recv_line(sock, timeout=5.0)
        print(f"  Response: {response!r}")

        if "denied" in response.lower() or "timeout" in response.lower():
            print("  TCP auth failed.")
            sock.close()
            return

        print("  Listening for live data (30 s)...")
        start = time.time()
        while time.time() - start < 30:
            sock.settimeout(2.0)   # [Fix #18] non-blocking recv
            try:
                data = sock.recv(1024).decode("utf-8", errors="replace")  # [Fix #19]
                if data:
                    print(f"  [TCP] {data.strip()}")
            except socket.timeout:
                continue
            except OSError:
                break

        sock.close()
        print("  TCP test completed.")

    except Exception as e:
        print(f"  TCP test failed: {e}")


def test_web_interface():
    """HTTP smoke test: main page and /logs endpoint."""
    if not HAS_REQUESTS:
        print("Skipping HTTP test (requests not installed).")
        return

    print("\nTesting HTTP interface...")
    auth = (USERNAME, PASSWORD)
    base = f"http://{MILK_MACHINE_IP}:{WEB_PORT}"

    try:
        r = requests.get(f"{base}/", auth=auth, timeout=10)
        if r.status_code == 200:
            print(f"  Main page OK  ({len(r.text)} bytes)")
        else:
            print(f"  Main page returned HTTP {r.status_code}")

        r = requests.get(f"{base}/logs", auth=auth, timeout=10)
        if r.status_code == 200:
            print(f"  /logs OK  ({len(r.text)} bytes)")
            preview = r.text[:500].replace("\n", " | ")
            print(f"  Preview: {preview}...")
        else:
            print(f"  /logs returned HTTP {r.status_code}")

    except Exception as e:
        print(f"  HTTP test failed: {e}")


def continuous_monitor():
    """Tail the TCP log stream until Ctrl+C."""
    print(f"\nConnecting to continuous monitor on {MILK_MACHINE_IP}:{TCP_PORT} ...")
    print("Press Ctrl+C to stop.\n")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        sock.connect((MILK_MACHINE_IP, TCP_PORT))
        print("Connected.")

        # Authenticate
        prompt = _recv_line(sock, timeout=5.0)
        if prompt:
            print(f"  {prompt}")
        sock.sendall((PASSWORD + "\n").encode("utf-8"))
        welcome = _recv_line(sock, timeout=5.0)
        if welcome:
            print(f"  {welcome}")
        if "denied" in welcome.lower():
            print("Authentication failed.")
            sock.close()
            return

        # [Fix #18] Set a recv timeout so the loop doesn't hang on a silent server
        sock.settimeout(5.0)

        while True:
            try:
                data = sock.recv(1024)
                if not data:
                    print("[connection closed by server]")
                    break
                # [Fix #19] errors='replace' — handles garbled bytes on ESP32 reset
                text = data.decode("utf-8", errors="replace")
                ts = time.strftime("%H:%M:%S")
                print(f"[{ts}] {text.strip()}")
            except socket.timeout:
                continue  # server quiet — keep waiting
            except KeyboardInterrupt:
                raise

    except KeyboardInterrupt:
        print("\nMonitoring stopped.")
    except Exception as e:
        print(f"Monitoring failed: {e}")
    finally:
        try:
            sock.close()
        except Exception:
            pass


if __name__ == "__main__":
    print("=== Milk Machine Remote Monitor Test ===")
    print(f"Target: {MILK_MACHINE_IP}")

    if len(sys.argv) > 1 and sys.argv[1] == "monitor":
        continuous_monitor()
    else:
        test_tcp_connection()
        test_web_interface()
        print("\n=== Test Summary ===")
        print("TCP and HTTP interfaces tested.")
        # [Fix #20] f-string so the IP is actually substituted
        print(f"Web interface: http://{MILK_MACHINE_IP}/")
        print(f"Run 'python {sys.argv[0]} monitor' for continuous log tail.")
