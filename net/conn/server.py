import socket, time, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
mode = sys.argv[2] if len(sys.argv) > 2 else "normal"  # normal / no_close / close_first / no_read

ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(("0.0.0.0", port))
ls.listen(128)
print("listen", port, "mode=", mode)

c, addr = ls.accept()
print("accepted", addr)

if mode == "close_first":
    print("server closes first")
    c.close()
    time.sleep(999)

if mode == "no_read":
    print("server does not read, just sleep")
    time.sleep(999)

# normal: read until peer closes, then close
data = c.recv(1024)
print("recv data size is ", len(data))
print("wait peer FIN...")


while True:
    x = c.recv(1024)
    if not x:
        print("peer closed (recv=0)")
        break

if mode == "no_close":
    print("INTENTIONALLY not closing -> keep CLOSE-WAIT")
    input("Type anything then press Enter to close the connection: ")

print("server close() now")
c.close()
time.sleep(30)
