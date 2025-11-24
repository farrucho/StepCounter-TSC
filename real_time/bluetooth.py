import asyncio
from bleak import BleakClient

# CHANGE THIS to your Arduino’s BLE MAC address:
ADDRESS = "CA:2E:65:03:DD:B6"

# Your characteristic UUID
CHAR_UUID = "506cad0b-684a-4666-91c7-56d4490b4acc"


# Callback for incoming BLE notifications
def handle_notification(sender, data):
    try:
        text = data.decode("utf-8").strip()
        print(f"Received: {text}")
    except:
        print(f"Raw data: {data}")


async def main():
    print("Connecting to", ADDRESS)
    async with BleakClient(ADDRESS) as client:
        if not client.is_connected:
            print("Failed to connect.")
            return
        
        print("Connected! Enabling notifications...")

        await client.start_notify(CHAR_UUID, handle_notification)

        print("Listening... (Press CTRL+C to quit)")
        while True:
            await asyncio.sleep(1)


# Run
asyncio.run(main())
