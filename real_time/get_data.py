import asyncio
from bleak import BleakClient
import threading
import time

# ==============================
# CONFIG
# ==============================
ADDRESS = "CA:2E:65:03:DD:B6"  # <<< change to your Arduino BLE MAC address
CHAR_UUID = "506cad0b-684a-4666-91c7-56d4490b4acc"

DATA_FILENAME = "../data/sensor_data.csv"
MANUAL_SAMPLES_FILENAME = "../data/manual_step_samples.csv"

# Shared state
sample_counter = 0
stop_event = threading.Event()


# ==========================================================
# BLE LOGGER THREAD — receives BLE notifications continuously
# ==========================================================
def start_ble_logger():
    asyncio.run(ble_logger_task())


async def ble_logger_task():
    global sample_counter

    print(f"[BLE] Connecting to {ADDRESS}...")

    while not stop_event.is_set():
        try:
            async with BleakClient(ADDRESS) as client:
                if not client.is_connected:
                    print("[BLE] Failed to connect. Retrying in 1s...")
                    await asyncio.sleep(1)
                    continue

                print("[BLE] Connected!")
                print("[BLE] Subscribing to notifications...")

                # Open file outside callback, keep it open for entire session
                with open(DATA_FILENAME, "w") as f:
                    f.write("ax,ay,az,state,ultrasound,stepdetected\n")

                    def handle(sender, data):
                        global sample_counter
                        try:
                            text = data.decode("utf-8").strip()
                            f.write(text + "\n")
                            f.flush()
                            sample_counter += 1
                        except Exception as e:
                            print(f"[BLE] Decode error: {e}")

                    await client.start_notify(CHAR_UUID, handle)
                    print("[BLE] Logging started.")

                    # Keep the connection alive and logging running until stop event
                    while not stop_event.is_set():
                        await asyncio.sleep(0.1)

                    print("[BLE] Stop requested. Stopping notifications and closing file...")
                    await client.stop_notify(CHAR_UUID)
                    # File closed automatically by 'with' context

                return  # exit after graceful stop

        except Exception as e:
            print(f"[BLE] Error: {e}. Reconnecting in 1s...")
            await asyncio.sleep(1)


# ==========================================================
# MAIN THREAD — handles user input ("press ENTER when you step")
# ==========================================================
if __name__ == "__main__":

    print("Starting BLE logging thread...")
    ble_thread = threading.Thread(target=start_ble_logger)
    ble_thread.start()

    # Wait a bit for BLE thread to connect and start logging
    time.sleep(2)

    print("\n" + "=" * 50)
    print("      DATA RECORDING HAS STARTED (BLE MODE)")
    print("=" * 50)
    print("\n >> Press [ENTER] each time you take a step.")
    print(f" >> Step sample numbers will be saved to '{MANUAL_SAMPLES_FILENAME}'.")
    print("\n >> Type 'q' and press [ENTER] to quit.\n")

    step_count = 0

    with open(MANUAL_SAMPLES_FILENAME, "w") as f_steps:
        f_steps.write("sample_number\n")

        while True:
            user_input = input()
            if user_input.lower() == 'q':
                print("[MAIN] Quit received. Stopping BLE logging...")
                break

            current_sample = sample_counter
            f_steps.write(f"{current_sample}\n")
            f_steps.flush()
            step_count += 1

            print(f"--- Step {step_count} recorded at SAMPLE NUMBER: {current_sample} ---")

    # graceful shutdown
    stop_event.set()
    ble_thread.join()

    print("\nRecording stopped.")
    print(f"Sensor Data: {DATA_FILENAME}")
    print(f"Manual Steps: {MANUAL_SAMPLES_FILENAME}")
