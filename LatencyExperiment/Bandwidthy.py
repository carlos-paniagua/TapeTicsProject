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

async def measure_bandwidth(device_address, num_commands=1000):
    async with BleakClient(device_address) as client:
        if not client.is_connected:
            print("Failed to connect to the device.")
            return

        print("Connected to the device. Measuring bandwidth...")

        start_time = time.time()
        for i in range(num_commands):
            command = f"1,{i % 100},0,0,0".encode('utf-8')
            await client.write_gatt_char(CHARACTERISTIC_UUID, command)
        end_time = time.time()

        elapsed_time = end_time - start_time
        bandwidth = num_commands / elapsed_time
        print(f"Sent {num_commands} commands in {elapsed_time:.2f} seconds.")
        print(f"Estimated bandwidth: {bandwidth:.2f} commands/second")

async def main():
    device_address = await scan_and_connect()
    if device_address:
        await measure_bandwidth(device_address)
    else:
        print("Device not found. Exiting.")

if __name__ == "__main__":
    asyncio.run(main())
