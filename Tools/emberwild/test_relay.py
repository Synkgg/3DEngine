"""TLS + actual loopback UDP bridge integration. Test dependency: cryptography."""
import asyncio, contextlib, datetime, hashlib, json, ssl, tempfile, unittest
from pathlib import Path
from types import SimpleNamespace
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID
from relay import Relay, bridge, packet, send
class Echo(asyncio.DatagramProtocol):
    def connection_made(self,transport):self.transport=transport
    def datagram_received(self,data,addr):self.transport.sendto(b'host:'+data,addr)
class Inbox(asyncio.DatagramProtocol):
    def __init__(self):self.messages=asyncio.Queue()
    def datagram_received(self,data,addr):self.messages.put_nowait(data)
class Tests(unittest.IsolatedAsyncioTestCase):
    async def test_tls_room_and_udp(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
            name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'localhost')]);now=datetime.datetime.now(datetime.timezone.utc)
            cert=(x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(minutes=1)).not_valid_after(now+datetime.timedelta(hours=1)).add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]),False).sign(key,hashes.SHA256()))
            (root/'cert.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM));(root/'key.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
            ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(root/'cert.pem',root/'key.pem')
            relay=Relay();server=await asyncio.start_server(relay.handle,'127.0.0.1',0,ssl=ctx,limit=2048)
            port=server.sockets[0].getsockname()[1];clientctx=ssl.create_default_context(cafile=str(root/'cert.pem'))
            writers=[]
            async def peer(room,role):
                r,w=await asyncio.open_connection('127.0.0.1',port,ssl=clientctx,server_hostname='localhost');writers.append(w)
                w.write((json.dumps({'room':hashlib.sha256(room.encode()).hexdigest(),'role':role})+'\n').encode());await w.drain()
                return r,w,await r.readline()
            try:
                hr,hw,ok=await peer('room-a','host');self.assertEqual(ok,b'OK\n')
                gr,gw,ok=await peer('room-a','guest');self.assertEqual(ok,b'OK\n')
                _,_,busy=await peer('room-a','host');self.assertEqual(busy,b'BUSY\n')
                ir,iw,_=await peer('isolated','guest')
                await send(hw,b'hello');self.assertEqual(await asyncio.wait_for(packet(gr),1),b'hello')
                await send(gw,b'reply');self.assertEqual(await asyncio.wait_for(packet(hr),1),b'reply')
                with self.assertRaises(asyncio.TimeoutError):await asyncio.wait_for(packet(ir),.1)
                # Exercise both shipping bridge roles through TLS and real UDP.
                loop=asyncio.get_running_loop();echo,_=await loop.create_datagram_endpoint(Echo,local_addr=('127.0.0.1',0));gameport=echo.get_extra_info('sockname')[1]
                inbox=Inbox();guest,_=await loop.create_datagram_endpoint(lambda:inbox,local_addr=('127.0.0.1',0))
                # Reserve a loopback port, then release immediately for the guest helper.
                import socket
                with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reserve:reserve.bind(('127.0.0.1',0));localport=reserve.getsockname()[1]
                common=dict(server='localhost',port=port,ca=str(root/'cert.pem'),room='integration-room-secret',game_port=gameport,local_port=localport)
                tasks=[asyncio.create_task(bridge(SimpleNamespace(role=role,**common))) for role in ('host','guest')]
                try:
                    await asyncio.sleep(.2)
                    response=None
                    for _ in range(20):
                        guest.sendto(b'engine-packet',('127.0.0.1',localport))
                        try:response=await asyncio.wait_for(inbox.messages.get(),.1);break
                        except asyncio.TimeoutError:pass
                    self.assertEqual(response,b'host:engine-packet')
                finally:
                    for task in tasks:task.cancel()
                    await asyncio.gather(*tasks,return_exceptions=True);echo.close();guest.close()
            finally:
                for w in writers:w.close()
                for w in writers:
                    with contextlib.suppress(OSError):await w.wait_closed()
                server.close();await server.wait_closed()
                for _ in range(20):
                    if not relay.rooms:break
                    await asyncio.sleep(.01)
                self.assertEqual(relay.rooms,{})
if __name__=='__main__':unittest.main()

