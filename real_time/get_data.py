import serial
import threading
import sys

# === CONFIGURATION ===
# --- Serial Port ---
PORT = "/dev/ttyACM0"
BAUD = 115200 # Match your Arduino's Serial.begin() rate

# --- Output Files ---
DATA_FILENAME = "../data/sensor_data.csv"
MANUAL_SAMPLES_FILENAME = "../data/manual_step_samples.csv"
# ======================


# --- Shared State between threads ---
# stop_event signals the logging thread to stop gracefully
stop_event = threading.Event()
# sample_counter is incremented by the logger and read by the main thread
sample_counter = 0

def log_serial_data(port, baudrate, data_filename):
    """
    This function runs in a separate thread.
    It opens the serial port, writes each line of data to a file,
    and increments a global sample counter.
    """
    global sample_counter
    
    print(f"[Logger Thread] Trying to open serial port {port}...")
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            print(f"[Logger Thread] Serial port opened. Writing data to '{data_filename}'")
            with open(data_filename, "w") as f_data:
                # Write a header for clarity
                # f_data.write("ax,ay,az,gx,gy,gz\n")
                f_data.write("ax,ay,az\n")
                
                while not stop_event.is_set():
                    try:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            # Write the raw sensor data to its file
                            f_data.write(f"{line}\n")
                            # Increment the shared sample counter
                            sample_counter += 1
                    except serial.SerialException:
                        print("[Logger Thread] Error reading from serial port. Exiting.")
                        break

    except serial.SerialException as e:
        print(f"[Logger Thread] FATAL: Could not open serial port {port}: {e}", file=sys.stderr)
        return

    print("[Logger Thread] Stopped.")


# --- Main part of the script ---
if __name__ == "__main__":
    # Create and start the thread that will handle serial logging
    serial_thread = threading.Thread(target=log_serial_data, args=(PORT, BAUD, DATA_FILENAME))
    serial_thread.start()

    # Give the thread a moment to start and check the serial port
    # A short sleep is good practice to ensure the thread is running before we proceed.
    threading.Event().wait(2) 
    if not serial_thread.is_alive():
        print("[Main] Logger thread failed to start, likely a serial port issue. Exiting.")
        sys.exit(1)

    print("\n" + "="*50)
    print("      DATA RECORDING HAS STARTED")
    print("="*50)
    print("\n >> Press [ENTER] each time you take a step.")
    print(f" >> Step sample numbers will be saved to '{MANUAL_SAMPLES_FILENAME}'.")
    print("\n >> Type 'q' and press [ENTER] to quit gracefully.\n")

    step_count = 0
    with open(MANUAL_SAMPLES_FILENAME, "w") as f_steps:
        # Write a header
        f_steps.write("sample_number\n")
        
        while True:
            user_input = input() 
            if user_input.lower() == 'q':
                print("[Main] 'q' received. Shutting down...")
                break
            
            # Read the current sample number from the global variable
            current_sample = sample_counter
            f_steps.write(f"{current_sample}\n")
            step_count += 1
            print(f"--- Step {step_count} recorded at SAMPLE NUMBER: {current_sample} ---")

    # --- Graceful shutdown ---
    print("[Main] Telling logger thread to stop...")
    stop_event.set()
    serial_thread.join()

    print("\nRecording stopped. Your data has been saved.")
    print(f"Sensor Readings: {DATA_FILENAME}")
    print(f"Manual Step Samples: {MANUAL_SAMPLES_FILENAME}")