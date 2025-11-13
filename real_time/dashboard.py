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

# --- STEP COUNTER CONFIG (with Positive and Negative thresholds) ---
POS_PEAK_THRESH = 0.02     # Threshold for the positive peak
NEG_PEAK_THRESH = -0.02    # Threshold for the subsequent negative peak
MIN_STEP_INTERVAL_SAMPLES = 30 # Minimum number of samples between steps

# --- Open Serial ---
try:
    ser = serial.Serial(PORT, BAUD, timeout=0.001)
    print(f"Serial port {PORT} opened successfully.")
except serial.SerialException as e:
    print(f"Error opening serial port {PORT}: {e}")
    exit()

# === UI Setup ===
app = QtWidgets.QApplication.instance()
if app is None:
    app = QtWidgets.QApplication([])

win = pg.GraphicsLayoutWidget(title="Step Detection Signal Processing Pipeline (Horizontal Layout)")
win.resize(1600, 900)
win.show()

# --- Plotting Buffers ---
buffer_ax = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_ay = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_az = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_ax_lp = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_ay_lp = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_az_lp = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_mag_lp = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_mag_lp_avg = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)
buffer_step_wave = collections.deque([0.0]*WINDOW_SIZE, maxlen=WINDOW_SIZE)

# =================================================================
# PLOT SETUP (MODIFIED LAYOUT)
# =================================================================
# TOP ROW: Stays the same, occupying 3 columns
plot_ax = win.addPlot(title="Accel X vs. EMA Filtered"); plot_ay = win.addPlot(title="Accel Y vs. EMA Filtered"); plot_az = win.addPlot(title="Accel Z vs. EMA Filtered")
plot_ax.setYRange(-3, 3); plot_ay.setYRange(-3, 3); plot_az.setYRange(-3, 3)
curve_ax = plot_ax.plot(pen=pg.mkPen('b', width=1)); curve_ay = plot_ay.plot(pen=pg.mkPen('b', width=1)); curve_az = plot_az.plot(pen=pg.mkPen('b', width=1))
curve_ax_lp = plot_ax.plot(pen=pg.mkPen('y', width=2)); curve_ay_lp = plot_ay.plot(pen=pg.mkPen('y', width=2)); curve_az_lp = plot_az.plot(pen=pg.mkPen('y', width=2))

# --- MODIFIED: Move to the next row just once ---
win.nextRow()

# --- MODIFIED: Place the next two plots side-by-side in the second row ---
# MIDDLE PLOT (now on the left of the second row, spanning 2/3 of the width)
plot_mag = win.addPlot(title="Sum Square of Accel_EMA vs. Moving Average", colspan=2)
plot_mag.setYRange(-0.2, 0.2)
curve_mag_lp = plot_mag.plot(pen=pg.mkPen('y', width=2)); curve_mag_lp_avg = plot_mag.plot(pen=pg.mkPen('r', width=2))

# BOTTOM PLOT (now on the right of the second row, spanning 1/3 of the width)
plot_step = win.addPlot(title="Final Signal & Step Detection (Pos -> Neg Logic)")
plot_step.setYRange(-0.2, 0.2)
curve_final_signal = plot_step.plot(pen=pg.mkPen('r', width=2))
curve_step_wave = plot_step.plot(pen=pg.mkPen('g', width=2), fillLevel=0, brush=(50,205,50,100))
pos_thresh_line = pg.InfiniteLine(angle=0, pos=POS_PEAK_THRESH, pen=pg.mkPen('w', style=QtCore.Qt.PenStyle.DashLine))
neg_thresh_line = pg.InfiniteLine(angle=0, pos=NEG_PEAK_THRESH, pen=pg.mkPen('c', style=QtCore.Qt.PenStyle.DashLine))
plot_step.addItem(pos_thresh_line); plot_step.addItem(neg_thresh_line)
step_text = pg.TextItem(text="Steps: 0", color=(255, 255, 255), anchor=(0,0))
plot_step.addItem(step_text)
step_text.setPos(5, -0.95)

# === EMA Filter State ===
last_ax_lp, last_ay_lp, last_az_lp = 0.0, 0.0, 0.0

# === Step Logic State Machine ===
step_count = 0
sample_count = 0
last_step_sample = -MIN_STEP_INTERVAL_SAMPLES - 1
waiting_for_neg_peak = False

# === Update Loop (NO CHANGES) ===
def update():
    global last_ax_lp, last_ay_lp, last_az_lp
    global step_count, waiting_for_neg_peak, sample_count, last_step_sample

    line = ser.readline().decode(errors='ignore').strip()
    if not line: return
    try:
        vals = list(map(float, line.split(',')))
        if len(vals) != 6: return
        
        sample_count += 1
        ax, ay, az, gx, gy, gz = vals

        # --- 1. EMA Filter ---
        ax_lp = (EMA_ALPHA * ax) + (1 - EMA_ALPHA) * last_ax_lp; last_ax_lp = ax_lp
        ay_lp = (EMA_ALPHA * ay) + (1 - EMA_ALPHA) * last_ay_lp; last_ay_lp = ay_lp
        az_lp = (EMA_ALPHA * az) + (1 - EMA_ALPHA) * last_az_lp; last_az_lp = az_lp

        # --- 2. & 3. Magnitude and Moving Average ---
        mag_lp = np.sqrt(ax_lp**2 + ay_lp**2 + az_lp**2) - 1.0
        buffer_mag_lp.append(mag_lp)
        temp_deque_for_avg = collections.deque(list(buffer_mag_lp)[-MOVING_AVG_WINDOW:], maxlen=MOVING_AVG_WINDOW)
        mag_lp_avg = sum(temp_deque_for_avg) / len(temp_deque_for_avg) if temp_deque_for_avg else 0.0

        # --- 4. Step Detection ---
        final_signal_val = mag_lp_avg
        step_pulse_value = 0.0

        if not waiting_for_neg_peak:
            if final_signal_val > POS_PEAK_THRESH and (sample_count - last_step_sample) > MIN_STEP_INTERVAL_SAMPLES:
                waiting_for_neg_peak = True
        else:
            if final_signal_val < NEG_PEAK_THRESH:
                step_count += 1
                last_step_sample = sample_count
                step_text.setText(f"Steps: {step_count}")
                step_pulse_value = plot_step.getAxis('left').range[1] * 0.9
                waiting_for_neg_peak = False

        # --- 5. & 6. Update Buffers and Curves ---
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