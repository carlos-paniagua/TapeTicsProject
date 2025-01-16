import asyncio
from bleak import BleakScanner, BleakClient
from pythonosc import udp_client
from pythonosc.osc_message_builder import OscMessageBuilder
import sys
import numpy as np
from datetime import datetime
import os

SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0"
CHARACTERISTIC_UUID = "abcdef12-3456-7890-abcd-ef1234567890"
DEVICE_NAME = "Tapetics_Controller"

# UDP settings
UDP_IP = "127.0.0.1"
UDP_PORT = 12345

# Max8にメッセージを送り返すための設定
UDP_IP_SEND = "127.0.0.1"
UDP_PORT_SEND = 54321

class MessageFormatter:
    def format_message(self, data):
        # print("Received data:", data)

        # Initialize the result variables
        # seconds = data[0]
        node_info = []

        # Temporary list to accumulate node data for each color
        current_node = []

        # Iterate through the data starting from the second element
        for i in range(1, len(data)):
            if data[i] in ['R', 'G', 'B']:  # Check if it's a color
                if current_node:  # If current node has data, append it to node_info
                    color = current_node[0]
                    node_number = current_node[1]
                    # Create points list by pairing the remaining values
                    points = []
                    for j in range(2, len(current_node), 2):
                        if j + 1 < len(current_node):  # Ensure we don't go out of range
                            points.append((current_node[j], current_node[j + 1]))
                    node_info.append({'color': color, 'node_number': node_number, 'points': points})
                current_node = [data[i]]  # Start a new node list with the color
            elif isinstance(data[i], str) and data[i] == 'node':  # Skip 'node' string
                continue
            elif data[i] == 'end':  # Skip 'end' for now
                continue
            else:
                current_node.append(data[i])  # Add node data

        # Add the last node to node_info if it exists
        if current_node:
            color = current_node[0]
            node_number = current_node[1]
            # Check if there are additional values for points
            points = []
            for j in range(2, len(current_node), 2):
                if j + 1 < len(current_node):  # Ensure we don't go out of range
                    points.append((current_node[j], current_node[j + 1]))
            node_info.append({'color': color, 'node_number': node_number, 'points': points})

        # Initialize list to hold node-wise converted data
        node_data = {}

        # Convert each node's data to the desired format
        for node in node_info:
            points = node.get('points', [])
            color = node['color']
            node_number = node['node_number']
            
            node_data[node_number] = []  # Initialize list for each node
            
            if points:
                # For each point, convert it to the format:
                for i in range(len(points) - 1):
                    y1, x1 = points[i]
                    y2, x2 = points[i + 1]
                    
                    # Check if range can be interpolated at 0.1 interval
                    if x2 - x1 >= 0.1:
                        # Interpolate x and y values
                        x_values = np.linspace(x1, x2, int((x2 - x1) / 0.1) + 1)
                        y_values = np.linspace(y1, y2, len(x_values))
                        
                        for x, y in zip(x_values, y_values):
                            intensity = int(round(y, 1))  # Intensity
                            # Set RGB based on color
                            if color == 'R':
                                R, G, B = intensity, 0, 0
                            elif color == 'G':
                                R, G, B = 0, intensity, 0
                            elif color == 'B':
                                R, G, B = 0, 0, intensity
                            else:
                                R, G, B = 0, 0, 0  # Default to black if unknown color

                            # Append formatted string
                            node_data[node_number].append(f"{node_number},{intensity},{R},{G},{B}")

        # Flatten node data in interleaved format
        converted_info = []
        max_length = max(len(v) for v in node_data.values())

        for i in range(max_length):
            for node_id in sorted(node_data.keys()):
                if i < len(node_data[node_id]):
                    converted_info.append(node_data[node_id][i])
            converted_info.append('delay')

        # Output the converted info
        # for entry in converted_info:
        #     print(entry)
        print(converted_info)
        return converted_info
    
    def save_to_file(self, converted_info, filename="output.txt"):
        """Save converted_info to a text file, one entry per line."""
        try:
            with open(filename, "w") as file:
                for entry in converted_info:
                    file.write(entry + "\n")
            print(f"Data saved to {filename}")
        except Exception as e:
            print(f"Error saving to file: {e}")
class UDPServerProtocol(asyncio.DatagramProtocol):
    def __init__(self, message_formatter):
        self.message_formatter = message_formatter
        directory = os.path.dirname(os.path.realpath(__file__))
        # ファイル名を現在の日時に基づいて設定
        current_time = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.filename = f"VibrationPattern_{current_time}.txt"
        self.received_data = []
        self.reading_enabled = False
        self.chunks = []  # 分割されたデータチャンク
        self.current_chunk_index = 0  # 現在のチャンクインデックス
        super().__init__()

    def connection_made(self, transport):
        self.transport = transport
        print(f"UDP server listening on {UDP_IP}:{UDP_PORT}")

    def datagram_received(self, data, addr):
        try:
            message = data.decode('utf-8', errors='replace').strip()
            msg = self.parse_message(message)

            if msg == 'set':
                self.reading_enabled = True
                self.current_chunk_index = 0
                self.chunks = []
                self.received_data = []  # ここで received_data をクリア
                print("Reading data.")
                return
            
            elif msg == 'end':
                self.reading_enabled = False
                self.received_data.append(msg)
                print("press 'send' to send to M5stack")
                return
            
            if msg == 'send':
                formatted_messages = self.message_formatter.format_message(self.received_data)
                self.message_formatter.save_to_file(formatted_messages, self.filename)
                chunk_size = max(1, 100) 
                self.chunks = self.split_into_chunks(formatted_messages, chunk_size=chunk_size)  
                self.send_chunk(self.current_chunk_index)
                send_osc_message("/endSend", "1")
                return
            
            elif msg == 'run':
                if self.current_chunk_index < len(self.chunks):
                    asyncio.create_task(write_to_ble(msg)) 
                    self.current_chunk_index += 1
                    asyncio.create_task(self.send_remaining_chunks())
                return
            
            elif msg in ('wave', 'random', 'shock'):
                asyncio.create_task(write_to_ble([msg]))
                return
            
            elif self.reading_enabled:
                if msg is not None:
                    self.received_data.append(msg)

        except Exception as e:
            print(f"Error processing UDP message: {e}")
    
    def split_into_chunks(self, messages, chunk_size):
        # print("Splitted to chunks")
        chunks = [messages[i:i + chunk_size] for i in range(0, len(messages), chunk_size)]
        print(f'number of chunks: {len(chunks)}')
        # print(f"Chunks created: {chunks}")
        return chunks
    
    def send_chunk(self, index):
        """指定されたインデックスのチャンクを送信する"""
        if index < len(self.chunks):
            # print(f"Sending chunk {index}: {self.chunks[index]}")
            asyncio.create_task(write_to_ble(self.chunks[index]))
    
    async def send_remaining_chunks(self):
        """残りのチャンクを順次送信する"""
        while self.current_chunk_index < len(self.chunks):
            # await asyncio.sleep(0.1)
            self.send_chunk(self.current_chunk_index)
            self.current_chunk_index += 1
        # 最後のチャンクが送信された後にメッセージを表示
        print("All messages processed. FINISH")
    
    def parse_message(self, message):
        parts = message.split(',')
        for part in parts:
            part = part.replace('\x00', '').strip()
            try:
                # 整数か少数かを確認して数値に変換
                if '.' in part:
                    if part == "0.":
                        part = "0.01"
                    value = float(part)
                    # print(f"Parsed float value: {value}")

                    return float(part)
                else:
                    value = float(part)
                    # print(f"Parsed int value: {value}")
                    if value == 0:
                        value = 0.01
                    return int(part)
            except ValueError:
                # 数値でない場合の処理
                if part in ('R', 'G', 'B', 'run', 'set', 'send', 'node', 'end', 'wave', 'random', 'shock'):
                    # print(f"Parsed string: {part}")
                    return part
        return None

async def udp_server(message_formatter, filename):
    loop = asyncio.get_running_loop()
    connect = loop.create_datagram_endpoint(lambda: UDPServerProtocol(message_formatter), local_addr=(UDP_IP, UDP_PORT))

    transport, protocol = await connect
    try:
        await asyncio.sleep(float('inf'))  # Keep the server running
    finally:
        transport.close()

async def scan_devices():
    try:
        devices = await BleakScanner.discover()
        print("Scanned devices:")
        for i, d in enumerate(devices):
            uuids = d.metadata.get('uuids', []) if 'uuids' in d.metadata else []
            if d.name is not None:
                print(f"{i + 1}: address: {d.address}, name: {d.name}, uuid: {uuids}")
        return devices
    except Exception as e:
        print("Error during scanning:", e)
        return []

async def select_device(devices):
    try:
        target_device_name = DEVICE_NAME
        selected_device = None
        for device in devices:
            if device.name == target_device_name:
                selected_device = device
                break
        if selected_device is None:
            print(f"Target device '{DEVICE_NAME}' not found.")  # 追加メッセージ
            return None
        print(f"Auto-selected device: {selected_device.name}")
        return selected_device
    except Exception as e:
        print("Error selecting device:", e)

async def connect_and_send_data(selected_device):
    global client, characteristic
    try:
        # 初回接続
        async with BleakClient(selected_device.address) as client:
            services = await client.get_services()
            characteristic = None
            for service in services:
                for char in service.characteristics:
                    if char.uuid == CHARACTERISTIC_UUID:
                        characteristic = char
                        break
                if characteristic:
                    break
            if not characteristic:
                raise Exception("Characteristic not found.")
            print("BLE setup complete.")

            # 接続中は処理を続ける
            while client.is_connected:
                await asyncio.sleep(1)  # 1秒ごとに接続状態をチェック

            # 切断された場合の処理
            print("Connection lost, attempting to reconnect...")
            await reconnect_and_send_data(selected_device)  # 再接続を試みる

    except Exception as e:
        print("Error connecting or sending data:", e)

async def reconnect_and_send_data(selected_device):
    """接続が切断された後、再接続を試みる"""
    try:
        print(f"Attempting to reconnect to {selected_device.name}...")
        while True:
            # 再接続を試みる
            async with BleakClient(selected_device.address) as client:
                print("Reconnected successfully.")
                await connect_and_send_data(selected_device)  # 再接続後にデータを送信
                break  # 接続が成功したらループを抜ける
    except Exception as e:
        print(f"Failed to reconnect: {e}")
        await asyncio.sleep(2)  # 再接続失敗後は少し待ってから再試行

async def write_to_ble(messages):
    global client, characteristic
    try:
        if client and client.is_connected and characteristic:
            if isinstance(messages, list):
                concatenated_messages = "\n".join(messages)
                message_bytes = concatenated_messages.encode("utf-8")
                print(f"Formatted messages contain {len(messages)} lines.")
                print(f"Single message size: {len(message_bytes)} bytes")
                await client.write_gatt_char(CHARACTERISTIC_UUID, message_bytes)
                # print(f"Data sent to BLE device:\n{concatenated_messages}")
                await client.write_gatt_char(CHARACTERISTIC_UUID, "endSend".encode("utf-8"))
            else:
                message_bytes = messages.encode("utf-8")
                print(f"Formatted messages contain {len(messages)} lines.")
                print(f"Single message size: {len(message_bytes)} bytes")
                await client.write_gatt_char(CHARACTERISTIC_UUID, message_bytes)
                # print(f"Data sent to BLE device: {messages}")
        else:
            print("BLE client or characteristic is not set.")
            await connect_and_send_data(client)
    except Exception as e:
        print("Error sending data to BLE device:", e)

async def print_formatted_messages_count(formatted_messages):
    num_lines = len(formatted_messages)
    print(f"Formatted messages contain {num_lines} lines.")

def send_osc_message(address, message):
    try:
        client = udp_client.SimpleUDPClient(UDP_IP_SEND, UDP_PORT_SEND)
        msg = OscMessageBuilder(address=address)
        msg.add_arg(message)
        msg = msg.build()
        client.send(msg)
        print(f"Sent OSC message: {message} to address: {address}")
    except Exception as e:
        print(f"Error sending OSC message: {e}")

async def main():
    global client, characteristic
    try:
        # 現在のスクリプトが置かれているディレクトリを取得
        directory = os.path.dirname(os.path.realpath(__file__))
        
        # 現在の日時を取得し、ファイル名に付け加える
        current_time = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.join(directory, f"VibrationPattern_{current_time}.txt")
        message_formatter = MessageFormatter()
        
        while True:  # スキャンを繰り返し行うループ
            devices = await scan_devices()
            if not devices:
                print("No devices found, retrying...")
                await asyncio.sleep(2)  # 2秒待機してから再度スキャン
                continue  # 次のスキャンを実行
            selected_device = await select_device(devices)
            if not selected_device:
                print("Device selection failed, retrying...")
                await asyncio.sleep(2)
                continue  # 次のスキャンを実行

            asyncio.create_task(connect_and_send_data(selected_device))

            # Start UDP server
            await udp_server(message_formatter, filename)
            break  # デバイスが見つかり、接続されたらループを抜ける
        
    except Exception as e:
        print("Error in main function:", e)

if __name__ == "__main__":
    client = None
    characteristic = None
    asyncio.run(main())
