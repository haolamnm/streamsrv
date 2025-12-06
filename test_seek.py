#!/usr/bin/env python3
"""Test RTSP SEEK functionality on raw MJPEG"""

import socket
import time
import sys

def send_rtsp_command(sock, cmd):
    """Send RTSP command and read response"""
    print(f">>> {cmd}")
    sock.send((cmd + "\r\n").encode())
    
    response = b""
    while True:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            if b"\r\n\r\n" in response:  # RTSP header end
                break
        except socket.timeout:
            break
        except:
            break
    
    print(f"<<< {response.decode('latin1', errors='ignore')[:200]}")
    return response

def test_seek():
    """Test seek functionality"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(2)
    
    try:
        sock.connect(("localhost", 8554))
        print("[OK] Connected to server\n")
        
        # DESCRIBE to get video info
        send_rtsp_command(sock, "DESCRIBE rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 1")
        time.sleep(0.5)
        
        # SETUP for RTP port
        send_rtsp_command(sock, "SETUP rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 2\r\nTransport: RTP/AVP;unicast;client_port=5000-5001")
        time.sleep(0.5)
        
        # PLAY from beginning
        send_rtsp_command(sock, "PLAY rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 3\r\nRange: npt=0.0-")
        time.sleep(1)
        
        # SEEK forward to frame 100
        print("\n[TEST] Seeking FORWARD to frame 100...")
        send_rtsp_command(sock, "SEEK rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 4\r\nX-Frame: 100")
        time.sleep(1)
        
        # SEEK backward to frame 50
        print("\n[TEST] Seeking BACKWARD to frame 50...")
        send_rtsp_command(sock, "SEEK rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 5\r\nX-Frame: 50")
        time.sleep(1)
        
        # SEEK backward to frame 10
        print("\n[TEST] Seeking BACKWARD to frame 10...")
        send_rtsp_command(sock, "SEEK rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 6\r\nX-Frame: 10")
        time.sleep(1)
        
        # SEEK forward to frame 150
        print("\n[TEST] Seeking FORWARD to frame 150...")
        send_rtsp_command(sock, "SEEK rtsp://localhost:8554/video.mjpeg RTSP/1.0\r\nCSeq: 7\r\nX-Frame: 150")
        time.sleep(1)
        
        sock.close()
        print("\n[OK] Test completed")
        
    except Exception as e:
        print(f"[ERROR] {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    test_seek()
