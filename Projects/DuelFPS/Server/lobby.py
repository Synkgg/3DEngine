"""Run one lobby and a bounded pool of independent, on-demand match processes."""
import argparse
from dataclasses import dataclass, field
import hashlib
import hmac
import multiprocessing
import os
import re
import secrets
import socket
import time

import wire
from match import run_match
from accounts import Accounts


@dataclass
class Allocation:
    port: int
    code: str
    worker: multiprocessing.Process
    tickets: tuple
    creator: tuple
    roster: tuple
    listed: bool
    map_id: int
    connected: object
    guests: dict = field(default_factory=dict)

    def assignment(self, slot):
        return wire.message(wire.LOBBY, f"MATCH:{self.port}:{self.code}:{self.tickets[slot]}")


class LobbyService:
    def __init__(self, bind, port, max_matches=2, accounts_path=None):
        self.bind, self.port = bind, port
        if max_matches < 1 or port < 1 or port+max_matches > 65535:
            raise ValueError("The lobby and all match ports must fit in UDP ports 1–65535")
        self.max_matches = max_matches
        self.secret = secrets.token_bytes(32)
        self.matches = {}
        self.waiting = {}
        self.accounts = Accounts(accounts_path) if accounts_path else None

    def cookie(self, addr, interval):
        data = f"{addr[0]}:{addr[1]}:{interval}".encode()
        digest = hmac.digest(self.secret, data, hashlib.sha256)
        return int.from_bytes(digest[:4], "little") or 1

    def reap(self):
        for port, match in list(self.matches.items()):
            if not match.worker.is_alive():
                match.worker.join()
                match.worker.close()
                del self.matches[port]

    def allocate(self, addr, rules, size=1, roster=(), listed=False):
        # Bind before returning an endpoint. The child receives the bound socket.
        codes = {m.code for m in self.matches.values()}
        code = str(10000000 + secrets.randbelow(90000000))
        while code in codes:
            code = str(10000000 + secrets.randbelow(90000000))
        for port in range(self.port+1, self.port+self.max_matches+1):
            if port in self.matches:
                continue
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as game:
                try:
                    game.bind((self.bind, port))
                except OSError:
                    continue
                tickets = tuple(secrets.token_hex(16) for _ in range(2*size))
                context = multiprocessing.get_context("spawn")
                connected = context.Value("i",0)
                worker = context.Process(target=run_match, args=(game,rules,tickets,connected,
                    self.accounts.path if self.accounts else None),daemon=True)
                worker.start()
            match = Allocation(port, code, worker, tickets, addr, roster, listed, rules[0], connected)
            self.matches[port] = match
            return match
        raise OSError("No match port available")

    def quick_play(self, addr, size):
        now = time.monotonic()
        self.waiting = {a: entry for a, entry in self.waiting.items() if now-entry[0] < 4}
        for match in self.matches.values():
            if addr in match.roster:
                return match.assignment(match.roster.index(addr))
        if addr not in self.waiting and len(self.waiting) >= 128:
            return wire.message(wire.LOBBY, "ERROR:Queue full. Try again shortly.")
        self.waiting[addr] = (now,size)
        if len(self.matches) >= self.max_matches:
            return wire.message(wire.LOBBY, "WAIT:Waiting for a free server...")
        roster = tuple(a for a, (_,mode) in self.waiting.items() if mode==size)[:2*size]
        if len(roster) < 2*size:
            return wire.message(wire.LOBBY, f"WAIT:{size}v{size} matchmaking: {len(roster)}/{2*size} players")
        try:
            match = self.allocate(roster[0], (2, 5, 90), size, roster)
        except OSError:
            return wire.message(wire.LOBBY, "ERROR:Match port unavailable")
        for player in roster:
            del self.waiting[player]
        if addr in roster:
            return match.assignment(roster.index(addr))
        return wire.message(wire.LOBBY, "WAIT:Waiting for a free server...")

    def reply(self, data, addr):
        kind, body = wire.unpack(data)
        interval = int(time.monotonic()//30)
        if kind == wire.HELLO:
            return wire.welcome(self.cookie(addr, interval))
        if kind == wire.PING:
            return wire.packet(wire.PONG)
        sender, channel, request = wire.read_message(body)
        if kind != wire.MESSAGE or channel not in (wire.LOBBY,wire.ACCOUNT):
            return None
        if sender not in (self.cookie(addr, interval), self.cookie(addr, interval-1)):
            return wire.welcome(self.cookie(addr, interval))
        if channel==wire.ACCOUNT:
            response=self.accounts.respond(addr,request)[0] if self.accounts else "DENIED:Shop unavailable"
            return wire.message(wire.ACCOUNT,response)
        self.reap()
        if request=="LIST":
            rows=[]
            for m in self.matches.values():
                if m.listed and m.connected.value<len(m.tickets) and len(m.guests)<len(m.tickets)-1:
                    rows.append(f"{m.code},{m.map_id},{len(m.tickets)},{m.connected.value}")
            return wire.message(wire.LOBBY,"LIST:"+ ";".join(rows[:6]))
        play = re.fullmatch(r"PLAY(?::([123]))?", request)
        if play:
            return self.quick_play(addr, int(play[1] or 1))
        if request == "CANCEL":
            self.waiting.pop(addr, None)
            return None
        create = re.fullmatch(r"(?:CREATE|OPEN):([12]):([357]):(60|90|120)(?::([123]))?", request)
        if create:
            match = next((m for m in self.matches.values() if not m.roster and m.creator==addr),None)
            if not match and len(self.matches)>=self.max_matches:
                return wire.message(wire.LOBBY, "ERROR:All servers busy")
            if not match:
                try:
                    match = self.allocate(addr, tuple(map(int, create.groups()[:3])), int(create[4] or 1),
                                          listed=request.startswith("OPEN:"))
                except OSError:
                    return wire.message(wire.LOBBY, "ERROR:Match port unavailable")
            slot = 0
        elif request.startswith("JOIN:"):
            match = next((m for m in self.matches.values() if not m.roster and m.code==request[5:]),None)
            if not match:
                return wire.message(wire.LOBBY, "ERROR:Lobby code not found")
            if addr == match.creator:
                slot = 0
            elif len(match.tickets)==2:
                slot = 1
            else:
                if addr not in match.guests and len(match.guests)>=len(match.tickets)-1:
                    return wire.message(wire.LOBBY, "ERROR:Match full")
                slot = match.guests.setdefault(addr,len(match.guests)+1)
        else:
            return wire.message(wire.LOBBY, "ERROR:Lobby code not found")
        return match.assignment(slot)

    def close(self):
        for match in self.matches.values():
            if match.worker.is_alive():
                match.worker.terminate()
            match.worker.join()
            match.worker.close()
        self.matches.clear()

    def serve(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind((self.bind, self.port))
            sock.settimeout(1)
            print(f"DuelFPS lobby listening on {self.bind}:{self.port} ({self.max_matches} match slots)", flush=True)
            try:
                while True:
                    self.reap()
                    try:
                        data, addr = sock.recvfrom(512)
                        response = self.reply(data, addr)
                        if response:
                            sock.sendto(response, addr)
                    except (TimeoutError, ConnectionResetError):
                        continue
            finally:
                self.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7780)
    parser.add_argument("--max-matches", type=int, default=2)
    parser.add_argument("--accounts-db", default=os.path.join(os.environ.get("STATE_DIRECTORY","."),"duelfps.sqlite3"))
    args = parser.parse_args()
    if args.max_matches < 1 or not 1 <= args.port <= 65535-args.max_matches:
        parser.error("--port must leave room for all --max-matches adjacent ports")
    try:
        LobbyService(args.bind,args.port,args.max_matches,args.accounts_db).serve()
    except KeyboardInterrupt:
        pass
