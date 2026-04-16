import socket
import struct
import time

def recvall(sock, n):
    data = bytearray()
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            return None # Сервер закрив з'єднання
        data.extend(packet)
    return bytes(data)

def main():
    host = '127.0.0.1'
    port = 8080
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        print("[Python Client] Connected to server.")
        
        N = 3
        threads = 2
        matrix = [i * 10 for i in range(1, N*N + 1)]
        print(f"[Python Client] Sending matrix: {matrix}")
        
        # 1. Надсилаємо INIT (0x01)
        payload = struct.pack(f'!I I {N*N}i', N, threads, *matrix)
        header = struct.pack('!B I', 0x01, len(payload))
        s.sendall(header + payload)
        
        # Використовуємо recvall замість recv
        resp_hdr = recvall(s, 5)
        if resp_hdr:
            cmd, length = struct.unpack('!B I', resp_hdr)
            if cmd == 0x10: 
                print("[Python Client] INIT acknowledged.")

        # 2. Надсилаємо START (0x02)
        header = struct.pack('!B I', 0x02, 0)
        s.sendall(header)
        
        resp_hdr = recvall(s, 5)
        if resp_hdr:
            cmd, length = struct.unpack('!B I', resp_hdr)
            if cmd == 0x10: 
                print("[Python Client] Server started computations...")

        # 3. Опитування статусу (0x03)
        while True:
            header = struct.pack('!B I', 0x03, 0)
            s.sendall(header)
            
            resp_hdr = recvall(s, 5)
            if not resp_hdr:
                print("[Python Client] Connection lost.")
                break
                
            cmd, length = struct.unpack('!B I', resp_hdr)
            
            # Гарантовано читаємо весь payload!
            payload = recvall(s, length)
            if not payload:
                break
                
            status = payload[0]
            
            if status == 1:
                # Тепер payload[1:] гарантовано містить 36 байт
                result_matrix = struct.unpack(f'!{N*N}i', payload[1:])
                print(f"[Python Client] Done! Transposed matrix: {result_matrix}")
                break
            else:
                print("[Python Client] Still processing... waiting 1 sec")
                time.sleep(1)

if __name__ == "__main__":
    main()