# 🔐 SecureEntry OTP: GSM-Based Two-Step Time-Limited Door Access System

An embedded systems project built on the **NXP LPC2148 (ARM7TDMI-S)** microcontroller that turns a normal PIN door lock into a **two-factor access system**: the user types a **User ID and PIN** on a 4x4 keypad, the controller then sends a **one-time password (OTP) by SMS** through a **SIMCom GSM modem**, and the door motor opens only when the correct OTP is typed within **60 seconds**. The owner can **BLOCK / UNBLOCK** any account by replying to the SMS, and a keypad-driven, password-protected **Admin Mode** lets staff change time, mobile numbers and PINs on site — all stored safely in an external **SPI EEPROM**.

---

## 📑 Table of Contents

1. [Project Overview / Purpose](#-project-overview--purpose)
2. [Features and Functionality](#-features-and-functionality)
3. [Repository Structure](#-repository-structure)
4. [System Architecture (Block Diagram)](#-system-architecture-block-diagram)
5. [Flow Chart](#-flow-chart)
6. [Hardware Details](#-hardware-details)
7. [Pin Mapping](#-pin-mapping)
8. [Software Setup & Build Instructions](#-software-setup--build-instructions)
9. [Admin Mode — Usage Guide](#-admin-mode--usage-guide)
10. [Testing Instructions](#-testing-instructions)
11. [Troubleshooting Guidelines](#-troubleshooting-guidelines)
12. [Real-Time Use Cases](#-real-time-use-cases)
13. [Application Screenshots / Results](#-application-screenshots--results)
14. [Project Submission Checklist](#-project-submission-checklist)
15. [Author](#-author)

---

## 🎯 Project Overview / Purpose

### Objective
A keypad PIN alone is a weak lock: PINs get shoulder-surfed, shared, or guessed. This project implements a **low-cost, standalone embedded controller** that adds a second factor — a **one-time password delivered to the owner's mobile phone** — without needing Wi-Fi, a server, or a smartphone app. Only a SIM card and mobile network coverage are required.

### Business Problem Addressed
- Single-factor PIN locks can be opened by anyone who learns the PIN.
- Owners have no way to know, or to stop, an access attempt happening right now.
- Administrators need a simple, on-the-spot way to change a PIN or a registered mobile number without a PC or special software.

### Scope
The system is a **single-door prototype** that:
- Stores **3 users** (`A101`, `B102`, `C103` by default) with a 4-digit PIN, a registered mobile number and a block flag each — in an external **SPI EEPROM**, so data survives power loss.
- Generates a **6-digit OTP** for every access attempt and sends it by SMS (`AT+CMGS`) through a SIMCom A76xx GSM modem on UART0.
- Runs a **60-second OTP window** using a hardware **1 ms Timer1 tick**; the LCD shows the countdown from **59 down to 00**, then **OTP EXPIRED**.
- Drives a DC motor through an **L293D**: open for 3 s, a short both-off gap, close for 2 s, with an interrupt-level motor failsafe.
- Reads incoming SMS in the background so the owner can reply **`BLOCK <ID>`** or **`UNBLOCK <ID>`**; a block cancels a pending OTP immediately.
- Provides a **PIN-protected Admin Mode** (fired by a hardware interrupt) to set the RTC time/date, change a user's mobile number or PIN, and change the admin PIN.
- Uses a **power-cut-safe EEPROM record format** (two slots, CRC, commit byte): a power loss during a save can never lose or reset a PIN.

**Out of scope (by design, for this prototype):** multiple doors, a mobile app or cloud dashboard, fingerprint/RFID, and a camera. The GSM modem is the only external communication channel.

---

## ✨ Features and Functionality

| Feature | Description |
|---|---|
| **Two-Factor Access (PIN + SMS OTP)** | User ID → 4-digit PIN → 6-digit OTP received by SMS. All three must be correct before the door opens. |
| **60-Second OTP Window with Live Countdown** | The OTP is valid for 60 s. The LCD shows `Time Left: 59s` counting down to `00`, then `OTP EXPIRED` — no key press needed. |
| **Remote BLOCK / UNBLOCK by SMS** | The owner replies `BLOCK A101` or `UNBLOCK A101` (any letter case). Only the registered mobile number is accepted. A block takes effect even while the user is typing the OTP. |
| **Confirmation SMS** | The owner gets a confirmation after a block/unblock and a success SMS after every door opening. |
| **Background SMS Receiver** | New-message notifications are read in the background; the SIM is also polled every 30 s so no command is lost. |
| **Persistent User Database (SPI EEPROM)** | PIN, mobile number and block flag are stored in an AT25LC512 EEPROM using **A/B slots + CRC + commit byte** — safe against power loss at any instant. |
| **Interrupt-Driven Admin Mode** | The admin switch fires **EINT1**; the key wait is aborted immediately (the door cycle always finishes first), then ID + admin PIN are required. |
| **Admin PIN (separate from user PINs)** | Default `2580`, changeable from the menu. A normal user PIN does **not** open the admin menu. |
| **Brute-Force Back-off** | 3 wrong PINs return to the ID prompt; repeated failed sessions add an increasing delay (5 s per extra session, max 30 s). |
| **Door Motor Control (L293D)** | Open 3 s → both inputs low 0.1 s → close 2 s. A Timer1-ISR failsafe forces the motor off if the main loop ever stalls. |
| **Real-Time Clock** | The on-chip RTC time stamps the OTP SMS and can be set from the admin menu. |
| **Keypad Data Entry** | 4x4 matrix keypad with debounce; `C` = backspace, `D` = clear, `#` = confirm. |
| **Clear Error Messages** | Unknown ID, blocked ID, wrong PIN, wrong/expired OTP, and the **reason** an SMS could not be sent (no SIM, modem offline, no network) are all shown on the 20x4 LCD. |
| **Serial Programming Interface** | A DB9 (UART) connector is used with Flash Magic to program the board. |

---

## 📁 Repository Structure

```
Secure-Entry-OTP-GSM-Door-Access-Controller/
│
├── Applocation.c               # main() — calls Scheduler_Init_Hardware() and Run_System_Scheduler()
├── scheduler.h / scheduler.c   # Finite state machine: ID → PIN → OTP SMS → OTP entry → unlock, admin handling
├── userDB.h / userDB.c         # 3-user database, A/B EEPROM slots, CRC, block flag, lookup
├── gsm.h / gsm.c               # SIMCom driver: send OTP/alerts, background SMS receiver, BLOCK/UNBLOCK
├── uart.h / uart.c             # UART0 driver, interrupt-driven RX ring buffer, line reader
├── menu.h / menu.c             # Admin dashboard: admin PIN, set time/date, mobile, PIN
├── kpm.h / kpm.c               # 4x4 keypad scan with debounce, ReadPass(), ReadNum(), ReadStringKeypad()
├── lcd.h / lcd.c               # 20x4 HD44780 LCD driver (4-bit mode)
├── rtc.h / rtc.c               # On-chip RTC init, get/set time and date
├── spi.h / spi.c               # SPI0 master driver
├── spi_eeprom.h / spi_eeprom.c # AT25LC512 byte read/write
├── timer.h / timer.c           # Timer0 delays, Timer1 1 ms tick, OTP countdown, motor failsafe
├── eint1_sw.h / eint1_sw.c     # EINT1 admin-switch interrupt init & ISR (sets admin_flag)
├── delay.h / delay.c           # us / ms / s delay routines
├── pin_connect_block.h / .c    # Pin function select helpers
├── all_peripheral_defines.h    # Centralized pin map, bit macros, clock/baud constants
├── pin_function_defines.h / spi_defines.h / spi_eeprom_defines.h / uart_defines.h
├── types.h                     # Project-wide typedefs (u8, s8, u16, u32, ...)
├── Startup.s                   # ARM7 startup / vector table / stack setup
├── *.uvproj / *.uvopt / *.sct  # Keil µVision project & scatter-load files
├── host_tests/                 # Optional PC tests (gcc): EEPROM power-loss + end-to-end simulation
└── images/                     # Block diagram, flow chart, results (this README)
    ├── block_diagram.png
    ├── flowchart.png
    ├── results/system_overview_all.png
    └── hardware/               # add your hardware photos here
```

### Key Files at a Glance
| File | Why it matters |
|---|---|
| `scheduler.c` | The "brain" — the state machine that runs the whole login flow, the OTP countdown display, the door cycle and the admin switch handling. |
| `gsm.c` | Everything GSM: AT command handling, sending the OTP SMS, reading incoming SMS, `BLOCK` / `UNBLOCK` parsing, error reasons. |
| `userDB.c` | The three users and the power-cut-safe EEPROM storage. Put the **real mobile numbers** here. |
| `menu.c` | Admin Mode: admin PIN check, set time/date, change mobile number, change PIN. |
| `kpm.c` | Keypad driver and all keypad entry routines (PIN is masked with `*`). |
| `timer.c` | 1 ms system tick, OTP countdown, delay routines and the motor failsafe. |
| `all_peripheral_defines.h` | One place to look up or change every pin assignment and clock constant. |

---

## 🧩 System Architecture (Block Diagram)

![Block Diagram]<img width="1264" height="900" alt="block_diagram" src="https://github.com/user-attachments/assets/c4af16a9-61b8-4a87-bdc6-22bfaf75abfe" />


**Signal flow:**
- **Inputs** → Admin Edit Switch (external interrupt EINT1), 4x4 Matrix Keypad, the on-chip RTC, the user database in the SPI EEPROM, and incoming SMS from the owner's phone.
- **Processing (LPC2148)** → reads the keypad, verifies ID/PIN, generates the OTP, sends it through the GSM modem, runs the 60 s countdown, verifies the OTP, drives the door motor, parses incoming SMS and handles admin configuration.
- **Outputs** → a 20x4 character LCD, the L293D + DC motor (door lock) and the GSM modem (SMS to the owner's phone).
- A DB9 serial connector is used to program the board from a PC.

---

## 🔄 Flow Chart

End-to-end process flow of the login state machine, the admin interrupt and the background SMS receiver:

![Flow Chart](<img width="1024" height="1760" alt="flowchart" src="https://github.com/user-attachments/assets/d836292a-74ae-4b33-a199-a0326c93689e" />
)

**Data movement summary:**
1. Keypad → `ReadStringKeypad()` / `ReadPass()` → `scheduler.c` looks the ID up in `userDB.c` and checks the block flag and the PIN.
2. A correct PIN → an OTP is generated (seeded from uptime + RTC) → `gsm.c` sends it with `AT+CMGS` → the 60 s countdown starts (`timer.c`).
3. The OTP typed by the user is compared with the generated OTP; the LCD hook updates the countdown every 250 ms while the keypad is being scanned.
4. A correct OTP → the motor runs open → gap → close; a success SMS is queued to the owner.
5. In the background, `GSM_Process_Incoming_SMS_Traffic()` reads `+CMTI` notifications (and polls the SIM every 30 s), executes `BLOCK` / `UNBLOCK` for the registered number, saves the block flag to EEPROM and queues a confirmation SMS.
6. Admin switch (EINT1, P0.3) → `admin_flag` → the key wait is aborted; once the door is idle, `RunAdminDashboard()` asks for the ID and the admin PIN, then writes changes through the safe A/B EEPROM commit.

---

## 🔧 Hardware Details

| Component | Quantity | Role in the System | Notes |
|---|---|---|---|
| LPC2148 (ARM7TDMI-S) development board | 1 | Core controller — runs all firmware logic | `F_OSC = 12 MHz`, `CCLK = 60 MHz`, `PCLK = CCLK / 4 = 15 MHz` |
| 20x4 Character LCD (HD44780) | 1 | Prompts, OTP countdown, status and admin menu | 4-bit parallel interface |
| 4x4 Matrix Keypad | 1 | User ID, PIN, OTP and admin entry | Port 1 |
| Push-Button Switch | 1 | Triggers Admin Mode via external interrupt (EINT1) | P0.3 — use a pull-up resistor |
| SIMCom A76xx GSM Modem + SIM card | 1 | Sends the OTP SMS and receives BLOCK / UNBLOCK | UART0, 9600 baud. Many A76xx modules use **1.8 V logic** — use a level shifter / 3.3 V port |
| AT25LC512 SPI EEPROM | 1 | Stores PIN, mobile number and block flag of every user | SPI0, mode 3 |
| L293D motor driver + DC motor | 1 | Door lock actuator | Direction by IN1 / IN2 |
| DB9 Serial Connector | 1 | UART link for PC ↔ MCU programming | Used with Flash Magic |
| Power supply | 1 + 1 | 5 V regulated for the board; **separate supply for the GSM module** | GSM transmit bursts draw high current — use a bulk capacitor |

> 📷 **Add photographs of your assembled hardware here** — e.g., `images/hardware/full_setup.jpg`, `images/hardware/lcd_display.jpg`, `images/hardware/gsm_modem_wiring.jpg`, `images/hardware/door_motor.jpg` — with a short caption for each describing what is shown.

---

## 🔌 Pin Mapping

Sourced from `all_peripheral_defines.h`:

| Peripheral | Function | MCU Pin |
|---|---|---|
| Admin Edit Switch | External Interrupt (EINT1) | P0.3 |
| GSM Modem | UART0 TXD0 / RXD0 | P0.0 / P0.1 |
| SPI EEPROM | SCK0 / MISO0 / MOSI0 | P0.4 / P0.5 / P0.6 |
| SPI EEPROM | Chip Select (CS) | P0.7 |
| LCD Data Bus (D4–D7) | 4-bit data | P0.10 – P0.13 |
| LCD RS | Register Select | P0.17 |
| LCD EN | Enable | P0.18 |
| LCD RW | Read/Write | P0.19 |
| Motor driver L293D | IN1 (open) / IN2 (close) | P0.20 / P0.21 |
| Keypad Rows (R0–R3) | Row scan | P1.16 – P1.19 |
| Keypad Columns (C0–C3) | Column scan | P1.20 – P1.23 |

> ⚠️ **Note:** P0.24 does **not** exist on the LPC2148, so the motor inputs are defined on P0.20 / P0.21. If you wire other free pins, change only `MOTOR_IN1` / `MOTOR_IN2` in `all_peripheral_defines.h`.

---

## 🛠️ Software Setup & Build Instructions

**Tools required:**
- **MCU:** NXP LPC2148 (ARM7TDMI-S)
- **IDE / Toolchain:** Keil µVision (ARM) — project files (`.uvproj`, `.sct`) are already included
- **Flash Utility:** Flash Magic (via UART/DB9 serial connector)
- **Language:** Embedded C
- **Extras:** an active SIM card with SMS enabled and mobile network coverage

**Steps:**
1. Clone this repository.
2. Open `GSM-Based_prog.uvproj` in Keil µVision (all `.c`/`.h` files are already part of the project).
3. In `userDB.c`, replace the placeholder `+91XXXXXXXXXX` with each user's **real mobile number in international format** (e.g. `+919876543210`).
4. Build the project (`Project → Rebuild all target files`) to generate the `.hex` file.
5. Connect the board to the PC via the DB9 serial connector and put the board into ISP/bootloader mode.
6. Use Flash Magic to flash the generated `.hex` file onto the LPC2148.
7. Power the board; the GSM modem is configured for a few seconds, then the LCD shows `SecureEntry System` / `Enter User ID:`.

**Default accounts (change them from the Admin menu):**

| User ID | PIN | Notes |
|---|---|---|
| `A101` | `1234` | block flag off |
| `B102` | `5678` | block flag off |
| `C103` | `9012` | block flag off |
| Admin PIN | `2580` | `DEFAULT_ADMIN_PIN` in `menu.c`; stored in EEPROM after first use |

---

## 👨‍💼 Admin Mode — Usage Guide

1. Press the **Admin Edit Switch** (P0.3) at any prompt — this fires **EINT1**, aborts the current key wait and opens the Admin Dashboard immediately. (If the door is moving, the door cycle finishes first.)
2. Type the **User ID** you want to edit (`Confirm User ID:`).
3. Enter the **4-digit Admin PIN** (default `2580`). A wrong PIN shows `Auth Failure!` and adds a growing delay. A normal user PIN does **not** open Admin Mode.
4. Choose an option from the **Admin Menu**:
   - **`1`** → *Set Time / Date* — sub-menu: `1` Edit Time (Hour 0–23, Minute 0–59), `2` Edit Date (Date 1–31, Month 1–12), `5` Change **Admin** PIN.
   - **`2`** → *Set Mobile* — type the new mobile number (**digits only, 10–13 digits**; the keypad has no `+`, so use e.g. `919876543210`). The number is saved to EEPROM.
   - **`3`** → *Set PIN* — enter the new **4-digit** PIN twice (confirm).
   - **`4`** (or `D` / `C`) → *Exit* — returns to normal operation.
5. While typing any value:
   - Digits **0–9** are echoed (PIN entry echoes `*`).
   - **`C`** acts as backspace.
   - **`D`** clears the current entry.
   - **`#`** confirms the value.
6. After a change the LCD confirms it (e.g., `PIN Changed OK!`, `Mobile Updated`, `Time Synchronized`). If the EEPROM write fails you see `EEPROM FAIL!` and nothing is changed.

**Remote control by SMS (owner's phone):**

| SMS text | Result |
|---|---|
| `BLOCK A101` | A101 is blocked immediately; a pending OTP is cancelled; the owner gets a confirmation SMS. |
| `UNBLOCK A101` | A101 works again; confirmation SMS. |

Commands are accepted in any letter case and **only from the mobile number registered for that user**.

---

## 🧪 Testing Instructions

### Test Setup
1. Flash the firmware as described in [Software Setup & Build Instructions](#-software-setup--build-instructions).
2. Insert an active SIM, connect the antenna, and register your own mobile number for user `A101` (in `userDB.c` or via Admin Mode → `2`).
3. Power the board and confirm the LCD initialises correctly and `Enter User ID:` appears.
4. Set the RTC to the correct time via Admin Mode (`1`) so the time in the OTP SMS can be checked.

### Test Case Matrix

| # | Test Case | Steps | Expected Result |
|---|---|---|---|
| 1 | Normal unlock | ID `A101`, PIN `1234`, enter the OTP received by SMS | `ACCESS GRANTED!`; motor opens ~3 s, ~0.1 s pause, closes ~2 s; success SMS received |
| 2 | OTP SMS content | Read the SMS | Contains the user ID, `hh:mm`, a 6-digit OTP, `Valid 60s`, and the BLOCK hint |
| 3 | Countdown display | Watch the LCD on the OTP screen | `Time Left` counts **59 → 00** once per second, then `OTP EXPIRED!` |
| 4 | OTP expiry | Wait 61 s without typing | `OTP EXPIRED!` and `Re-enter password`; door stays closed |
| 5 | Wrong PIN ×3 | Enter 3 wrong PINs | `WRONG PASSWORD!` twice, then `Too many errors!`; back to ID prompt; no SMS sent |
| 6 | Wrong OTP ×2 | Enter 2 wrong OTPs | `INVALID OTP!`, then `Invalid OTP twice`; back to password prompt |
| 7 | PIN length | Enter a 3-digit PIN | `Password must be exactly 4 digits`; no attempt used |
| 8 | Unknown ID | Type `A999` | `UNKNOWN USER ID!` |
| 9 | User ID with C | Type `C`,`1`,`0`,`3` | `C103` accepted (`User Verified`) |
| 10 | Remote BLOCK | SMS `BLOCK A101` from the registered phone | Confirmation SMS; next login shows `ID IS BLOCKED!` and no OTP is sent |
| 11 | BLOCK while typing OTP | Type 3 OTP digits, send `BLOCK A101`, finish the OTP | `OTP CANCELLED!!`; door stays closed |
| 12 | Remote UNBLOCK | SMS `UNBLOCK A101` | Confirmation SMS; login works again |
| 13 | Mixed-case / wrong sender | SMS `Block a101` from the registered phone; `BLOCK A101` from another number | The first is accepted; the second is ignored |
| 14 | Admin switch | Press the admin switch at the ID prompt | Admin dashboard opens immediately (`Confirm User ID:`) |
| 15 | Admin PIN check | Admin switch → ID `B102` → PIN `0000` or `5678` | `Auth Failure!` (only the admin PIN is accepted) |
| 16 | Change PIN | Admin → `3` → new PIN twice → **power-cycle** → log in | The new PIN works after reset |
| 17 | Change mobile | Admin → `2` → new number → power-cycle | OTP goes to the new number; value kept after reset |
| 18 | Set time | Admin → `1` → `1` → hour/minute | `Time Synchronized`; next OTP SMS shows the new time |
| 19 | No SIM / modem off | Remove the SIM (or power the modem off) and log in | `GSM Network Error` + reason on the LCD within ~30 s; returns to the ID prompt |
| 20 | Admin during unlock | Press the admin switch while the door is moving | The door cycle completes first, then the admin menu opens |
| 21 | Power cut during save | Cut power while saving a PIN | After reboot the **old or the new** PIN works — never the factory default |
| 22 | Keypad mashing | Press keys rapidly on every prompt | No phantom or doubled characters |

### Validation Procedure
- Compare the OTP SMS time with a reference clock.
- Time the countdown with a stopwatch: it should reach `00` after about 60 s.
- Scope or LED-check the motor inputs: forward ≈3 s, both low ≈0.1 s, reverse ≈2 s, never both high.
- Repeat the BLOCK / UNBLOCK tests for a second user to confirm the sender-number check.

### Optional PC-side tests (no hardware needed)
The `host_tests/` folder simulates the real firmware against a fake LCD, modem, EEPROM and clock (needs `gcc` and `python3`):
```
cd host_tests
sh run_tests.sh
```
It checks PIN persistence across power cycles, thousands of simulated power cuts during EEPROM writes, and the full login flow (normal unlock with motor timing, wrong PIN/OTP, expiry, BLOCK while typing, admin switch, silent modem).

---

## 🩹 Troubleshooting Guidelines

| Symptom | Likely Root Cause | Resolution |
|---|---|---|
| `GSM Network Error` / `SMS NOT SENT` right after the PIN | No SIM, no signal, no balance, modem not powered, or wrong UART wiring | Read the reason line on the LCD (`Modem Offline`, `No SMS Prompt`, `SMS Rejected`, …); check SIM, antenna, supply and TX/RX wiring |
| `Modem Offline` | No reply to `AT` | Check the modem power (it needs a strong 3.4–4.2 V supply), baud 9600, and that TXD0/RXD0 are crossed correctly; many A76xx modules need a **1.8 V ↔ 3.3 V level shifter** |
| OTP SMS never arrives | Wrong mobile number stored, SMS delay, or the placeholder `+91XXXXXXXXXX` is still in `userDB.c` | Set the real number in `userDB.c` or Admin Mode → `2`; test the SIM in a phone |
| `OTP EXPIRED!` every time | SMS delivery is slow (network delay eats the 60 s) | Move the antenna/improve signal; the countdown starts when the modem accepts the SMS |
| `BLOCK A101` SMS has no effect | Sent from a different number than the registered one, or wrong ID | Send it from the registered phone; check the ID spelling (case does not matter) |
| LCD shows no text / blank | Wrong RS/RW/EN/data wiring, contrast (VEE) not set | Verify against the [Pin Mapping](#-pin-mapping); adjust the contrast potentiometer |
| LCD shows garbled characters | Power dips (GSM bursts), or `InitLCD()` interrupted | Give the GSM module its own supply and a bulk capacitor; check the board supply |
| Keypad key not detected / wrong key | Loose row/column wiring or rows/columns swapped; a key held at reset | Re-check `ROW0–ROW3 = P1.16–P1.19`, `COL0–COL3 = P1.20–P1.23`; do not hold a key while resetting (P1.20 is a reset-strap pin) |
| Admin switch does nothing | Switch not on P0.3, or no pull-up (P0.3 is an open-drain pin) | Wire the switch to P0.3 with an external pull-up and confirm `Init_EINT1()` runs |
| Motor does not move | Motor inputs not on the pins in `all_peripheral_defines.h` (P0.24 does not exist on the LPC2148) | Wire IN1/IN2 to P0.20/P0.21 or edit `MOTOR_IN1`/`MOTOR_IN2` |
| Forgot the admin PIN | PIN changed via the admin menu and not recorded | Re-flash and clear the EEPROM (a blank chip restores the default `2580`) |
| A saved PIN/mobile seems to revert | Old firmware or a failed EEPROM write | Use this firmware (A/B slots); an `EEPROM FAIL!` message means the chip/wiring needs checking |
| Board doesn't respond to Flash Magic | Not in ISP mode, wrong COM port / baud | Re-check the ISP jumper, the COM port in Device Manager and the DB9 cable |

---

## 🌍 Real-Time Use Cases

1. **Homes and Apartments** — A door or gate that needs a one-time code on the owner's phone each time, even if the PIN leaks.
2. **Hostels, Labs and Offices** — Restricted rooms where the owner/warden must approve each entry and can block an account from anywhere by SMS.
3. **Server Rooms and Equipment Cabinets** — Two-factor access without network infrastructure.
4. **Lockers and Cash Boxes** — Low-cost locker locks with remote blocking of lost credentials.
5. **Remote / Offline Sites** — Farms, pump houses and workshops with mobile coverage but no internet.
6. **Embedded Systems Teaching Aid** — Demonstrates GSM AT commands, interrupt-driven UART, keypad/LCD interfacing, SPI EEPROM data integrity, timers and state-machine design in one project.

---

## 📸 Application Screenshots / Results

The composite below shows the LCD screens of the running firmware: login prompts, OTP entry with the 59 → 00 countdown, the access-granted, wrong-OTP, blocked, expired and GSM-failure screens, a remote-block cancellation, and the admin PIN and menu screens.

![Results overview](<img width="1365" height="1458" alt="system_overview_all" src="https://github.com/user-attachments/assets/8d746be9-09bc-4b2e-8ffe-c8108c842baa" />
)

> 📷 The image above is rendered from the exact screen strings in `scheduler.c`, `menu.c` and `gsm.c`. **Replace it with photographs or Proteus screenshots of your running hardware** (`images/results/`), and add the received SMS screenshot (OTP + BLOCK confirmation).

### 🗺️ Visual Validation Reference

| Screen in the composite | System State |
|---|---|
| **1** | Idle — `Enter User ID:` |
| **2** | ID accepted — PIN entry masked with `*` |
| **3** | PIN correct — OTP SMS is being sent |
| **4** | OTP entry with the live countdown (59 → 00) |
| **5** | Correct OTP — `ACCESS GRANTED!`, motor opening |
| **6** | Wrong OTP |
| **7** | Blocked user — `ID IS BLOCKED!` |
| **8** | OTP not entered in 60 s — `OTP EXPIRED!` |
| **9** | SMS could not be sent — GSM error with reason |
| **10** | Owner blocked the account by SMS while the OTP was being typed |
| **11 / 12** | Admin PIN prompt and the Admin Menu |

---

## ✅ Project Submission Checklist

As per the assigned project workflow, before submitting the GitHub repository link on the student portal, confirm:

- [ ] Project implemented and tested on real hardware within the given time frame.
- [ ] Repository includes all source files (`.c` / `.h`) and the Keil project (`.uvproj` / `.sct`).
- [ ] Repository includes system architecture details (block diagram + flow chart, as above).
- [ ] Repository includes hardware images/demo video.
- [ ] README file (this file) is complete and accurate, covering overview, features, structure, testing, troubleshooting, diagrams, use cases, and results.
- [ ] Real mobile numbers are set in `userDB.c` (do not commit personal numbers to a public repository).
- [ ] GitHub repository link submitted through the student login portal.
- [ ] Ready for hardware verification, GitHub review, and explanation/viva.
- [ ] Any modification requests from the review process are addressed and resubmitted.
- [ ] Final evaluation completed within the assigned project duration.

---

## 👤 Author

- **Student Name:** _MD SHARIQUE_
- **Roll No / ID:** _V25HE10M13_
- **Guide/Mentor:** Mr.Chandramouli Sir

---

## 📄 License

This project is submitted as part of an academic embedded systems course assignment.
