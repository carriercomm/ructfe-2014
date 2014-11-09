import os
import sys

N = 64

SERVER_DATA = """mode p2p
port {0}
dev tun{1}
ifconfig 10.60.{1}.1 10.60.{1}.2
route 10.70.{1}.0 255.255.255.0
keepalive 10 60
ping-timer-rem
persist-tun
persist-key

tun-mtu 1500
fragment 1300
mssfix

<secret>
{2}
</secret>
"""

if __name__ != "__main__":
    print("I am not a module")
    sys.exit(0)

# gen client configs
os.chdir(os.path.dirname(os.path.realpath(__file__)))
try:
    os.mkdir("server")
except FileExistsError:
    print("Remove ./server dir first")
    sys.exit(1)

for i in range(N):
    key = open("keys/%d.key" % i).read()

    data = SERVER_DATA.format(30000+i, i, key)
    open("server/%d.conf" % i, "w").write(data)

print("Finished, check ./server dir")
    