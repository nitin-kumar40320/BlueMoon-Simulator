# BlueMoon Simulator

An interactive, pipelined RISC‑V simulator built with Flutter Web and WebAssembly. Explore every stage of the classic five‑stage pipeline with live controls, rich visualizations, and detailed metrics. 

# Team Members

| Name           | Entry No.   | Github      |
|----------------|-------------|-------------|
| Arpit Goel     | 2023CSB1099 | [Arpit01Goel](https://github.com/Arpit01Goel) |
| Nitin Kumar    | 2023CSB1141 | [2023csb1141](https://github.com/2023csb1141), [nitin-kumar40320](https://github.com/nitin-kumar40320) |
| Parth Kulkarni | 2023CSB1142 | [ParthKulkarni445](https://github.com/ParthKulkarni445), [Parth445](https://github.com/Parth445) |
---

## 🚀 Overview

BlueMoon Simulator is a web-based tool to learn and experiment with RISC‑V pipelining. Write or paste assembly code, assemble to machine code, and step through execution cycle‑by‑cycle or run full programs. Toggle pipeline features, inspect hazards, and generate reports—all in your browser.

## 🔗 Live Demo:
https://riscv-simulator.vercel.app/

## 🛠️ Features

1. **Code Editor**  
   - Write or paste RISC‑V assembly (`.asm`)  
   - Syntax highlighting and auto‑format  
   - One‑click assemble to machine code

2. **Simulator Tab**  
   - ▶️ Run / Step / Reset controls (full‑program or cycle‑by‑cycle)  
   - Machine Code table with addresses, binary encoding, and comments  
   - Console output: memory dumps & custom exit messages  
   - Knob toolbar to toggle pipelining, data‑forwarding, register dumps, buffer tracing, branch‑predictor view, per‑instruction trace, and more

3. **Visualize Tab**  
   - Interactive block diagram of pipeline stages (IF → ID → EX → MEM → WB)  
   - Data forwarding paths with bypass highlights  
   - Hazard detection panel showing real‑time alerts  
   - Branch‑prediction display with PHT & BTB entries and misprediction events  
   - Cycle counter, PC display, stall vs. forwarded indicators

4. **Configurable Pipeline “Knobs”**  
   - Pipelining On/Off to compare serial vs. pipelined execution  
   - Data‑forwarding vs. stall‑mode to watch hazards resolve live  
   - Full register dump per cycle for detailed state inspection  
   - Inter‑stage trace buffers for IF/ID/EX/MEM/WB  
   - Instruction focus spotlight (e.g. instruction #10)  
   - Branch‑prediction unit simulating one‑bit predictor, PHT, BTB, flush & restart

5. **Metrics & Reporting**  
   - Overall stats: total cycles, instructions executed, CPI  
   - Instruction breakdown (loads/stores, ALU ops, control instructions)  
   - Hazard counts: data hazards, control hazards, stalls, bubbles, mispredictions  

---

## 📦 Installation & Setup

1. **Clone the repository**

   ```bash
   git clone https://github.com/ParthKulkarni445/BlueMoon-Simulator.git
   cd BlueMoon-Simulator
   ```

2. **Install dependencies**

   ```bash
   flutter pub get
   ```

3. **Run in development**

   ```bash
   flutter run -d chrome
   ```

4. **Build for production**

   ```bash
   flutter build web
   ```

   Serve the contents of `build/web/` with any static web server.

---

## 🚦 Usage

1. Open the **Code Editor** tab to write or paste your RISC‑V assembly.  
2. Click **Assemble** to generate machine code.  
3. Switch to the **Simulator** tab to run or step through your program.  
4. Toggle pipeline **Knobs** to experiment with hazards and forwarding.  
5. View cycle‑by‑cycle visuals and metrics in the **Visualize** tab.  
6. Export a report for offline analysis.

---

## 🏗️ Tech Stack

- **Flutter Web & Dart** for a responsive UI  
- **C++ → WebAssembly** core for high‑performance simulation  
- **JavaScript Interop** to bridge Flutter and WASM modules

---

## 🤝 Contributing 

Contributions are welcome! Please follow these steps:

1. Fork the repository.  
2. Create a feature branch: `git checkout -b feature/YourFeature`  
3. Commit your changes: `git commit -m "Add new knob for XYZ"`  
4. Push to your branch: `git push origin feature/YourFeature`  
5. Open a Pull Request against `main`.  

For bugs and feature requests, please use GitHub Issues: https://github.com/ParthKulkarni445/BlueMoon-Simulator/issues

---

*Happy pipelining!*
