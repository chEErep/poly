"""Проверка собранных Windows-программ только через localhost."""
from pathlib import Path
import os
import socket
import struct
import subprocess
import tempfile
import threading
import time
import unittest

ROOT=Path(__file__).resolve().parent
CLIENT=ROOT/'build/tcpclient.exe'
SERVER=ROOT/'build/udpserver.exe'


def packet(index, text=b'hello'):
    return struct.pack('!I',index)+b'+70000000001+70000000002'+bytes([12,30,15])+text+b'\0'


class ProtocolTests(unittest.TestCase):
    def test_tcp_long_message_and_fragmented_ack(self):
        with tempfile.TemporaryDirectory() as temp, socket.socket() as listener:
            listener.bind(('127.0.0.1',0)); listener.listen()
            payload='x'*12000
            source=Path(temp)/'input.txt'
            source.write_text('+70000000001 +70000000002 12:30:15 '+payload+'\n',encoding='ascii')
            received=[]
            def serve():
                with listener.accept()[0] as conn:
                    conn.settimeout(5)
                    data=b''
                    while not data.endswith(b'\0'):
                        data+=conn.recv(4096)
                    received.append(data)
                    conn.sendall(b'o'); time.sleep(.05); conn.sendall(b'k')
            thread=threading.Thread(target=serve,daemon=True); thread.start()
            result=subprocess.run([str(CLIENT),f'127.0.0.1:{listener.getsockname()[1]}',str(source)],capture_output=True,timeout=10)
            thread.join(5)
            self.assertEqual(result.returncode,0,result.stdout)
            self.assertEqual(received,[b'put'+packet(0,payload.encode())])

    def test_invalid_port(self):
        result=subprocess.run([str(CLIENT),'127.0.0.1:9000junk','unused.txt'],capture_output=True,timeout=5)
        self.assertNotEqual(result.returncode,0)

    def test_udp_validation_duplicates_and_ack(self):
        with tempfile.TemporaryDirectory() as temp, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
            sock.bind(('127.0.0.1',0)); port=sock.getsockname()[1]
            # Освобождаем выбранный порт перед запуском сервера.
        with tempfile.TemporaryDirectory() as temp:
            proc=subprocess.Popen([str(SERVER),str(port),str(port)],cwd=temp,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            try:
                time.sleep(.3)
                # Некорректные сообщения не должны занимать таблицу клиентов.
                for _ in range(70):
                    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as bad:
                        bad.sendto(b'bad',('127.0.0.1',port))
                with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as client:
                    client.settimeout(2)
                    client.sendto(packet(7),('127.0.0.1',port))
                    self.assertIn(struct.pack('!I',7),client.recv(4096))
                    client.sendto(packet(7),('127.0.0.1',port))
                    client.recv(4096)
                    invalid=bytearray(packet(8)); invalid[28]=25
                    client.sendto(invalid,('127.0.0.1',port))
                    client.settimeout(.15)
                    with self.assertRaises(socket.timeout): client.recv(4096)
                    client.settimeout(2)
                    client.sendto(packet(9,b'stop'),('127.0.0.1',port))
                    client.recv(4096)
                self.assertEqual(proc.wait(timeout=5),0)
                lines=(Path(temp)/'msg.txt').read_text().splitlines()
                self.assertEqual(len(lines),2)
                self.assertTrue(lines[0].endswith('hello'))
                self.assertTrue(lines[1].endswith('stop'))
            finally:
                if proc.poll() is None: proc.terminate(); proc.wait(timeout=5)


if __name__=='__main__':
    unittest.main()
