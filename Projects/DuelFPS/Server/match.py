"""Headless two-team elimination. Odd and even slots form the opposing teams."""
import math
import time
import uuid
from collections import deque
from dataclasses import dataclass, field

import arena
import wire
from accounts import Accounts

WAITING, WARMUP, ACTIVE, ROUND_END, MATCH_END = range(5)


@dataclass
class Player:
    addr: tuple
    seen: float
    loadout: tuple = ("rifle", "pistol")
    history: deque = field(default_factory=lambda: deque(maxlen=16))
    move_seq: int = 0
    shot_seq: int = 0
    last_shot: float = -100
    ack: str = ""
    account: str = ""
    skin: str = "standard"


class Match:
    def __init__(self, sock, rules, tickets, connected=None, accounts_path=None):
        self.sock = sock
        self.rules = rules
        self.tickets = tickets
        self.players = {}
        self.boxes = arena.cover(rules[0])
        self.weapons = arena.weapons()
        self.started = time.monotonic()
        self.next_state = self.started
        self.sequence = self.round = 0
        self.state, self.deadline = WAITING, 0
        self.health = [0] + [100]*len(tickets)
        self.score = [0, 0, 0]
        self.winner = 0
        self.votes = set()
        self.connected = connected
        self.accounts = Accounts(accounts_path) if accounts_path else None
        self.game_id = uuid.uuid4().hex
        self.next_skins = 0

    def broadcast(self, data):
        for player in self.players.values():
            self.sock.sendto(data, player.addr)

    def state_packet(self, now):
        self.sequence += 1
        fields = (self.sequence, self.state, round(max(0, self.deadline-now)*10),
                  self.round, *self.health[1:3], *self.score[1:], self.winner,
                  self.winner if self.state == MATCH_END else 0, int((now-self.started)*1000),
                  *self.health[3:])
        return wire.message(wire.COMBAT, "STATE:" + ":".join(map(str, fields)))

    def warmup(self, now):
        self.round += 1
        self.winner = 0
        self.health[1:] = [100]*len(self.tickets)
        for player in self.players.values():
            player.last_shot = -100
            player.ack = ""
            player.history.clear()
        self.state, self.deadline = WARMUP, now + 3

    def finish_round(self, winner, now):
        self.winner = winner
        if winner:
            self.score[winner] += 1
        if self.score[winner] >= self.rules[1]:
            self.state, self.deadline = MATCH_END, 0
            if self.accounts:
                self.accounts.reward(self.game_id,[p.account for slot,p in self.players.items()
                    if p.account and (1 if slot%2 else 2)==winner])
        else:
            self.state, self.deadline = ROUND_END, now + 3
        self.next_state = 0

    def tick(self, now):
        if self.state == WAITING and len(self.players) == len(self.tickets):
            self.warmup(now)
        elif self.state == WARMUP and now >= self.deadline:
            self.state, self.deadline = ACTIVE, now + self.rules[2]
        elif self.state == ACTIVE and now >= self.deadline:
            h1, h2 = sum(self.health[1::2]), sum(self.health[2::2])
            self.finish_round(0 if h1 == h2 else 1 if h1 > h2 else 2, now)
        elif self.state == ROUND_END and now >= self.deadline:
            self.warmup(now)
        if now >= self.next_state:
            self.broadcast(self.state_packet(now))
            self.next_state = now + .2
        if now>=self.next_skins and any(p.account for p in self.players.values()):
            skins=",".join(f"{s}={p.skin}" for s,p in self.players.items())
            self.broadcast(wire.message(wire.ACCOUNT,"SKINS:"+skins))
            self.next_skins=now+1

    def shot(self, slot, parts, now):
        if len(parts) not in (10, 11):
            return
        player = self.players[slot]
        try:
            seq, rnd = int(parts[1]), int(parts[2])
            coords = tuple(map(float, parts[4:10]))
            stamp = float(parts[10]) + self.started if len(parts) == 11 else now
        except ValueError:
            return
        if not all(math.isfinite(v) and abs(v) <= 1e6 for v in coords) or not math.isfinite(stamp):
            return
        if seq == player.shot_seq and player.ack:
            self.sock.sendto(wire.message(wire.COMBAT, player.ack), player.addr)
            return
        weapon_id = parts[3]
        if (self.state != ACTIVE or rnd != self.round or not player.history or not self.health[slot] or
                not 0 < seq <= 2**31 or seq <= player.shot_seq or weapon_id not in player.loadout):
            return
        weapon = self.weapons[weapon_id]
        origin, direction = coords[:3], coords[3:]
        pos = player.history[-1][1]
        if (not .95 <= math.hypot(*direction) <= 1.05 or math.dist(pos[:3], origin) > 3 or
                now-player.last_shot < .8*weapon["fireInterval"]):
            return
        eye = (pos[0], pos[1]+(.40 if pos[5] > .5 else .65), pos[2])
        displacement = math.dist(origin, eye)
        if displacement > .15:
            ray = tuple((a-b)/displacement for a, b in zip(origin, eye))
            if any(arena.distance(box, eye, ray, displacement-.02) < math.inf for box in self.boxes):
                return
        player.shot_seq, player.last_shot = seq, now
        targets = {}
        when = max(now-.18, min(now, stamp))
        for other, candidate in self.players.items():
            history = candidate.history
            if other == slot or not self.health[other] or not history:
                continue
            target = next((p for at, p in reversed(history) if at <= when), history[0][1])
            targets[other] = (target[:3], (.325, .58 if target[5] > .5 else .9, .325), 1, 0)
        hit_enemy = False
        for ray in arena.pellets(direction, weapon):
            hit, other = min(((arena.distance(box, origin, ray, weapon["range"]), other)
                              for other, box in targets.items()), default=(math.inf, 0))
            if hit == math.inf or slot%2 == other%2:
                continue
            if all(arena.distance(b, origin, ray, hit) == math.inf for b in self.boxes):
                self.health[other] = max(0,self.health[other]-int(weapon["damage"]))
                hit_enemy = True
        player.ack = f"ACK:{slot}:{seq}:{int(hit_enemy)}"
        self.broadcast(wire.message(wire.COMBAT, player.ack))
        self.broadcast(wire.message(wire.COMBAT, f"FX:{slot}:" + ":".join(parts[3:10])))
        team = 1 if slot%2 else 2
        if not any(self.health[3-team::2]):
            self.finish_round(team, now)

    def receive(self, data, addr, now):
        kind, body = wire.unpack(data)
        slot = next((s for s, p in self.players.items() if p.addr == addr), None)
        if kind == wire.HELLO:
            if body not in self.tickets:
                return True
            target = self.tickets.index(body)+1
            if target in self.players and self.players[target].addr != addr:
                self.sock.sendto(wire.packet(wire.FULL), addr)
                return True
            if slot is not None and slot != target:
                return True
            if not slot:
                self.players[target] = Player(addr, now)
                if self.connected is not None:
                    self.connected.value=len(self.players)
            self.sock.sendto(wire.welcome(target), addr)
            return True
        if not slot:
            return True
        p = self.players[slot]
        p.seen = now
        sender, channel, payload = wire.read_message(body) if kind == wire.MESSAGE else (None, None, "")
        if kind == wire.GOODBYE:
            return False
        if kind == wire.PING:
            self.sock.sendto(wire.packet(wire.PONG), addr)
        elif kind == wire.TRANSFORM and len(body) == wire.MOVE.size:
            player_id, seq, *pos = wire.MOVE.unpack(body)
            if player_id == slot and seq > p.move_seq and all(math.isfinite(v) and abs(v)<1e5 for v in pos):
                p.move_seq = seq
                p.history.append((now, pos))
                for other, recipient in self.players.items():
                    if other != slot:
                        self.sock.sendto(data, recipient.addr)
        elif channel == wire.LOBBY and sender == slot and payload == "RULES":
            rules = ":".join(map(str, self.rules))
            if len(self.tickets)>2:
                rules += f":{len(self.tickets)//2}"
            self.sock.sendto(wire.message(wire.LOBBY, f"ADMITTED:{slot}:{rules}"), addr)
        elif channel==wire.ACCOUNT and sender==slot and self.accounts:
            reply,account=self.accounts.respond(addr,payload,profile_only=True)
            if account and (not p.account or p.account==account) and reply.startswith("PROFILE:"):
                p.account=account
                p.skin=reply.split(":")[3]
            self.sock.sendto(wire.message(wire.ACCOUNT,reply),addr)
        elif channel == wire.COMBAT and sender == slot:
            parts = payload.split(":")
            if parts[0] == "LOADOUT" and len(parts) == 3 and all(k in self.weapons for k in parts[1:]) and parts[1] != parts[2]:
                p.loadout = tuple(parts[1:])
            elif parts[0] == "SHOT":
                self.shot(slot, parts, now)
            elif payload == "REMATCH" and self.state == MATCH_END:
                self.votes.add(slot)
                if len(self.votes) == len(self.tickets):
                    self.votes.clear()
                    self.score = [0, 0, 0]
                    self.round = 0
                    self.game_id = uuid.uuid4().hex
                    self.warmup(now)
        return True


def run_match(sock, rules, tickets, connected=None, accounts_path=None):
    with sock:
        match = Match(sock,rules,tuple(t.encode("ascii") for t in tickets),connected,accounts_path)
        sock.settimeout(.03)
        try:
            while True:
                now = time.monotonic()
                if any(now-p.seen > 12 for p in match.players.values()):
                    break
                if len(match.players) < len(tickets) and now-match.started > (300 if match.players else 20):
                    break
                match.tick(now)
                try:
                    data, addr = sock.recvfrom(512)
                except TimeoutError:
                    continue
                except ConnectionResetError:
                    continue
                if not match.receive(data, addr, time.monotonic()):
                    break
        finally:
            match.broadcast(wire.packet(wire.STOP))
