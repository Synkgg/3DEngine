"""Persistent cosmetic inventory and one-use, action-bound account challenges."""
import hashlib
import re
import secrets
import sqlite3
import time
from contextlib import closing

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

PRICES = {"copper": 50, "glacier": 100, "violet": 150}
STARTING_CREDITS = 100
WIN_CREDITS = 25


class Accounts:
    def __init__(self, path):
        self.path = path
        self.pending = {}
        with closing(sqlite3.connect(path)) as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.executescript("""
                CREATE TABLE IF NOT EXISTS profiles (
                    id TEXT PRIMARY KEY, credits INTEGER NOT NULL CHECK(credits >= 0), equipped TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS items (
                    id TEXT NOT NULL, skin TEXT NOT NULL, PRIMARY KEY(id,skin));
                CREATE TABLE IF NOT EXISTS wins (
                    game TEXT NOT NULL, id TEXT NOT NULL, PRIMARY KEY(game,id));
            """)

    def reward(self, game, winners):
        with closing(sqlite3.connect(self.path)) as db, db:
            for account in set(winners):
                if db.execute("INSERT OR IGNORE INTO wins VALUES (?,?)", (game,account)).rowcount:
                    db.execute("UPDATE profiles SET credits=credits+? WHERE id=?", (WIN_CREDITS,account))

    def profile(self, account, action):
        with closing(sqlite3.connect(self.path)) as db, db:
            db.execute("BEGIN IMMEDIATE")
            db.execute("INSERT OR IGNORE INTO profiles VALUES (?,?,'standard')",(account,STARTING_CREDITS))
            credits, equipped = db.execute("SELECT credits,equipped FROM profiles WHERE id=?",(account,)).fetchone()
            owned = {"standard"} | {row[0] for row in db.execute("SELECT skin FROM items WHERE id=?",(account,))}
            verb, _, skin = action.partition(".")
            if verb == "BUY" and skin not in owned:
                if credits < PRICES[skin]:
                    return "DENIED:Insufficient credits"
                credits -= PRICES[skin]
                owned.add(skin)
                db.execute("INSERT INTO items VALUES (?,?)",(account,skin))
                db.execute("UPDATE profiles SET credits=? WHERE id=?",(credits,account))
            if verb == "EQUIP":
                if skin not in owned:
                    return "DENIED:Purchase this skin first"
                equipped = skin
                db.execute("UPDATE profiles SET equipped=? WHERE id=?",(skin,account))
            return f"PROFILE:{account[:12]}:{credits}:{equipped}:" + ",".join(sorted(owned))

    def respond(self, addr, request, *, profile_only=False):
        now = time.monotonic()
        fields = request.split(":")
        entry = self.pending.get(addr)
        if len(fields)==3 and fields[0]=="ASK":
            _, action, public = fields
            if action!="PROFILE" and (profile_only or not re.fullmatch(r"(BUY|EQUIP)\.(standard|copper|glacier|violet)",action)):
                return "DENIED:Unknown action", None
            if not re.fullmatch(r"04[0-9a-f]{128}", public):
                return "DENIED:Invalid account key", None
            if not entry or entry[0]<now or entry[1:3]!=(action,public):
                self.pending = {a:v for a,v in self.pending.items() if v[0]>=now}
                if len(self.pending)>=512:
                    return "DENIED:Account service busy",None
                entry = (now+15,action,public,secrets.token_hex(16),None)
                self.pending[addr]=entry
            return f"CHALLENGE:{action}:{entry[3]}",None
        if len(fields)!=3 or fields[0]!="PROVE" or not entry or entry[0]<now or fields[1]!=entry[3]:
            return "RETRY",None
        expires,action,public,nonce,result=entry
        if not re.fullmatch(r"[0-9a-f]{128}",fields[2]):
            return "DENIED:Invalid signature",None
        account=hashlib.sha256(bytes.fromhex(public)).hexdigest()
        if result is not None:
            # Only repeat a response to an identical proof.
            return (result[1],account) if result[0]==fields[2] else ("DENIED:Invalid signature",None)
        try:
            key=ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),bytes.fromhex(public))
            raw=bytes.fromhex(fields[2])
            signature=utils.encode_dss_signature(int.from_bytes(raw[:32],"big"),int.from_bytes(raw[32:],"big"))
            key.verify(signature,f"DuelFPS/v1:{action}:{nonce}".encode(),ec.ECDSA(hashes.SHA256()))
        except (InvalidSignature,ValueError):
            return "DENIED:Invalid signature",None
        reply=self.profile(account,action)
        self.pending[addr]=(expires,action,public,nonce,(fields[2],reply))
        return reply,account
