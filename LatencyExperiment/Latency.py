import asyncio
import time
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0"
CHARACTERISTIC_UUID = "abcdef12-3456-7890-abcd-ef1234567890"
BLE_DEVICE_NAME = "Tapetics_Controller"

async def scan_and_connect():
    print("Scanning for BLE devices...")
    devices = await BleakScanner.discover()
    for device in devices:
        print(f"Found device: {device.name}, Address: {device.address}")
        if device.name == BLE_DEVICE_NAME:
            print(f"Target device '{BLE_DEVICE_NAME}' found!")
            return device.address
    print(f"Target device '{BLE_DEVICE_NAME}' not found.")
    return None

async def measure_latency(device_address, num_measurements=100):
    latencies = []
    async with BleakClient(device_address) as client:
        if not client.is_connected:
            print("Failed to connect to the device.")
            return

        print("Connected to the device. Measuring latency...")

        # 100回繰り返して交互にコマンドを送信
        for i in range(num_measurements):
            if i % 2 == 0:
                command = b'1,50,50,0,0'
            else:
                command = b'1,0,0,0,0'
                
            await asyncio.sleep(0.5)
            
            round_trip_latency = await send_command(client, command)
            latencies.append(round_trip_latency)
        
        # 平均レイテンシの計算
        if latencies:
            avg_latency = sum(latencies) / len(latencies)
            print(f"Average round-trip latency: {avg_latency:.2f} ms")
        else:
            print("No valid latencies measured.")

async def send_command(client, command):
    try:
        start_time = time.time()
        await client.write_gatt_char(CHARACTERISTIC_UUID, command)  # コマンド送信
        response = await client.read_gatt_char(CHARACTERISTIC_UUID)  # 応答読み取り
        end_time = time.time()

        # レイテンシ計算
        round_trip_latency = (end_time - start_time) * 1000  # ミリ秒に変換
        print(f"Command: {command.decode('utf-8')}, Response: {response.decode('utf-8')}")
        print(f"Round-trip latency: {round_trip_latency:.2f} ms")
        return round_trip_latency
    except Exception as e:
        print(f"Error during command execution: {e}")
        return None

async def main():
    device_address = await scan_and_connect()
    if device_address:
        await measure_latency(device_address)
    else:
        print("Device not found. Exiting.")

if __name__ == "__main__":
    asyncio.run(main())
