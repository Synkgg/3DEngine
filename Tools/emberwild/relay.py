"""Two-player TLS relay and loopback UDP bridge. Python 3.11+, stdlib only."""
import argparse, asyncio, hashlib, json, secrets, ssl, struct
MAX_PACKET = 1400
async def packet(reader):
    size = struct.unpack('!H', await reader.readexactly(2))[0]
    if not 1 <= size <= MAX_PACKET: raise ValueError('Invalid packet size')
    return await reader.readexactly(size)
async def send(writer, data):
    writer.write(struct.pack('!H', len(data)) + data)
    await writer.drain()
class Relay:
    def __init__(self): self.rooms = {}; self.connections = 0
    async def handle(self, reader, writer):
        room = role = None
        self.connections += 1
        try:
            if self.connections > 64: return
            hello = json.loads(await asyncio.wait_for(reader.readline(), 10))
            if not isinstance(hello,dict): return
            candidate, role = hello.get('room'), hello.get('role')
            if not isinstance(candidate,str): return
            room = candidate
            if not isinstance(room,str) or len(room)!=64 or any(c not in '0123456789abcdef' for c in room) or role not in ('host','guest'): return
            if room not in self.rooms and len(self.rooms)>=32: return
            peers = self.rooms.setdefault(room,{})
            if role in peers: writer.write(b'BUSY\n');await writer.drain();return
            peers[role] = writer
            writer.write(b'OK\n');await writer.drain()
            while True:
                data = await asyncio.wait_for(packet(reader),30)
                if data == b'\x00': continue  # keepalive, never forwarded to the engine
                other = peers.get('guest' if role=='host' else 'host')
                if other: await send(other,data)
        except (OSError, ValueError, asyncio.IncompleteReadError, asyncio.TimeoutError): pass
        finally:
            peers=self.rooms.get(room,{})
            if peers.get(role) is writer: peers.pop(role)
            if room in self.rooms and not peers: self.rooms.pop(room)
            self.connections-=1;writer.close()
            try: await writer.wait_closed()
            except OSError: pass
class LocalUDP(asyncio.DatagramProtocol):
    def __init__(self, queue, host, game_port): self.queue=queue;self.target=('127.0.0.1',game_port) if host else None
    def datagram_received(self,data,addr):
        if addr[0]!='127.0.0.1' or len(data)>MAX_PACKET or not data: return
        if self.target is None: self.target=addr
        if addr!=self.target:return
        try:self.queue.put_nowait(data)
        except asyncio.QueueFull:pass
async def bridge(args):
    ctx=ssl.create_default_context(cafile=args.ca)
    reader,writer=await asyncio.open_connection(args.server,args.port,ssl=ctx,server_hostname=args.server)
    room=hashlib.sha256(args.room.encode()).hexdigest()
    writer.write((json.dumps({'room':room,'role':args.role})+'\n').encode());await writer.drain()
    if await asyncio.wait_for(reader.readline(),10)!=b'OK\n': raise RuntimeError('Room role already occupied or rejected')
    queue=asyncio.Queue(128);protocol=LocalUDP(queue,args.role=='host',args.game_port)
    transport,_=await asyncio.get_running_loop().create_datagram_endpoint(lambda:protocol,local_addr=('127.0.0.1',0 if args.role=='host' else args.local_port))
    print('Relay connected. Choose '+('HOST CAMP' if args.role=='host' else 'JOIN VIA RELAY')+' in Emberwild.',flush=True)
    async def outbound():
        while True:
            try:data=await asyncio.wait_for(queue.get(),5)
            except asyncio.TimeoutError:data=b'\x00'
            await send(writer,data)
    async def inbound():
        while True:
            data=await packet(reader)
            if protocol.target:transport.sendto(data,protocol.target)
    tasks=[asyncio.create_task(outbound()),asyncio.create_task(inbound())]
    try:
        done,_=await asyncio.wait(tasks,return_when=asyncio.FIRST_COMPLETED)
        for task in done:task.result()
    finally:
        for task in tasks:task.cancel()
        await asyncio.gather(*tasks,return_exceptions=True)
        transport.close();writer.close();await writer.wait_closed()
async def serve(args):
    ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.minimum_version=ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(args.cert,args.key)
    relay=Relay()
    server=await asyncio.start_server(relay.handle,args.bind,args.port,ssl=ctx,limit=2048,ssl_handshake_timeout=10)
    print(f'Emberwild relay listening on TCP {args.port}',flush=True)
    async with server:await server.serve_forever()
def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    srv=sub.add_parser('serve');srv.add_argument('--bind',default='0.0.0.0');srv.add_argument('--port',type=int,default=7443);srv.add_argument('--cert',required=True);srv.add_argument('--key',required=True)
    cli=sub.add_parser('connect');cli.add_argument('--server',required=True);cli.add_argument('--port',type=int,default=7443);cli.add_argument('--room',required=True);cli.add_argument('--role',choices=['host','guest'],required=True);cli.add_argument('--ca');cli.add_argument('--local-port',type=int,default=17777);cli.add_argument('--game-port',type=int,default=7777)
    sub.add_parser('room')
    args=parser.parse_args()
    if args.command=='room':print(secrets.token_urlsafe(24));return
    if args.command=='connect' and len(args.room)<16:parser.error('Use a random room code of at least 16 characters (run room to generate one).')
    try:asyncio.run(serve(args) if args.command=='serve' else bridge(args))
    except KeyboardInterrupt:pass
    except (OSError,RuntimeError,asyncio.IncompleteReadError,asyncio.TimeoutError) as error:parser.exit(1,f'Connection closed: {error}\nRestart the helper and rejoin the camp.\n')
if __name__=='__main__':main()
