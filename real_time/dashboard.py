import serial
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets, QtCore
import collections
import numpy as np

# === CONFIG ===
PORT = "/dev/ttyACM0"  # CHANGE THIS to your serial port
BAUD = 1000000
WINDOW_SIZE = 400      # Number of samples visible on plots

# --- SENSOR AND SCRIPT TIMING CONFIG ---
TIMER_UPDATE_MS = 10    # GUI update speed in milliseconds

# --- SIGNAL PROCESSING CONFIG ---
EMA_ALPHA = 0.03
MOVING_AVG_WINDOW = 30

# =================================================================
# --- NEW STEP COUNTER CONFIG (Using Your Optimal Parameters) ---
# =================================================================
# Reconstructing the bounds from your optimization results
min_peak_interval = 40
max1_min_diff_bounds = (0.0169, 0.0169 + 0.4281)  # (lower, lower + delta)
max2_min_diff_bounds = (0.0000, 0.0000 + 0.2362)
max1_max2_diff_bounds = (0.0000, 0.0000 + 0.2641) # This is an upper bound on the difference

# --- Open Serial ---
try:
    ser = serial.Serial(PORT, BAUD, timeout=0.001)
    print(f"Serial port {PORT} opened successfully.")
except serial.SerialException as e:
    print(f"Error opening serial port {PORT}: {e}")
    exit()

# === UI Setup ===
app = QtWidgets.QApplication.instance(); app = QtWidgets.QApplication([]) if app is None else app
win = pg.GraphicsLayoutWidget(title="Step Detection using Optimized Difference-Based Algorithm")
win.resize(1600, 900)
win.show()

# --- Plotting Buffers ---
buffer_ax, buffer_ay, buffer_az = [collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE) for _ in range(3)]
buffer_ax_lp, buffer_ay_lp, buffer_az_lp = [collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE) for _ in range(3)]
buffer_mag_lp = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_mag_lp_avg = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_step_wave = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)

# =================================================================
# PLOT SETUP (Threshold lines removed)
# =================================================================
# TOP ROW
plot_ax = win.addPlot(title="Accel X vs. EMA Filtered"); plot_ay = win.addPlot(title="Accel Y vs. EMA Filtered"); plot_az = win.addPlot(title="Accel Z vs. EMA Filtered")
plot_ax.setYRange(-3, 3); plot_ay.setYRange(-3, 3); plot_az.setYRange(-3, 3)
curve_ax = plot_ax.plot(pen='b'); curve_ay = plot_ay.plot(pen='b'); curve_az = plot_az.plot(pen='b')
curve_ax_lp = plot_ax.plot(pen='y', width=2); curve_ay_lp = plot_ay.plot(pen='y', width=2); curve_az_lp = plot_az.plot(pen='y', width=2)
win.nextRow()

# SECOND ROW
plot_mag = win.addPlot(title="Sum Square of Accel_EMA vs. Moving Average", colspan=2)
plot_mag.setYRange(-0.2, 0.2)
curve_mag_lp = plot_mag.plot(pen='y', width=2); curve_mag_lp_avg = plot_mag.plot(pen='r', width=2)

plot_step = win.addPlot(title="Final Signal & Step Detection")
plot_step.setYRange(-0.2, 0.2)
curve_final_signal = plot_step.plot(pen='r', width=2)
curve_step_wave = plot_step.plot(pen='g', width=2, fillLevel=0, brush=(50,205,50,100))
step_text = pg.TextItem(text="Steps: 0", color=(255, 255, 255), anchor=(0,0))
plot_step.addItem(step_text)
step_text.setPos(5, -0.19)

# === EMA Filter State ===
last_ax_lp, last_ay_lp, last_az_lp = 0.0, 0.0, 0.0

# =================================================================
# --- NEW: Real-Time State Machine for Step Detection ---
# =================================================================
step_count = 0
sample_count = 0
state = "LOOKING_FOR_FIRST_MAX"
candidate_first_max_index, candidate_min_index = -1, -1
candidate_first_max_val, candidate_min_val = 0.0, 0.0
# A small buffer is needed to detect peaks in real-time
signal_buffer = collections.deque([0.0]*3, maxlen=3)

def update():
    global last_ax_lp, last_ay_lp, last_az_lp, sample_count, step_count
    global state, candidate_first_max_index, candidate_min_index
    global candidate_first_max_val, candidate_min_val

    line = ser.readline().decode(errors='ignore').strip()
    if not line: return
    try:
        vals = list(map(float, line.split(',')))
        if len(vals) != 6: return
        
        sample_count += 1
        ax, ay, az, _, _, _ = vals

        # --- 1 & 2. Signal Processing (EMA, Magnitude, SMA) ---
        ax_lp = (EMA_ALPHA * ax) + (1 - EMA_ALPHA) * last_ax_lp; last_ax_lp = ax_lp
        ay_lp = (EMA_ALPHA * ay) + (1 - EMA_ALPHA) * last_ay_lp; last_ay_lp = ay_lp
        az_lp = (EMA_ALPHA * az) + (1 - EMA_ALPHA) * last_az_lp; last_az_lp = az_lp
        mag_lp = np.sqrt(ax_lp**2 + ay_lp**2 + az_lp**2) - 1.0
        buffer_mag_lp.append(mag_lp)
        temp_deque_for_avg = collections.deque(list(buffer_mag_lp)[-MOVING_AVG_WINDOW:], maxlen=MOVING_AVG_WINDOW)
        mag_lp_avg = sum(temp_deque_for_avg) / len(temp_deque_for_avg) if temp_deque_for_avg else 0.0
        
        # --- 3. Real-Time Peak Detection and State Machine Logic ---
        signal_buffer.append(mag_lp_avg)
        if sample_count < 3: return # Need at least 3 points to detect a peak
        
        p_old, p_mid, p_new = signal_buffer[0], signal_buffer[1], signal_buffer[2]
        current_peak_index = sample_count - 2 # The index of the middle point
        step_pulse_value = 0.0

        # --- Check for local maximum (positive peak) ---
        if p_mid > p_old and p_mid > p_new:
            if state == "LOOKING_FOR_FIRST_MAX":
                state = "LOOKING_FOR_MIN"
                candidate_first_max_index, candidate_first_max_val = current_peak_index, p_mid
            elif state == "LOOKING_FOR_MIN":
                candidate_first_max_index, candidate_first_max_val = current_peak_index, p_mid
            elif state == "LOOKING_FOR_SECOND_MAX":
                diff1 = candidate_first_max_val - candidate_min_val
                diff2 = p_mid - candidate_min_val
                diff3 = abs(candidate_first_max_val - p_mid)
                width = current_peak_index - candidate_first_max_index
                
                if (width >= min_peak_interval and
                    max1_min_diff_bounds[0] <= diff1 <= max1_min_diff_bounds[1] and
                    max2_min_diff_bounds[0] <= diff2 <= max2_min_diff_bounds[1] and
                    max1_max2_diff_bounds[0] <= diff3 <= max1_max2_diff_bounds[1]):
                    step_count += 1
                    step_text.setText(f"Steps: {step_count}")
                    step_pulse_value = plot_step.getAxis('left').range[1] * 0.9
                    state = "LOOKING_FOR_MIN"
                    candidate_first_max_index, candidate_first_max_val = current_peak_index, p_mid
                else:
                    state = "LOOKING_FOR_MIN"
                    candidate_first_max_index, candidate_first_max_val = current_peak_index, p_mid

        # --- Check for local minimum (negative peak) ---
        if p_mid < p_old and p_mid < p_new:
            if state == "LOOKING_FOR_MIN":
                state = "LOOKING_FOR_SECOND_MAX"
                candidate_min_index, candidate_min_val = current_peak_index, p_mid
        
        # --- Update Buffers and Curves ---
        buffer_ax.append(ax); buffer_ay.append(ay); buffer_az.append(az)
        buffer_ax_lp.append(ax_lp); buffer_ay_lp.append(ay_lp); buffer_az_lp.append(az_lp)
        buffer_mag_lp_avg.append(mag_lp_avg); buffer_step_wave.append(step_pulse_value)
        
        curve_ax.setData(list(buffer_ax)); curve_ax_lp.setData(list(buffer_ax_lp))
        curve_ay.setData(list(buffer_ay)); curve_ay_lp.setData(list(buffer_ay_lp))
        curve_az.setData(list(buffer_az)); curve_az_lp.setData(list(buffer_az_lp))
        curve_mag_lp.setData(list(buffer_mag_lp)); curve_mag_lp_avg.setData(list(buffer_mag_lp_avg))
        curve_final_signal.setData(list(buffer_mag_lp_avg)); curve_step_wave.setData(list(buffer_step_wave))

    except (ValueError, IndexError): pass

# === Timer ===
timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(TIMER_UPDATE_MS)

pg.exec()
ser.close()
print("Serial port closed.")