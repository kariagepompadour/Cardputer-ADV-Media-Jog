/*
  MediaJog_2a_2 — Stage 2a-2: Stage 2a with BLE-B1's GATT table (docs/STAGE_2A_2_DESIGN.md,
  docs/STAGE_2A_2.md)

  Base: MediaJog_2a (unchanged in MediaJog_2a/). BLE GATT part taken from MediaJog_BLE_B1
  (hardware PASS: one HID service with keyboard input, keyboard output and mouse input;
  pointer, drag, B7 reconnections), copied byte for byte:
    - the static NimBLE table (Device Information, ONE HID service, Battery), its access
      callbacks, registerGattTable() and the subscribe listener onSubscribeEvent();
    - notifyReport() (ble_gatts_notify_custom(), only with that report's CCCD on);
    - bleBegin() (subscribe listener before the trace listener; table queued after
      createServer(); one bleServer->start()).
  BLEHIDDevice and ReportCallbacks are gone. The BLE-1 r4 / B7 functions, USB, transport
  switching, profiles, CUSTOM, HOLD, SETUP and NVS are Stage 2a's, unchanged. The table and
  so every handle are those of BLE-B1: a Mac bonded with BLE-B1 keeps its bond.
  Known limit: NimBLE stores 3 CCCD records per BLE host (keyboard, mouse, battery) and at
  most 8; a third bonded host can make NimBLE delete the oldest other host's bond. Supported:
  at most 2 BLE hosts (Edgar, STAGE_2A_2_DESIGN.md §4 option a).
  Build id 2a-2; screen tag "USB 2a2" / "BLE 2a2".

  --- Stage 2a notes (unchanged) ---
  MediaJog_2a — Stage 2a: USB HID or BLE HID in one firmware (docs/STAGE_2A_DESIGN.md, docs/STAGE_2A.md)

  Base: MediaJog_1e_4 (USB release candidate) for everything the user does: four
  profiles, U135 input, HOLD, CUSTOM SETUP, NVS settings. BLE backend taken from
  MediaJog_BLE_1_r4 (hardware-tested) by copying its functions unchanged, in
  particular the handling of CCCD RESTORE events that NimBLE delivers before
  CONNECT (adoptConnHandle / appConnect / appDisconnect / appConnectFailed /
  handleSubscribe, docs/BLE_1.md §13). Both sources stay unchanged in their folders.
    - One active transport (activeTransport). Reports are built once (8-byte
      keyboard, 5-byte mouse, the same layout on USB and BLE) and handed to the
      active backend only; nothing is ever sent to both.
    - Backend results: SENT / NOT_SENT / UNKNOWN (USB: IN transfer completed /
      endpoint not ready / completion not seen; BLE: accepted by NimBLE /
      not accepted). The 1e-4 send rules are unchanged on top of them.
    - Fn+B opens CONNECTION (1 = USB, 2 = BLE, ok = apply, esc = close,
      t = BLE trace). Switching runs without a restart: the old transport gets an
      all-up report within 200 ms (BLE: + 100 ms to transmit), then the switch
      commits; an unconfirmed release is shown as a warning ("!") and is sent
      first when the old transport is READY again. Nothing pressed or queued is
      carried over. The BLE stack starts only when BLE is first used.
    - BLE mode: USB HID stays enumerated (static devices) but sends nothing
      (screen: USB HID INACTIVE), except an owed release.
    - Transport saved in NVS namespace "mediajog", key "link" (own record with
      CRC), at once after a confirmed switch; the 1e-4 "cfg" record is unchanged.
  Build tag: 2a.

  --- Stage 1e-4 notes (unchanged) ---
  MediaJog_1e_4 — Stage 1e-4: settings kept across power cycles (docs/STAGE_1E_4.md)

  Base: MediaJog_1e_3 (kept unchanged in MediaJog_1e_3/). HID behaviour is unchanged.
    - Saved in NVS, namespace "mediajog", key "cfg": one versioned 21-byte record
      (magic "MJCF", version 1, length, profile, sensitivity index, the three Custom
      actions incl. BTN TAP/HOLD, CRC-32). Never saved: held keys or mouse buttons,
      queues, USB / U135 state, the setup draft.
    - When it is written (agreed policy, DEVELOPMENT_PLAN.md §6.9):
        profile      at once after a confirmed Fn+1..4 switch
        Custom       only on Save in CUSTOM SETUP (Cancel writes nothing)
        sensitivity  SENS_SAVE_DELAY_MS (2 s) after the last Fn+; / Fn+. step
      Each trigger changes only its own fields; identical records are not written.
      Writes run in loop() after the HID sends of that loop, so a release report
      is never delayed by a flash write.
    - At boot the record is read (never written) and applied only if every check
      passes; otherwise the defaults are used. A record of a newer version, or an
      NVS read error, makes this session read-only (nothing is overwritten).
    - Never erases NVS: no nvs_flash_erase(), no nvs_erase_*(). Before a write the
      free space is checked (NV_MIN_AVAILABLE_ENTRIES). The Arduino core's own
      boot-time erase path (esp32-hal-misc.c:331-337) is outside this sketch and
      exists in every Arduino build; see docs/STAGE_1E_4.md §6.
    - Status "NV ..." next to the build tag on every screen. Build tag: 1e-4.

  --- Stage 1e-3 notes (unchanged) ---
  MediaJog_1e_3 — Stage 1e-3: HOLD for the Custom button (docs/STAGE_1E_3.md)

  Base: MediaJog_1e_2_r3 (kept unchanged in MediaJog_1e_2_r3/).
    - Profile 4 BTN can be TAP or HOLD (Fn+H in EDIT BTN). HOLD presses the
      assigned key/modifiers on knob press and releases them on knob release.
      CW / CCW stay TAP only.
    - Keyboard reports are built from a held state (kbHoldMods / kbHoldUsage)
      plus the tap being sent, so a CW/CCW tap during HOLD does not drop the held
      key or modifiers (e.g. HOLD Shift + CW Right = Shift+Right). The report after
      a tap returns to the held state instead of all-keys-up. With nothing held
      (always the case in Profiles 1-3) the reports are byte-identical to 1e-2.
    - HOLD down/up go through the same ordered queue as taps; a HOLD up is never
      dropped (a full queue is cleared first).
    - A held key is released at once, outside the queue, on: profile change,
      CUSTOM SETUP entry, U135 loss, USB not ready. A new physical press is then
      needed; nothing is resumed automatically.
    - Modifier-only actions (e.g. HOLD Shift with no key) are now valid: an action
      is assigned if it has a key OR a modifier.
    RAM only (NVS = stage 1e-4). Build tag: 1e-3.

  --- Stage 1e-2 r3 notes (unchanged) ---
  MediaJog_1e_2_r3 — Stage 1e-2, revision 3 (CUSTOM SETUP overview readability only)

  r3 changes only how the CUSTOM SETUP overview draws the assigned key in the
  CW / CCW / BTN rows: the largest of text sizes 2, 1.5, 1 whose measured width
  fits the row is used, so short names are larger and the longest possible name
  ("Ctl+Sft+Alt+Cmd+Backspace", 25 chars) still fits at size 1. The EDIT screen,
  the Profile 4 normal screen, all key handling and all HID behaviour are
  unchanged from r2 (docs/STAGE_1E_2.md §12). Build tag: 1e-2r3.

  --- Stage 1e-2 r2 notes (unchanged) ---
  MediaJog_1e_2_r2 — Stage 1e-2, revision 2 (identification and diagnostics only)

  Why r2: in the 1e-2 hardware test (2026-10-13) Fn+S did not open CUSTOM SETUP.
  Source review found no defect in the Fn+S path (the installed M5Cardputer 1.1.1
  delivers 's' with fn=true exactly like the hardware-verified '4'). It did find
  that the 1e-2 build still labelled itself "MEDIA JOG 1e-1" on the spinner screen,
  so the running build could not be identified on the device. r2 therefore:
    - shows the build tag (BUILD_TAG) on every screen;
    - shows the live Cardputer key state (e.g. "key:Fn+s") on the Profile 4 screen;
    - logs every recognised Fn chord on the serial port.
  The Fn+S logic and all HID behaviour are unchanged from 1e-2 (docs/STAGE_1E_2.md §9).

  --- Stage 1e-2 notes (unchanged) ---
  MediaJog_1e_2 — Cardputer ADV + Unit Encoder (U135) -> USB HID keyboard + mouse (stage 1e-2)

  Stage 1e-2 (on top of 1e-1, which is kept unchanged in MediaJog_1e_1/):
    PROFILE 4 (Custom) gets an on-device setup UI (docs/STAGE_1E_2.md):
      - In Profile 4, Fn+S opens CUSTOM SETUP. All HID output stops while it is open,
        and everything held is released when it opens.
      - Three actions (CW, CCW, BTN), each one ordinary keyboard key plus optional
        Ctrl / Shift / Alt(Option) / GUI(Command). TAP only (HOLD is stage 1e-3).
      - Keys are typed on the Cardputer or picked from a list with the knob.
      - ok (Enter) saves, esc (`) cancels and discards the draft.
    Settings live in RAM only: no NVS (stage 1e-4), lost at power-off.
    Profiles 1-3, USB handling, report format and error handling are unchanged.

  --- Stage 1e-1 notes (unchanged) ---
  MediaJog_1e_1 — Cardputer ADV + Unit Encoder (U135) -> USB HID keyboard + mouse (stage 1e-1)

  Base: MediaJog_1d (kept unchanged in MediaJog_1d/). Stage 1e-1 only adds
        PROFILE 3 (Video Editing, fixed defaults) and PROFILE 4 (Custom, not yet
        configurable: no HID output), selected with Fn+3 / Fn+4.
        Profiles 1 and 2, USB handling, report format, error handling, the mouse
        button safety and sensitivity control are unchanged from 1d.
        No configuration UI and no NVS persistence in this stage (docs/DEVELOPMENT_PLAN.md §6).
    PROFILE 3 (Video Editing, proposed defaults; NOT tested in any editing application):
      U135 clockwise 1 detent        -> Right Arrow (Usage 0x4F, no modifier)
      U135 counter-clockwise 1 detent -> Left Arrow  (Usage 0x50, no modifier)
      U135 push (1 press)             -> Space       (Usage 0x2C, no modifier)
    PROFILE 4 (Custom placeholder): rotation and push send nothing.

  Stage 1d scope, unchanged:
    PROFILE 1 (YouTube, same as 1b/1c):
      U135 clockwise 1 detent        -> USB HID key "."  (Usage 0x37, no modifier)
      U135 counter-clockwise 1 detent -> USB HID key ","  (Usage 0x36, no modifier)
      U135 push (1 press)             -> USB HID key "k"  (Usage 0x0E, no modifier)
    PROFILE 2 (general-purpose spinner):
      U135 rotation -> USB HID mouse relative X, SENSITIVITY HID counts per detent
      U135 push     -> mouse left button held while pressed, released on release
      (1d) Fn+; (up arrow legend)   -> sensitivity one step up
      (1d) Fn+. (down arrow legend) -> sensitivity one step down
           14 steps 1/2/3/4/6/8/12/16/24/32/48/64/96/128, initial 8 (= 1c).
           Not stored: resets at power-on.
    Fn+1 / Fn+2 select the profile. Cardputer keys are never sent to the host.
  Not done: acceleration, inertia, automatic sensitivity, NVS storage, BLE.

  USB HID layout (1c): one HID interface, report protocol only (no boot
  interface), Report ID 1 = keyboard, Report ID 2 = relative mouse. See
  docs/STAGE_1C.md §5-1 for the source references.

  Hardware facts used (measured with U135_Probe v2 on 2026-10-11):
    - 1 detent = counter +2 (clockwise) / -2 (counter-clockwise)
    - button raw = 0 while pressed, 1 while released

  Required Arduino IDE settings (Tools menu), enforced by #error below:
    Board: M5Cardputer (m5stack:esp32 3.3.9)
    USB Mode: USB-OTG (TinyUSB)
    USB CDC On Boot: Enabled
*/
#include <atomic>
#include <M5Cardputer.h>
#include <Wire.h>
#include "Unit_Encoder.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include <nvs.h>  // (1e-4) settings; nvs_flash_init() is done by the core before setup()
// (2a) BLE backend: the BLE library bundled with m5stack:esp32 3.3.9 (NimBLE), as BLE-1 r4.
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <host/ble_store.h>  // BLE-1 r2/r3 diagnostics: ble_store_read_cccd() / ble_store_util_count(), read-only
#include <host/ble_gatt.h>     // (B1) static GATT table, ble_gatts_notify_custom()
#include <host/ble_hs_mbuf.h>  // (B1) ble_hs_mbuf_from_flat() / ble_hs_mbuf_to_flat()
#if !defined(CONFIG_NIMBLE_ENABLED)
#error "Stage 2a is written for the NimBLE configuration of the m5stack:esp32 3.3.9 BLE library."
#endif

// (1e-2 r2) Shown on every screen so the running build can be identified on the device.
static constexpr const char* BUILD_TAG = "2a-2";
static constexpr const char* SCREEN_TAG = "2a2";  // (2a-2) top-right tag: "<USB|BLE> 2a2"

#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 0
#error "Tools > USB Mode must be 'USB-OTG (TinyUSB)'. In 'Hardware CDC and JTAG' mode USB HID does nothing."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || ARDUINO_USB_CDC_ON_BOOT != 1
// With CDC On Boot disabled, Serial becomes UART0 (TX = GPIO43), which the
// Cardputer ADV uses for audio I2S. Keep Serial on USB CDC.
#error "Tools > USB CDC On Boot must be 'Enabled'."
#endif

// ---------------------------------------------------------------- U135 (same as v2)
// Port pins per M5Unified pin table for board_M5CardputerADV (EX SCL=G1, SDA=G2).
static constexpr uint8_t PORT_SDA = 2;
static constexpr uint8_t PORT_SCL = 1;
static constexpr uint32_t I2C_FREQ = 200000;  // library default, unchanged

static constexpr int16_t COUNTS_PER_DETENT = 2;   // measured with v2
static constexpr uint8_t BUTTON_PRESSED_RAW = 0;  // measured with v2

Unit_Encoder sensor;

// ---------------------------------------------------------------- HID key actions
struct KeyAction {
    uint8_t modifiers;  // HID modifier bits (0x02 = Left Shift, ...)
    uint8_t usage;      // HID Usage ID, Keyboard/Keypad page (0x07)
    const char* label;
};

static constexpr KeyAction ACTION_CW    = {0x00, 0x37, "."};
static constexpr KeyAction ACTION_CCW   = {0x00, 0x36, ","};
static constexpr KeyAction ACTION_PRESS = {0x00, 0x0E, "k"};
static_assert(ACTION_CW.usage == HID_KEY_PERIOD, "'.' usage");
static_assert(ACTION_CCW.usage == HID_KEY_COMMA, "',' usage");
static_assert(ACTION_PRESS.usage == HID_KEY_K, "'k' usage");

// (1e-1) PROFILE 3, Video Editing: proposed defaults for Premiere Pro / Final Cut Pro
// (arrow = one frame, Space = play/stop). Not tested in either application.
static constexpr KeyAction EDIT_CW    = {0x00, 0x4F, "Right"};
static constexpr KeyAction EDIT_CCW   = {0x00, 0x50, "Left"};
static constexpr KeyAction EDIT_PRESS = {0x00, 0x2C, "Space"};
static_assert(EDIT_CW.usage == HID_KEY_ARROW_RIGHT, "Right Arrow usage");
static_assert(EDIT_CCW.usage == HID_KEY_ARROW_LEFT, "Left Arrow usage");
static_assert(EDIT_PRESS.usage == HID_KEY_SPACE, "Space usage");

// (1e-1) Profile numbers. Existing 1d code keeps comparing with the literal 2 for
// the spinner; the asserts tie the names to those values.
enum ProfileId : uint8_t {
    PROFILE_YOUTUBE = 1,
    PROFILE_SPINNER = 2,
    PROFILE_EDIT    = 3,
    PROFILE_CUSTOM  = 4,
};
static_assert(PROFILE_SPINNER == 2, "1d code uses profile == 2 for the spinner");
static uint8_t profile = PROFILE_YOUTUBE;  // selected with Fn+1..Fn+4

// ---------------------------------------------------------------- (1e-2) Custom profile data
// Declared before any function: the Arduino builder inserts prototypes above the
// first function, and several prototypes use these types.
//
// One ordinary keyboard key per action, with optional modifiers.
// (1e-3) An action is ASSIGNED if it has a key or at least one modifier
// (customAssigned()); modifier-only actions such as "HOLD Shift" are valid.
// Before 1e-3, usage 0 meant "not assigned" even when modifiers were set.
// (1e-4) Stored in NVS on Save only (Settings / docs/STAGE_1E_4.md); before 1e-4 RAM only.
struct CustomKey {
    uint8_t modifiers;  // HID modifier bits, MOD_* below
    uint8_t usage;      // HID Usage ID, Keyboard/Keypad page; 0 = no key (modifiers may still be set)
    uint8_t flags;      // (1e-3) CK_HOLD: BTN only. CW/CCW are always TAP
};
static constexpr uint8_t CK_HOLD = 0x01;
enum CustomSlot : uint8_t { SLOT_CW, SLOT_CCW, SLOT_BTN, SLOT_COUNT };

static constexpr uint8_t MOD_CTRL  = 0x01;  // Left Ctrl
static constexpr uint8_t MOD_SHIFT = 0x02;  // Left Shift
static constexpr uint8_t MOD_ALT   = 0x04;  // Left Alt (Option on a Mac)
static constexpr uint8_t MOD_GUI   = 0x08;  // Left GUI (Command on a Mac)
static constexpr uint8_t MOD_ALL   = MOD_CTRL | MOD_SHIFT | MOD_ALT | MOD_GUI;  // (1e-4) validation
static_assert(MOD_CTRL == KEYBOARD_MODIFIER_LEFTCTRL && MOD_SHIFT == KEYBOARD_MODIFIER_LEFTSHIFT
              && MOD_ALT == KEYBOARD_MODIFIER_LEFTALT && MOD_GUI == KEYBOARD_MODIFIER_LEFTGUI,
              "modifier bits as in TinyUSB hid.h");

// Keys that cannot be typed on the Cardputer (or are used by the setup UI itself),
// chosen with the knob in the edit screen.
struct PickKey {
    uint8_t usage;
    const char* name;
};
static constexpr PickKey PICKER[] = {
    {0x4F, "Right"}, {0x50, "Left"}, {0x52, "Up"}, {0x51, "Down"},
    {0x2C, "Space"}, {0x28, "Enter"}, {0x29, "Esc"}, {0x2B, "Tab"},
    {0x2A, "Backspace"}, {0x4C, "Delete"}, {0x49, "Insert"},
    {0x4A, "Home"}, {0x4D, "End"}, {0x4B, "PageUp"}, {0x4E, "PageDown"},
    {0x3A, "F1"}, {0x3B, "F2"}, {0x3C, "F3"}, {0x3D, "F4"}, {0x3E, "F5"}, {0x3F, "F6"},
    {0x40, "F7"}, {0x41, "F8"}, {0x42, "F9"}, {0x43, "F10"}, {0x44, "F11"}, {0x45, "F12"},
    {0x35, "`"},
};
static constexpr uint8_t PICKER_COUNT = sizeof(PICKER) / sizeof(PICKER[0]);
static_assert(PICKER[0].usage == HID_KEY_ARROW_RIGHT && PICKER[1].usage == HID_KEY_ARROW_LEFT
              && PICKER[2].usage == HID_KEY_ARROW_UP && PICKER[3].usage == HID_KEY_ARROW_DOWN
              && PICKER[4].usage == HID_KEY_SPACE && PICKER[5].usage == HID_KEY_ENTER
              && PICKER[6].usage == HID_KEY_ESCAPE && PICKER[7].usage == HID_KEY_TAB
              && PICKER[8].usage == HID_KEY_BACKSPACE && PICKER[9].usage == HID_KEY_DELETE
              && PICKER[10].usage == HID_KEY_INSERT && PICKER[11].usage == HID_KEY_HOME
              && PICKER[12].usage == HID_KEY_END && PICKER[13].usage == HID_KEY_PAGE_UP
              && PICKER[14].usage == HID_KEY_PAGE_DOWN && PICKER[15].usage == HID_KEY_F1
              && PICKER[26].usage == HID_KEY_F12 && PICKER[27].usage == HID_KEY_GRAVE,
              "picker usages as in TinyUSB hid.h");

enum UiMode : uint8_t { UI_NORMAL, UI_SETUP, UI_EDIT, UI_CONN /* (2a) */, UI_TRACE /* (2a) */ };
// Setup-UI key events, found by comparing keysState() between loops.
enum SetupKey : uint8_t { SK_CHAR, SK_ENTER, SK_DEL, SK_TAB };

// ---------------------------------------------------------------- HID objects
// Construction order matters (globals in one file are built in declaration order):
// the FIRST USBHID constructed fixes the interface protocol (USBHID.cpp:277-287).
// hidOut is declared first with HID_ITF_PROTOCOL_NONE, so the interface is a
// report-protocol HID interface whatever the order of the two devices below.
// hidOut adds no device; it only sends, so the result of each send can be checked
// (USBHIDKeyboard::sendReport() discards it).
USBHID hidOut(HID_ITF_PROTOCOL_NONE);
// Each device adds its report descriptor to the same interface, in this order:
USBHIDMouse UsbMouse;        // Report ID 2 (HID_REPORT_ID_MOUSE), relative mouse
USBHIDKeyboard UsbKeyboard;  // Report ID 1 (HID_REPORT_ID_KEYBOARD)
static_assert(HID_REPORT_ID_KEYBOARD == 1 && HID_REPORT_ID_MOUSE == 2, "report IDs");

// ---------------------------------------------------------------- mouse (PROFILE 2)
static_assert(MOUSE_LEFT == 0x01, "left button = hid_mouse_report_t.buttons bit 0");  // USBHIDMouse.h:30
// One report carries at most +-127 in X (TUD_HID_REPORT_DESC_MOUSE, logical -127..127).
static constexpr int32_t MOUSE_REPORT_MAX = 127;
static constexpr uint8_t MAX_MOUSE_REPORTS_PER_LOOP = 4;
// Unsent movement is capped at two loops' worth of reports; the excess is dropped.
static constexpr int32_t MOUSE_PENDING_MAX = MOUSE_REPORT_MAX * MAX_MOUSE_REPORTS_PER_LOOP * 2;

// (1d) Spinner sensitivity = HID counts per detent. 1 detent = COUNTS_PER_DETENT
// encoder counts, so each encoder count is worth SENS / 2 HID counts; the odd
// half is carried in mouseHalfRem. 1c used a fixed 8 (= 4 per encoder count).
// sensIndex is the value stored in NVS (1e-4).
// About 1.5x per step (plan B, agreed with Edgar 2026-10-12). Whether 128 is
// enough is to be judged on hardware.
static constexpr uint8_t SENS_STEPS[] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128};
static constexpr uint8_t SENS_COUNT = sizeof(SENS_STEPS) / sizeof(SENS_STEPS[0]);
static_assert(SENS_COUNT == 14, "14 steps");
static constexpr uint8_t SENS_DEFAULT_INDEX = 5;
static_assert(SENS_STEPS[SENS_DEFAULT_INDEX] == 8, "initial sensitivity = 1c (8 counts per detent)");
static_assert(COUNTS_PER_DETENT == 2, "half-count carry below assumes 2 encoder counts per detent");
static uint8_t sensIndex = SENS_DEFAULT_INDEX;

// ---------------------------------------------------------------- (1e-4) persistent settings
// Declared before any function (Arduino prototypes). Only these values are ever
// stored; HID state (held keys, mouse button, queues) has no place in the record.
struct Settings {
    uint8_t profile;                 // 1..4
    uint8_t sensIndex;               // 0..SENS_COUNT-1
    CustomKey custom[SLOT_COUNT];    // CW, CCW, BTN; flags: CK_HOLD on BTN only
};

// Record layout, version 1 (21 bytes, little-endian CRC):
//   0..3  magic "MJCF"      4  version (1)        5  length (21)
//   6     profile            7  sensitivity index
//   8..16 CW, CCW, BTN: modifiers, usage, flags (3 bytes each)
//   17..20 CRC-32 (IEEE, as zlib) of bytes 0..16
// A future version N reads version 1 by its own length and converts it, or
// rejects it; an older firmware rejects version N (CFG_NEWER_VERSION) and then
// does not write at all, so a newer record is never overwritten by a downgrade.
static constexpr uint8_t CFG_MAGIC[4] = {'M', 'J', 'C', 'F'};
static constexpr uint8_t CFG_VERSION = 1;
static constexpr size_t CFG_BODY = 17;                // bytes covered by the CRC
static constexpr size_t CFG_SIZE = CFG_BODY + 4;      // 21
static constexpr size_t CFG_MAX_READ = 64;            // larger blobs are rejected unread
static constexpr const char* NV_NAMESPACE = "mediajog";  // unique to this app, <= 15 chars
static constexpr const char* NV_KEY = "cfg";
// A write is refused (status NV FULL) unless at least this many 32-byte NVS entries
// are available; the record needs about 3. MediaJog never takes the last free
// space of an NVS partition it shares with the Launcher and other apps.
static constexpr size_t NV_MIN_AVAILABLE_ENTRIES = 32;
static constexpr uint32_t SENS_SAVE_DELAY_MS = 2000;  // after the last sensitivity step

enum CfgResult : uint8_t {
    CFG_OK, CFG_NONE, CFG_READ_ERROR, CFG_BAD_LENGTH, CFG_BAD_MAGIC,
    CFG_NEWER_VERSION, CFG_BAD_VERSION, CFG_BAD_CRC, CFG_BAD_FIELD
};
// Shown as "NV ..." on every screen.
enum NvState : uint8_t { NV_OK, NV_NEW, NV_BAD, NV_VER, NV_ERR, NV_FULL, NV_WFAIL };

static Settings persisted = {};        // the record NVS holds (defaults if none is valid)
static bool persistedInNvs = false;    // persisted is really stored: identical writes are skipped
static bool nvWritable = true;         // false after a read error or a newer-version record
static NvState nvState = NV_NEW;
static CfgResult cfgLoadResult = CFG_NONE;
static bool profileSavePending = false;  // set by a confirmed profile switch
static bool customSavePending = false;   // set by Save in CUSTOM SETUP only
static bool sensSavePending = false;     // set by a sensitivity step; debounced
static uint32_t sensChangedMs = 0;
static uint32_t nvWrites = 0, nvWriteFails = 0, nvSkips = 0, nvMaxWriteMs = 0;

// Button reports carry a state, not an event: sending the same state twice is
// harmless, so the button state is resent until a report is confirmed.
// Movement is relative: a report with unknown outcome is never resent.
static uint8_t mouseWanted = 0;        // buttons the host should see now
static uint8_t mouseHost = 0;          // buttons in the last confirmed report
static bool mouseSyncNeeded = false;   // resend the button state (unknown outcome, reconnect, profile switch)
static int32_t mousePendingDx = 0;     // movement not yet sent (HID counts)
static int32_t mouseHalfRem = 0;       // (1d) leftover half HID count: -1, 0 or +1
static int32_t mouseXSent = 0;         // sum of confirmed X movement (diagnostic)
static uint32_t mouseReports = 0;
static uint32_t mouseFails = 0;        // reports with unknown outcome
static uint32_t mouseClipped = 0;      // movement dropped at MOUSE_PENDING_MAX

// ---------------------------------------------------------------- event queue
enum Event : uint8_t { EV_CW, EV_CCW, EV_PRESS,
                       EV_HOLD_DOWN, EV_HOLD_UP /* (1e-3) Custom BTN HOLD, ordered with the taps */ };
// Declared here, before any function: the Arduino builder inserts prototypes above the first function.
enum TapResult : uint8_t { TAP_SENT, TAP_NOT_SENT, TAP_UNKNOWN };

// (1e-2) Custom profile state. Slot order equals Event order, so a queued event
// indexes its slot directly.
static_assert((uint8_t)SLOT_CW == (uint8_t)EV_CW && (uint8_t)SLOT_CCW == (uint8_t)EV_CCW
              && (uint8_t)SLOT_BTN == (uint8_t)EV_PRESS, "slot order = event order");
static CustomKey customSaved[SLOT_COUNT] = {};  // what Profile 4 sends; changed only by Save
static CustomKey customDraft[SLOT_COUNT] = {};  // edited in the setup UI; discarded by Cancel
static KeyAction customAction[SLOT_COUNT] = {}; // customSaved in the form the send path uses
static char customLabel[SLOT_COUNT][28] = {};   // labels for customAction (e.g. "Cmd+Right")
static uint8_t uiMode = UI_NORMAL;
static uint8_t setupSel = SLOT_CW;     // action selected in the setup screen
static CustomKey editUndo = {};        // value of the action when its edit started
static int8_t pickerIndex = -1;        // position in PICKER, -1 = not using the list
static int16_t uiAccum = 0;            // encoder counts not yet turned into a UI step
static bool setupDirty = true;         // setup screen needs a redraw
static bool draftChanged = false;      // draft differs from what was saved
// Previous keysState() for key-press edge detection in the setup UI.
static bool prevEnter = false, prevDel = false, prevTab = false;
static char prevChars[8] = {};
static uint8_t prevCharCount = 0;

// ---------------------------------------------------------------- (2a) transports
// Declared before any function (Arduino prototypes).
enum Transport : uint8_t { TRANSPORT_USB = 0, TRANSPORT_BLE = 1 };
// Outcome of one report on a backend (docs/STAGE_2A_DESIGN.md §2.3):
//   TX_SENT      USB: IN transfer completed;  BLE: accepted by the NimBLE host
//   TX_NOT_SENT  nothing left the device (USB endpoint not ready / BLE not accepted)
//   TX_UNKNOWN   USB only: queued, completion not seen within 100 ms
enum TxResult : uint8_t { TX_SENT, TX_NOT_SENT, TX_UNKNOWN };
enum SwitchState : uint8_t { SW_IDLE, SW_RELEASING };
static Transport activeTransport = TRANSPORT_USB;
static const char* const TRANSPORT_NAME[2] = {"USB", "BLE"};
static constexpr const char* BLE_DEVICE_NAME = "MediaJog BLE";  // as BLE-1 (the Mac's bond refers to it)

// Switching (design §4.3): release on the old transport, then commit.
static constexpr uint32_t SWITCH_RELEASE_MS = 200;     // budget for the release on the old transport
static constexpr uint32_t BLE_RELEASE_FLUSH_MS = 100;  // BLE: time to transmit an accepted release
static SwitchState switchState = SW_IDLE;
static Transport switchFrom = TRANSPORT_USB;
static Transport switchTo = TRANSPORT_USB;
static uint32_t switchStartMs = 0;
static uint32_t switchReleasedMs = 0;
static bool switchKbOk = false, switchMsOk = false;
// A release is needed only if the old transport has a host link at the start of the switch:
// USB mounted (also when suspended), BLE connected. Without a link no host can hold a key
// from this device, and a later link starts all-up (fresh enumeration / BLE link-up).
static bool switchReleaseNeeded = false;
static uint32_t switchCount = 0, switchWarnings = 0;
// A release that was not confirmed on a transport (warning "!"), and which halves of it got
// through since: cleared when the keyboard all-up and the mouse release were SENT on it.
static bool releaseOwed[2] = {false, false};
static bool owedKbOk[2] = {false, false};
static bool owedMsOk[2] = {false, false};
static Transport connSel = TRANSPORT_USB;              // selection on the CONNECTION screen
static bool usbHostReady = false;                      // USB mounted and not suspended (any mode)
static bool bleStarted = false;                        // BLE stack initialized (lazily, once)
static bool bleOn = false;                             // BLE is wanted: advertising / connected
static uint32_t headerSig = 0;                         // what the screen headers show (redraw on change)
// Saved transport (NVS namespace "mediajog", key "link"; design §7). Written at once after
// a confirmed switch, like the profile; read (never written) at boot.
static constexpr uint8_t LINK_MAGIC[4] = {'M', 'J', 'L', 'K'};
static constexpr uint8_t LINK_VERSION = 1;
static constexpr size_t LINK_BODY = 7;                 // bytes covered by the CRC
static constexpr size_t LINK_SIZE = LINK_BODY + 4;     // 11
static constexpr const char* NV_LINK_KEY = "link";
static bool linkSavePending = false;
static bool linkWritable = true;     // false after a read error or a newer-version link record
static bool linkStored = false;      // a valid link record is in NVS
static uint8_t linkStoredMode = TRANSPORT_USB;
static uint8_t linkLoadResult = 1;   // CfgResult of the boot read (1 = CFG_NONE)
static uint32_t linkWrites = 0, linkWriteFails = 0;

// ---------------------------------------------------------------- HID reports
// Report map = TinyUSB's TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)) followed by
// TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2)), written out byte by byte.
static constexpr uint8_t REPORT_ID_KEYBOARD = 1;
static constexpr uint8_t REPORT_ID_MOUSE = 2;
static const uint8_t HID_REPORT_MAP[] = {
    // Keyboard, Report ID 1
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x95, 0x08,        //   Report Count (8)
    0x75, 0x01,        //   Report Size (1)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)   modifiers
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Constant)                   reserved byte
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x91, 0x02,        //   Output (Data, Variable, Absolute)  LEDs
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Constant)                  LED padding
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0xFF, 0x00,  //   Usage Maximum (255)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x00,        //   Input (Data, Array, Absolute)      6 key codes
    0xC0,              // End Collection

    // Mouse, Report ID 2
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x05,        //     Usage Maximum (5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x05,        //     Report Count (5)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)  buttons
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x03,        //     Report Size (3)
    0x81, 0x01,        //     Input (Constant)                  padding
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data, Variable, Relative)  X, Y
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data, Variable, Relative)  wheel
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x06,        //     Input (Data, Variable, Relative)  pan
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// HID over GATT: the characteristic value is the report WITHOUT the Report ID;
// the Report ID comes from the characteristic's Report Reference descriptor
// (BLEHIDDevice::inputReport(id)). (The core's Server_Gamepad example puts the ID
// into the value; that is not done here.)
struct KeyboardReport {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
} __attribute__((packed));
struct MouseReport {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
    int8_t pan;
} __attribute__((packed));
static_assert(sizeof(KeyboardReport) == 8, "keyboard input report = 8 bytes");
static_assert(sizeof(MouseReport) == 5, "mouse input report = 5 bytes (as TinyUSB hid_mouse_report_t)");

enum LinkState : uint8_t { LINK_ADVERTISING, LINK_CONNECTED, LINK_ENCRYPTED, LINK_READY };

// ---------------------------------------------------------------- BLE objects and link state
static BLEServer* bleServer = nullptr;
// (B1) A characteristic value of the static GATT table: its value handle (written by
// ble_gatts_start(); 0xFFFF until then) and its current value (read by the host through
// gattValueAccess(), written by notifyReport() or by the host). Guarded by gattValueMux.
struct GattValue {
    uint16_t handle;
    uint8_t len;
    uint8_t maxLen;
    uint8_t data[8];
    uint16_t getHandle() const { return handle; }
};
static GattValue kbInputValue = {0xFFFF, 8, 8, {0}};       // Report ID 1, input (notify): all keys up
static GattValue kbOutputValue = {0xFFFF, 1, 1, {0}};      // Report ID 1, output (LEDs; written by the host, ignored)
static GattValue mouseInputValue = {0xFFFF, 5, 5, {0}};    // Report ID 2, input (notify): released
static GattValue protocolModeValue = {0xFFFF, 1, 1, {0x01}};  // Report Protocol
static GattValue controlPointValue = {0xFFFF, 1, 1, {0}};  // HID Control Point (suspend / exit), ignored
static GattValue batteryValue = {0xFFFF, 1, 1, {100}};     // fixed; the battery is not measured
static GattValue* const kbInput = &kbInputValue;
static GattValue* const kbOutput = &kbOutputValue;
static GattValue* const mouseInput = &mouseInputValue;
static portMUX_TYPE gattValueMux = portMUX_INITIALIZER_UNLOCKED;
static int gattRegisterRc = -1;  // (B1) ble_gatts_count_cfg() / ble_gatts_add_svcs() result

// Written by the NimBLE host task (BLE callbacks), read by loop(): atomics.
static std::atomic<bool> bleConnected{false};
static std::atomic<uint16_t> bleConnHandle{0xFFFF};
static std::atomic<uint32_t> bleConnGen{0};      // +1 on every connect and disconnect
static std::atomic<bool> kbSubscribed{false};    // keyboard input CCCD: notifications on
static std::atomic<bool> mouseSubscribed{false}; // mouse input CCCD: notifications on
static std::atomic<uint32_t> bleConnects{0};
static std::atomic<uint32_t> bleDisconnects{0};
static std::atomic<uint32_t> ledWrites{0};
static std::atomic<uint32_t> foreignSubscribes{0};  // (r2) subscribe events for another connection
static std::atomic<uint32_t> connAdopted{0};        // (r4) handles adopted from subscribe events before CONNECT

// (r3) Diagnostic trace. Records come from the NimBLE host task (GAP listener and
// the BLE library's callbacks) and from loop() (link state), so a short critical
// section protects the ring. Nothing in it is ever sent or stored.
enum TraceKind : uint8_t {  // app-level records; NimBLE events keep their BLE_GAP_EVENT_* number (< 64)
    TR_APP_CONNECT = 100,     // ServerCallbacks::onConnect as BLE-1 handled it
    TR_APP_DISCONNECT = 101,  // ServerCallbacks::onDisconnect
    TR_APP_SUBSCRIBE = 102,   // handleSubscribe(): reason 1 = accepted, 0 = foreign handle
    TR_LINK_READY = 103,      // loop(): link became READY
    TR_LINK_LOST = 104,       // loop(): link lost
    TR_APP_ADOPT = 105        // (r4) handle adopted from a subscribe event that came before CONNECT
};
struct TraceRec {
    uint32_t seq;     // 1, 2, 3 ...
    uint32_t ms;
    uint32_t gen;     // connection generation (counts NimBLE CONNECT events)
    int32_t code;     // full status / reason / value (never truncated)
    uint16_t conn;    // 0xFFFF = none
    uint16_t attr;    // SUBSCRIBE: characteristic value handle
    uint8_t type;     // BLE_GAP_EVENT_* or TraceKind
    uint8_t reason;   // SUBSCRIBE: W/T/R reason; TR_APP_SUBSCRIBE: 1 accepted, 0 foreign
    uint8_t notify;   // SUBSCRIBE: bit 0 cur_notify, bit 1 prev_notify; TR_APP_SUBSCRIBE: subValue bit 0
    uint8_t sec;      // bit 7 = link found; bit 0 encrypted, bit 1 bonded, bit 2 authenticated
};
static constexpr uint32_t TRACE_SIZE = 64;
static TraceRec traceBuf[TRACE_SIZE];
static uint32_t traceSeq = 0;                 // last sequence number written (guarded by traceMux)
static portMUX_TYPE traceMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint32_t> traceGen{0};     // NimBLE connection generation
static uint32_t traceTail = 0;                // loop(): last sequence number logged
static uint32_t traceLost = 0;
static constexpr uint8_t TRACE_TOKENS = 6;
static char traceTokens[TRACE_TOKENS][12];    // last tokens for the status page, oldest first
static struct ble_gap_event_listener gapListener;

// Link flags of a connection for a record (bit 7 = found). Not inside the critical section.
static uint8_t traceSecOf(uint16_t conn)
{
    if (conn == 0xFFFF) return 0;
    struct ble_gap_conn_desc d;
    if (ble_gap_conn_find(conn, &d) != 0) return 0;
    return 0x80 | (d.sec_state.encrypted ? 1 : 0) | (d.sec_state.bonded ? 2 : 0)
         | (d.sec_state.authenticated ? 4 : 0);
}

static void pushTrace(TraceRec r)
{
    r.gen = traceGen;
    r.ms = millis();
    portENTER_CRITICAL(&traceMux);
    r.seq = ++traceSeq;
    traceBuf[(r.seq - 1) % TRACE_SIZE] = r;
    portEXIT_CRITICAL(&traceMux);
}

static uint32_t traceNewest()
{
    portENTER_CRITICAL(&traceMux);
    uint32_t n = traceSeq;
    portEXIT_CRITICAL(&traceMux);
    return n;
}

// Copy of record seq (>= 1); false if it was already overwritten.
static bool traceRead(uint32_t seq, TraceRec* out)
{
    portENTER_CRITICAL(&traceMux);
    TraceRec r = traceBuf[(seq - 1) % TRACE_SIZE];
    portEXIT_CRITICAL(&traceMux);
    if (r.seq != seq) return false;
    *out = r;
    return true;
}

// (r2) Recorded by the host task when encryption succeeds (diagnostics only).
// cccdPeer / cccdPeerKb: CCCD records stored for this peer's identity address
// (all characteristics / the keyboard input report); cccdAllKb: for any peer.
struct EncSnapshot {
    uint32_t seq, gen;  // (r3) record number and connection generation of the ENC_CHANGE
    int32_t status;     // (r3) full ENC_CHANGE status (0 = ok)
    bool found;         // (r3) the link was found at that moment
    bool encrypted;     // (r3) its encrypted flag at that moment
    bool bonded;
    uint8_t idType, otaType;
    uint8_t id[6], ota[6];
    int cccdPeer, cccdPeerKb, cccdAllKb, bonds;
};
static EncSnapshot encSnap = {};
static std::atomic<uint32_t> encSnapCount{0};
static std::atomic<uint16_t> kbValueHandle{0};     // set after the GATT server started
static std::atomic<uint16_t> mouseValueHandle{0};

// (BLE-1 r4) loop() only.
static LinkState linkState = LINK_ADVERTISING;
static bool linkReady = false;         // BLE: connected, encrypted and keyboard subscribed
static bool linkBonded = false;
static uint32_t seenConnGen = 0;
static bool seenMouseSubscribed = false;
static uint32_t linkUps = 0;
static uint32_t linkLosses = 0;

// (B1) Outcome of the last notifyReport() (loop task).
enum NotifyStatus : int { NOTIFY_NONE = -1, NOTIFY_OK = 0, NOTIFY_NO_CLIENT, NOTIFY_NO_SUBSCRIBER, NOTIFY_HOST_ERROR };
static int lastNotifyStatus = NOTIFY_NONE;
static uint32_t lastNotifyCode = 0;    // ble_gatts_notify_custom() rc for NOTIFY_HOST_ERROR
static bool releaseRepeatPending = false;  // (BLE-1) repeat the all-up once after a BLE link-up
static uint32_t releaseRepeatAt = 0;
static constexpr uint32_t RELEASE_REPEAT_MS = 100;
static uint32_t notifyFails = 0;         // reports not accepted by the BLE host
static uint32_t bleKbReports = 0;        // BLE keyboard reports accepted
static uint32_t bleMouseReports = 0;     // BLE mouse reports accepted

// (BLE-1 r3) Trace pages: a frozen copy of the ring.
static constexpr uint8_t TRACE_LINES = 13;
static uint8_t tracePage = 0;           // 1.. while the trace pages are shown
static bool pageDirty = false;
static TraceRec viewBuf[TRACE_SIZE];   // chronological copy taken when the trace pages open
static uint32_t viewCount = 0;
static uint32_t viewAtMs = 0;
static uint8_t connCount = 0;           // open connections found by NimBLE (handles 0..15)
static uint32_t lastConnScanMs = 0;

// (1e-1) Names as shown on the status screen ("PROFILE 1 YOUTUBE" as in 1d; at most
// "PROFILE n " + 10 characters to fit one line at text size 2). Defined after the
// Event / TapResult enums: the Arduino builder inserts prototypes above the first function.
static const char* profileName(uint8_t p)
{
    switch (p) {
        case PROFILE_SPINNER: return "SPINNER";
        case PROFILE_EDIT:    return "EDITING";
        case PROFILE_CUSTOM:  return "CUSTOM";
        default:              return "YOUTUBE";
    }
}

// Events are mapped to keys when they are sent. Only the keyboard profiles (1 and 3)
// queue events, and setProfile() clears the queue on every switch, so a queued
// event is always sent with the mapping of the profile it was generated in.
static const KeyAction& actionFor(Event ev)
{
    // (1e-2) Profile 4 only queues events for assigned actions ((1e-3) key or modifier).
    if (profile == PROFILE_CUSTOM) return customAction[ev];
    if (profile == PROFILE_EDIT) {
        switch (ev) {
            case EV_CW:  return EDIT_CW;
            case EV_CCW: return EDIT_CCW;
            default:     return EDIT_PRESS;
        }
    }
    switch (ev) {
        case EV_CW:  return ACTION_CW;
        case EV_CCW: return ACTION_CCW;
        default:     return ACTION_PRESS;
    }
}

// Holds events in order until they are sent. The size only bounds memory;
// whether to cap or drop events is decided after measurement (plan §7-2).
static constexpr uint8_t QUEUE_SIZE = 32;
static Event queueBuf[QUEUE_SIZE];
static uint8_t queueHead = 0;
static uint8_t queueCount = 0;

static constexpr uint8_t MAX_TAPS_PER_LOOP = 4;

// ---------------------------------------------------------------- state
static bool unitFound = false;
static int16_t lastValue = 0;
static uint8_t lastButton = 1;
static int16_t accum = 0;  // counts not yet turned into a detent event (PROFILE 1)
static uint32_t lastProbeMs = 0;
static uint32_t readErrors = 0;

static volatile bool usbSuspended = false;
static bool hidReady = false;   // (2a, was hidReady) the ACTIVE backend is READY
static bool releasePending = false;  // the host may not have the base keyboard state yet

// (1e-3) Base keyboard state = what is held by a Custom HOLD (both 0 when nothing
// is held, which is always the case in Profiles 1-3). Every keyboard report is
// built as base + the tap being sent; the report that follows a tap returns to
// the base. kbHold* change only when a queued HOLD event is processed in
// sendQueued() (so taps keep their order) or in releaseHoldNow() (safety).
static uint8_t kbHoldMods = 0;
static uint8_t kbHoldUsage = 0;
static bool holdArmed = false;       // a HOLD down was queued and its HOLD up was not
static uint32_t holdDowns = 0;       // diagnostics
static uint32_t holdSafetyReleases = 0;

static uint32_t txCount[3] = {0, 0, 0};  // press report confirmed, per Event
static uint32_t sendFails = 0;           // press report outcome unknown (dropped)
static uint32_t skipped = 0;             // inputs while USB not ready
static uint32_t maxWorkMs = 0;           // longest loop() body, excluding delay(20)
static uint32_t maxTapMs = 0;            // longest press+release
static uint32_t queueOverflow = 0;
static uint8_t queueMax = 0;
static char lastEvent[24] = "-";

// Profile selected with the Cardputer keyboard: `profile` is declared above actionFor().
// Fn chords, judged by keysState().fn plus the base character in keysState().word.
enum Chord : uint8_t { CHORD_NONE, CHORD_P1, CHORD_P2, CHORD_P3, CHORD_P4, CHORD_SENS_UP, CHORD_SENS_DOWN,
                       CHORD_SETUP /* (1e-2) Fn+S, Profile 4 only */, CHORD_CONN /* (2a) Fn+B */ };
static_assert((uint8_t)CHORD_P1 == (uint8_t)PROFILE_YOUTUBE
              && (uint8_t)CHORD_P2 == (uint8_t)PROFILE_SPINNER
              && (uint8_t)CHORD_P3 == (uint8_t)PROFILE_EDIT
              && (uint8_t)CHORD_P4 == (uint8_t)PROFILE_CUSTOM,
              "chord value doubles as the profile number");
static uint32_t customIgnored = 0;  // inputs dropped in Profile 4 for unassigned actions
static uint8_t lastChord = CHORD_NONE;
static uint8_t kbPressed = 0;   // number of Cardputer keys held (diagnostic)
static bool kbFn = false;       // Fn held (diagnostic)
static char kbLastChar = 0;     // (1d) last base character seen (diagnostic)
static uint32_t profileChanges = 0;

static bool dirty = true;
static uint32_t lastDrawMs = 0;
static constexpr uint32_t DRAW_INTERVAL_MS = 100;

// (1d) PROFILE 2 screen: drawn in full only when the layout or USB state changes;
// afterwards only the sensitivity number and the diagnostics rows are overwritten.
enum Layout : uint8_t { LAYOUT_NONE, LAYOUT_STATUS, LAYOUT_SPINNER, LAYOUT_SETUP /* (1e-2) */,
                       LAYOUT_CONN /* (2a) */, LAYOUT_TRACE /* (2a) */ };
static uint8_t drawnLayout = LAYOUT_NONE;
static bool spinnerFull = true;     // redraw the whole spinner screen
static bool sensDirty = false;      // redraw the sensitivity number now
static uint32_t lastDiagMs = 0;
static constexpr uint32_t DIAG_INTERVAL_MS = 250;

// ---------------------------------------------------------------- BLE callbacks (NimBLE host task)
// Only flags and counters are changed here; loop() acts on them.

// (r4) true if NimBLE knows this handle as an open connection.
static bool connAlive(uint16_t h)
{
    if (h == 0xFFFF) return false;
    struct ble_gap_conn_desc d;
    return ble_gap_conn_find(h, &d) == 0;
}

// (r4) Registers h as the current connection if none is registered (or the
// registered one is no longer open, e.g. after a connect that failed before
// CONNECT was posted) and h is an open link. Returns true if h was adopted.
// Host task (BLE library callbacks).
static bool adoptConnHandle(uint16_t h)
{
    uint16_t cur = bleConnHandle;
    if (cur == h) return false;
    if (cur != 0xFFFF && connAlive(cur)) return false;  // another live connection is current
    if (!connAlive(h)) return false;
    bleConnHandle = h;
    kbSubscribed = false;  // nothing of an earlier connection is carried over
    mouseSubscribed = false;
    connAdopted++;
    TraceRec r = {};
    r.type = TR_APP_ADOPT;
    r.conn = h;
    r.code = cur;
    r.sec = traceSecOf(h);
    pushTrace(r);
    return true;
}

// (r4) Library onConnect(). The subscription flags are kept only if this handle
// was already adopted from an early subscribe event; otherwise they start cleared.
static void appConnect(uint16_t h)
{
    bool kept = (bleConnHandle == h);
    if (!kept) {
        kbSubscribed = false;
        mouseSubscribed = false;
    }
    bleConnHandle = h;
    bleConnected = true;
    bleConnGen++;
    bleConnects++;
    TraceRec r = {};
    r.type = TR_APP_CONNECT;
    r.conn = h;
    r.code = kept ? 1 : 0;
    r.sec = traceSecOf(h);
    pushTrace(r);
}

// (r4) A connection that failed before NimBLE posted CONNECT is reported as a
// CONNECT event with a non-zero status (ble_gap_conn_broken -> BLE_HS_EAGAIN); the
// BLE library then calls no callback. Called from the GAP listener (host task):
// drop an adoption of that handle, so a later link that reuses the handle number
// never inherits its subscription flags.
static void appConnectFailed(uint16_t h)
{
    if (bleConnected || bleConnHandle != h) return;
    bleConnHandle = 0xFFFF;
    kbSubscribed = false;
    mouseSubscribed = false;
}

// (r4) Library onDisconnect(). Only the current connection changes the state.
static void appDisconnect(uint16_t h)
{
    TraceRec r = {};  // handle being closed and the handle BLE-1 considered current
    r.type = TR_APP_DISCONNECT;
    r.conn = h;
    r.code = bleConnHandle;
    pushTrace(r);
    bleDisconnects++;
    if (h != bleConnHandle) return;  // (r4) not the current connection: state unchanged
    bleConnected = false;
    kbSubscribed = false;
    mouseSubscribed = false;
    bleConnHandle = 0xFFFF;  // (r2) no current connection
    bleConnGen++;
    // Advertising restarts by itself (BLEServer::advertiseOnDisconnect(true)).
}

// (r2) CCCD subscription of one input report. Only the current connection counts.
static void handleSubscribe(std::atomic<bool>* subscribed, uint16_t connHandle, uint16_t subValue)
{
    if (subscribed == nullptr) return;
    TraceRec r = {};  // (r3) what BLE-1 did with the library's onSubscribe()
    r.type = TR_APP_SUBSCRIBE;
    r.conn = connHandle;
    r.attr = (subscribed == &kbSubscribed) ? kbValueHandle.load() : mouseValueHandle.load();
    r.notify = (subValue & 0x0001) ? 1 : 0;
    r.code = bleConnHandle;  // the current handle at that moment
    r.sec = traceSecOf(connHandle);
    // (r4) NimBLE posts CONNECT for a peripheral link only after the peer's
    // version / feature exchange, but ENC_CHANGE and the CCCD RESTORE of a bonded
    // peer can come first. While no live connection is registered, adopt the
    // handle of this event if NimBLE knows it as a live link.
    if (adoptConnHandle(connHandle)) r.code = connHandle;
    if (connHandle != bleConnHandle) {
        foreignSubscribes++;
        r.reason = 0;
        pushTrace(r);
        return;
    }
    *subscribed = (subValue & 0x0001) != 0;  // bit 0 = notifications (NIMBLE_SUB_NOTIFY)
    r.reason = 1;
    pushTrace(r);
}
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*, ble_gap_conn_desc* desc) override
    {
        appConnect(desc->conn_handle);  // (r4)
    }
    void onDisconnect(BLEServer*, ble_gap_conn_desc* desc) override
    {
        appDisconnect(desc->conn_handle);  // (r4)
    }
};

// ---------------------------------------------------------------- (B1) static GATT table (NimBLE C API)
// Values and permissions as BLE-1 r4 registered them through the library (NimBLE path, where
// BLECharacteristic::setAccessPermissions() has no effect): reports READ | NOTIFY (input) and
// READ | WRITE | WRITE_NO_RSP (output) without an encryption flag; Report Map, HID Information
// READ; Control Point WRITE_NO_RSP; Protocol Mode READ | WRITE_NO_RSP. The link itself must be
// encrypted before anything is sent (READY rule). Descriptors are read-only here (the library
// gave them READ | WRITE).
static const ble_uuid16_t UUID_DIS = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t UUID_HID = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t UUID_BAS = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t UUID_MANUFACTURER = BLE_UUID16_INIT(0x2A29);
static const ble_uuid16_t UUID_PNP_ID = BLE_UUID16_INIT(0x2A50);
static const ble_uuid16_t UUID_HID_INFO = BLE_UUID16_INIT(0x2A4A);
static const ble_uuid16_t UUID_REPORT_MAP = BLE_UUID16_INIT(0x2A4B);
static const ble_uuid16_t UUID_HID_CONTROL = BLE_UUID16_INIT(0x2A4C);
static const ble_uuid16_t UUID_REPORT = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t UUID_PROTOCOL_MODE = BLE_UUID16_INIT(0x2A4E);
static const ble_uuid16_t UUID_BATTERY_LEVEL = BLE_UUID16_INIT(0x2A19);
static const ble_uuid16_t UUID_REPORT_REF = BLE_UUID16_INIT(0x2908);
static const ble_uuid16_t UUID_PRESENTATION = BLE_UUID16_INIT(0x2904);

struct GattConst {
    const uint8_t* data;
    uint16_t len;
};
static const char MANUFACTURER_NAME[] = "M5Stack Cardputer";
// As BLEHIDDevice::pnp(0x02, 0x303A, 0x1001, 0x0100), which writes VID / PID / version
// high byte first; kept byte for byte as BLE-1 r4 sent it.
static const uint8_t PNP_ID[] = {0x02, 0x30, 0x3A, 0x10, 0x01, 0x01, 0x00};
static const uint8_t HID_INFO[] = {0x11, 0x01, 0x00, 0x01};  // bcdHID 1.11, country 0, flags as hidInfo(0x00, 0x01)
static const uint8_t REF_KB_INPUT[] = {REPORT_ID_KEYBOARD, 0x01};   // Report Reference: ID 1, Input
static const uint8_t REF_KB_OUTPUT[] = {REPORT_ID_KEYBOARD, 0x02};  // ID 1, Output
static const uint8_t REF_MOUSE_INPUT[] = {REPORT_ID_MOUSE, 0x01};   // ID 2, Input
// As BLEHIDDevice's BLE2904: format uint8 (0x04), exponent 0, unit 0x27AD (percentage),
// namespace 1, description 0.
static const uint8_t BATTERY_FORMAT[] = {0x04, 0x00, 0xAD, 0x27, 0x01, 0x00, 0x00};
static const GattConst CONST_MANUFACTURER = {(const uint8_t*)MANUFACTURER_NAME, sizeof(MANUFACTURER_NAME) - 1};
static const GattConst CONST_PNP_ID = {PNP_ID, sizeof(PNP_ID)};
static const GattConst CONST_HID_INFO = {HID_INFO, sizeof(HID_INFO)};
static const GattConst CONST_REPORT_MAP = {HID_REPORT_MAP, sizeof(HID_REPORT_MAP)};
static const GattConst CONST_REF_KB_INPUT = {REF_KB_INPUT, sizeof(REF_KB_INPUT)};
static const GattConst CONST_REF_KB_OUTPUT = {REF_KB_OUTPUT, sizeof(REF_KB_OUTPUT)};
static const GattConst CONST_REF_MOUSE_INPUT = {REF_MOUSE_INPUT, sizeof(REF_MOUSE_INPUT)};
static const GattConst CONST_BATTERY_FORMAT = {BATTERY_FORMAT, sizeof(BATTERY_FORMAT)};

// Fixed values (characteristics and descriptors): read only. NimBLE applies the read offset.
static int gattConstAccess(uint16_t, uint16_t, struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    const GattConst* v = (const GattConst*)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR && ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(ctxt->om, v->data, v->len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// Variable values: read the current value; a host write (keyboard LEDs, Protocol Mode,
// Control Point) is stored if its length fits. The characteristic flags decide which
// operations NimBLE lets through.
static int gattValueAccess(uint16_t, uint16_t, struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    GattValue* v = (GattValue*)arg;
    uint8_t buf[sizeof(v->data)];
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        portENTER_CRITICAL(&gattValueMux);
        uint8_t n = v->len;
        memcpy(buf, v->data, n);
        portEXIT_CRITICAL(&gattValueMux);
        return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
        if (n == 0 || n > v->maxLen) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &n) != 0) return BLE_ATT_ERR_UNLIKELY;
        portENTER_CRITICAL(&gattValueMux);
        memcpy(v->data, buf, n);
        v->len = (uint8_t)n;
        portEXIT_CRITICAL(&gattValueMux);
        if (v == kbOutput) ledWrites++;  // keyboard LED output report: accepted and ignored
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void setGattValue(GattValue* v, const void* data, size_t len)
{
    if (len > v->maxLen) len = v->maxLen;
    portENTER_CRITICAL(&gattValueMux);
    memcpy(v->data, data, len);
    v->len = (uint8_t)len;
    portEXIT_CRITICAL(&gattValueMux);
}

static ble_gatt_dsc_def kbInputDscs[] = {
    {.uuid = &UUID_REPORT_REF.u, .att_flags = BLE_ATT_F_READ, .min_key_size = 0,
     .access_cb = gattConstAccess, .arg = (void*)&CONST_REF_KB_INPUT},
    {.uuid = nullptr, .att_flags = 0, .min_key_size = 0, .access_cb = nullptr, .arg = nullptr},
};
static ble_gatt_dsc_def kbOutputDscs[] = {
    {.uuid = &UUID_REPORT_REF.u, .att_flags = BLE_ATT_F_READ, .min_key_size = 0,
     .access_cb = gattConstAccess, .arg = (void*)&CONST_REF_KB_OUTPUT},
    {.uuid = nullptr, .att_flags = 0, .min_key_size = 0, .access_cb = nullptr, .arg = nullptr},
};
static ble_gatt_dsc_def mouseInputDscs[] = {
    {.uuid = &UUID_REPORT_REF.u, .att_flags = BLE_ATT_F_READ, .min_key_size = 0,
     .access_cb = gattConstAccess, .arg = (void*)&CONST_REF_MOUSE_INPUT},
    {.uuid = nullptr, .att_flags = 0, .min_key_size = 0, .access_cb = nullptr, .arg = nullptr},
};
static ble_gatt_dsc_def batteryDscs[] = {
    {.uuid = &UUID_PRESENTATION.u, .att_flags = BLE_ATT_F_READ, .min_key_size = 0,
     .access_cb = gattConstAccess, .arg = (void*)&CONST_BATTERY_FORMAT},
    {.uuid = nullptr, .att_flags = 0, .min_key_size = 0, .access_cb = nullptr, .arg = nullptr},
};

// Every field is given (no -Wmissing-field-initializers); NimBLE adds a CCCD to each NOTIFY
// characteristic, right after its value attribute.
#define B1_CHR(uuid_, cb_, arg_, dscs_, flags_, valHandle_)                                       \
    {.uuid = &(uuid_).u, .access_cb = (cb_), .arg = (void*)(arg_), .descriptors = (dscs_),      \
     .flags = (flags_), .min_key_size = 0, .val_handle = (valHandle_), .cpfd = nullptr}
#define B1_CHR_END {.uuid = nullptr, .access_cb = nullptr, .arg = nullptr, .descriptors = nullptr, \
                    .flags = 0, .min_key_size = 0, .val_handle = nullptr, .cpfd = nullptr}

static const ble_gatt_chr_def DIS_CHRS[] = {
    B1_CHR(UUID_MANUFACTURER, gattConstAccess, &CONST_MANUFACTURER, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    B1_CHR(UUID_PNP_ID, gattConstAccess, &CONST_PNP_ID, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    B1_CHR_END,
};
// One HID service: the attribute order (and so every handle) is fixed by this table.
static const ble_gatt_chr_def HID_CHRS[] = {
    B1_CHR(UUID_HID_INFO, gattConstAccess, &CONST_HID_INFO, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    B1_CHR(UUID_REPORT_MAP, gattConstAccess, &CONST_REPORT_MAP, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    B1_CHR(UUID_HID_CONTROL, gattValueAccess, &controlPointValue, nullptr, BLE_GATT_CHR_F_WRITE_NO_RSP,
           &controlPointValue.handle),
    B1_CHR(UUID_PROTOCOL_MODE, gattValueAccess, &protocolModeValue, nullptr,
           BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP, &protocolModeValue.handle),
    B1_CHR(UUID_REPORT, gattValueAccess, &kbInputValue, kbInputDscs, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
           &kbInputValue.handle),
    B1_CHR(UUID_REPORT, gattValueAccess, &kbOutputValue, kbOutputDscs,
           BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP, &kbOutputValue.handle),
    B1_CHR(UUID_REPORT, gattValueAccess, &mouseInputValue, mouseInputDscs, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
           &mouseInputValue.handle),
    B1_CHR_END,
};
static const ble_gatt_chr_def BAS_CHRS[] = {
    B1_CHR(UUID_BATTERY_LEVEL, gattValueAccess, &batteryValue, batteryDscs, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
           &batteryValue.handle),
    B1_CHR_END,
};
static const ble_gatt_svc_def GATT_SERVICES[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_DIS.u, .includes = nullptr, .characteristics = DIS_CHRS},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_HID.u, .includes = nullptr, .characteristics = HID_CHRS},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_BAS.u, .includes = nullptr, .characteristics = BAS_CHRS},
    {.type = 0, .uuid = nullptr, .includes = nullptr, .characteristics = nullptr},
};

// Queue the table. Must run after BLEDevice::createServer() (which resets NimBLE's table and
// adds the GAP / GATT services) and before bleServer->start() (ble_gatts_start(), the only
// point where queued services are registered and the val_handle variables are written).
static int registerGattTable()
{
    int rc = ble_gatts_count_cfg(GATT_SERVICES);
    if (rc == 0) rc = ble_gatts_add_svcs(GATT_SERVICES);
    return rc;
}

// Subscribe events of the two input reports. The library's BLEServer handled SUBSCRIBE only
// for its own characteristics (none here) and called BLECharacteristic::setSubscribe() ->
// ReportCallbacks::onSubscribe() -> handleSubscribe(). This listener does the same: same
// event, same NimBLE host task, the same conn_find check, the same value (bit 0 = notify;
// the reports have no indicate), so handleSubscribe() and the B7 handling behave as in r4.
// Registered before the trace listener: NimBLE calls the newest listener first (trace,
// then this one), and both run before the library's connection callback.
static ble_gap_event_listener subscribeListener;
static int onSubscribeEvent(struct ble_gap_event* ev, void*)
{
    if (ev->type != BLE_GAP_EVENT_SUBSCRIBE) return 0;
    std::atomic<bool>* flag = nullptr;
    if (ev->subscribe.attr_handle == kbInput->handle) {
        flag = &kbSubscribed;
    } else if (ev->subscribe.attr_handle == mouseInput->handle) {
        flag = &mouseSubscribed;
    } else {
        return 0;  // battery level or anything else: ignored, as before
    }
    ble_gap_conn_desc desc;
    if (ble_gap_conn_find(ev->subscribe.conn_handle, &desc) != 0) return 0;  // as setSubscribe()
    uint16_t subValue = ev->subscribe.cur_notify ? 0x0001 : 0x0000;  // NIMBLE_SUB_NOTIFY
    handleSubscribe(flag, desc.conn_handle, subValue);
    return 0;
}

// "Just Works" pairing, as in the core's Server_Gamepad example.
class SecurityCallbacks : public BLESecurityCallbacks {
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override
    {
        Serial.printf("[BLE] authentication %s\n", desc->sec_state.encrypted ? "complete" : "failed");
    }
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t) override {}
    bool onConfirmPIN(uint32_t) override { return true; }
};

// ---------------------------------------------------------------- BLE HID: reports
// The only function that sends a report. (B1) ble_gatts_notify_custom() to the current
// connection, only while this report's CCCD is on (the library's notify() checked its
// subscriber list). true = accepted by the NimBLE host (rc 0, the former SUCCESS_NOTIFY;
// not a delivery confirmation); false = not sent (no link, no CCCD, no buffer, host error).
static bool notifyReport(GattValue* c, const void* data, size_t len)
{
    if (c == nullptr) return false;
    lastNotifyStatus = NOTIFY_NONE;
    lastNotifyCode = 0;
    setGattValue(c, data, len);  // the value a host read returns
    const std::atomic<bool>& subscribed = (c == mouseInput) ? mouseSubscribed : kbSubscribed;
    uint16_t h = bleConnHandle;
    if (!bleConnected || h == 0xFFFF) {
        lastNotifyStatus = NOTIFY_NO_CLIENT;
    } else if (!subscribed) {
        lastNotifyStatus = NOTIFY_NO_SUBSCRIBER;
    } else {
        struct os_mbuf* om = ble_hs_mbuf_from_flat(data, (uint16_t)len);
        int rc = om ? ble_gatts_notify_custom(h, c->handle, om) : BLE_HS_ENOMEM;  // consumes om
        lastNotifyCode = (uint32_t)rc;
        lastNotifyStatus = (rc == 0) ? NOTIFY_OK : NOTIFY_HOST_ERROR;
    }
    if (lastNotifyStatus == NOTIFY_OK) return true;
    notifyFails++;
    Serial.printf("[BLE] notify not accepted: status %d code %lu\n", lastNotifyStatus,
                  (unsigned long)lastNotifyCode);
    return false;
}

// ---------------------------------------------------------------- (2a) HID backends
// One report goes to exactly one backend: the active one (hidSend*), or, during a
// switch / for an owed release, a named one (usbSend* / bleSend*).
static TxResult usbSendKeyboard(const hid_keyboard_report_t& r)
{
    if (!hidOut.ready()) return TX_NOT_SENT;  // nothing sent
    return hidOut.SendReport(HID_REPORT_ID_KEYBOARD, &r, sizeof(r)) ? TX_SENT : TX_UNKNOWN;
}

static TxResult usbSendMouse(const hid_mouse_report_t& r)
{
    if (!hidOut.ready()) return TX_NOT_SENT;
    return hidOut.SendReport(HID_REPORT_ID_MOUSE, &r, sizeof(r)) ? TX_SENT : TX_UNKNOWN;
}

// BLE: only while the BLE link is READY (connected, encrypted, keyboard CCCD); the
// mouse also needs its own CCCD. notifyReport() is BLE-1 r4's (SUCCESS_NOTIFY only).
static TxResult bleSendKeyboard(const hid_keyboard_report_t& r)
{
    if (!linkReady) return TX_NOT_SENT;
    if (!notifyReport(kbInput, &r, sizeof(r))) return TX_NOT_SENT;
    bleKbReports++;
    return TX_SENT;
}

static TxResult bleSendMouse(const hid_mouse_report_t& r)
{
    if (!linkReady || !mouseSubscribed) return TX_NOT_SENT;
    if (!notifyReport(mouseInput, &r, sizeof(r))) return TX_NOT_SENT;
    bleMouseReports++;
    return TX_SENT;
}

static TxResult hidSendKeyboard(const hid_keyboard_report_t& r)
{
    return (activeTransport == TRANSPORT_BLE) ? bleSendKeyboard(r) : usbSendKeyboard(r);
}

static TxResult hidSendMouse(const hid_mouse_report_t& r)
{
    return (activeTransport == TRANSPORT_BLE) ? bleSendMouse(r) : usbSendMouse(r);
}

// An owed release (design §4.3) is done when, on that transport, an ALL-KEYS-UP keyboard
// report and a mouse report with NO BUTTON pressed were SENT (review 1: callers pass
// keyboard / mouse = true only for such reports; a base report that carries a CUSTOM
// HOLD, or a mouse report with the drag button down, does not count). On BLE without a
// mouse CCCD the host cannot hold a BLE mouse button, so the keyboard half is enough there.
static void noteReleaseSent(Transport t, bool keyboard, bool mouse)
{
    if (!releaseOwed[t]) return;
    if (keyboard) owedKbOk[t] = true;
    if (mouse) owedMsOk[t] = true;
    bool msDone = owedMsOk[t] || (t == TRANSPORT_BLE && !mouseSubscribed);
    if (owedKbOk[t] && msDone) {
        releaseOwed[t] = false;
        snprintf(lastEvent, sizeof(lastEvent), "%s RELEASE SENT", TRANSPORT_NAME[t]);
        Serial.printf("[SWITCH] owed release on %s done\n", TRANSPORT_NAME[t]);
        dirty = true;
    }
}

// ---------------------------------------------------------------- U135 reads (same as v2)
static bool probeUnit()
{
    Wire.beginTransmission(ENCODER_ADDR);
    return Wire.endTransmission() == 0;
}

// Same transaction sequence as Unit_Encoder::readBytes() (M5Unit-Encoder 0.0.2),
// but returns false instead of passing 0xFF filler bytes on as data.
static bool readRegChecked(uint8_t reg, uint8_t* buf, uint8_t len)
{
    Wire.beginTransmission(ENCODER_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((uint8_t)ENCODER_ADDR, len) != len) return false;
    for (uint8_t i = 0; i < len; i++) {
        int b = Wire.read();
        if (b < 0) return false;
        buf[i] = (uint8_t)b;
    }
    return true;
}

static bool readEncoder(int16_t& value)
{
    uint8_t d[2];
    if (!readRegChecked(ENCODER_REG, d, 2)) return false;
    value = (int16_t)(d[0] | (d[1] << 8));
    return true;
}

// Register 0x20 is documented as 0 or 1; anything else is rejected.
static bool readButtonRaw(uint8_t& raw)
{
    uint8_t d;
    if (!readRegChecked(BUTTON_REG, &d, 1)) return false;
    if (d > 1) return false;
    raw = d;
    return true;
}

// Returns false if either initial read fails; the caller then treats the unit as not found.
static bool readInitialState()
{
    if (!readEncoder(lastValue) || !readButtonRaw(lastButton)) {
        readErrors++;
        Serial.println("[U135] initial read failed");
        return false;
    }
    accum = 0;
    Serial.printf("[U135] found. initial value=%d button_raw=%u\n", lastValue, lastButton);
    return true;
}

// ---------------------------------------------------------------- USB HID: keyboard (same as 1b)
static void onUsbEvent(void*, esp_event_base_t base, int32_t id, void*)
{
    if (base != ARDUINO_USB_EVENTS) return;
    if (id == ARDUINO_USB_SUSPEND_EVENT) {
        usbSuspended = true;
    } else if (id == ARDUINO_USB_RESUME_EVENT || id == ARDUINO_USB_STARTED_EVENT
               || id == ARDUINO_USB_STOPPED_EVENT) {
        usbSuspended = false;
    }
}

// SendReport() returns true only after TinyUSB reports the IN transfer complete
// (the host has read the report). false covers two different cases:
//   - not ready / not queued: nothing reached the host
//   - queued but no completion within 100 ms: it may still reach the host later
// Checking ready() first separates the first case, which is safe to retry.
// (1e-3) The report is the base (held) state plus the given tap. With nothing
// held (kbHoldMods = kbHoldUsage = 0) this is exactly the 1e-2 report:
// modifier = modifiers, keycode[0] = usage, rest 0.
static TxResult sendKeyReport(uint8_t modifiers, uint8_t usage)
{
    hid_keyboard_report_t r = {};
    r.modifier = kbHoldMods | modifiers;
    uint8_t n = 0;
    if (kbHoldUsage) r.keycode[n++] = kbHoldUsage;
    // A tap of the key that is already held cannot be expressed in a report
    // (the key is already down); the report then equals the base state.
    if (usage && usage != kbHoldUsage) r.keycode[n++] = usage;
    return hidSendKeyboard(r);  // (2a) active backend: SENT / NOT_SENT / UNKNOWN
}

// Base-state report: (1e-3) the held Custom HOLD key/modifiers if any, otherwise
// all keys up (exactly the 1e-2 all-keys-up report). Retried every loop until it
// succeeds, so a failed release cannot leave a key held (and auto-repeating) on
// the host. Resending it is harmless: it is a state, not an event.
static bool sendRelease()
{
    if (sendKeyReport(0, 0) == TX_SENT) {
        releasePending = false;
        // (2a review 1) Counts as a release only if the base carried no HOLD (all keys up).
        if (kbHoldMods == 0 && kbHoldUsage == 0) noteReleaseSent(activeTransport, true, false);
        return true;
    }
    releasePending = true;
    return false;
}

static TapResult tapKey(const KeyAction& a)
{
    uint32_t t0 = millis();
    TxResult pressed = sendKeyReport(a.modifiers, a.usage);
    if (pressed == TX_NOT_SENT) return TAP_NOT_SENT;  // (2a) nothing sent; keep the event
    // Always follow with a release, also when the press timed out: the press may
    // still reach the host, and a pending release is retried until it succeeds.
    sendRelease();
    uint32_t dt = millis() - t0;
    if (dt > maxTapMs) maxTapMs = dt;
    // A press with unknown outcome is not resent: that could double the step.
    return (pressed == TX_SENT) ? TAP_SENT : TAP_UNKNOWN;
}

static void queueClear()
{
    queueHead = 0;
    queueCount = 0;
}

// ---------------------------------------------------------------- (1e-3) Custom HOLD
static bool isHoldEvent(Event ev)
{
    return ev == EV_HOLD_DOWN || ev == EV_HOLD_UP;
}

static void queuePushRaw(Event ev)
{
    queueBuf[(queueHead + queueCount) % QUEUE_SIZE] = ev;
    queueCount++;
    if (queueCount > queueMax) queueMax = queueCount;
}

// Knob pressed with BTN = HOLD. Queued behind any taps already waiting, so those
// are sent without the held modifiers.
static void pushHoldDown()
{
    if (queueCount >= QUEUE_SIZE) {
        queueOverflow++;  // no room: this press is dropped, nothing is held
        return;
    }
    queuePushRaw(EV_HOLD_DOWN);
    holdArmed = true;
}

// Knob released after a queued HOLD down. Taps queued while the knob was held are
// sent first (with the held modifiers). A HOLD up must never be lost: if the queue
// is full it is cleared first, dropping the waiting taps rather than the release.
static void pushHoldUp()
{
    holdArmed = false;
    if (queueCount >= QUEUE_SIZE) {
        queueOverflow += queueCount;
        queueClear();
    }
    queuePushRaw(EV_HOLD_UP);
}

// Safety release, at once and outside the queue: profile change, CUSTOM SETUP
// entry, U135 loss, USB not ready. Clears the queue (including any HOLD down not
// yet processed) and requests the all-keys-up base report. Nothing is resumed
// later: a new physical knob press is needed (the knob edge logic sees no new press).
// Does nothing when no HOLD is armed or held, so the callers' 1e-2 behavior is
// unchanged in that case (no extra queue clear, no extra report).
static void releaseHoldNow()
{
    if (!holdArmed && !kbHoldMods && !kbHoldUsage) return;
    holdSafetyReleases++;
    Serial.println("[HOLD] safety release");
    holdArmed = false;
    kbHoldMods = 0;
    kbHoldUsage = 0;
    queueClear();
    releasePending = true;
}

// Events are only queued while USB is ready; otherwise they are counted and
// dropped, so plugging the cable in never replays old rotation.
static void pushEvent(Event ev)
{
    if (!hidReady) {
        skipped++;
        if (activeTransport == TRANSPORT_BLE) {
            // (BLE-1 r2) why: not connected / connected but not encrypted / no keyboard CCCD
            snprintf(lastEvent, sizeof(lastEvent), "%s",
                     linkState == LINK_ENCRYPTED ? "SKIP (no CCCD)"
                     : linkState == LINK_CONNECTED ? "SKIP (not encrypted)" : "SKIP (no link)");
        } else {
            snprintf(lastEvent, sizeof(lastEvent), "SKIP %s", actionFor(ev).label);
        }
        Serial.printf("[HID] skip %s (%s not ready)\n", actionFor(ev).label, TRANSPORT_NAME[activeTransport]);
        return;
    }
    if (queueCount >= QUEUE_SIZE) {
        queueOverflow++;
        Serial.println("[HID] queue overflow");
        return;
    }
    queueBuf[(queueHead + queueCount) % QUEUE_SIZE] = ev;
    queueCount++;
    if (queueCount > queueMax) queueMax = queueCount;
}

static void sendQueued()
{
    if (releasePending && !sendRelease()) return;

    for (uint8_t n = 0; n < MAX_TAPS_PER_LOOP && queueCount > 0; n++) {
        Event ev = queueBuf[queueHead];
        // (1e-3) HOLD down/up change the base state in queue order, then the base
        // report is sent (and retried by releasePending until confirmed) before
        // any later tap. Only Profile 4 queues these events.
        if (isHoldEvent(ev)) {
            queueHead = (queueHead + 1) % QUEUE_SIZE;
            queueCount--;
            if (ev == EV_HOLD_DOWN) {
                kbHoldMods = customSaved[SLOT_BTN].modifiers;
                kbHoldUsage = customSaved[SLOT_BTN].usage;
                holdDowns++;
                // Display text only: at most 18 label characters fit after "HOLD ".
                snprintf(lastEvent, sizeof(lastEvent), "HOLD %.18s", customLabel[SLOT_BTN]);
            } else {
                kbHoldMods = 0;
                kbHoldUsage = 0;
                snprintf(lastEvent, sizeof(lastEvent), "HOLD UP");
            }
            Serial.printf("[HOLD] %s\n", ev == EV_HOLD_DOWN ? "down" : "up");
            dirty = true;
            releasePending = true;
            if (!sendRelease()) return;  // base state retried next loop, before any tap
            continue;
        }
        const KeyAction& a = actionFor(ev);
        TapResult res = tapKey(a);
        if (res == TAP_NOT_SENT) return;  // retried next loop, order kept
        if (res == TAP_SENT) {
            txCount[ev]++;
            snprintf(lastEvent, sizeof(lastEvent), "TX %s", a.label);
            Serial.printf("[HID] tap %s\n", a.label);
        } else {
            sendFails++;
            snprintf(lastEvent, sizeof(lastEvent), "FAIL %s", a.label);
            Serial.printf("[HID] press outcome unknown: %s\n", a.label);
        }
        queueHead = (queueHead + 1) % QUEUE_SIZE;
        queueCount--;
        dirty = true;
        if (releasePending) return;
    }
}

// ---------------------------------------------------------------- USB HID: mouse (1c)
static void setMouseButton(uint8_t buttons)
{
    if (buttons == mouseWanted) return;
    mouseWanted = buttons;
    snprintf(lastEvent, sizeof(lastEvent), "MOUSE %s", buttons ? "DOWN" : "UP");
    Serial.printf("[MOUSE] button %s\n", buttons ? "down" : "up");
    dirty = true;
}

// Sends the current button state (and any pending X movement) until the host has
// confirmed the wanted state. Called every loop while USB is ready, in every
// profile, so a release still goes out after leaving PROFILE 2.
static void sendMouse()
{
    bool needState = mouseSyncNeeded || (mouseWanted != mouseHost);

    for (uint8_t n = 0; n < MAX_MOUSE_REPORTS_PER_LOOP && (needState || mousePendingDx != 0); n++) {
        int32_t chunk = mousePendingDx;
        if (chunk > 127) chunk = 127;
        if (chunk < -127) chunk = -127;

        hid_mouse_report_t r = {};
        r.buttons = mouseWanted;
        r.x = (int8_t)chunk;
        TxResult t = hidSendMouse(r);  // (2a) active backend
        if (t == TX_NOT_SENT) return;  // nothing sent; state and movement kept for next loop
        mousePendingDx -= chunk;  // movement is not resent, whatever the outcome

        if (t != TX_SENT) {
            // Outcome unknown: the button state is resent next loop (harmless).
            mouseFails++;
            mouseSyncNeeded = true;
            dirty = true;
            Serial.println("[MOUSE] report outcome unknown");
            return;
        }
        mouseReports++;
        // (2a review 1) Counts as a release only if no button was pressed in this report.
        if (r.buttons == 0) noteReleaseSent(activeTransport, false, true);
        mouseXSent += chunk;
        if (mouseHost != mouseWanted || mouseSyncNeeded) dirty = true;
        mouseHost = mouseWanted;
        mouseSyncNeeded = false;
        needState = false;
    }
}

// ---------------------------------------------------------------- (2a) link up / down (both transports)
// The active backend stopped being READY: from 1e-4 updateUsbState(). Nothing can be
// sent now. Forget the press and the movement, so reconnecting never presses the
// button or replays rotation; a new press of the knob is needed.
static void onLinkDown()
{
    releaseHoldNow();  // (1e-3) forget a Custom HOLD; the ready period starts all-up
    queueClear();
    setMouseButton(0);
    mousePendingDx = 0;
    mouseHalfRem = 0;
    releaseRepeatPending = false;  // (BLE-1)
}

// The active backend became READY: start with an all-keys-up report and a mouse
// report with the button released (mouseWanted is 0 here). BLE (no delivery
// confirmation) also repeats the all-up once after RELEASE_REPEAT_MS (BLE-1).
static void onLinkUp(uint32_t now, bool repeatAllUp)
{
    releasePending = true;
    mouseSyncNeeded = true;
    if (repeatAllUp) {
        releaseRepeatPending = true;
        releaseRepeatAt = now + RELEASE_REPEAT_MS;
    }
}

// ---------------------------------------------------------------- USB state
static void updateUsbState()
{
    bool ready = (bool)USB && !usbSuspended;
    if (ready != usbHostReady) {  // (2a) host state, in either mode (display, owed release)
        usbHostReady = ready;
        Serial.printf("[USB] %s\n", ready ? "ready" : "not ready");
        dirty = true;
        spinnerFull = true;  // (1d) the USB line is part of the static spinner screen
    }
    // (2a) Only the active transport drives hidReady; never during a switch.
    if (activeTransport != TRANSPORT_USB || switchState != SW_IDLE) return;
    if (ready != hidReady) {
        hidReady = ready;
        if (!ready) onLinkDown();
        else onLinkUp(millis(), false);  // USB: unchanged from 1e-4 (no repeat)
    }
}

// ---------------------------------------------------------------- (2a) BLE link (BLE-1 r4)
// BLE-1 r4's link-state hooks. The BLE link state is always tracked (trace,
// display); it drives hidReady only while BLE is the active transport.
static void onLinkLost()
{
    linkLosses++;
    TraceRec r = {};  // (r3)
    r.type = TR_LINK_LOST;
    r.conn = bleConnHandle;
    r.sec = traceSecOf(r.conn);
    pushTrace(r);
    Serial.println("[BLE] link lost");
    if (activeTransport != TRANSPORT_BLE || switchState != SW_IDLE || !hidReady) return;
    hidReady = false;
    onLinkDown();
    accum = 0;  // (BLE-1)
    snprintf(lastEvent, sizeof(lastEvent), "LINK LOST");
}

static void onLinkReady(uint32_t now)
{
    linkUps++;
    TraceRec r = {};  // (r3)
    r.type = TR_LINK_READY;
    r.conn = bleConnHandle;
    r.sec = traceSecOf(r.conn);
    pushTrace(r);
    Serial.println("[BLE] link ready");
    if (activeTransport != TRANSPORT_BLE || switchState != SW_IDLE) return;
    hidReady = true;
    onLinkUp(now, true);
    snprintf(lastEvent, sizeof(lastEvent), "LINK READY");
}

// (BLE-1 r4, unchanged)
static void updateLinkState(uint32_t now)
{
    bool connected = bleConnected;
    uint32_t gen = bleConnGen;
    bool encrypted = false;
    bool bonded = false;
    if (connected) {
        ble_gap_conn_desc d;
        if (ble_gap_conn_find(bleConnHandle, &d) == 0) {
            encrypted = d.sec_state.encrypted;
            bonded = d.sec_state.bonded;
        }
    }
    bool ready = connected && encrypted && kbSubscribed;

    // A disconnect + reconnect between two loops is still a link loss.
    if (gen != seenConnGen) {
        seenConnGen = gen;
        if (linkReady) {
            linkReady = false;
            onLinkLost();
        }
        dirty = true;
    }
    if (linkReady && !ready) {
        linkReady = false;
        onLinkLost();
    } else if (!linkReady && ready) {
        linkReady = true;
        onLinkReady(now);
    }
    // Mouse subscription can come after the keyboard's: sync its state then.
    bool ms = mouseSubscribed;
    if (linkReady && ms && !seenMouseSubscribed) mouseSyncNeeded = true;
    seenMouseSubscribed = ms;

    LinkState s = !connected ? LINK_ADVERTISING
                : ready ? LINK_READY
                : encrypted ? LINK_ENCRYPTED : LINK_CONNECTED;
    if (s != linkState || bonded != linkBonded) {
        linkState = s;
        linkBonded = bonded;
        Serial.printf("[BLE] state %u bonded %u\n", s, bonded ? 1 : 0);
        dirty = true;
    }
}

// ---------------------------------------------------------------- (r2) diagnostics: GAP event trace
// Identity form of an address type, as NimBLE uses it for store keys
// (ble_hs_misc_peer_addr_type_to_id()).
static ble_addr_t toIdAddr(ble_addr_t a)
{
    if (a.type == BLE_ADDR_PUBLIC_ID) a.type = BLE_ADDR_PUBLIC;
    else if (a.type == BLE_ADDR_RANDOM_ID) a.type = BLE_ADDR_RANDOM;
    return a;
}

// Number of CCCD records in NimBLE's store matching the key (read-only).
// peer all-zero = any peer; chrValHandle 0 = any characteristic. Host task only.
static int countCccd(const ble_addr_t& peer, uint16_t chrValHandle)
{
    struct ble_store_key_cccd key = {};
    key.peer_addr = peer;
    key.chr_val_handle = chrValHandle;
    struct ble_store_value_cccd val;
    int n = 0;
    for (uint8_t i = 0; i < 32; i++) {
        key.idx = i;
        if (ble_store_read_cccd(&key, &val) != 0) break;
        n++;
    }
    return n;
}

// NimBLE GAP event listener (host task). Records, never changes, anything. It is
// called before the BLE library's own handler for the same event.
static int onGapEvent(struct ble_gap_event* ev, void*)
{
    TraceRec t = {};
    t.type = ev->type;
    t.conn = 0xFFFF;
    switch (ev->type) {
        case BLE_GAP_EVENT_CONNECT:
            t.conn = ev->connect.conn_handle;
            t.code = ev->connect.status;
            if (t.code == 0) traceGen++;  // a new connection generation starts here
            else appConnectFailed(t.conn);  // (r4) the only state change made by the listener
            break;
        case BLE_GAP_EVENT_LINK_ESTAB:
            t.conn = ev->link_estab.conn_handle;
            t.code = ev->link_estab.status;
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            // The descriptor in the event still carries the link flags.
            t.conn = ev->disconnect.conn.conn_handle;
            t.code = ev->disconnect.reason;
            t.sec = 0x80 | (ev->disconnect.conn.sec_state.encrypted ? 1 : 0)
                  | (ev->disconnect.conn.sec_state.bonded ? 2 : 0)
                  | (ev->disconnect.conn.sec_state.authenticated ? 4 : 0);
            pushTrace(t);
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE: {
            t.conn = ev->enc_change.conn_handle;
            t.code = ev->enc_change.status;
            // Delivered before NimBLE restores the CCCDs of a bonded peer
            // (ble_gap_enc_event -> ble_gatts_bonding_restored). Recorded for
            // success AND failure.
            EncSnapshot e = {};
            e.status = t.code;
            e.gen = traceGen;
            struct ble_gap_conn_desc d;
            if (ble_gap_conn_find(t.conn, &d) == 0) {
                e.found = true;
                e.encrypted = d.sec_state.encrypted;
                e.bonded = d.sec_state.bonded;
                e.idType = d.peer_id_addr.type;
                e.otaType = d.peer_ota_addr.type;
                memcpy(e.id, d.peer_id_addr.val, 6);
                memcpy(e.ota, d.peer_ota_addr.val, 6);
                ble_addr_t any = {};
                ble_addr_t peer = toIdAddr(d.peer_id_addr);
                e.cccdPeer = countCccd(peer, 0);
                e.cccdPeerKb = countCccd(peer, kbValueHandle);
                e.cccdAllKb = countCccd(any, kbValueHandle);
            }
            if (ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &e.bonds) != 0) e.bonds = -1;
            t.sec = traceSecOf(t.conn);
            pushTrace(t);
            e.seq = traceNewest();
            encSnap = e;
            encSnapCount++;
            return 0;
        }
        case BLE_GAP_EVENT_PARING_COMPLETE:  // a fresh pairing finished (not a restore)
            t.conn = ev->pairing_complete.conn_handle;
            t.code = ev->pairing_complete.status;
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            t.conn = ev->subscribe.conn_handle;
            t.attr = ev->subscribe.attr_handle;
            t.reason = ev->subscribe.reason;
            t.notify = (ev->subscribe.cur_notify ? 1 : 0) | (ev->subscribe.prev_notify ? 2 : 0);
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING:
            t.conn = ev->repeat_pairing.conn_handle;
            break;
        case BLE_GAP_EVENT_IDENTITY_RESOLVED:
            t.conn = ev->identity_resolved.conn_handle;
            break;
        case BLE_GAP_EVENT_MTU:
            t.conn = ev->mtu.conn_handle;
            t.code = ev->mtu.value;
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            t.conn = ev->conn_update.conn_handle;
            t.code = ev->conn_update.status;
            break;
        case BLE_GAP_EVENT_TERM_FAILURE:
            t.conn = ev->term_failure.conn_handle;
            t.code = ev->term_failure.status;
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            t.conn = ev->passkey.conn_handle;
            t.code = ev->passkey.params.action;
            break;
        default:
            return 0;  // not traced (e.g. NOTIFY_TX)
    }
    t.sec = traceSecOf(t.conn);
    pushTrace(t);
    return 0;
}

static char subChar(uint16_t attr, uint16_t kbHandle, uint16_t msHandle)
{
    return (attr == kbHandle) ? 'k' : (attr == msHandle) ? 'm' : 'x';
}

static char subReasonChar(uint8_t reason)
{
    return reason == BLE_GAP_SUBSCRIBE_REASON_WRITE ? 'W'
         : reason == BLE_GAP_SUBSCRIBE_REASON_TERM ? 'T'
         : reason == BLE_GAP_SUBSCRIBE_REASON_RESTORE ? 'R' : '?';
}

// Compact token for the status page. Codes are printed in full (hex, no padding):
// E!D = 0x00D, E!20D = 0x20D, E!50D = 0x50D.
static void formatTrace(const TraceRec& t, uint16_t kbHandle, uint16_t msHandle, char* out, size_t n)
{
    unsigned c = (unsigned)t.code;
    switch (t.type) {
        case BLE_GAP_EVENT_CONNECT:
            if (t.code == 0) snprintf(out, n, "C%u", t.conn);
            else snprintf(out, n, "C!%X", c);
            break;
        case BLE_GAP_EVENT_LINK_ESTAB: snprintf(out, n, t.code == 0 ? "L0" : "L!%X", c); break;
        case BLE_GAP_EVENT_DISCONNECT: snprintf(out, n, "D%X", c); break;
        case BLE_GAP_EVENT_ENC_CHANGE: snprintf(out, n, t.code == 0 ? "E0" : "E!%X", c); break;
        case BLE_GAP_EVENT_PARING_COMPLETE: snprintf(out, n, t.code == 0 ? "P0" : "P!%X", c); break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            snprintf(out, n, "S%c%c%u", subChar(t.attr, kbHandle, msHandle), subReasonChar(t.reason),
                     (unsigned)(t.notify & 1));
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING: snprintf(out, n, "RP"); break;
        case BLE_GAP_EVENT_IDENTITY_RESOLVED: snprintf(out, n, "ID"); break;
        case BLE_GAP_EVENT_MTU: snprintf(out, n, "M"); break;
        case BLE_GAP_EVENT_CONN_UPDATE: snprintf(out, n, "U"); break;
        case BLE_GAP_EVENT_TERM_FAILURE: snprintf(out, n, "TF"); break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: snprintf(out, n, "PK"); break;
        case TR_APP_CONNECT: snprintf(out, n, t.code ? "a+k" : "a+"); break;
        case TR_APP_ADOPT: snprintf(out, n, "aA%u", t.conn); break;
        case TR_APP_DISCONNECT: snprintf(out, n, "a-"); break;
        case TR_APP_SUBSCRIBE:
            if (t.reason) snprintf(out, n, "a%c%u", subChar(t.attr, kbHandle, msHandle), (unsigned)(t.notify & 1));
            else snprintf(out, n, "afs");
            break;
        case TR_LINK_READY: snprintf(out, n, "RDY"); break;
        case TR_LINK_LOST: snprintf(out, n, "LST"); break;
        default: snprintf(out, n, "?%u", t.type); break;
    }
}

// One line of the trace page (40 characters at text size 1), e.g.
// "123 1234.5 g3 h1 ENC  !D     E1B1" or "124 1234.6 g3 h1 SUB  k R1   E1B1".
static void formatTraceLine(const TraceRec& t, uint16_t kbHandle, uint16_t msHandle, char* out, size_t n)
{
    const char* name;
    char detail[12];
    unsigned c = (unsigned)t.code;
    detail[0] = 0;
    switch (t.type) {
        case BLE_GAP_EVENT_CONNECT: name = "CONN"; snprintf(detail, sizeof(detail), t.code == 0 ? "ok" : "!%X", c); break;
        case BLE_GAP_EVENT_LINK_ESTAB: name = "LEST"; snprintf(detail, sizeof(detail), t.code == 0 ? "ok" : "!%X", c); break;
        case BLE_GAP_EVENT_DISCONNECT: name = "DISC"; snprintf(detail, sizeof(detail), "%X", c); break;
        case BLE_GAP_EVENT_ENC_CHANGE: name = "ENC"; snprintf(detail, sizeof(detail), t.code == 0 ? "ok" : "!%X", c); break;
        case BLE_GAP_EVENT_PARING_COMPLETE: name = "PAIR"; snprintf(detail, sizeof(detail), t.code == 0 ? "ok" : "!%X", c); break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            name = "SUB";
            snprintf(detail, sizeof(detail), "%c %c%u", subChar(t.attr, kbHandle, msHandle),
                     subReasonChar(t.reason), (unsigned)(t.notify & 1));
            break;
        case BLE_GAP_EVENT_REPEAT_PAIRING: name = "RPAI"; break;
        case BLE_GAP_EVENT_IDENTITY_RESOLVED: name = "IDRS"; break;
        case BLE_GAP_EVENT_MTU: name = "MTU"; snprintf(detail, sizeof(detail), "%ld", (long)t.code); break;
        case BLE_GAP_EVENT_CONN_UPDATE: name = "UPD"; snprintf(detail, sizeof(detail), t.code == 0 ? "ok" : "!%X", c); break;
        case BLE_GAP_EVENT_TERM_FAILURE: name = "TFAI"; snprintf(detail, sizeof(detail), "%X", c); break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: name = "PKEY"; snprintf(detail, sizeof(detail), "%ld", (long)t.code); break;
        case TR_APP_CONNECT: name = "aCON"; snprintf(detail, sizeof(detail), t.code ? "keep" : "new"); break;
        case TR_APP_ADOPT: name = "aADP"; break;
        case TR_APP_DISCONNECT:
            name = "aDIS";
            if ((uint16_t)t.code == 0xFFFF) snprintf(detail, sizeof(detail), "cur -");
            else snprintf(detail, sizeof(detail), "cur %u", (unsigned)(uint16_t)t.code);
            break;
        case TR_APP_SUBSCRIBE:
            name = "aSUB";
            snprintf(detail, sizeof(detail), "%c%u %s", subChar(t.attr, kbHandle, msHandle),
                     (unsigned)(t.notify & 1), t.reason ? "ok" : "FGN");
            break;
        case TR_LINK_READY: name = "RDY"; break;
        case TR_LINK_LOST: name = "LOST"; break;
        default: name = "?"; snprintf(detail, sizeof(detail), "%u", t.type); break;
    }
    char h[6];
    if (t.conn == 0xFFFF) snprintf(h, sizeof(h), "-");
    else snprintf(h, sizeof(h), "%u", t.conn);
    char sec[6];
    if (t.sec & 0x80) snprintf(sec, sizeof(sec), "E%uB%u", t.sec & 1, (t.sec >> 1) & 1);
    else snprintf(sec, sizeof(sec), "--");
    snprintf(out, n, "%3lu %4lu.%lu g%lu h%s %-4s %-7s %s", (unsigned long)(t.seq % 1000),
             (unsigned long)(t.ms / 1000), (unsigned long)((t.ms / 100) % 10), (unsigned long)t.gen,
             h, name, detail, sec);
}

// loop(): moves new records to Serial and to the status-page tokens.
static void drainTrace()
{
    uint32_t newest = traceNewest();
    if (newest - traceTail > TRACE_SIZE) {  // overwritten before they were logged
        traceLost += newest - traceTail - TRACE_SIZE;
        traceTail = newest - TRACE_SIZE;
    }
    uint16_t kb = kbValueHandle, ms = mouseValueHandle;
    while (traceTail != newest) {
        TraceRec t;
        uint32_t seq = traceTail + 1;
        traceTail = seq;
        if (!traceRead(seq, &t)) {
            traceLost++;
            continue;
        }
        char tok[12];
        formatTrace(t, kb, ms, tok, sizeof(tok));
        for (uint8_t i = 0; i + 1 < TRACE_TOKENS; i++) memcpy(traceTokens[i], traceTokens[i + 1], sizeof(traceTokens[i]));
        snprintf(traceTokens[TRACE_TOKENS - 1], sizeof(traceTokens[0]), "%s", tok);
        char line[48];
        formatTraceLine(t, kb, ms, line, sizeof(line));
        Serial.printf("[TRACE] %s | seq %lu type %u code 0x%lx attr %u reason %u notify %u sec 0x%02x\n",
                      line, (unsigned long)t.seq, t.type, (unsigned long)t.code, t.attr, t.reason,
                      t.notify, t.sec);
        dirty = true;
    }
    static uint32_t seenSnap = 0;
    uint32_t sc = encSnapCount;
    if (sc != seenSnap) {
        seenSnap = sc;
        const EncSnapshot e = encSnap;
        Serial.printf("[ENC] seq %lu gen %lu status 0x%lx found %u enc %u bonded %u peer id %u:%02X:%02X:%02X:%02X:%02X:%02X "
                      "ota %u:%02X:%02X:%02X:%02X:%02X:%02X cccd peer %d peer-kb %d any-kb %d bonds %d\n",
                      (unsigned long)e.seq, (unsigned long)e.gen, (unsigned long)e.status, e.found ? 1 : 0,
                      e.encrypted ? 1 : 0, e.bonded ? 1 : 0, e.idType, e.id[5], e.id[4], e.id[3], e.id[2],
                      e.id[1], e.id[0], e.otaType, e.ota[5], e.ota[4], e.ota[3], e.ota[2], e.ota[1], e.ota[0],
                      e.cccdPeer, e.cccdPeerKb, e.cccdAllKb, e.bonds);
        dirty = true;
    }
}

// ---------------------------------------------------------------- BLE setup
static void bleBegin()
{
    BLEDevice::init(BLE_DEVICE_NAME);
    // (B1) Subscribe events of the static table's input reports -> handleSubscribe().
    // Registered first, so the trace listener (registered next) runs before it.
    int subRc = ble_gap_event_listener_register(&subscribeListener, onSubscribeEvent, nullptr);
    Serial.printf("[GAP] subscribe listener register rc=%d\n", subRc);
    // (r2) Diagnostics: observe all GAP events (read-only listener).
    int rc = ble_gap_event_listener_register(&gapListener, onGapEvent, nullptr);
    Serial.printf("[GAP] listener register rc=%d\n", rc);

    // As Server_Gamepad: no IO capability ("Just Works"), bonding, no MITM, LE
    // Secure Connections. Bonds are stored by NimBLE (NVS namespace nimble_bond).
    BLESecurity* security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setAuthenticationMode(true, false, true);
    BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());
    bleServer->advertiseOnDisconnect(true);

    // (B1) No BLEHIDDevice and no library service: the static table (device information,
    // one HID service with keyboard input / output and mouse input, battery) is queued now and
    // registered by the single server start. Initial values: all keys up, mouse released,
    // Report Protocol, battery 100 (GattValue initializers).
    gattRegisterRc = registerGattTable();
    bleServer->start();  // once: ble_gatts_start() assigns all handles
    if (gattRegisterRc != 0) snprintf(lastEvent, sizeof(lastEvent), "GATT REG rc%d", gattRegisterRc);

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->setAppearance(0x03C1);  // HID Keyboard
    adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);  // as Server_Gamepad
    adv->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    kbValueHandle = kbInput->getHandle();
    mouseValueHandle = mouseInput->getHandle();
    Serial.printf("[BLE] advertising; gatt rc %d, server started %d; handles: protocol %u control %u "
                  "kb in %u kb out %u mouse in %u battery %u\n",
                  gattRegisterRc, bleServer->isStarted() ? 1 : 0, (unsigned)protocolModeValue.handle,
                  (unsigned)controlPointValue.handle, (unsigned)kbInput->getHandle(),
                  (unsigned)kbOutput->getHandle(), (unsigned)mouseInput->getHandle(),
                  (unsigned)batteryValue.handle);
}

// (2a) BLE on / off. The stack is initialized once, when BLE is first used (at boot if
// the saved transport is BLE); it is never de-initialized. Off = no advertising, the
// current link terminated (bonds are not touched).
static void bleStart()
{
    bleOn = true;
    if (!bleStarted) {
        bleBegin();  // BLE-1 r4: also starts advertising
        bleStarted = true;
        return;
    }
    bleServer->advertiseOnDisconnect(true);
    if (!bleConnected) BLEDevice::startAdvertising();
    Serial.println("[BLE] on");
}

static void bleStop()
{
    bleOn = false;
    if (!bleStarted) return;
    bleServer->advertiseOnDisconnect(false);
    BLEDevice::stopAdvertising();
    uint16_t h = bleConnHandle;
    if (bleConnected && h != 0xFFFF) {
        int rc = ble_gap_terminate(h, BLE_ERR_REM_USER_CONN_TERM);
        Serial.printf("[BLE] off: terminate handle %u rc=%d\n", h, rc);
    } else {
        Serial.println("[BLE] off");
    }
}

// ---------------------------------------------------------------- (2a) transport switch
// docs/STAGE_2A_DESIGN.md §4.3. Runs in loop(); never blocks. The internal state is
// all-up from the start; the old transport gets an all-up keyboard report and a
// released mouse report within SWITCH_RELEASE_MS (BLE: plus BLE_RELEASE_FLUSH_MS to
// transmit them before the link is terminated). Then the switch commits, released
// or not; an unconfirmed release is kept as owed and shown as a warning.
static bool transportReady(Transport t)
{
    return (t == TRANSPORT_USB) ? usbHostReady : linkReady;
}

static bool switchMouseNeeded(Transport t)
{
    return (t == TRANSPORT_USB) || mouseSubscribed;
}

static void startSwitch(Transport to, uint32_t now)
{
    switchFrom = activeTransport;
    switchTo = to;
    switchStartMs = now;
    switchReleasedMs = 0;
    switchKbOk = false;
    switchMsOk = false;
    switchReleaseNeeded = (switchFrom == TRANSPORT_USB) ? (bool)USB : bleConnected.load();
    switchState = SW_RELEASING;
    // Nothing pressed or queued is carried over (CONNECTION entry did the same).
    releaseHoldNow();
    queueClear();
    setMouseButton(0);
    mousePendingDx = 0;
    mouseHalfRem = 0;
    accum = 0;
    Serial.printf("[SWITCH] %s -> %s: releasing\n", TRANSPORT_NAME[switchFrom], TRANSPORT_NAME[to]);
    dirty = true;
    setupDirty = true;
}

static void commitSwitch(uint32_t now, bool released)
{
    Transport from = switchFrom, to = switchTo;
    if (!released) {
        releaseOwed[from] = true;
        owedKbOk[from] = false;
        owedMsOk[from] = false;
        switchWarnings++;
        snprintf(lastEvent, sizeof(lastEvent), "! %s RELEASE ?", TRANSPORT_NAME[from]);
    } else {
        releaseOwed[from] = false;
        snprintf(lastEvent, sizeof(lastEvent), "NOW %s", TRANSPORT_NAME[to]);
    }
    hidReady = false;
    onLinkDown();
    if (from == TRANSPORT_BLE) bleStop();
    activeTransport = to;
    switchState = SW_IDLE;
    linkSavePending = true;  // stored at once, in loop() (serviceLinkSave)
    switchCount++;
    if (to == TRANSPORT_BLE) {
        bleStart();
        if (linkReady) {  // a link that is still READY (e.g. a quick switch back)
            hidReady = true;
            onLinkUp(now, true);
        }
    }
    // USB: updateUsbState() makes it READY on the next loop when mounted.
    Serial.printf("[SWITCH] now %s, release %s (%lu ms)\n", TRANSPORT_NAME[to],
                  released ? "confirmed" : "NOT confirmed", (unsigned long)(now - switchStartMs));
    uiMode = UI_NORMAL;
    uiAccum = 0;
    dirty = true;
    spinnerFull = true;
}

static void serviceSwitch(uint32_t now)
{
    if (switchState != SW_RELEASING) return;
    Transport f = switchFrom;
    if (!switchReleaseNeeded) {  // no host link on the old transport: nothing to release
        commitSwitch(now, true);
        return;
    }
    bool needMouse = switchMouseNeeded(f);
    bool released = switchKbOk && (switchMsOk || !needMouse);
    if (!released && transportReady(f)) {
        hid_keyboard_report_t k = {};  // all keys up
        hid_mouse_report_t m = {};     // button released, no movement
        if (!switchKbOk) {
            TxResult t = (f == TRANSPORT_USB) ? usbSendKeyboard(k) : bleSendKeyboard(k);
            switchKbOk = (t == TX_SENT);
        }
        if (needMouse && !switchMsOk) {
            TxResult t = (f == TRANSPORT_USB) ? usbSendMouse(m) : bleSendMouse(m);
            switchMsOk = (t == TX_SENT);
        }
        released = switchKbOk && (switchMsOk || !needMouse);
        if (released) switchReleasedMs = now;
    }
    uint32_t elapsed = now - switchStartMs;
    bool flushed = released && (f != TRANSPORT_BLE || now - switchReleasedMs >= BLE_RELEASE_FLUSH_MS);
    if (flushed) {
        commitSwitch(now, true);
    } else if (!released && elapsed >= SWITCH_RELEASE_MS) {
        commitSwitch(now, false);
    } else if (elapsed >= SWITCH_RELEASE_MS + BLE_RELEASE_FLUSH_MS) {
        commitSwitch(now, released);  // safety bound; a release accepted by BLE counts as sent
    }
}

// BLE mode: the only USB traffic is a release owed from a switch (design §4.3 d).
static void serviceOwedUsbRelease()
{
    if (activeTransport != TRANSPORT_BLE || switchState != SW_IDLE) return;
    if (!releaseOwed[TRANSPORT_USB] || !usbHostReady) return;
    if (!owedKbOk[TRANSPORT_USB]) {
        hid_keyboard_report_t k = {};
        if (usbSendKeyboard(k) == TX_SENT) noteReleaseSent(TRANSPORT_USB, true, false);
    }
    if (releaseOwed[TRANSPORT_USB] && !owedMsOk[TRANSPORT_USB]) {
        hid_mouse_report_t m = {};
        if (usbSendMouse(m) == TX_SENT) noteReleaseSent(TRANSPORT_USB, false, true);
    }
}

// ---------------------------------------------------------------- (1e-2) Custom: keys and names
// Typed-key capture uses keysState().word. With Shift / Ctrl / CapsLock active the
// library puts the second legend there (Keyboard.cpp:202-207), e.g. '!' for '1'.
// baseChar() maps it back with the library's own key table (_key_value_map in
// Keyboard.h), so a captured key is always the unshifted key; Shift is added only
// with the explicit Fn+S toggle. Note: the special keys (Tab, Backspace, Enter) are
// never in word and are read from the keysState() flags instead, because the
// KEY_TAB / KEY_BACKSPACE macros differ between M5Cardputer and USBHIDKeyboard.h.
static char baseChar(char c)
{
    for (int r = 0; r < 4; r++) {
        for (int k = 0; k < 14; k++) {
            const KeyValue_t& kv = _key_value_map[r][k];
            if ((uint8_t)kv.value_second == (uint8_t)c && kv.value_first >= 0x20
                && kv.value_first < 0x7F) {
                return (char)kv.value_first;
            }
        }
    }
    return c;
}

// HID usage of an unshifted Cardputer character; 0 if it has none.
static uint8_t usageForChar(char c)
{
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x04 + (c - 'a'));
    if (c >= '1' && c <= '9') return (uint8_t)(0x1E + (c - '1'));
    switch (c) {
        case '0':  return 0x27;
        case ' ':  return 0x2C;
        case '-':  return 0x2D;
        case '=':  return 0x2E;
        case '[':  return 0x2F;
        case ']':  return 0x30;
        case '\\': return 0x31;
        case ';':  return 0x33;
        case '\'': return 0x34;
        case '`':  return 0x35;
        case ',':  return 0x36;
        case '.':  return 0x37;
        case '/':  return 0x38;
        default:   return 0;
    }
}
static_assert(HID_KEY_A == 0x04 && HID_KEY_Z == 0x1D && HID_KEY_1 == 0x1E && HID_KEY_0 == 0x27
              && HID_KEY_MINUS == 0x2D && HID_KEY_EQUAL == 0x2E && HID_KEY_BRACKET_LEFT == 0x2F
              && HID_KEY_BRACKET_RIGHT == 0x30 && HID_KEY_BACKSLASH == 0x31
              && HID_KEY_SEMICOLON == 0x33 && HID_KEY_APOSTROPHE == 0x34 && HID_KEY_GRAVE == 0x35
              && HID_KEY_COMMA == 0x36 && HID_KEY_PERIOD == 0x37 && HID_KEY_SLASH == 0x38,
              "usageForChar() values as in TinyUSB hid.h");

// Display name of a key usage. Names describe the key, not a character: what a
// letter or symbol key types depends on the host's keyboard layout.
static void keyName(uint8_t usage, char* buf, size_t n)
{
    static constexpr PickKey SYMBOLS[] = {
        {0x2D, "-"}, {0x2E, "="}, {0x2F, "["}, {0x30, "]"}, {0x31, "\\"},
        {0x33, ";"}, {0x34, "'"}, {0x36, ","}, {0x37, "."}, {0x38, "/"},
    };
    if (usage >= 0x04 && usage <= 0x1D) { snprintf(buf, n, "%c", 'A' + (usage - 0x04)); return; }
    if (usage >= 0x1E && usage <= 0x26) { snprintf(buf, n, "%c", '1' + (usage - 0x1E)); return; }
    if (usage == 0x27) { snprintf(buf, n, "0"); return; }
    for (const PickKey& s : SYMBOLS) {
        if (s.usage == usage) { snprintf(buf, n, "%s", s.name); return; }
    }
    for (const PickKey& p : PICKER) {
        if (p.usage == usage) { snprintf(buf, n, "%s", p.name); return; }
    }
    snprintf(buf, n, "0x%02X", usage);
}

// "Ctl+Sft+Alt+Cmd+Right", or "(none)" for an unassigned action.
static void formatKey(const CustomKey& k, char* buf, size_t n)
{
    if (k.usage == 0 && k.modifiers == 0) { snprintf(buf, n, "(none)"); return; }
    if (k.usage == 0) {
        // (1e-3) Modifier-only action, e.g. "Sft" or "Ctl+Cmd" (no trailing '+').
        snprintf(buf, n, "%s%s%s%s",
                 (k.modifiers & MOD_CTRL) ? "Ctl+" : "", (k.modifiers & MOD_SHIFT) ? "Sft+" : "",
                 (k.modifiers & MOD_ALT) ? "Alt+" : "", (k.modifiers & MOD_GUI) ? "Cmd+" : "");
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '+') buf[len - 1] = '\0';
        return;
    }
    char name[12];
    keyName(k.usage, name, sizeof(name));
    snprintf(buf, n, "%s%s%s%s%s",
             (k.modifiers & MOD_CTRL) ? "Ctl+" : "", (k.modifiers & MOD_SHIFT) ? "Sft+" : "",
             (k.modifiers & MOD_ALT) ? "Alt+" : "", (k.modifiers & MOD_GUI) ? "Cmd+" : "", name);
}

// Rebuild the send-path form of the saved assignments (called only by Save).
static void rebuildCustomActions()
{
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        formatKey(customSaved[i], customLabel[i], sizeof(customLabel[i]));
        customAction[i] = KeyAction{customSaved[i].modifiers, customSaved[i].usage, customLabel[i]};
    }
}

// (1e-3) Assigned = a key or at least one modifier (modifier-only actions are valid).
static bool customAssigned(const CustomKey& k)
{
    return k.usage != 0 || k.modifiers != 0;
}

static bool customAnyAssigned()
{
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        if (customAssigned(customSaved[i])) return true;
    }
    return false;
}

// Profile 4 in normal operation: queue a tap only for an assigned action.
static void pushCustom(Event ev)
{
    if (customAssigned(customSaved[ev])) {
        pushEvent(ev);
    } else {
        customIgnored++;
    }
}

// ---------------------------------------------------------------- (1e-4) settings record
// CRC-32 (IEEE 802.3, reflected 0xEDB88320; "123456789" -> 0xCBF43926).
static uint32_t cfgCrc32(const uint8_t* p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// Usages the setup UI can produce: typed keys (usageForChar(), Enter / Backspace /
// Tab flags) and the picker list. Anything else in a record is rejected.
static bool usageValid(uint8_t u)
{
    return u == 0 || (u >= 0x04 && u <= 0x31) || (u >= 0x33 && u <= 0x38)
        || (u >= 0x3A && u <= 0x45) || (u >= 0x49 && u <= 0x52);
}

// Every field is checked; also used before a write, so an invalid record is never stored.
static bool settingsValid(const Settings& s)
{
    if (s.profile < PROFILE_YOUTUBE || s.profile > PROFILE_CUSTOM) return false;
    if (s.sensIndex >= SENS_COUNT) return false;
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        const CustomKey& k = s.custom[i];
        if (k.modifiers & ~MOD_ALL) return false;
        if (!usageValid(k.usage)) return false;
        uint8_t allowed = (i == SLOT_BTN) ? CK_HOLD : 0;  // CW / CCW are TAP only
        if (k.flags & ~allowed) return false;
    }
    return true;
}

static Settings defaultSettings()
{
    Settings s = {};
    s.profile = PROFILE_YOUTUBE;
    s.sensIndex = SENS_DEFAULT_INDEX;
    return s;  // Custom: nothing assigned, TAP
}

static void encodeSettings(const Settings& s, uint8_t* b)
{
    memcpy(b, CFG_MAGIC, 4);
    b[4] = CFG_VERSION;
    b[5] = (uint8_t)CFG_SIZE;
    b[6] = s.profile;
    b[7] = s.sensIndex;
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        b[8 + i * 3] = s.custom[i].modifiers;
        b[9 + i * 3] = s.custom[i].usage;
        b[10 + i * 3] = s.custom[i].flags;
    }
    uint32_t crc = cfgCrc32(b, CFG_BODY);
    for (int i = 0; i < 4; i++) b[CFG_BODY + i] = (uint8_t)(crc >> (8 * i));
}

// Writes out only if every check passes: a record is applied whole or not at all.
static CfgResult decodeSettings(const uint8_t* b, size_t n, Settings& out)
{
    if (n < 6) return CFG_BAD_LENGTH;
    if (memcmp(b, CFG_MAGIC, 4) != 0) return CFG_BAD_MAGIC;
    if (b[4] > CFG_VERSION) return CFG_NEWER_VERSION;
    if (b[4] != CFG_VERSION) return CFG_BAD_VERSION;  // no older version was ever written
    if (b[5] != CFG_SIZE || n != CFG_SIZE) return CFG_BAD_LENGTH;
    uint32_t crc = 0;
    for (int i = 0; i < 4; i++) crc |= (uint32_t)b[CFG_BODY + i] << (8 * i);
    if (crc != cfgCrc32(b, CFG_BODY)) return CFG_BAD_CRC;
    Settings t = {};
    t.profile = b[6];
    t.sensIndex = b[7];
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        t.custom[i].modifiers = b[8 + i * 3];
        t.custom[i].usage = b[9 + i * 3];
        t.custom[i].flags = b[10 + i * 3];
    }
    if (!settingsValid(t)) return CFG_BAD_FIELD;
    out = t;
    return CFG_OK;
}

static bool settingsEqual(const Settings& a, const Settings& b)
{
    uint8_t ea[CFG_SIZE], eb[CFG_SIZE];
    encodeSettings(a, ea);
    encodeSettings(b, eb);
    return memcmp(ea, eb, CFG_SIZE) == 0;
}

// ---------------------------------------------------------------- (1e-4) NVS access
// The only two functions that touch NVS. Only nvs_open / get / set / commit /
// get_stats / close on this app's own namespace: no erase call of any kind.
// Read: ESP_OK (len = stored size; not read if larger than cap),
// ESP_ERR_NVS_NOT_FOUND (no namespace or no record) or another error.
// (2a) Keyed form, used for "cfg" (via nvBackendRead) and "link".
static esp_err_t nvBackendReadKey(const char* key, uint8_t* buf, size_t cap, size_t* len)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NV_NAMESPACE, NVS_READONLY, &h);  // read-only: never creates anything
    if (e != ESP_OK) return e;
    size_t n = 0;
    e = nvs_get_blob(h, key, NULL, &n);
    if (e == ESP_OK && n <= cap) e = nvs_get_blob(h, key, buf, &n);
    *len = n;
    nvs_close(h);
    return e;
}

// Write and read back. ESP_ERR_NVS_NOT_ENOUGH_SPACE also when fewer than
// NV_MIN_AVAILABLE_ENTRIES entries are available (then nothing is written).
static esp_err_t nvBackendWriteKey(const char* key, const uint8_t* buf, size_t len)
{
    nvs_stats_t st;
    esp_err_t e = nvs_get_stats(NULL, &st);
    if (e != ESP_OK) return e;
    if (st.available_entries < NV_MIN_AVAILABLE_ENTRIES) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    nvs_handle_t h;
    e = nvs_open(NV_NAMESPACE, NVS_READWRITE, &h);
    if (e != ESP_OK) return e;
    e = nvs_set_blob(h, key, buf, len);
    if (e == ESP_OK) e = nvs_commit(h);
    if (e == ESP_OK) {
        uint8_t chk[CFG_MAX_READ];
        size_t n = sizeof(chk);
        e = nvs_get_blob(h, key, chk, &n);
        if (e == ESP_OK && (n != len || memcmp(chk, buf, len) != 0)) e = ESP_FAIL;
    }
    nvs_close(h);
    return e;
}

// (1e-4) The settings record; unchanged behaviour.
static esp_err_t nvBackendRead(uint8_t* buf, size_t cap, size_t* len)
{
    return nvBackendReadKey(NV_KEY, buf, cap, len);
}

static esp_err_t nvBackendWrite(const uint8_t* buf, size_t len)
{
    return nvBackendWriteKey(NV_KEY, buf, len);
}

// ---------------------------------------------------------------- (1e-4) load / save
static void nvChanged()
{
    dirty = true;
    sensDirty = true;   // the spinner screen shows the status beside the sensitivity
    setupDirty = true;
}

static void applySettings(const Settings& s)
{
    profile = s.profile;
    sensIndex = s.sensIndex;
    memcpy(customSaved, s.custom, sizeof(customSaved));
    rebuildCustomActions();
}

// Boot only. Reads, never writes. All fields come from one fully valid record,
// or all are defaults.
static void loadSettings()
{
    Settings def = defaultSettings();
    Settings s = def;
    uint8_t buf[CFG_MAX_READ];
    size_t len = 0;
    esp_err_t e = nvBackendRead(buf, sizeof(buf), &len);
    CfgResult r;
    if (e == ESP_ERR_NVS_NOT_FOUND) r = CFG_NONE;
    else if (e != ESP_OK) r = CFG_READ_ERROR;
    else if (len > sizeof(buf)) r = CFG_BAD_LENGTH;
    else r = decodeSettings(buf, len, s);
    cfgLoadResult = r;

    persisted = def;
    persistedInNvs = false;
    switch (r) {
        case CFG_OK:            persisted = s; persistedInNvs = true; nvState = NV_OK; break;
        case CFG_NONE:          nvState = NV_NEW; break;
        case CFG_NEWER_VERSION: nvState = NV_VER; nvWritable = false; break;  // keep it for the newer firmware
        case CFG_READ_ERROR:    nvState = NV_ERR; nvWritable = false; break;  // do not write what cannot be read
        default:                nvState = NV_BAD; break;  // replaced only by a later user save
    }
    applySettings(r == CFG_OK ? s : def);

    static const char* const TEXT[] = {
        "CFG RESTORED", "NO SAVED CFG", "NVS READ ERROR", "CFG BAD LENGTH", "CFG BAD MAGIC",
        "CFG NEWER VERSION", "CFG BAD VERSION", "CFG BAD CRC", "CFG BAD VALUE"
    };
    snprintf(lastEvent, sizeof(lastEvent), "%s", TEXT[r]);
    Serial.printf("[NV] load: %s (err 0x%x, %u bytes)\n", TEXT[r], (unsigned)e, (unsigned)len);
}

// Stores next unless it equals what NVS already holds. persisted changes only
// after a confirmed write (ESP_OK from set, commit and the read-back).
static bool writeSettings(const Settings& next)
{
    if (!nvWritable) return false;               // status stays NV VER / NV ERR
    if (!settingsValid(next)) {                  // never store an invalid record
        nvWriteFails++;
        nvState = NV_WFAIL;
        return false;
    }
    if (persistedInNvs && settingsEqual(next, persisted)) {
        nvSkips++;
        return true;
    }
    uint8_t buf[CFG_SIZE];
    encodeSettings(next, buf);
    uint32_t t0 = millis();
    esp_err_t e = nvBackendWrite(buf, CFG_SIZE);
    uint32_t dt = millis() - t0;
    if (dt > nvMaxWriteMs) nvMaxWriteMs = dt;
    Serial.printf("[NV] write p%u s%u: err 0x%x, %lu ms\n", next.profile, next.sensIndex,
                  (unsigned)e, (unsigned long)dt);
    if (e == ESP_OK) {
        persisted = next;
        persistedInNvs = true;
        nvWrites++;
        nvState = NV_OK;
        return true;
    }
    nvWriteFails++;
    nvState = (e == ESP_ERR_NVS_NOT_ENOUGH_SPACE) ? NV_FULL : NV_WFAIL;
    return false;  // persisted unchanged; not retried automatically
}

// Called from loop() after the HID sends. Each trigger updates only its own
// fields of the stored record (policy §6.9); triggers due together share one write.
static void serviceSettings(uint32_t now)
{
    bool sensDue = sensSavePending && (now - sensChangedMs >= SENS_SAVE_DELAY_MS);
    if (!profileSavePending && !customSavePending && !sensDue) return;
    Settings next = persisted;
    if (profileSavePending) next.profile = profile;
    if (customSavePending) memcpy(next.custom, customSaved, sizeof(next.custom));
    if (sensDue) next.sensIndex = sensIndex;
    bool custom = customSavePending;
    profileSavePending = false;
    customSavePending = false;
    if (sensDue) sensSavePending = false;
    bool ok = writeSettings(next);
    if (custom) {
        snprintf(lastEvent, sizeof(lastEvent), "%s",
                 ok ? "SAVED" : (nvWritable ? "SAVE NOT STORED" : "SAVED (RAM ONLY)"));
    }
    nvChanged();
}

static const char* nvStateText(uint16_t* color)
{
    if (sensSavePending && nvWritable) { *color = TFT_YELLOW; return "NV WAIT"; }
    switch (nvState) {
        case NV_OK:   *color = TFT_DARKGREEN; return "NV OK";
        case NV_NEW:  *color = TFT_DARKGREY;  return "NV NEW";
        case NV_BAD:  *color = TFT_ORANGE;    return "NV BAD";
        case NV_VER:  *color = TFT_ORANGE;    return "NV VER";
        case NV_ERR:  *color = TFT_RED;       return "NV ERR";
        case NV_FULL: *color = TFT_RED;       return "NV FULL";
        default:      *color = TFT_RED;       return "NV WFAIL";
    }
}

// ---------------------------------------------------------------- (2a) saved transport
// Record "link", version 1, 11 bytes: 0..3 "MJLK", 4 version, 5 length (11),
// 6 transport (0 = USB, 1 = BLE), 7..10 CRC-32 (as cfg) of bytes 0..6.
static void encodeLink(uint8_t mode, uint8_t* b)
{
    memcpy(b, LINK_MAGIC, 4);
    b[4] = LINK_VERSION;
    b[5] = (uint8_t)LINK_SIZE;
    b[6] = mode;
    uint32_t crc = cfgCrc32(b, LINK_BODY);
    for (int i = 0; i < 4; i++) b[LINK_BODY + i] = (uint8_t)(crc >> (8 * i));
}

// Same checks and order as decodeSettings(); *mode is written only if all pass.
static CfgResult decodeLink(const uint8_t* b, size_t n, uint8_t* mode)
{
    if (n < 6) return CFG_BAD_LENGTH;
    if (memcmp(b, LINK_MAGIC, 4) != 0) return CFG_BAD_MAGIC;
    if (b[4] > LINK_VERSION) return CFG_NEWER_VERSION;
    if (b[4] != LINK_VERSION) return CFG_BAD_VERSION;
    if (b[5] != LINK_SIZE || n != LINK_SIZE) return CFG_BAD_LENGTH;
    uint32_t crc = 0;
    for (int i = 0; i < 4; i++) crc |= (uint32_t)b[LINK_BODY + i] << (8 * i);
    if (crc != cfgCrc32(b, LINK_BODY)) return CFG_BAD_CRC;
    if (b[6] > TRANSPORT_BLE) return CFG_BAD_FIELD;
    *mode = b[6];
    return CFG_OK;
}

// Boot only; reads, never writes. Anything but a valid record means USB (design D2).
// A read error or a newer record makes the link record read-only for this session.
static void loadLink()
{
    uint8_t buf[CFG_MAX_READ];
    size_t len = 0;
    uint8_t mode = TRANSPORT_USB;
    esp_err_t e = nvBackendReadKey(NV_LINK_KEY, buf, sizeof(buf), &len);
    CfgResult r;
    if (e == ESP_ERR_NVS_NOT_FOUND) r = CFG_NONE;
    else if (e != ESP_OK) r = CFG_READ_ERROR;
    else if (len > sizeof(buf)) r = CFG_BAD_LENGTH;
    else r = decodeLink(buf, len, &mode);
    linkLoadResult = r;
    linkStored = (r == CFG_OK);
    linkStoredMode = linkStored ? mode : (uint8_t)TRANSPORT_USB;
    if (r == CFG_READ_ERROR || r == CFG_NEWER_VERSION) linkWritable = false;
    activeTransport = (Transport)linkStoredMode;
    Serial.printf("[NV] link: result %u, transport %s\n", (unsigned)r, TRANSPORT_NAME[activeTransport]);
}

// After a confirmed switch (loop, after the HID sends): store the transport unless NVS
// already holds it. Same guards as the settings record (no erase, space check, read-back).
static void serviceLinkSave()
{
    if (!linkSavePending) return;
    linkSavePending = false;
    uint8_t mode = (uint8_t)activeTransport;
    if (!nvWritable || !linkWritable) return;  // status stays NV VER / NV ERR
    if (linkStored && linkStoredMode == mode) return;
    uint8_t buf[LINK_SIZE];
    encodeLink(mode, buf);
    esp_err_t e = nvBackendWriteKey(NV_LINK_KEY, buf, LINK_SIZE);
    Serial.printf("[NV] link write %s: err 0x%x\n", TRANSPORT_NAME[mode], (unsigned)e);
    if (e == ESP_OK) {
        linkStored = true;
        linkStoredMode = mode;
        linkWrites++;
        nvState = NV_OK;
    } else {
        linkWriteFails++;
        nvState = (e == ESP_ERR_NVS_NOT_ENOUGH_SPACE) ? NV_FULL : NV_WFAIL;
    }
    nvChanged();
}

// ---------------------------------------------------------------- (1e-2) Custom: setup UI
static void markDraft()
{
    draftChanged = (memcmp(customDraft, customSaved, sizeof(customDraft)) != 0);
    setupDirty = true;
}

// Remember the current keys so that keys already held when a screen opens
// (e.g. Fn+S) do not count as new presses.
static void snapshotSetupKeys(const Keyboard_Class::KeysState& ks)
{
    prevEnter = ks.enter;
    prevDel = ks.del;
    prevTab = ks.tab;
    prevCharCount = 0;
    for (char c : ks.word) {
        if (prevCharCount < sizeof(prevChars)) prevChars[prevCharCount++] = baseChar(c);
    }
}

// Opens CUSTOM SETUP. Before the UI takes over, everything that could be held on
// the host is released, with the same clean-up as a profile switch (setProfile()):
// queued taps and pending movement dropped, mouse button up and resent, and an
// all-keys-up keyboard report requested.
static void enterSetup(const Keyboard_Class::KeysState& ks)
{
    releaseHoldNow();  // (1e-3) a held Custom key is released before the UI takes over
    queueClear();
    accum = 0;
    mousePendingDx = 0;
    mouseHalfRem = 0;
    setMouseButton(0);
    mouseSyncNeeded = true;
    releasePending = true;

    memcpy(customDraft, customSaved, sizeof(customDraft));
    draftChanged = false;
    setupSel = SLOT_CW;
    pickerIndex = -1;
    uiAccum = 0;
    uiMode = UI_SETUP;
    snapshotSetupKeys(ks);
    setupDirty = true;
    snprintf(lastEvent, sizeof(lastEvent), "SETUP");
    Serial.println("[SETUP] open");
}

// Save copies the draft to what Profile 4 sends; Cancel discards the draft.
static void exitSetup(bool save)
{
    if (save) {
        memcpy(customSaved, customDraft, sizeof(customSaved));
        rebuildCustomActions();
        customSavePending = true;  // (1e-4) the only path that stores Custom; Cancel writes nothing
        snprintf(lastEvent, sizeof(lastEvent), "SAVING");
    } else {
        snprintf(lastEvent, sizeof(lastEvent), "SETUP CANCELLED");
    }
    Serial.printf("[SETUP] %s\n", save ? "saved (stored in loop)" : "cancelled (nothing stored)");
    uiMode = UI_NORMAL;
    uiAccum = 0;
    accum = 0;
    queueClear();
    dirty = true;
}

static void startEdit(uint8_t slot)
{
    setupSel = slot;
    editUndo = customDraft[slot];
    pickerIndex = -1;
    uiAccum = 0;
    uiMode = UI_EDIT;
    setupDirty = true;
}

static void finishEdit()
{
    uiMode = UI_SETUP;
    uiAccum = 0;
    setupDirty = true;
}

// One knob detent in the setup UI: move the selection, or step through PICKER.
static void setupStep(int8_t dir)
{
    if (uiMode == UI_SETUP) {
        setupSel = (uint8_t)((setupSel + SLOT_COUNT + dir) % SLOT_COUNT);
        setupDirty = true;
        return;
    }
    int idx = pickerIndex;
    if (idx < 0) {
        idx = -1;
        for (uint8_t i = 0; i < PICKER_COUNT; i++) {
            if (PICKER[i].usage == customDraft[setupSel].usage) { idx = i; break; }
        }
        idx = (idx < 0) ? (dir > 0 ? 0 : PICKER_COUNT - 1) : idx + dir;
    } else {
        idx += dir;
    }
    idx = (idx + PICKER_COUNT) % PICKER_COUNT;
    pickerIndex = (int8_t)idx;
    customDraft[setupSel].usage = PICKER[idx].usage;  // modifiers are kept
    markDraft();
}

static void setupRotate(int16_t delta)
{
    uiAccum += delta;
    while (uiAccum >= COUNTS_PER_DETENT) {
        uiAccum -= COUNTS_PER_DETENT;
        setupStep(+1);
    }
    while (uiAccum <= -COUNTS_PER_DETENT) {
        uiAccum += COUNTS_PER_DETENT;
        setupStep(-1);
    }
}

// One newly pressed Cardputer key while the setup UI is open. Nothing here ever
// reaches the USB host.
static void handleSetupKey(uint8_t type, char c, bool fn)
{
    if (uiMode == UI_SETUP) {
        if (type == SK_ENTER) { exitSetup(true); return; }                     // ok = SAVE
        if (type == SK_CHAR && !fn) {
            if (c == '`') { exitSetup(false); return; }                        // esc = CANCEL
            if (c >= '1' && c <= '3') { startEdit((uint8_t)(c - '1')); return; }  // EDIT
        }
        return;  // anything else, including Fn+1..Fn+4, is ignored here
    }

    // UI_EDIT
    CustomKey& k = customDraft[setupSel];
    switch (type) {
        case SK_ENTER:
            finishEdit();  // ok = done with this action (still unsaved)
            return;
        case SK_DEL:
            k.usage = 0;   // del = clear this action
            k.modifiers = 0;
            k.flags = 0;   // (1e-3) and back to TAP
            pickerIndex = -1;
            markDraft();
            return;
        case SK_TAB:
            k.usage = HID_KEY_TAB;
            pickerIndex = -1;
            markDraft();
            return;
        default:
            break;
    }
    if (fn) {
        // Modifier toggles
        if (c == 'c') k.modifiers ^= MOD_CTRL;
        else if (c == 's') k.modifiers ^= MOD_SHIFT;
        else if (c == 'a') k.modifiers ^= MOD_ALT;
        else if (c == 'g') k.modifiers ^= MOD_GUI;
        else if (c == 'h' && setupSel == SLOT_BTN) k.flags ^= CK_HOLD;  // (1e-3) TAP <-> HOLD, BTN only
        else return;
        markDraft();
        return;
    }
    if (c == '`') {  // esc = undo the changes to this action
        k = editUndo;
        markDraft();
        finishEdit();
        return;
    }
    uint8_t u = usageForChar(c);
    if (u != 0) {
        k.usage = u;  // modifiers are kept
        pickerIndex = -1;
        markDraft();
    }
}

static bool hasChar(const char* set, uint8_t n, char c)
{
    for (uint8_t i = 0; i < n; i++) {
        if (set[i] == c) return true;
    }
    return false;
}

// Key-press edges from keysState(): Enter / Backspace / Tab flags going true, and
// base characters that were not present in the previous loop.
static void processSetupKeys(const Keyboard_Class::KeysState& ks)
{
    char cur[sizeof(prevChars)];
    uint8_t n = 0;
    for (char c : ks.word) {
        char b = baseChar(c);
        if (n < sizeof(cur) && !hasChar(cur, n, b)) cur[n++] = b;
    }
    bool enterNow = ks.enter && !prevEnter;
    bool delNow = ks.del && !prevDel;
    bool tabNow = ks.tab && !prevTab;
    bool fn = ks.fn;

    prevEnter = ks.enter;
    prevDel = ks.del;
    prevTab = ks.tab;
    char old[sizeof(prevChars)];
    uint8_t oldCount = prevCharCount;
    memcpy(old, prevChars, sizeof(old));
    memcpy(prevChars, cur, sizeof(prevChars));
    prevCharCount = n;

    if (enterNow) handleUiKey(SK_ENTER, 0, fn);  // (2a) setup or connection screen
    if (uiMode != UI_NORMAL && delNow) handleUiKey(SK_DEL, 0, fn);
    if (uiMode != UI_NORMAL && tabNow) handleUiKey(SK_TAB, 0, fn);
    for (uint8_t i = 0; i < n && uiMode != UI_NORMAL; i++) {
        if (!hasChar(old, oldCount, cur[i])) handleUiKey(SK_CHAR, cur[i], fn);
    }
}

// ---------------------------------------------------------------- (2a) CONNECTION screen
// Fn+B in any profile. Like CUSTOM SETUP it releases everything first and sends
// nothing while open. 1 = USB, 2 = BLE (or turn the knob), ok / knob press = apply,
// esc = close, t = BLE trace pages.
static void enterConn(const Keyboard_Class::KeysState& ks)
{
    releaseHoldNow();
    queueClear();
    accum = 0;
    mousePendingDx = 0;
    mouseHalfRem = 0;
    setMouseButton(0);
    mouseSyncNeeded = true;
    releasePending = true;
    connSel = activeTransport;
    uiAccum = 0;
    uiMode = UI_CONN;
    snapshotSetupKeys(ks);
    setupDirty = true;
    snprintf(lastEvent, sizeof(lastEvent), "CONNECTION");
    Serial.println("[CONN] open");
}

static void closeConn()
{
    uiMode = UI_NORMAL;
    uiAccum = 0;
    accum = 0;
    queueClear();
    dirty = true;
    spinnerFull = true;
}

static void applyConn()
{
    if (switchState != SW_IDLE) return;
    if (connSel == activeTransport) {
        closeConn();
        return;
    }
    startSwitch(connSel, millis());
}

static void connRotate(int16_t delta)
{
    if (switchState != SW_IDLE || uiMode != UI_CONN) return;
    uiAccum += delta;
    if (uiAccum >= COUNTS_PER_DETENT || uiAccum <= -COUNTS_PER_DETENT) {
        uiAccum = 0;
        connSel = (connSel == TRANSPORT_USB) ? TRANSPORT_BLE : TRANSPORT_USB;
        setupDirty = true;
    }
}

static void handleConnKey(uint8_t type, char c, bool fn)
{
    (void)fn;
    if (switchState != SW_IDLE) return;  // a switch is committing
    if (uiMode == UI_TRACE) {
        if (type == SK_CHAR && (c == 't' || c == 'T')) {
            if (tracePage < tracePages()) tracePage++;
            else uiMode = UI_CONN;
        } else if (type == SK_ENTER || (type == SK_CHAR && c == '`')) {
            uiMode = UI_CONN;
        }
        pageDirty = true;
        setupDirty = true;
        return;
    }
    if (type == SK_ENTER) {
        applyConn();
        return;
    }
    if (type != SK_CHAR) return;
    if (c == '1') connSel = TRANSPORT_USB;
    else if (c == '2') connSel = TRANSPORT_BLE;
    else if (c == '`') {
        closeConn();
        return;
    } else if (c == 't' || c == 'T') {
        freezeTrace();
        tracePage = 1;
        uiMode = UI_TRACE;
        pageDirty = true;
    } else {
        return;
    }
    setupDirty = true;
}

// Key events of the setup and connection screens go to their own handler.
static void handleUiKey(uint8_t type, char c, bool fn)
{
    if (uiMode == UI_CONN || uiMode == UI_TRACE) handleConnKey(type, c, fn);
    else handleSetupKey(type, c, fn);
}

// ---------------------------------------------------------------- (1e-2 r2) build tag
// Small grey tag in the top-right corner (x 204-240). The longest size-2 first line
// ("USB NOT MOUNTED", "CUSTOM SETUP") ends before x = 190, so nothing overlaps.
static void drawBuildTag()
{
    auto& d = M5Cardputer.Display;
    d.setTextSize(1);
    // (2a) "[!]<USB|BLE> 2a", right-aligned by measured width (BLE-1 r3 method). The mode
    // is the active transport; a red "!" means a release on a transport is unconfirmed.
    // (2a-2) "[!]<USB|BLE> 2a2": at most 8 characters (48 px, from x 191): the first
    // size-2 line ends before x 190.
    char tail[8];
    snprintf(tail, sizeof(tail), " %s", SCREEN_TAG);
    const char* mode = TRANSPORT_NAME[activeTransport];
    bool warn = releaseOwed[TRANSPORT_USB] || releaseOwed[TRANSPORT_BLE];
    int w = d.textWidth(mode) + d.textWidth(tail) + (warn ? d.textWidth("!") : 0);
    d.setCursor(d.width() - w - 1, 0);
    if (warn) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.print("!");
    }
    d.setTextColor(activeTransport == TRANSPORT_BLE ? TFT_CYAN : TFT_GREEN, TFT_BLACK);
    d.print(mode);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.print(tail);
    // (1e-4) NVS status under the tag, x 192-240, y 10-17 (first size-2 line ends
    // before x 190; the setup rows start at y 21).
    uint16_t c;
    const char* t = nvStateText(&c);
    d.setTextColor(c, TFT_BLACK);
    d.setCursor(192, 10);
    d.print(t);
}

// ---------------------------------------------------------------- (1e-2) Custom: screens
// Profile 4 status body (below the USB and PROFILE lines of drawStatus()).
static void drawCustomStatusBody()
{
    auto& d = M5Cardputer.Display;
    static const char* const NAMES[SLOT_COUNT] = {"CW ", "CCW", "BTN"};
    char buf[28];
    int y = d.getCursorY() + 2;
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        formatKey(customSaved[i], buf, sizeof(buf));
        d.setCursor(4, y + i * 10);
        // (1e-3) "BTN: HOLD Sft" when BTN is HOLD (max 35 chars = 210 px at size 1)
        bool hold = (i == SLOT_BTN) && customAssigned(customSaved[i])
                    && (customSaved[i].flags & CK_HOLD);
        d.printf("%s: %s%s", NAMES[i], hold ? "HOLD " : "", buf);
    }
    y += 34;
    d.setTextSize(2);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(4, y);
    d.print(customAnyAssigned() ? "Fn+S: SETUP" : "NOT CONFIGURED");
    d.setCursor(4, y + 18);
    d.print(customAnyAssigned() ? lastEvent : "Fn+S: SETUP");
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, y + 38);
    d.printf("FAIL%lu SKIP%lu ERR%lu ign%lu%s", (unsigned long)sendFails,
             (unsigned long)skipped, (unsigned long)readErrors, (unsigned long)customIgnored,
             releasePending ? " REL!" : "");
    d.setCursor(4, y + 48);
    // (1e-2 r2) Live key state: holding Fn+S should read "key:Fn+s".
    d.printf("W%lu KB%u key:%s%c nv w%lu f%lu", (unsigned long)maxWorkMs, kbPressed,
             kbFn ? "Fn+" : "", kbLastChar ? kbLastChar : '-', (unsigned long)nvWrites,
             (unsigned long)nvWriteFails);
}

// CUSTOM SETUP (action list) and EDIT (one action) screens. Full redraws: the
// setup UI produces no HID output, so the drawing time does not delay any report.
static void drawSetupScreen()
{
    auto& d = M5Cardputer.Display;
    static const char* const LABELS[SLOT_COUNT] = {"1 CW", "2 CCW", "3 BTN"};
    static const char* const SHORT[SLOT_COUNT] = {"CW", "CCW", "BTN"};
    char buf[28];
    d.fillScreen(TFT_BLACK);
    drawBuildTag();  // (1e-2 r2)
    d.setTextSize(2);

    if (uiMode == UI_SETUP) {
        d.setTextColor(TFT_CYAN, TFT_BLACK);
        d.setCursor(4, 2);
        d.print("CUSTOM SETUP");
        for (uint8_t i = 0; i < SLOT_COUNT; i++) {
            int y = 22 + i * 18;
            bool sel = (i == setupSel);
            uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;
            if (sel) d.fillRect(0, y - 1, 240, 17, TFT_NAVY);
            d.setTextSize(2);
            d.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, bg);
            d.setCursor(4, y);
            d.print(LABELS[i]);
            formatKey(customDraft[i], buf, sizeof(buf));
            // (1e-2 r3) Largest text size whose measured width fits the row. Labels end
            // at x = 64 ("2 CCW", size 2), so the key starts at x = 72 and may use up to
            // x = 236. Each size is vertically placed inside the 17 px highlight
            // (y-1 .. y+15): size 2 = 16 px high at y, 1.5 = 12 px at y+2, 1 = 8 px at y+4.
            // The longest possible name (25 chars) is 150 px at size 1, so the last
            // size always fits and nothing is clipped.
            static constexpr int KEY_X = 72;
            static constexpr int KEY_W = 240 - 4 - KEY_X;
            static constexpr float KEY_SIZES[] = {2.0f, 1.5f, 1.0f};
            static constexpr int KEY_DY[] = {0, 2, 4};
            for (uint8_t s = 0; s < 3; s++) {
                d.setTextSize(KEY_SIZES[s]);
                if (s == 2 || d.textWidth(buf) <= KEY_W) {
                    d.setCursor(KEY_X, y + KEY_DY[s]);
                    d.print(buf);
                    break;
                }
            }
        }
        d.setTextSize(1);
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.setCursor(4, 80);
        d.print("1-3/turn: select  knob press: edit");
        d.setCursor(4, 90);
        d.print("ok (Enter): SAVE   esc (`): CANCEL");
        // (1e-3) BTN mode line (the rows above keep the r3 adaptive key sizing).
        bool btnHold = (customDraft[SLOT_BTN].flags & CK_HOLD) != 0;
        d.setCursor(4, 102);
        d.setTextColor(btnHold ? TFT_ORANGE : TFT_WHITE, TFT_BLACK);
        d.print(btnHold ? "BTN: HOLD" : "BTN: TAP");
        d.setCursor(100, 102);
        d.setTextColor(draftChanged ? TFT_YELLOW : TFT_DARKGREY, TFT_BLACK);
        d.print(draftChanged ? "Unsaved changes" : "No changes");
        d.setCursor(4, 114);
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.print(nvWritable ? "SAVE also stores it (NVS)" : "NVS read-only: RAM only");  // (1e-4)
        return;
    }

    // UI_EDIT
    const CustomKey& k = customDraft[setupSel];
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(4, 2);
    d.printf("EDIT %s", SHORT[setupSel]);
    if (setupSel == SLOT_BTN) {
        // (1e-3) TAP / HOLD badge, header line, x 124-172 (clear of the build tag at x 204)
        bool hold = (k.flags & CK_HOLD) != 0;
        d.setTextColor(hold ? TFT_ORANGE : TFT_WHITE, TFT_BLACK);
        d.setCursor(124, 2);
        d.print(hold ? "HOLD" : "TAP");
    }
    static const char* const MODS[4] = {"Ctl", "Sft", "Alt", "Cmd"};
    static const uint8_t BITS[4] = {MOD_CTRL, MOD_SHIFT, MOD_ALT, MOD_GUI};
    for (uint8_t i = 0; i < 4; i++) {
        bool on = (k.modifiers & BITS[i]) != 0;
        d.setTextColor(on ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        d.setCursor(4 + i * 58, 22);
        d.print(MODS[i]);
    }
    d.setTextSize(3);
    if (k.usage == 0) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        // (1e-3) modifiers without a key are a valid (modifier-only) action
        snprintf(buf, sizeof(buf), k.modifiers ? "(no key)" : "(none)");
    } else {
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        keyName(k.usage, buf, sizeof(buf));
    }
    d.setCursor(4, 44);
    d.print(buf);
    d.setTextSize(1);
    if (pickerIndex >= 0) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.setCursor(190, 70);
        d.printf("%d/%u", pickerIndex + 1, PICKER_COUNT);
    }
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 80);
    d.print("Type a key, or turn knob for list");
    d.setCursor(4, 90);
    d.print("Fn+C Ctl  Fn+S Sft  Fn+A Alt  Fn+G Cmd");
    d.setCursor(4, 100);
    d.print("del: clear  ok/knob press: done");
    d.setCursor(4, 110);
    // (1e-3) BTN only: TAP/HOLD toggle hint
    d.print(setupSel == SLOT_BTN ? "Fn+H: TAP/HOLD   esc (`): undo" : "esc (`): undo this action");
}

// ---------------------------------------------------------------- display
// ---------------------------------------------------------------- (2a) link status texts
// First line of the status screens and right side of the Spinner header.
static const char* linkStatusText(uint16_t* color)
{
    if (activeTransport == TRANSPORT_USB) {
        if (!(bool)USB) { *color = TFT_RED; return "USB NOT MOUNTED"; }
        if (usbSuspended) { *color = TFT_YELLOW; return "USB SUSPENDED"; }
        *color = TFT_GREEN;
        return "USB HID READY";
    }
    if (!bleStarted) { *color = TFT_DARKGREY; return "BLE STARTING"; }
    switch (linkState) {
        case LINK_READY:     *color = TFT_GREEN;  return "BLE READY";
        case LINK_ENCRYPTED: *color = TFT_YELLOW; return "BLE SUBSCRIBING";
        case LINK_CONNECTED: *color = TFT_YELLOW; return "BLE PAIRING";
        default:             *color = TFT_CYAN;   return "BLE ADVERTISING";
    }
}

// The inactive transport, for the CONNECTION screen (design §4.1 / §6).
static const char* otherStatusText(Transport t, uint16_t* color)
{
    if (t == TRANSPORT_USB) {
        *color = TFT_DARKGREY;
        return (bool)USB ? "USB HID INACTIVE (mounted)" : "USB HID INACTIVE (not mounted)";
    }
    *color = TFT_DARKGREY;
    if (!bleStarted) return "BLE OFF (not started)";
    return bleConnected ? "BLE OFF (disconnecting)" : "BLE OFF";
}

// Redraw the headers when what they show changes (link state, mode, warning).
static void checkHeaderChange()
{
    uint32_t sig = (uint32_t)activeTransport | ((uint32_t)linkState << 2) | ((usbHostReady ? 1u : 0u) << 5)
                 | (((bool)USB ? 1u : 0u) << 6) | ((usbSuspended ? 1u : 0u) << 7)
                 | ((releaseOwed[0] ? 1u : 0u) << 8) | ((releaseOwed[1] ? 1u : 0u) << 9)
                 | ((bleStarted ? 1u : 0u) << 10) | ((uint32_t)switchState << 11);
    if (sig != headerSig) {
        headerSig = sig;
        dirty = true;
        spinnerFull = true;
        setupDirty = true;
    }
}

static void drawConnScreen()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    drawBuildTag();
    d.setTextSize(2);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(4, 2);
    d.print("CONNECTION");

    // Selection: two boxes, the selected one highlighted; the active one marked "*".
    for (uint8_t i = 0; i < 2; i++) {
        bool sel = (connSel == (Transport)i);
        int x = 4 + i * 80;
        d.fillRect(x, 22, 72, 20, sel ? TFT_NAVY : TFT_BLACK);
        d.drawRect(x, 22, 72, 20, sel ? TFT_YELLOW : TFT_DARKGREY);
        d.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, sel ? TFT_NAVY : TFT_BLACK);
        d.setCursor(x + 6, 24);
        d.printf("%u %s%s", i + 1, TRANSPORT_NAME[i], activeTransport == (Transport)i ? "*" : "");
    }

    d.setTextSize(1);
    uint16_t c;
    const char* t = linkStatusText(&c);
    d.setTextColor(c, TFT_BLACK);
    d.setCursor(4, 48);
    d.printf("%s: %s", TRANSPORT_NAME[activeTransport], t);
    Transport other = (activeTransport == TRANSPORT_USB) ? TRANSPORT_BLE : TRANSPORT_USB;
    t = otherStatusText(other, &c);
    d.setTextColor(c, TFT_BLACK);
    d.setCursor(4, 58);
    d.print(t);

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 70);
    uint16_t h = bleConnHandle;
    if (h == 0xFFFF) d.printf("BLE bond=%u kb=%u ms=%u h- ad%lu fs%lu", linkBonded ? 1 : 0,
                              kbSubscribed ? 1 : 0, mouseSubscribed ? 1 : 0,
                              (unsigned long)connAdopted, (unsigned long)foreignSubscribes);
    else d.printf("BLE bond=%u kb=%u ms=%u h%u ad%lu fs%lu", linkBonded ? 1 : 0, kbSubscribed ? 1 : 0,
                  mouseSubscribed ? 1 : 0, (unsigned)h, (unsigned long)connAdopted,
                  (unsigned long)foreignSubscribes);
    d.setCursor(4, 80);
    d.printf("sw%lu warn%lu  link rec %s w%lu f%lu", (unsigned long)switchCount, (unsigned long)switchWarnings,
             linkStored ? TRANSPORT_NAME[linkStoredMode] : "-", (unsigned long)linkWrites,
             (unsigned long)linkWriteFails);

    int y = 92;
    for (uint8_t i = 0; i < 2; i++) {
        if (!releaseOwed[i]) continue;
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.setCursor(4, y);
        d.printf("! OLD %s RELEASE UNCONFIRMED", TRANSPORT_NAME[i]);
        y += 10;
    }
    if (switchState != SW_IDLE) {
        d.setTextColor(TFT_YELLOW, TFT_BLACK);
        d.setCursor(4, y);
        d.printf("switching %s -> %s ...", TRANSPORT_NAME[switchFrom], TRANSPORT_NAME[switchTo]);
    }
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 116);
    d.print("1 USB 2 BLE ok/press:apply esc(`):close");
    // (2a-2) The GATT value handles of the three reports (fixed by the BLE-B1 table), or
    // the registration error. FFFF = BLE not started yet.
    d.setCursor(4, 126);
    if (gattRegisterRc != 0 && bleStarted) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.printf("t:trace  GATT REG rc%d", gattRegisterRc);
    } else {
        d.printf("t:trace knob:select ki%X ko%X mi%X", (unsigned)kbInput->getHandle(),
                 (unsigned)kbOutput->getHandle(), (unsigned)mouseInput->getHandle());
    }
}

// (r3) Number of open connections NimBLE knows (read-only).
static uint8_t countConnections()
{
    uint8_t n = 0;
    struct ble_gap_conn_desc d;
    for (uint16_t h = 0; h < 16; h++) {
        if (ble_gap_conn_find(h, &d) == 0) n++;
    }
    return n;
}

// (r3) Freeze the ring (chronological) for the trace pages.
static void freezeTrace()
{
    uint32_t newest = traceNewest();
    uint32_t first = (newest > TRACE_SIZE) ? newest - TRACE_SIZE + 1 : 1;
    viewCount = 0;
    for (uint32_t seq = first; seq <= newest && newest > 0; seq++) {
        TraceRec r;
        if (traceRead(seq, &r)) viewBuf[viewCount++] = r;
    }
    viewAtMs = millis();
}

static uint8_t tracePages()
{
    uint32_t p = (viewCount + TRACE_LINES - 1) / TRACE_LINES;
    return p == 0 ? 1 : (uint8_t)p;
}

// Page 1 = the newest TRACE_LINES records, page 2 the ones before, ...; each page
// is chronological (oldest at the top).
static void drawTracePage()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(0, 0);
    uint16_t h = bleConnHandle;
    char hs[6];
    if (h == 0xFFFF) snprintf(hs, sizeof(hs), "-");
    else snprintf(hs, sizeof(hs), "%u", h);
    d.printf("TRACE %u/%u n%u h%s @%lu.%lu lost%lu", tracePage, tracePages(), connCount, hs,
             (unsigned long)(viewAtMs / 1000), (unsigned long)((viewAtMs / 100) % 10),
             (unsigned long)traceLost);
    drawBuildTag();
    int32_t end = (int32_t)viewCount - (int32_t)(tracePage - 1) * TRACE_LINES;
    int32_t begin = end - TRACE_LINES;
    if (begin < 0) begin = 0;
    uint16_t kb = kbValueHandle, ms = mouseValueHandle;
    int y = 10;
    for (int32_t i = begin; i < end; i++) {
        char line[48];
        formatTraceLine(viewBuf[i], kb, ms, line, sizeof(line));
        uint8_t ty = viewBuf[i].type;
        d.setTextColor(ty == TR_LINK_READY ? TFT_GREEN
                       : (ty == TR_LINK_LOST || ty == BLE_GAP_EVENT_DISCONNECT) ? TFT_ORANGE
                       : (ty >= TR_APP_CONNECT) ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        d.setCursor(0, y);
        d.print(line);
        y += 9;
    }
}

static void drawStatus()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    drawBuildTag();  // (1e-2 r2)
    d.setTextSize(2);
    d.setCursor(4, 4);

    {  // (2a) the active transport's state
        uint16_t c;
        const char* t = linkStatusText(&c);
        d.setTextColor(c, TFT_BLACK);
        d.println(t);
    }

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.printf("PROFILE %u %s\n", profile, profileName(profile));

    if (!unitFound) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.println("U135 NOT FOUND");
        d.setTextSize(1);
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.println();
        d.println(" I2C 0x40 on G2(SDA)/G1(SCL)");
        d.println(" retry every 1 s");
        d.printf(" KB keys=%u fn=%u chg=%lu\n", kbPressed, kbFn ? 1 : 0,
                 (unsigned long)profileChanges);
        d.printf(" MS host=%u want=%u%s\n", mouseHost, mouseWanted,
                 mouseSyncNeeded ? " SYNC" : "");
        return;
    }

    // (1e-2) Profile 4 has its own body: the saved assignments and how to edit them.
    if (profile == PROFILE_CUSTOM) {
        drawCustomStatusBody();
        return;
    }

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("VAL %d R%+d\n", lastValue, accum);
    if (profile == 2) {
        d.printf("BTN %s X%+ld\n", mouseWanted ? "DOWN" : "UP", (long)mouseXSent);
    } else if (profile == PROFILE_EDIT) {
        // (1e-1) The tap counters are shared with PROFILE 1, so show the mapping instead.
        d.println("Right/Left/Space");
    } else {
        d.printf(".%lu ,%lu k%lu\n", (unsigned long)txCount[EV_CW],
                 (unsigned long)txCount[EV_CCW], (unsigned long)txCount[EV_PRESS]);
    }
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.printf("%s\n", lastEvent);

    // Diagnostics in small text.
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, d.getCursorY() + 4);
    d.printf("BTN raw=%u  Q%u/%u OVF%lu\n", lastButton, queueCount, queueMax,
             (unsigned long)queueOverflow);
    d.setCursor(4, d.getCursorY());
    d.printf("FAIL%lu SKIP%lu ERR%lu%s\n", (unsigned long)sendFails, (unsigned long)skipped,
             (unsigned long)readErrors, releasePending ? " REL!" : "");
    d.setCursor(4, d.getCursorY());
    d.printf("W%lu T%lu nv w%lu f%lu %lums\n", (unsigned long)maxWorkMs, (unsigned long)maxTapMs,
             (unsigned long)nvWrites, (unsigned long)nvWriteFails, (unsigned long)nvMaxWriteMs);
    d.setCursor(4, d.getCursorY());
    d.printf("KB keys=%u fn=%u chg=%lu\n", kbPressed, kbFn ? 1 : 0,
             (unsigned long)profileChanges);
    d.setCursor(4, d.getCursorY());
    d.printf("MS host=%u want=%u rep%lu fail%lu clip%lu%s\n", mouseHost, mouseWanted,
             (unsigned long)mouseReports, (unsigned long)mouseFails,
             (unsigned long)mouseClipped, mouseSyncNeeded ? " SYNC" : "");
}

// ---------------------------------------------------------------- (1d) spinner screen
// PROFILE 2 with the U135 present. Text is drawn with a background colour and
// padded to a fixed width, so a field can be overwritten without fillScreen().
static constexpr int SPIN_SENS_Y = 58;   // size 4 digits, 32 px high
static constexpr int SPIN_DIAG_Y = 116;  // two size 1 rows

static void drawSpinnerSensitivity()
{
    auto& d = M5Cardputer.Display;
    d.setTextSize(4);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(84, SPIN_SENS_Y);
    d.printf("%3u", SENS_STEPS[sensIndex]);
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(164, SPIN_SENS_Y + 24);
    d.printf("%u/%u", sensIndex + 1, SENS_COUNT);
    // (1e-4) NVS status beside the number (x 164-212); padded, overwritten in place.
    uint16_t c;
    const char* t = nvStateText(&c);
    d.setTextColor(c, TFT_BLACK);
    d.setCursor(164, SPIN_SENS_Y + 8);
    d.printf("%-8s", t);
}

static void drawSpinnerDiag()
{
    auto& d = M5Cardputer.Display;
    char line[41];
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(line, sizeof(line), "BTN %s X%+ld rep%lu fail%lu%s",
             mouseWanted ? "DOWN" : "UP", (long)mouseXSent, (unsigned long)mouseReports,
             (unsigned long)mouseFails, mouseSyncNeeded ? " SYNC" : "");
    d.setCursor(4, SPIN_DIAG_Y);
    d.printf("%-38.38s", line);
    snprintf(line, sizeof(line), "clip%lu ERR%lu W%lu key:%s%c nv%lu",
             (unsigned long)mouseClipped,
             (unsigned long)readErrors, (unsigned long)maxWorkMs, kbFn ? "Fn+" : "",
             kbLastChar ? kbLastChar : '-', (unsigned long)nvWrites);
    d.setCursor(4, SPIN_DIAG_Y + 10);
    d.printf("%-38.38s", line);
}

static void drawSpinnerFull()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);

    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 3);
    d.printf("MEDIA JOG %s", BUILD_TAG);  // (1e-2 r2) was a stale "MEDIA JOG 1e-1"
    {  // (2a) the active transport's state, right-aligned; "!" = a release is unconfirmed
        uint16_t c;
        const char* t = linkStatusText(&c);
        bool warn = releaseOwed[TRANSPORT_USB] || releaseOwed[TRANSPORT_BLE];
        int w = d.textWidth(t) + (warn ? d.textWidth("!") : 0);
        d.setCursor(d.width() - w - 4, 3);
        if (warn) {
            d.setTextColor(TFT_RED, TFT_BLACK);
            d.print("!");
        }
        d.setTextColor(c, TFT_BLACK);
        d.print(t);
    }
    d.drawFastHLine(4, 13, 232, TFT_DARKGREY);

    d.setTextSize(2);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(4, 18);
    d.print("PROFILE 2:SPINNER");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 38);
    d.print("SENSITIVITY");

    d.setTextSize(2);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(4, 94);
    d.print("Fn+. -   Fn+; +");

    drawSpinnerSensitivity();
    drawSpinnerDiag();
}

// ---------------------------------------------------------------- (1d) sensitivity
// Unsent movement and the half-count carry were computed with the old step;
// they are dropped so that old and new scales never mix.
static void changeSensitivity(int8_t dir)
{
    int next = (int)sensIndex + dir;
    if (next < 0 || next >= SENS_COUNT) return;  // already at the end: no change
    sensIndex = (uint8_t)next;
    mousePendingDx = 0;
    mouseHalfRem = 0;
    sensDirty = true;
    dirty = true;
    sensSavePending = true;   // (1e-4) stored SENS_SAVE_DELAY_MS after the last step
    sensChangedMs = millis();
    Serial.printf("[SENS] %u counts/detent (%u/%u)\n", SENS_STEPS[sensIndex], sensIndex + 1,
                  SENS_COUNT);
}

// ---------------------------------------------------------------- profiles
// Nothing from the previous profile is carried over: unsent key taps and
// movement are dropped, the mouse button is released and its state resent.
static void setProfile(uint8_t p)
{
    profile = p;
    profileChanges++;
    releaseHoldNow();  // (1e-3) a held Custom key is released on any profile change
    queueClear();
    accum = 0;
    mousePendingDx = 0;
    mouseHalfRem = 0;
    setMouseButton(0);
    mouseSyncNeeded = true;
    profileSavePending = true;  // (1e-4) stored at once, in loop() after this loop's HID sends
    Serial.printf("[KB] PROFILE %u\n", profile);
    dirty = true;
}

// ---------------------------------------------------------------- Cardputer keyboard
// Fn sets keysState().fn; a key adds its base character to keysState().word
// (Fn does not change the character; only Shift / Ctrl / Caps do), per
// M5Cardputer 1.1.1 Keyboard.cpp. The library has no arrow key codes: the arrows
// are the Fn legends printed on ';' (up) and '.' (down) on the Cardputer ADV.
// Nothing here is ever passed to the USB HID side.
static void pollKeyboard()
{
    M5Cardputer.update();  // M5.update() (reads G0 only on ADV) + one TCA8418 event

    auto& kb = M5Cardputer.Keyboard;
    const auto& ks = kb.keysState();
    uint8_t pressed = kb.isPressed();
    char lastChar = ks.word.empty() ? kbLastChar : ks.word.back();
    if (pressed != kbPressed || ks.fn != kbFn || lastChar != kbLastChar) {
        kbPressed = pressed;
        kbFn = ks.fn;
        kbLastChar = lastChar;
        dirty = true;
    }

    // (1e-2) While CUSTOM SETUP is open, every Cardputer key belongs to the setup UI:
    // no profile switching, no sensitivity keys, and nothing is sent to the host.
    if (uiMode != UI_NORMAL) {
        processSetupKeys(ks);
        return;
    }

    // Exactly one recognised character together with Fn makes a chord;
    // anything else (none, or two at once) is ignored.
    uint8_t chord = CHORD_NONE;
    if (ks.fn) {
        uint8_t found = 0;
        for (char c : ks.word) {
            uint8_t ch = CHORD_NONE;
            if (c == '1') ch = CHORD_P1;
            if (c == '2') ch = CHORD_P2;
            if (c == '3') ch = CHORD_P3;  // (1e-1) same number row as '1'/'2' (Keyboard.h:21-22)
            if (c == '4') ch = CHORD_P4;
            if (c == ';') ch = CHORD_SENS_UP;
            if (c == '.') ch = CHORD_SENS_DOWN;
            if (c == 's') ch = CHORD_SETUP;  // (1e-2) acts in Profile 4 only
            if (c == 'b') ch = CHORD_CONN;   // (2a) CONNECTION, any profile
            if (ch != CHORD_NONE) {
                chord = ch;
                found++;
            }
        }
        if (found != 1) chord = CHORD_NONE;
    }

    // Act once when the chord appears, not every loop while it is held.
    if (chord == lastChord) return;
    lastChord = chord;
    if (chord != CHORD_NONE) {  // (1e-2 r2) diagnostic log only
        Serial.printf("[KB] Fn chord %u in profile %u\n", chord, profile);
    }
    switch (chord) {
        case CHORD_P1:
        case CHORD_P2:
        case CHORD_P3:
        case CHORD_P4:
            if (chord != profile) setProfile(chord);  // chord value == profile number
            break;
        case CHORD_SENS_UP:
            if (profile == 2) changeSensitivity(+1);  // PROFILE 2 only
            break;
        case CHORD_SENS_DOWN:
            if (profile == 2) changeSensitivity(-1);
            break;
        case CHORD_SETUP:
            if (profile == PROFILE_CUSTOM) enterSetup(ks);  // ignored in Profiles 1-3
            break;
        case CHORD_CONN:
            enterConn(ks);  // (2a)
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------- setup / loop
void setup()
{
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);  // never stall the loop when no one reads the CDC port
    Serial.printf("[BOOT] MediaJog %s\n", BUILD_TAG);  // (2a)
    delay(300);

    // Same display init as 1a, done first. M5.begin() below then finds the board
    // already detected: Display.init() returns at once (M5GFX.cpp init_impl) and
    // GPIO46 is not driven (M5Unified.hpp begin(): gpio46_hold).
    M5Cardputer.Display.begin();

    // M5.begin() is needed for M5.getBoard() == board_M5CardputerADV, which
    // Keyboard_Class::begin() uses to pick the TCA8418 reader (internal I2C G8/G9,
    // I2C_NUM_1 via Wire1, INT on G11). Everything this sketch does not use is off.
    auto cfg = M5.config();
    cfg.clear_display = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    cfg.external_imu = false;          // Port A (Ex_I2C) is never begun by M5Unified
    cfg.external_rtc = false;
    cfg.external_display_value = 0;
    cfg.external_speaker_value = 0;
    cfg.serial_baudrate = 0;           // Serial is already started above
    M5Cardputer.begin(cfg, true);

    // (1e-4) Saved settings, before anything is drawn. Reads NVS only. The HID
    // state is untouched: keys, HOLD and the mouse button start released, and a
    // restored BTN = HOLD acts only on a new knob press (readInitialState() below
    // takes the current knob state as the reference).
    loadSettings();
    loadLink();  // (2a) saved transport; USB if none. Reads NVS only.

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(128);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    // With CDC On Boot the core has already called USB.begin() (the HID devices
    // above were registered before that, at static init); calling it again returns
    // early. Both begin() calls create the same shared send semaphores.
    USB.onEvent(onUsbEvent);
    UsbMouse.begin();
    UsbKeyboard.begin();
    USB.begin();
    // (2a) USB HID is always enumerated (static devices); it sends only while USB is the
    // active transport. The BLE stack starts here only if the saved transport is BLE.
    if (activeTransport == TRANSPORT_BLE) bleStart();

    // Wire = I2C_NUM_0 on the external port, begun after M5.begin() as in 1a (after
    // the display init). This sketch never uses Wire1 (internal bus, owned by M5Unified).
    sensor.begin(&Wire, ENCODER_ADDR, PORT_SDA, PORT_SCL, I2C_FREQ);

    unitFound = probeUnit();
    Serial.printf("[U135] probe 0x40: %s\n", unitFound ? "ACK" : "no ACK");
    if (unitFound) {
        unitFound = readInitialState();
    }
    updateUsbState();
    drawStatus();
    drawnLayout = LAYOUT_STATUS;
    lastProbeMs = millis();
}

static void onRotation(int16_t delta)
{
    // (2a) CONNECTION / trace screens: the knob selects USB / BLE; nothing is sent.
    if (uiMode == UI_CONN || uiMode == UI_TRACE) {
        connRotate(delta);
        return;
    }
    // (1e-2) While CUSTOM SETUP is open the knob only drives the UI (no HID output).
    if (uiMode != UI_NORMAL) {
        setupRotate(delta);
        return;
    }

    if (profile == 2) {
        if (!hidReady) {
            skipped++;
            return;
        }
        // In half HID counts: delta encoder counts x (SENS / 2) HID counts each.
        // A reversal simply adds with the opposite sign, as in 1c (no extra handling).
        int32_t half = (int32_t)delta * SENS_STEPS[sensIndex];
        int32_t total = mouseHalfRem + half;
        int32_t whole = total / 2;            // C++ truncates toward zero
        mouseHalfRem = total - whole * 2;     // -1, 0 or +1, carried to the next delta
        mousePendingDx += whole;
        if (mousePendingDx > MOUSE_PENDING_MAX) {
            mouseClipped += mousePendingDx - MOUSE_PENDING_MAX;
            mousePendingDx = MOUSE_PENDING_MAX;
        } else if (mousePendingDx < -MOUSE_PENDING_MAX) {
            mouseClipped += -MOUSE_PENDING_MAX - mousePendingDx;
            mousePendingDx = -MOUSE_PENDING_MAX;
        }
        return;
    }

    // (1e-2) PROFILE 4, Custom: 1 detent = 1 tap of the saved key, if one is assigned;
    // unassigned directions send nothing (counted as ignored).
    if (profile == PROFILE_CUSTOM) {
        accum += delta;
        while (accum >= COUNTS_PER_DETENT) {
            accum -= COUNTS_PER_DETENT;
            pushCustom(EV_CW);
        }
        while (accum <= -COUNTS_PER_DETENT) {
            accum += COUNTS_PER_DETENT;
            pushCustom(EV_CCW);
        }
        dirty = true;
        return;
    }

    // PROFILE 1 and (1e-1) PROFILE 3: 1 detent = 1 key tap; actionFor() picks the key.
    // Remainder is kept as is (plan §7-2).
    accum += delta;
    while (accum >= COUNTS_PER_DETENT) {
        accum -= COUNTS_PER_DETENT;
        pushEvent(EV_CW);
    }
    while (accum <= -COUNTS_PER_DETENT) {
        accum += COUNTS_PER_DETENT;
        pushEvent(EV_CCW);
    }
}

static void onButton(bool pressed)
{
    // (2a) CONNECTION screen: the knob press applies the selection.
    if (uiMode == UI_CONN || uiMode == UI_TRACE) {
        if (pressed && uiMode == UI_CONN) applyConn();
        return;
    }
    // (1e-2) While CUSTOM SETUP is open the knob press only drives the UI:
    // on the action list it opens EDIT, in EDIT it means "done".
    if (uiMode != UI_NORMAL) {
        if (pressed) {
            if (uiMode == UI_SETUP) startEdit(setupSel);
            else finishEdit();
        }
        return;
    }

    if (profile == 2) {
        if (!pressed) {
            setMouseButton(0);
        } else if (hidReady) {
            setMouseButton(MOUSE_LEFT);
        } else {
            skipped++;
        }
        return;
    }
    // (1e-2) PROFILE 4, Custom: one tap of the saved key per press, if assigned.
    if (profile == PROFILE_CUSTOM) {
        const CustomKey& b = customSaved[SLOT_BTN];
        if (customAssigned(b) && (b.flags & CK_HOLD)) {
            // (1e-3) HOLD: key/modifiers down while the knob is pressed.
            if (pressed) {
                if (hidReady) pushHoldDown();
                else skipped++;   // as for the mouse button: no press while USB is not ready
            } else if (holdArmed) {
                pushHoldUp();     // only after a queued HOLD down; never resumes anything
            }
            return;
        }
        if (pressed) pushCustom(EV_PRESS);
        return;
    }
    // PROFILE 1 ('k') and (1e-1) PROFILE 3 (Space): one tap per press.
    if (pressed) pushEvent(EV_PRESS);
}

static void pollUnit(uint32_t now)
{
    if (!unitFound) {
        if (now - lastProbeMs >= 1000) {
            lastProbeMs = now;
            unitFound = probeUnit();
            Serial.printf("[U135] probe 0x40: %s\n", unitFound ? "ACK" : "no ACK");
            if (unitFound) {
                unitFound = readInitialState();
                dirty = true;
            }
        }
        return;
    }

    // Detect unplug without reading garbage from a missing device.
    if (now - lastProbeMs >= 500) {
        lastProbeMs = now;
        if (!probeUnit()) {
            unitFound = false;
            accum = 0;
            // The knob can no longer report a release: release the mouse button now.
            setMouseButton(0);
            mousePendingDx = 0;
            mouseHalfRem = 0;
            releaseHoldNow();  // (1e-3) same for a held Custom key
            Serial.println("[U135] lost (no ACK)");
            dirty = true;
            return;
        }
    }

    // A failed read is counted and skipped; it never becomes an input.
    int16_t value = lastValue;
    if (!readEncoder(value)) {
        readErrors++;
        Serial.println("[U135] encoder read failed");
        dirty = true;
    }
    int16_t delta = (int16_t)(value - lastValue);  // wrap-safe for int16 counter
    if (delta != 0) {
        Serial.printf("[U135] value=%d delta=%d\n", value, delta);
        lastValue = value;
        onRotation(delta);
        dirty = true;
    }

    // A failed button read keeps the previous state (a held button stays held).
    uint8_t button = lastButton;
    if (!readButtonRaw(button)) {
        readErrors++;
        Serial.println("[U135] button read failed");
        dirty = true;
    }
    if (button != lastButton) {
        Serial.printf("[U135] button_raw %u -> %u\n", lastButton, button);
        onButton(button == BUTTON_PRESSED_RAW);
        lastButton = button;
        dirty = true;
    }
}

void loop()
{
    uint32_t now = millis();

    if (bleStarted) {  // (2a) BLE-1 r4 link tracking and diagnostics, in either mode
        drainTrace();
        if (now - lastConnScanMs >= 250) {
            lastConnScanMs = now;
            uint8_t n = countConnections();
            if (n != connCount) {
                connCount = n;
                dirty = true;
            }
        }
        updateLinkState(now);
    }
    updateUsbState();
    pollKeyboard();
    pollUnit(now);
    serviceSwitch(now);  // (2a) release on the old transport, then commit
    if (hidReady && switchState == SW_IDLE) {
        if (releaseRepeatPending && (int32_t)(now - releaseRepeatAt) >= 0) {
            // (BLE-1 review 1) Repeat the all-up state once, whatever is queued:
            // through releasePending, so sendQueued() sends it before any further tap.
            releaseRepeatPending = false;
            releasePending = true;
        }
        sendMouse();   // first: a pending mouse release goes out before key taps
        sendQueued();
    }
    serviceOwedUsbRelease();  // (2a) BLE mode: a release owed to the USB host
    // (1e-4) After the sends, so a release report is never delayed by a flash
    // write. Independent of USB: a profile is stored even while USB is not ready.
    serviceSettings(now);
    serviceLinkSave();  // (2a) transport, at once after a confirmed switch
    checkHeaderChange();

    // (1d) PROFILE 2 with the U135 present uses the spinner screen: full redraw
    // only on layout / USB changes, the sensitivity number at once when changed,
    // the diagnostics rows at most every DIAG_INTERVAL_MS. Rotation alone never
    // triggers a full-screen redraw. Everything else uses the 1c screen.
    // (1e-2) While CUSTOM SETUP is open its own screen is shown instead.
    uint8_t layout = (uiMode == UI_TRACE) ? LAYOUT_TRACE
                   : (uiMode == UI_CONN) ? LAYOUT_CONN
                   : (uiMode != UI_NORMAL) ? LAYOUT_SETUP
                   : (profile == 2 && unitFound) ? LAYOUT_SPINNER : LAYOUT_STATUS;
    if (layout == LAYOUT_TRACE) {  // (2a) frozen trace pages: drawn once per page
        if (pageDirty || drawnLayout != LAYOUT_TRACE) {
            drawTracePage();
            drawnLayout = LAYOUT_TRACE;
            pageDirty = false;
            setupDirty = false;
            dirty = false;
        }
    } else if (layout == LAYOUT_CONN) {  // (2a)
        if ((setupDirty || dirty || drawnLayout != LAYOUT_CONN) && now - lastDrawMs >= DRAW_INTERVAL_MS) {
            drawConnScreen();
            drawnLayout = LAYOUT_CONN;
            setupDirty = false;
            dirty = false;
            lastDrawMs = now;
        }
    } else if (layout == LAYOUT_SETUP) {
        if ((setupDirty || drawnLayout != LAYOUT_SETUP) && now - lastDrawMs >= DRAW_INTERVAL_MS) {
            drawSetupScreen();
            drawnLayout = LAYOUT_SETUP;
            setupDirty = false;
            dirty = false;
            lastDrawMs = now;
        }
    } else if (layout == LAYOUT_SPINNER) {
        if (drawnLayout != LAYOUT_SPINNER || spinnerFull) {
            drawSpinnerFull();
            drawnLayout = LAYOUT_SPINNER;
            spinnerFull = false;
            sensDirty = false;
            dirty = false;
            lastDiagMs = now;
        } else {
            if (sensDirty) {
                drawSpinnerSensitivity();
                sensDirty = false;
            }
            if (dirty && now - lastDiagMs >= DIAG_INTERVAL_MS) {
                drawSpinnerDiag();
                dirty = false;
                lastDiagMs = now;
            }
        }
    } else if ((dirty || drawnLayout != LAYOUT_STATUS) && now - lastDrawMs >= DRAW_INTERVAL_MS) {
        dirty = false;
        lastDrawMs = now;
        drawStatus();
        drawnLayout = LAYOUT_STATUS;
    }

    uint32_t work = millis() - now;
    if (work > maxWorkMs) {
        maxWorkMs = work;
        dirty = true;
    }
    delay(20);  // same poll period as v2
}
