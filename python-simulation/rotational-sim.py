import numpy as np
import csv

class RotationalOscillator:
    def __init__(self, angle):
        self.angle = angle
        self.angular_velocity = 0.0
        self.damping = 0.1 # simplified friction
        self.power = 10.0 # max power multiplier

    def update(self, control_signal, dt):
        acceleration = (control_signal * self.power) - (self.damping * self.angular_velocity)
        self.angular_velocity += acceleration * dt
        self.angle += self.angular_velocity * dt
        return self.angle

class Sensor:
    def __init__(self, jitter=0.05, drift=0.0001):
        self.jitter = jitter # instant noise
        self.drift = drift # long-term drift
        self.bias = 0.0 # accumulated drift

    def read(self, true_angle):
        self.bias += np.random.normal(0, self.drift)
        return true_angle + self.bias + np.random.normal(0, self.jitter)

class controlStrategy:
    def __init__(self, kp, ki, kd, dt):
        self.kp = kp
        self.ki = ki
        self.kd = kd
        self.dt = dt
        self.prev_error = 0
        self.integral = 0

    def compute(self, strategy, setpoint, measured):
        error = setpoint - measured
        p = self.kp * error
        self.integral += error * self.dt
        self.integral = max(min(self.integral, 1.0), -1.0)
        i = self.ki * self.integral
        d = self.kd * (error - self.prev_error) / self.dt
        self.prev_error = error
        # clamp to [-1, 1] always
        if strategy == "PID":
            return max(min(p + i + d, 1.0), -1.0) 
        elif strategy == "P":
            return max(min(p, 1.0), -1.0)
        elif strategy == "OnOff":
            if error > 0: return 1.0
            elif error < 0: return -1.0
            else: return 0

# MAIN EXECUTION

dt = 0.001  # 1ms for physics calculations
control_dt = 0.01 # 100 Hz control loop
steps = 600000 # 60 seconds
used_strategy = input("Strategy to simulate (OnOff, P or PID): ") # OnOff, P or PID
kp = 0.3
ki = 0.01
kd = 0.02
control_val = 0
swap_setpoint = True # swap setpoint halfway
setpoint = 0.0

rig = RotationalOscillator(np.radians(45))
sensor = Sensor()
control = controlStrategy(kp, ki, kd, control_dt)

data_rows = []

print("Running experiment...")

for i in range(steps):
    if swap_setpoint and (i == steps//2):
        setpoint = np.pi/6 # change setpoint to 30°
        print("Setpoint changed")
    if i % (steps // 10) == 0: print(f"Completed {i} steps") # print 10 milestones
    t = i * dt
    measured = sensor.read(rig.angle)
    if (i % (control_dt // dt) == 0): # only compute control every x time
        control_val = control.compute(used_strategy, setpoint, measured)
    true_angle = rig.update(control_val, dt)
    # save data
    data_rows.append([t, true_angle, measured, control_val])

# write data
with open(f"rotational-simulation-{used_strategy}-{int(dt*steps)}s{"-SSwap" if swap_setpoint else ""}.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["Time", "True_Angle", "Measured_Angle", "Control_Signal"])
    writer.writerows(data_rows)
    
print("Done")