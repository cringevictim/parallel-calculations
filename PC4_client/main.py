import socket
import threading
import time

SERVER_ADDRESS = '127.0.0.1'
PORT = 27015
BUFFER_SIZE = 1024

def connect_and_send():
    while True:
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_socket.connect((SERVER_ADDRESS, PORT))
            return client_socket
        except socket.error as e:
            print(f"Connection failed: {e}. Retrying in 5 seconds...")
            time.sleep(5)

def handle_response(client_socket):
    while True:
        try:
            response = client_socket.recv(BUFFER_SIZE)
            if response:
                print(f"\nResponse: {response.decode()}")
            else:
                print("Connection closed by server.")
                client_socket.close()
                client_socket = connect_and_send()
        except socket.error as e:
            print(f"Recv failed: {e}")
            client_socket.close()
            client_socket = connect_and_send()

def main():
    client_socket = connect_and_send()
    threading.Thread(target=handle_response, args=(client_socket,), daemon=True).start()

    while True:
        command = input("Enter command (process N / status / exit): ")

        if command == "exit":
            break
        elif command.startswith("process"):
            try:
                _, n = command.split()
                n = int(n)
                client_socket.sendall(command.encode())
            except ValueError:
                print("Invalid command: N must be a number.")
        elif command == "status":
            client_socket.sendall(command.encode())
        else:
            print("Invalid command.")

    client_socket.close()

if __name__ == "__main__":
    main()
