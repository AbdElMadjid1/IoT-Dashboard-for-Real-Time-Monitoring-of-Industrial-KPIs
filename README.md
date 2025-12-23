# IoT-Dashboard-for-Real-Time-Monitoring-of-Industrial-KPIs


## Overview
This project demonstrates an ESP32‑based IoT system designed to monitor key production indicators on a flow‑shop production line in real time. Developed by Abd El Madjid together with Imad Banouri,Gourmala Houcine,Marouf Abderrezak, and Inssaf Betaouaf, the system integrates sensors, MQTT communication, and a Node‑RED dashboard to provide actionable insights for industrial environments.

## Features
- **Real‑time monitoring** of Work In Progress (WIP), cycle time, and throughput.
- **Infrared sensors** track every piece entering and exiting the line.
- **Control button** to start/stop monitoring.
- **Node‑RED dashboard** for visualization via MQTT.
- **Final KPI summary** when stopped: utilization, busy/idle time, and average performance indicators.
- **Scalability**: extendable with additional sensors at each workstation to measure processing times, detect bottlenecks, and evaluate efficiency.

## Limitations
- Assumes a simple FIFO flow (no rework or complex routing).
- Relies on clean sensor signals for accurate data collection.

## Technologies Used
- **ESP32 microcontroller**
- **Infrared sensors**
- **MQTT protocol**
- **Node‑RED dashboard**


