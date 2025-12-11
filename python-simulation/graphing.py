import matplotlib.pyplot as plt
import csv
import numpy as np
import os

swap_setpoint = True # change depending on what to analyze

user_input = input("Strategy to analyze (OnOff, P or PID): ")
if user_input == "all":
    strategies_to_run = ["OnOff", "P", "PID"]
else:
    strategies_to_run = [user_input]

for strategy in strategies_to_run:
    filename = f"rotational-simulation-{strategy}-60s{"-SSwap" if swap_setpoint else ""}.csv"
    output_image = f"rotational-simulation-{strategy}{"-SSwap" if swap_setpoint else ""}.png"
    times = []
    true_angles = []
    measured_angles = []

    if not os.path.exists(filename):
        print(f"File not found: {filename}")
        continue

    with open(filename, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            times.append(float(row["Time"]))
            true_angles.append(float(row["True_Angle"]))
            measured_angles.append(float(row["Measured_Angle"]))

    # deg for visualization
    true_angles_deg = np.degrees(true_angles)
    measured_angles_deg = np.degrees(measured_angles)

    plt.figure(figsize=(12, 6))
    plt.plot(times, measured_angles_deg, label="Sensor Reading (Noise and Bias)", color="red", alpha=0.3, linewidth=0.5)
    plt.plot(times, true_angles_deg, label="True Orientation", color="#007acc", linewidth=2)
    if swap_setpoint:
        plt.hlines(0, min(times), 30, color='gray', linestyle='--', linewidth=1.5, label="Target (0°, 30°)")
        plt.hlines(30, 30, max(times), color='gray', linestyle='--', linewidth=1.5)
        plt.axvline(30, color='green', linestyle=':', linewidth=1.5, label="Setpoint Change")
    else:
        plt.axhline(0, color='gray', linestyle='--', linewidth=1.5, label="Target (0°)")
    # proper naming
    used_strategy = ""
    if strategy == "OnOff": used_strategy = "On-Off"
    elif strategy == "P": used_strategy = "Proportional"
    elif strategy == "PID": used_strategy = "PID"

    plt.title(f"Rotational Oscillator Response ({used_strategy} Control{", Swapping Setpoint" if swap_setpoint else ""})", fontsize=14)
    plt.xlabel("Time (seconds)", fontsize=12)
    plt.ylabel("Angle (Degrees)", fontsize=12)
    plt.legend(loc="upper right")
    plt.grid(True, which='major', linestyle='-', alpha=0.6)
    plt.grid(True, which='minor', linestyle=':', alpha=0.3)
    plt.minorticks_on()
    plt.ylim(min(true_angles_deg) - 10, max(true_angles_deg) + 10)
    plt.tight_layout()
    plt.savefig(output_image, dpi=600)
    print(f"Done processing {strategy}")