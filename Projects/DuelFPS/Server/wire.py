"""Wire format shared with Engine/Network/NetworkManager.cpp (little endian)."""
import struct

MAGIC = 0x454E474E
HELLO, WELCOME, TRANSFORM, GOODBYE, MESSAGE, STOP, PING, PONG, FULL = range(1, 10)
HEADER = struct.Struct("<IB")
MOVE = struct.Struct("<II6f")
ENVELOPE = struct.Struct("<IHH")
COMBAT, RULES, LOBBY = 20, 21, 22
ACCOUNT = 23


def packet(kind, data=b""):
    return HEADER.pack(MAGIC, kind) + data


def welcome(player):
    return packet(WELCOME, struct.pack("<I", player))


def message(channel, text, sender=1):
    body = text.encode("ascii")
    return packet(MESSAGE, ENVELOPE.pack(sender, channel, len(body)) + body)


def unpack(data):
    if len(data) < HEADER.size:
        return None, b""
    magic, kind = HEADER.unpack_from(data)
    return (kind if magic == MAGIC else None), data[HEADER.size:]


def read_message(body):
    if len(body) < ENVELOPE.size:
        return None, None, ""
    player, channel, length = ENVELOPE.unpack_from(body)
    if length > 192 or length + ENVELOPE.size != len(body):
        return None, None, ""
    try:
        return player, channel, body[ENVELOPE.size:].decode("ascii")
    except UnicodeError:
        return None, None, ""
