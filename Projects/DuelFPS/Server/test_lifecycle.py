"""Exercise allocation retries, ticket admission, capacity and worker cleanup."""
import socket
import struct
import unittest
from unittest.mock import Mock

import wire
from lobby import LobbyService
from match import ACTIVE, MATCH_END, WARMUP, Match, Player


class Lifecycle(unittest.TestCase):
    def test_one_slot(self):
        address = "127.0.0.1"
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
            reserved.bind((address, 0))
            match_port = reserved.getsockname()[1]
        service = LobbyService(address, match_port-1, max_matches=1)
        creator, friend = (address, 20101), (address, 20102)

        def request(addr, text):
            _, body = wire.unpack(service.reply(wire.packet(wire.HELLO), addr))
            cookie, = struct.unpack("<I", body)
            packet = service.reply(wire.message(wire.LOBBY, text, cookie), addr)
            return wire.read_message(wire.unpack(packet)[1])[2]

        try:
            allocation = request(creator, "CREATE:1:3:60")
            _, port, code, token = allocation.split(":")
            self.assertEqual(allocation, request(creator, "CREATE:1:3:60"))
            self.assertIn("busy", request(friend, "CREATE:1:3:60"))
            self.assertIn("not found", request(friend, "JOIN:00000000"))
            guest = request(friend, "JOIN:"+code).split(":")[-1]
            self.assertNotEqual(guest, token)
            endpoint = (address, int(port))
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as a, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as b:
                for sock, ticket, slot in ((a, token, 1), (b, guest, 2)):
                    sock.settimeout(4)
                    sock.sendto(wire.packet(wire.HELLO, ticket.encode()), endpoint)
                    packet, _ = sock.recvfrom(512)
                    self.assertEqual(packet, wire.welcome(slot))
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as extra:
                    extra.settimeout(2)
                    extra.sendto(wire.packet(wire.HELLO, guest.encode()), endpoint)
                    self.assertEqual(extra.recvfrom(512)[0], wire.packet(wire.FULL))
                a.sendto(wire.packet(wire.GOODBYE), endpoint)
                service.matches[int(port)].worker.join(3)
                service.reap()
                self.assertFalse(service.matches)
            fresh = request(friend, "CREATE:2:7:120")
            self.assertTrue(fresh.startswith("MATCH:"))
            self.assertNotEqual(token, fresh.split(":")[-1])
        finally:
            service.close()

    def test_quick_play_queue(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
            reserved.bind(("127.0.0.1", 0))
            service = LobbyService("127.0.0.1", reserved.getsockname()[1]-1, max_matches=1)
        def request(port, command):
            addr = ("127.0.0.1", port)
            cookie = struct.unpack("<I", wire.unpack(service.reply(wire.packet(wire.HELLO),addr))[1])[0]
            response = service.reply(wire.message(wire.LOBBY,command,cookie), addr)
            return wire.read_message(wire.unpack(response)[1])[2] if response else ""
        try:
            self.assertTrue(request(20001,"PLAY").startswith("WAIT:"))
            self.assertTrue(request(20010,"PLAY:3").startswith("WAIT:"))
            self.assertTrue(request(20011,"PLAY:2").startswith("WAIT:"))
            request(20001,"CANCEL")
            self.assertTrue(request(20002,"PLAY").startswith("WAIT:"))
            first = request(20001,"PLAY")
            second = request(20002,"PLAY")
            self.assertTrue(first.startswith("MATCH:"))
            self.assertEqual(first.split(":")[:3], second.split(":")[:3])
            self.assertNotEqual(first.split(":")[3], second.split(":")[3])
            self.assertEqual(first, request(20001,"PLAY"))
            self.assertTrue(request(20003,"PLAY").startswith("WAIT:"))
            self.assertTrue(request(20003,"JOIN:"+first.split(":")[2]).startswith("ERROR:"))
            self.assertTrue(request(20003,"CREATE:1:3:60").startswith("ERROR:"))
        finally:
            service.close()

    def test_private_team_tickets(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1",0))
            service=LobbyService("127.0.0.1",probe.getsockname()[1]-1)
        def request(slot, command):
            addr=("127.0.0.1",21000+slot)
            cookie=struct.unpack("<I",wire.unpack(service.reply(wire.packet(wire.HELLO),addr))[1])[0]
            return wire.read_message(wire.unpack(service.reply(wire.message(wire.LOBBY,command,cookie),addr))[1])[2]
        try:
            creator=request(1,"CREATE:1:3:60:3")
            code=creator.split(":")[2]
            allocations=[creator]+[request(i,"JOIN:"+code) for i in range(2,7)]
            self.assertEqual(len(set(allocations)),6)
            self.assertEqual(request(4,"JOIN:"+code),allocations[3])
            self.assertEqual(request(7,"JOIN:"+code),"ERROR:Match full")
        finally:
            service.close()

    def test_concurrent_public_and_private_matches(self):
        host = "127.0.0.1"
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as probe:
            probe.bind((host,0))
            service = LobbyService(host,probe.getsockname()[1]-1)
        self.addCleanup(service.close)

        def request(client, command):
            addr = (host,31000+client)
            challenge = service.reply(wire.packet(wire.HELLO),addr)
            cookie, = struct.unpack("<I",wire.unpack(challenge)[1])
            reply = service.reply(wire.message(wire.LOBBY,command,cookie),addr)
            return wire.read_message(wire.unpack(reply)[1])[2]

        private = request(1,"CREATE:1:3:60:3")
        _, private_port, private_code, private_ticket = private.split(":")
        for client in range(2,7):
            self.assertTrue(request(client,"PLAY:3").startswith("WAIT:"))
        public = request(7,"PLAY:3")
        _, public_port, public_code, public_ticket = public.split(":")
        self.assertNotEqual(private_port,public_port)
        self.assertNotEqual(private_code,public_code)
        self.assertEqual(private,request(1,"CREATE:1:3:60:3"))
        self.assertEqual(public,request(7,"PLAY:3"))
        self.assertTrue(request(8,"CREATE:1:3:60").startswith("ERROR:All servers busy"))
        self.assertTrue(request(8,"JOIN:"+public_code).startswith("ERROR:"))
        self.assertEqual(request(8,"JOIN:"+private_code).split(":")[1],private_port)
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as peer:
            peer.settimeout(3)
            endpoint = (host,int(private_port))
            peer.sendto(wire.packet(wire.HELLO,private_ticket.encode()),endpoint)
            self.assertEqual(peer.recvfrom(512)[0],wire.welcome(1))
            peer.sendto(wire.packet(wire.GOODBYE),endpoint)
            service.matches[int(private_port)].worker.join(3)
        replacement = request(9,"CREATE:2:5:90")
        _, reused, new_code, new_ticket = replacement.split(":")
        self.assertEqual(reused,private_port)
        self.assertNotEqual(new_ticket,private_ticket)
        if new_code != private_code:
            self.assertTrue(request(8,"JOIN:"+private_code).startswith("ERROR:"))
        self.assertEqual(public,request(7,"PLAY:3"))
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as peer:
            peer.settimeout(3)
            peer.sendto(wire.packet(wire.HELLO,public_ticket.encode()),(host,int(public_port)))
            self.assertEqual(peer.recvfrom(512)[0],wire.welcome(6))

    def test_team_damage_timeout_and_rematch(self):
        match=Match(Mock(),(2,3,60),tuple(range(4)))
        now=match.started
        for slot in range(1,5):
            match.players[slot]=Player(("127.0.0.1",3000+slot),now)
        match.tick(now)
        match.tick(now+4)
        self.assertEqual(match.state,ACTIVE)
        # A living ally stands between the shooter and the enemy.
        for slot,z in ((1,5),(3,1),(2,-2),(4,-5)):
            match.players[slot].history.append((now+4,(-12.5,1,z,0,0,0)))
        def shot(seq):
            match.shot(1,f"SHOT:{seq}:1:rifle:-12.5:1.65:5:0:0:-1".split(":"),now+4+seq)
        shot(1)
        self.assertEqual(match.health[1:],[100]*4)
        match.players[3].history.clear()
        shot(2)
        self.assertEqual(match.health[2],80)
        match.score[1]=2
        match.tick(match.deadline+1)
        self.assertEqual((match.state,match.winner), (MATCH_END,1))
        for slot in range(1,5):
            match.receive(wire.message(wire.COMBAT,"REMATCH",slot),match.players[slot].addr,now+80)
            self.assertEqual(match.state,WARMUP if slot==4 else MATCH_END)
        self.assertEqual(match.health[1:],[100]*4)


if __name__ == "__main__":
    unittest.main()
