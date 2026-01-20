import socket, sys, time, struct

ip = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
mode = sys.argv[3] if len(sys.argv) > 3 else "close_first"  # close_first / rst

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.connect((ip, port))
print("connected")

print("wait 10 sec")
time.sleep(10)
print("send hi")
s.send(b"hi")

print("wait 10 sec")
time.sleep(10)

if mode == "rst":
    # SO_LINGER on, timeout=0 -> close sends RST
    s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    print("client close() (RST)")
    s.close()
    time.sleep(30)

# mode == "close_first":
print("client close() (FIN)")
s.close()
time.sleep(30)