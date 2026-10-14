/*
  MediaJog_BLE_B1 — BLE mouse, proposal B: ONE HID service with all reports, static NimBLE
  GATT table (docs/BLE_B1_DESIGN.md, docs/BLE_B1.md)

  Base: MediaJog_BLE_M1_r2 (hardware-tested; unchanged in MediaJog_BLE_M1_r2/), whose keyboard,
  B7 handling, MOUSE mode ('m'), 'g' and diagnostics are BLE-1 r4's plus the M1 additions.
  Why: the library's C++ GATT classes keep only one characteristic per UUID and service
  (BLEHIDDevice's keyboard output and mouse input never reached GATT), and they order
  services and characteristics by object address, so handles move between builds. A second
  HID service (BLE-M1 r2) registered, but the Mac did not subscribe to it (ms c0).
  B1 changes:
    - No BLEHIDDevice and no library BLEService. Device Information, ONE HID service and
      Battery are a static ble_gatt_svc_def table, queued with ble_gatts_count_cfg() /
      ble_gatts_add_svcs() after BLEDevice::createServer() and before the single
      bleServer->start() (ble_gatts_start()). The HID service holds HID Information, the
      BLE-1 report map (keyboard ID 1 + mouse ID 2), HID Control Point, Protocol Mode and three
      Report characteristics: keyboard input {1,1}, keyboard output {1,2}, mouse input {2,1}.
      NimBLE's own table has no duplicate-UUID drop; each report has its own value handle.
      Attribute values and permissions are those BLE-1 r4 actually registered (library NimBLE
      path); descriptors are read-only. Only the layout changes.
    - Subscribe input: a second GAP event listener matches the event's attribute handle to
      the keyboard / mouse input report and calls the unchanged handleSubscribe() with the
      same check and value as the library's setSubscribe() -> onSubscribe() did.
    - Notify output: ble_gatts_notify_custom() to the current connection while that report's
      CCCD is on; rc 0 = accepted by the host (the former SUCCESS_NOTIFY).
    - Status line: ki / ko / mi value handles (live), CCCD flags, LED writes.
  BLEServer (GAP connection, security request, advertising, ServerCallbacks), BLESecurity,
  the trace listener and all B7 functions are unchanged. No USB HID, no NVS writes.
  Screen tag BB1, build id BLE-B1. Test firmware only.

  --- BLE-M1 r2 notes (unchanged) ---
  MediaJog_BLE_M1_r2 — BLE-M1 revision 2: mouse service registered before the GATT server starts
  (docs/BLE_M1.md §7, §8)

  Base: MediaJog_BLE_M1 (hardware-tested; unchanged in MediaJog_BLE_M1/).
  Hardware result: kb h10, ms hFFFF. Cause (source): BLEHIDDevice::startServices() starts the
  device info, HID and battery services and then BLEServer::start() -> ble_gatts_start(), the
  only point where NimBLE registers the queued services and writes the value handles. BLE-M1
  created and started the mouse service after that, so it was queued but never registered.
  r2 changes:
    - bleBegin(): the three start() calls of startServices() are made one by one, then the
      mouse service is created and started (queued last, so the keyboard's services keep
      their place), then bleServer->start() once. Nothing else of startServices() is left out.
    - Status line: value handles read live from the characteristics (not a boot snapshot),
      plus the mouse service's start() result (s1 = queued, s0 = failed).
  Keyboard, B7 handling, Report Maps, MOUSE mode and the 'g' key are unchanged from BLE-M1.
  Screen tag BM1r2, build id BLE-M1r2. Test firmware only.

  --- BLE-M1 notes (unchanged) ---
  MediaJog_BLE_M1 — BLE mouse feasibility test: a second HID service (docs/BLE_M1.md)

  Base: MediaJog_BLE_1_r4 (hardware-tested; unchanged in MediaJog_BLE_1_r4/).
  Why: with m5stack:esp32 3.3.9 (NimBLE), BLEService::addCharacteristic() keeps only the
  FIRST characteristic of a UUID per service, so BLEHIDDevice's second and third Report
  characteristics (0x2A4D: keyboard output, mouse input) never reach the GATT table
  (handle 0xFFFF). Stage 2a's BLE mouse therefore cannot work (docs/STAGE_2A.md).
  This test build checks proposal A: the mouse gets its OWN HID service (0x1812, instance
  1) with Report Map (mouse, Report ID 2), HID Information, HID Control Point and one
  Report characteristic (input, notify, Report Reference {2, input}; NimBLE adds the
  CCCD). Inside that service no UUID repeats, so nothing is dropped.
    - Keyboard: BLEHIDDevice exactly as BLE-1 r4, incl. the B7 handling of subscribe
      events before CONNECT; its Report Map is now keyboard only.
    - Cardputer key 'm': the knob drives KEYS (Right / Left / Space, as BLE-1) or MOUSE
      (X movement 8 counts per detent, knob held = left button / drag).
    - Cardputer key 'g': send a GATT Service Changed indication (whole range), so a
      bonded Mac re-discovers the services. No bond or NVS data is deleted.
    - Diagnostics: value handles and CCCD state of both reports on the status page;
      subscribe trace lines show the attribute handle of unknown characteristics.
  Screen tag BM1, build id BLE-M1. Test firmware only.

  --- BLE-1 r4 notes (unchanged) ---
  MediaJog_BLE_1_r4 — BLE-1 revision 4: subscribe events before CONNECT (docs/BLE_1.md §13)

  Base: MediaJog_BLE_1_r3 (hardware-tested, kept unchanged in MediaJog_BLE_1_r3/).
  Cause found with the r3 trace and confirmed in source (esp-nimble 5c1a43c,
  ble_gap.c): for a peripheral connection NimBLE posts BLE_GAP_EVENT_CONNECT only
  after reading the peer's version and supported features
  (ble_gap_rx_rd_rem_sup_feat_complete), but the connection callback is installed
  when the link is created. A bonded Mac that starts encryption at once therefore
  produces ENC_CHANGE and the CCCD RESTORE subscribe events BEFORE CONNECT. Up to
  r3, BLE-1 learned the connection handle only in onConnect(): the early RESTORE
  was rejected as "foreign" (handle not registered yet) and onConnect() then
  cleared the subscription flags. kb stayed 0 although NimBLE had restored the
  CCCD; nothing could be sent (B7, and at start in the r3 test).
  r4 fix (connection-handle bookkeeping only):
    - A subscribe event while no live connection is registered adopts its handle,
      but only if NimBLE knows that handle as a live link (ble_gap_conn_find).
      Events for any other handle are still rejected (FGN).
    - onConnect() keeps the subscription flags when its handle was adopted that
      way; for any other handle it starts cleared, as before.
    - onDisconnect() changes the state only for the current connection.
    - A connect that fails before CONNECT is posted (CONNECT event with an error
      status, seen by the GAP listener) drops an adoption of that handle.
  Unchanged: security settings (incl. the library's Security Request at CONNECT),
  bonding, NVS use, HID reports, READY rules (connected + encrypted + keyboard
  CCCD), queue, U135, diagnostics. Screen tag B1r4, build id BLE-1r4.

  --- r3 notes (unchanged) ---
  MediaJog_BLE_1_r3 — BLE-1 revision 3: connection-resolved diagnostics (docs/BLE_1.md §12)

  Base: MediaJog_BLE_1_r2 (hardware-tested, kept unchanged in MediaJog_BLE_1_r2/).
  The r2 trace was not enough to explain the B7 photo (only 6 tokens, status codes
  cut to their low byte, no connection identity). r3 changes diagnostics only:
    - Every record: sequence number, time, connection generation, connection
      handle, the FULL status / reason code, and the link's encrypted / bonded /
      authenticated flags at that moment.
    - More NimBLE events: LINK_ESTAB, PARING_COMPLETE (fresh pairing vs. restore),
      MTU, CONN_UPDATE, TERM_FAILURE, PASSKEY_ACTION; plus app-level records: the
      library's connect / disconnect / subscribe callbacks as BLE-1 handled them
      (accepted or foreign), and LINK READY / LINK LOST from loop().
    - 64-record history; the Cardputer key 't' cycles: status -> trace pages
      (frozen copy, 13 records each, newest first page) -> status. Not sent anywhere.
    - Number of open connections (n) on the status page; full codes in the EV line.
    - Top-right screen tag "B1r3" (right-aligned). BUILD_TAG / logs keep "BLE-1r3".
  HID behaviour, security settings, bonding and NVS use are unchanged from r2 / BLE-1.

  --- r2 notes (unchanged) ---
  MediaJog_BLE_1_r2 — BLE-1 revision 2: reconnection diagnostics (docs/BLE_1.md §11)

  Base: MediaJog_BLE_1 (hardware-tested; kept unchanged in MediaJog_BLE_1/).
  Hardware test B7 (Mac Bluetooth OFF -> ON): the Mac shows "connected", the
  Cardputer stays at BLE SUBSCRIBING (connected + encrypted, keyboard CCCD not
  enabled), so nothing can be sent. The cause is not established from source.
  r2 changes:
    - Diagnostics, read-only: a NimBLE GAP event listener records CONNECT,
      DISCONNECT (reason), ENC_CHANGE (status), SUBSCRIBE (characteristic,
      reason WRITE / TERM / RESTORE, notify bit), REPEAT_PAIRING and
      IDENTITY_RESOLVED; shown as a compact trace line and logged on Serial.
      At each successful encryption the host task records the peer identity /
      over-the-air address types and how many CCCD records NimBLE has stored for
      this peer (the records ble_gatts_bonding_restored() would restore).
    - The connection handle is invalidated on disconnect; a subscribe event for
      any other connection is counted (fs) and ignored.
    - The SKIP text tells why an input was dropped: no link / not encrypted / no CCCD.
  HID behaviour, security settings, bonding and NVS use are unchanged.

  --- BLE-1 notes (unchanged) ---
  MediaJog_BLE_1 — BLE HID minimal prototype (docs/BLE_1.md)

  Separate firmware. The USB release candidate MediaJog_1e_4/ is not changed.
    - Cardputer ADV + Unit Encoder U135 -> BLE HID keyboard + mouse (HID over GATT)
      for macOS, with the BLE library bundled with m5stack:esp32 3.3.9 (NimBLE
      configuration): BLEServer, BLEHIDDevice, BLESecurity. No third-party library.
    - Fixed mapping, no profiles: CW = Right Arrow, CCW = Left Arrow,
      knob press = Space (TAP only).
    - Report ID 1 = keyboard (8-byte input, 1-byte LED output), Report ID 2 =
      mouse (5-byte input: buttons, X, Y, wheel, pan). Both layouts are the same
      as the TinyUSB keyboard / mouse reports of the USB builds, so the Stage 1e
      code can later send the same reports over either transport. The mouse is
      prepared (report map, characteristic, sendMouse()) but sends only the
      released state in BLE-1.
    - No USB HID: no USBHID object is created. USB-C still powers the device and
      carries the CDC serial log; the app is started from the Launcher as before.
    - Pairing: "Just Works" with bonding and LE Secure Connections, as in the
      core's Server_Gamepad example. Bonds are stored by NimBLE itself (NVS
      namespace "nimble_bond"). This sketch writes nothing to NVS and never
      erases NVS; it only reads NVS statistics for the diagnostics line.

  HID safety (docs/BLE_1.md §5):
    - The link is READY only when connected AND encrypted AND the keyboard input
      report is subscribed (CCCD notifications on). Nothing is sent otherwise.
    - Inputs while not READY are counted and dropped (never queued).
    - Every READY period starts with an all-keys-up keyboard report and a
      released mouse report. The keyboard all-up is repeated once RELEASE_REPEAT_MS
      later, before any further tap and retried until accepted (notify() has no
      delivery confirmation; a repeated state report is harmless).
    - Link loss (disconnect, lost encryption or subscription, a reconnect between
      two loops) clears the queue and the internal key / mouse state at once.
      Nothing is resent after a reconnect except the all-up state.
    - notify() returns void. Its outcome is read from BLECharacteristicCallbacks::
      onStatus(), which the library calls synchronously inside notify():
      SUCCESS_NOTIFY = accepted by the NimBLE host for sending. That is NOT a
      delivery confirmation from the Mac (USB's SendReport() completion is).
      Anything else, or no status at all, means the report was not accepted.

  Board settings: the established ones (USB Mode = USB-OTG (TinyUSB), USB CDC On
  Boot = Enabled), guarded below like the USB builds.
*/

#include <atomic>
#include <M5Cardputer.h>
#include <Wire.h>
#include "Unit_Encoder.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <nvs.h>  // nvs_get_stats() only (read-only diagnostics)
#include <host/ble_store.h>  // (r2) ble_store_read_cccd() / ble_store_util_count(): read-only diagnostics
#include <services/gatt/ble_svc_gatt.h>  // (M1) ble_svc_gatt_changed(): Service Changed indication
#include <host/ble_gatt.h>     // (B1) static GATT table, ble_gatts_notify_custom()
#include <host/ble_hs_mbuf.h>  // (B1) ble_hs_mbuf_from_flat() / ble_hs_mbuf_to_flat()

// Shown on every screen so the running build can be identified on the device.
static constexpr const char* BUILD_TAG = "BLE-B1";
// (r3) Short form for the top-right corner of the screen only (BLE-1 rN -> "B1rN").
static constexpr const char* SCREEN_TAG = "BB1";
static constexpr const char* BLE_DEVICE_NAME = "MediaJog BLE";

#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 0
#error "Tools > USB Mode must be 'USB-OTG (TinyUSB)' (the established MediaJog board settings)."
#endif
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || ARDUINO_USB_CDC_ON_BOOT != 1
// With CDC On Boot disabled, Serial becomes UART0 (TX = GPIO43), which the
// Cardputer ADV uses for audio I2S. Keep Serial on USB CDC.
#error "Tools > USB CDC On Boot must be 'Enabled'."
#endif
#if !defined(CONFIG_NIMBLE_ENABLED)
#error "BLE-1 is written for the NimBLE configuration of the m5stack:esp32 3.3.9 BLE library."
#endif

// ---------------------------------------------------------------- U135 (same as 1e-4)
// Port pins per M5Unified pin table for board_M5CardputerADV (EX SCL=G1, SDA=G2).
static constexpr uint8_t PORT_SDA = 2;
static constexpr uint8_t PORT_SCL = 1;
static constexpr uint32_t I2C_FREQ = 200000;  // library default, unchanged

static constexpr int16_t COUNTS_PER_DETENT = 2;   // measured with v2
static constexpr uint8_t BUTTON_PRESSED_RAW = 0;  // measured with v2

Unit_Encoder sensor;

// ---------------------------------------------------------------- HID key actions
struct KeyAction {
    uint8_t modifiers;  // HID modifier bits
    uint8_t usage;      // HID Usage ID, Keyboard/Keypad page (0x07)
    const char* label;
};
// Same usages as the hardware-verified Profile 3 (Video Editing) of the USB
// builds, where they are checked against TinyUSB's HID_KEY_* constants.
static constexpr KeyAction ACTION_CW    = {0x00, 0x4F, "Right"};  // Right Arrow
static constexpr KeyAction ACTION_CCW   = {0x00, 0x50, "Left"};   // Left Arrow
static constexpr KeyAction ACTION_PRESS = {0x00, 0x2C, "Space"};  // Space

// ---------------------------------------------------------------- HID reports
// Report map = TinyUSB's TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)) followed by
// TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2)), written out byte by byte.
static constexpr uint8_t REPORT_ID_KEYBOARD = 1;
static constexpr uint8_t REPORT_ID_MOUSE = 2;
// (B1) One HID service again: the BLE-1 r4 map, unchanged.
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

// ---------------------------------------------------------------- event queue
// Declared before any function: the Arduino builder inserts prototypes above the first function.
enum Event : uint8_t { EV_CW, EV_CCW, EV_PRESS };
enum TapResult : uint8_t { TAP_SENT, TAP_NOT_SENT };
enum LinkState : uint8_t { LINK_ADVERTISING, LINK_CONNECTED, LINK_ENCRYPTED, LINK_READY };

static constexpr uint8_t QUEUE_SIZE = 32;
static constexpr uint8_t MAX_TAPS_PER_LOOP = 4;
static Event queueBuf[QUEUE_SIZE];
static uint8_t queueHead = 0;
static uint8_t queueCount = 0;
static uint8_t queueMax = 0;
static uint32_t queueOverflow = 0;

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
static bool mouseMode = false;                      // (M1) knob drives the mouse ('m')
static bool keyMWasDown = false, keyGWasDown = false;
static uint32_t gattChangedSent = 0;
static constexpr int32_t MOUSE_PENDING_MAX = 127 * 4;  // (M1) unsent movement cap
static uint32_t mouseClipped = 0;

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

// loop() only.
static LinkState linkState = LINK_ADVERTISING;
static bool linkReady = false;         // connected, encrypted and keyboard subscribed
static bool linkBonded = false;
static uint32_t seenConnGen = 0;
static bool seenMouseSubscribed = false;
static uint32_t linkUps = 0;
static uint32_t linkLosses = 0;

// (B1) Outcome of the last notifyReport() (loop task).
enum NotifyStatus : int { NOTIFY_NONE = -1, NOTIFY_OK = 0, NOTIFY_NO_CLIENT, NOTIFY_NO_SUBSCRIBER, NOTIFY_HOST_ERROR };
static int lastNotifyStatus = NOTIFY_NONE;
static uint32_t lastNotifyCode = 0;    // ble_gatts_notify_custom() rc for NOTIFY_HOST_ERROR

// Keyboard state. kbHeldMods / kbHeldUsage is the base state every report is
// built from (as in 1e-3); always 0 in BLE-1 (no HOLD), kept so that the 1e-3
// HOLD logic can be ported without changing the report path.
static uint8_t kbHeldMods = 0;
static uint8_t kbHeldUsage = 0;
static bool releasePending = false;    // the base keyboard state must be (re)sent
static bool releaseRepeatPending = false;
static uint32_t releaseRepeatAt = 0;
static constexpr uint32_t RELEASE_REPEAT_MS = 100;

// Mouse state (1c structure). BLE-1 never presses the button or moves.
static uint8_t mouseWanted = 0;        // button state the firmware wants on the host
static bool mouseSyncNeeded = false;   // the button state must be (re)sent
static int32_t mousePendingDx = 0;     // movement not yet sent (always 0 in BLE-1)

// ---------------------------------------------------------------- counters and display state
static uint32_t txCount[3] = {0, 0, 0};  // taps whose press report was accepted, per Event
static uint32_t notifyFails = 0;         // reports not accepted by the BLE host
static uint32_t skipped = 0;             // inputs while the link was not READY
static uint32_t kbReports = 0;
static uint32_t mouseReports = 0;
static uint32_t maxWorkMs = 0;
static char lastEvent[24] = "-";

static bool unitFound = false;
static int16_t lastValue = 0;
static uint8_t lastButton = 1;
static int16_t accum = 0;  // counts not yet turned into a detent event
static uint32_t lastProbeMs = 0;
static uint32_t readErrors = 0;

// NVS statistics (read-only): at boot before BLE starts, and now.
static bool nvsStatsOk = false;
static nvs_stats_t nvsBoot = {};
static nvs_stats_t nvsNow = {};
static uint32_t lastNvsStatsMs = 0;
static constexpr uint32_t NVS_STATS_INTERVAL_MS = 5000;

static bool dirty = true;
static uint32_t lastDrawMs = 0;
static constexpr uint32_t DRAW_INTERVAL_MS = 100;

// (r3) Screen pages: 0 = status, 1.. = trace pages of a frozen copy of the ring.
static constexpr uint8_t TRACE_LINES = 13;
static uint8_t uiPage = 0;
static bool pageDirty = false;
static TraceRec viewBuf[TRACE_SIZE];   // chronological copy taken when the trace pages open
static uint32_t viewCount = 0;
static uint32_t viewAtMs = 0;
static bool keyTWasDown = false;
static uint8_t connCount = 0;           // open connections found by NimBLE (handles 0..15)
static uint32_t lastConnScanMs = 0;

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

// Keyboard report = base (held) state + the given tap, as in 1e-3.
static bool sendKeyReport(uint8_t modifiers, uint8_t usage)
{
    if (!linkReady) return false;
    KeyboardReport r = {};
    r.modifiers = kbHeldMods | modifiers;
    uint8_t n = 0;
    if (kbHeldUsage) r.keys[n++] = kbHeldUsage;
    if (usage && usage != kbHeldUsage) r.keys[n++] = usage;
    bool ok = notifyReport(kbInput, &r, sizeof(r));
    if (ok) kbReports++;
    return ok;
}

// Base-state report (all keys up in BLE-1). Retried every loop until accepted.
static bool sendRelease()
{
    if (sendKeyReport(0, 0)) {
        releasePending = false;
        return true;
    }
    releasePending = true;
    return false;
}

// A press that the BLE host did not accept was not sent at all, so the event is
// kept and retried next loop (unlike USB, there is no "outcome unknown" case).
static TapResult tapKey(const KeyAction& a)
{
    if (!sendKeyReport(a.modifiers, a.usage)) return TAP_NOT_SENT;
    sendRelease();  // a failed release is retried by releasePending
    return TAP_SENT;
}

// Mouse button state (and movement, unused in BLE-1), state-based as in 1c.
// Sent only while the mouse input report is subscribed.
static void sendMouse()
{
    if (!linkReady || !mouseSubscribed) return;
    if (!mouseSyncNeeded && mousePendingDx == 0) return;
    int32_t chunk = mousePendingDx;
    if (chunk > 127) chunk = 127;
    if (chunk < -127) chunk = -127;
    MouseReport r = {};
    r.buttons = mouseWanted;
    r.x = (int8_t)chunk;
    // (review 1) Movement is removed only when the report was accepted: a report
    // that was not accepted was not sent at all (unlike USB's "outcome unknown").
    if (notifyReport(mouseInput, &r, sizeof(r))) {
        mousePendingDx -= chunk;
        mouseReports++;
        mouseSyncNeeded = false;
    }
}

// ---------------------------------------------------------------- queue
static void queueClear()
{
    queueHead = 0;
    queueCount = 0;
}

// Events are queued only while the link is READY; otherwise they are counted and
// dropped, so a connection never replays old rotation or presses.
static void pushEvent(Event ev)
{
    if (!linkReady) {
        skipped++;
        // (r2) why: not connected / connected but not encrypted / no keyboard CCCD
        snprintf(lastEvent, sizeof(lastEvent), "%s",
                 linkState == LINK_ENCRYPTED ? "SKIP (no CCCD)"
                 : linkState == LINK_CONNECTED ? "SKIP (not encrypted)" : "SKIP (no link)");
        return;
    }
    if (queueCount >= QUEUE_SIZE) {
        queueOverflow++;
        return;
    }
    queueBuf[(queueHead + queueCount) % QUEUE_SIZE] = ev;
    queueCount++;
    if (queueCount > queueMax) queueMax = queueCount;
}

static const KeyAction& actionFor(Event ev)
{
    switch (ev) {
        case EV_CW:  return ACTION_CW;
        case EV_CCW: return ACTION_CCW;
        default:     return ACTION_PRESS;
    }
}

static void sendQueued()
{
    if (!linkReady) return;
    if (releasePending && !sendRelease()) return;  // base state first, order kept

    for (uint8_t n = 0; n < MAX_TAPS_PER_LOOP && queueCount > 0; n++) {
        Event ev = queueBuf[queueHead];
        const KeyAction& a = actionFor(ev);
        if (tapKey(a) == TAP_NOT_SENT) return;  // retried next loop, order kept
        queueHead = (queueHead + 1) % QUEUE_SIZE;
        queueCount--;
        txCount[ev]++;
        snprintf(lastEvent, sizeof(lastEvent), "TX %s", a.label);
        dirty = true;
        if (releasePending) return;  // release not accepted: retry it before the next tap
    }
}

// This loop's reports while READY: the scheduled all-up repeat, the mouse state,
// then queued taps. (review 1) Moved out of loop() unchanged except for the repeat.
static void serviceReports(uint32_t now)
{
    if (!linkReady) return;
    if (releaseRepeatPending && (int32_t)(now - releaseRepeatAt) >= 0) {
        // (review 1) Repeat the all-up state once, whatever is queued: through
        // releasePending, so sendQueued() sends it before any further tap and
        // retries it every loop until it is accepted.
        releaseRepeatPending = false;
        releasePending = true;
    }
    sendMouse();
    sendQueued();
}

// ---------------------------------------------------------------- link state
// Link lost: nothing can be sent. Everything pending is forgotten and the
// internal state is all-up; the next READY period starts with all-up reports.
static void onLinkLost()
{
    queueClear();
    kbHeldMods = 0;     // (HOLD state, empty in BLE-1)
    kbHeldUsage = 0;
    releasePending = false;
    releaseRepeatPending = false;
    mouseWanted = 0;
    mousePendingDx = 0;
    mouseSyncNeeded = false;
    accum = 0;
    linkLosses++;
    TraceRec r = {};  // (r3)
    r.type = TR_LINK_LOST;
    r.conn = bleConnHandle;
    r.sec = traceSecOf(r.conn);
    pushTrace(r);
    snprintf(lastEvent, sizeof(lastEvent), "LINK LOST");
    Serial.println("[BLE] link lost: queue cleared, state all-up");
}

// Link READY: start with all-up keyboard and mouse reports, and repeat the
// keyboard one once (no delivery confirmation over BLE).
static void onLinkReady(uint32_t now)
{
    releasePending = true;
    releaseRepeatPending = true;
    releaseRepeatAt = now + RELEASE_REPEAT_MS;
    mouseSyncNeeded = true;
    linkUps++;
    TraceRec r = {};  // (r3)
    r.type = TR_LINK_READY;
    r.conn = bleConnHandle;
    r.sec = traceSecOf(r.conn);
    pushTrace(r);
    snprintf(lastEvent, sizeof(lastEvent), "LINK READY");
    Serial.println("[BLE] link ready: sending all-up");
}

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
            if (subChar(t.attr, kbHandle, msHandle) == 'x') {  // (M1) unknown: show its handle
                snprintf(detail, sizeof(detail), "x%u %c%u", (unsigned)t.attr, subReasonChar(t.reason),
                         (unsigned)(t.notify & 1));
            } else {
                snprintf(detail, sizeof(detail), "%c %c%u", subChar(t.attr, kbHandle, msHandle),
                         subReasonChar(t.reason), (unsigned)(t.notify & 1));
            }
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

// ---------------------------------------------------------------- U135 reads (same as 1e-4)
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

// Returns false if either initial read fails; the caller then treats the unit as
// not found. The knob state at this moment is the reference: a knob held at
// start (or at reconnection) acts only after release and a new press.
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

// 1 detent = 1 key tap. Remainder is kept as is (as Profile 1/3 of the USB builds).
static void onRotation(int16_t delta)
{
    // (M1) MOUSE mode: X movement, 8 HID counts per detent (4 per encoder count).
    if (mouseMode) {
        if (!linkReady) {
            skipped++;
            return;
        }
        mousePendingDx += (int32_t)delta * 4;
        if (mousePendingDx > MOUSE_PENDING_MAX) { mouseClipped += mousePendingDx - MOUSE_PENDING_MAX; mousePendingDx = MOUSE_PENDING_MAX; }
        if (mousePendingDx < -MOUSE_PENDING_MAX) { mouseClipped += -MOUSE_PENDING_MAX - mousePendingDx; mousePendingDx = -MOUSE_PENDING_MAX; }
        return;
    }
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

// TAP only: one Space tap per press; the release sends nothing.
static void onButton(bool pressed)
{
    // (M1) MOUSE mode: knob held = left button (drag). A press while not READY is ignored.
    if (mouseMode) {
        if (pressed && !linkReady) {
            skipped++;
            return;
        }
        uint8_t b = pressed ? 1 : 0;
        if (b != mouseWanted) {
            mouseWanted = b;
            mouseSyncNeeded = true;
        }
        return;
    }
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
            // The knob can no longer report a release (matters for a future HOLD/drag).
            mouseWanted = 0;
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
        lastValue = value;
        onRotation(delta);
        dirty = true;
    }

    // A failed button read keeps the previous state.
    uint8_t button = lastButton;
    if (!readButtonRaw(button)) {
        readErrors++;
        Serial.println("[U135] button read failed");
        dirty = true;
    }
    if (button != lastButton) {
        onButton(button == BUTTON_PRESSED_RAW);
        lastButton = button;
        dirty = true;
    }
}

// ---------------------------------------------------------------- display
static void readNvsStats(nvs_stats_t* st)
{
    nvsStatsOk = (nvs_get_stats(NULL, st) == ESP_OK);  // read-only
}

// (r3) Top-right tag, right-aligned by its measured width so it always fits.
static void drawScreenTag()
{
    auto& d = M5Cardputer.Display;
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(d.width() - d.textWidth(SCREEN_TAG) - 1, 0);
    d.print(SCREEN_TAG);
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
    d.printf("TRACE %u/%u n%u h%s @%lu.%lu lost%lu", uiPage, tracePages(), connCount, hs,
             (unsigned long)(viewAtMs / 1000), (unsigned long)((viewAtMs / 100) % 10),
             (unsigned long)traceLost);
    drawScreenTag();
    int32_t end = (int32_t)viewCount - (int32_t)(uiPage - 1) * TRACE_LINES;
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

    drawScreenTag();  // (r3) right-aligned short tag

    d.setTextSize(2);
    d.setCursor(4, 4);
    switch (linkState) {
        case LINK_READY:
            d.setTextColor(TFT_GREEN, TFT_BLACK);
            d.println("BLE READY");
            break;
        case LINK_ENCRYPTED:
            d.setTextColor(TFT_YELLOW, TFT_BLACK);
            d.println("BLE SUBSCRIBING");
            break;
        case LINK_CONNECTED:
            d.setTextColor(TFT_YELLOW, TFT_BLACK);
            d.println("BLE PAIRING");
            break;
        default:
            d.setTextColor(TFT_CYAN, TFT_BLACK);
            d.println("BLE ADVERTISING");
            break;
    }
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.println(mouseMode ? "MOUSE: X / drag" : "KEYS: R / L / Space");  // (M1) 'm' toggles
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.println(lastEvent);

    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, d.getCursorY() + 4);
    uint16_t h = bleConnHandle;
    if (h == 0xFFFF) {
        d.printf("%s bond=%u kb=%u ms=%u h-\n", BLE_DEVICE_NAME, linkBonded ? 1 : 0,
                 kbSubscribed ? 1 : 0, mouseSubscribed ? 1 : 0);
    } else {
        d.printf("%s bond=%u kb=%u ms=%u h%u\n", BLE_DEVICE_NAME, linkBonded ? 1 : 0,
                 kbSubscribed ? 1 : 0, mouseSubscribed ? 1 : 0, (unsigned)h);
    }
    d.setCursor(4, d.getCursorY());
    d.printf("R%lu L%lu Sp%lu FAIL%lu SKIP%lu%s\n", (unsigned long)txCount[EV_CW],
             (unsigned long)txCount[EV_CCW], (unsigned long)txCount[EV_PRESS],
             (unsigned long)notifyFails, (unsigned long)skipped, releasePending ? " REL!" : "");
    d.setCursor(4, d.getCursorY());
    d.printf("c%lu d%lu u%lu l%lu Q%u W%lu fs%lu ad%lu n%u\n", (unsigned long)bleConnects,
             (unsigned long)bleDisconnects, (unsigned long)linkUps, (unsigned long)linkLosses,
             queueCount, (unsigned long)maxWorkMs, (unsigned long)foreignSubscribes,
             (unsigned long)connAdopted, connCount);
    d.setCursor(4, d.getCursorY());
    if (unitFound) {
        d.printf("U135 OK val=%d btn=%u ERR%lu\n", lastValue, lastButton, (unsigned long)readErrors);
    } else {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.printf("U135 NOT FOUND (retry 1 s) ERR%lu\n", (unsigned long)readErrors);
        d.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    // (M1) Both reports: value handle (FFFF = not in GATT) and CCCD (notify on), mouse reports
    // accepted, Service Changed indications sent. Replaces BLE-1's NVS line.
    // (B1) Value handles of the three reports, read live: keyboard input + CCCD, keyboard
    // output + LED writes, mouse input + CCCD.
    d.setCursor(4, d.getCursorY());
    d.printf("ki%X c%u ko%X w%lu mi%X c%u mr%lu g%lu\n", (unsigned)kbInput->getHandle(),
             kbSubscribed ? 1 : 0, (unsigned)kbOutput->getHandle(), (unsigned long)ledWrites.load(),
             (unsigned)mouseInput->getHandle(), mouseSubscribed ? 1 : 0, (unsigned long)mouseReports,
             (unsigned long)gattChangedSent);
    // (r2) Diagnostics, replacing the static "USB: power + serial only" line.
    d.setCursor(4, d.getCursorY());
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("EV");
    for (uint8_t i = 0; i < TRACE_TOKENS; i++) {
        if (traceTokens[i][0]) d.printf(" %s", traceTokens[i]);
    }
    d.println();
    d.setCursor(4, d.getCursorY());
    if (encSnapCount == 0) {
        d.print("ENC -");
    } else {
        // (r3) "ENC g<gen> <ok|!code> e<enc> i<type><2 bytes> o<type><2 bytes> c<peer>/<peer-kb>/<any-kb> b<bonds>"
        const EncSnapshot e = encSnap;
        char st[12];
        if (e.status == 0) snprintf(st, sizeof(st), "ok");
        else snprintf(st, sizeof(st), "!%lX", (unsigned long)e.status);
        d.printf("ENC g%lu %s e%u i%u%02X%02X o%u%02X%02X c%d/%d/%d b%d", (unsigned long)e.gen, st,
                 e.encrypted ? 1 : 0, e.idType, e.id[1], e.id[0], e.otaType, e.ota[1], e.ota[0],
                 e.cccdPeer, e.cccdPeerKb, e.cccdAllKb, e.bonds);
    }
}

// (r3) Cardputer key 't' (no Fn): status -> trace 1 -> trace 2 ... -> status. The
// trace is frozen when the first trace page opens. Nothing is sent to the host.
static void pollPageKey()
{
    M5Cardputer.update();
    const auto& ks = M5Cardputer.Keyboard.keysState();
    bool tDown = false;
    for (char c : ks.word) {
        if (c == 't' || c == 'T') tDown = true;
    }
    if (tDown && !keyTWasDown) {
        if (uiPage == 0) {
            freezeTrace();
            uiPage = 1;
        } else if (uiPage < tracePages()) {
            uiPage++;
        } else {
            uiPage = 0;
        }
        pageDirty = true;
        Serial.printf("[UI] page %u\n", uiPage);
    }
    keyTWasDown = tDown;

    // (M1) 'm' toggles KEYS / MOUSE; 'g' sends a Service Changed indication.
    bool mDown = false, gDown = false;
    for (char c : ks.word) {
        if (c == 'm' || c == 'M') mDown = true;
        if (c == 'g' || c == 'G') gDown = true;
    }
    if (mDown && !keyMWasDown) {
        mouseMode = !mouseMode;
        accum = 0;
        mousePendingDx = 0;
        if (mouseWanted) {  // never leave the button pressed when the target changes
            mouseWanted = 0;
            mouseSyncNeeded = true;
        }
        snprintf(lastEvent, sizeof(lastEvent), "MODE %s", mouseMode ? "MOUSE" : "KEYS");
        Serial.printf("[M1] mode %s\n", mouseMode ? "mouse" : "keys");
        dirty = true;
    }
    keyMWasDown = mDown;
    if (gDown && !keyGWasDown) {
        if (bleConnected) {
            ble_svc_gatt_changed(0x0001, 0xFFFF);
            gattChangedSent++;
            snprintf(lastEvent, sizeof(lastEvent), "GATT CHANGED SENT");
        } else {
            snprintf(lastEvent, sizeof(lastEvent), "GATT CHG: NO LINK");
        }
        Serial.printf("[M1] service changed indication (%lu)\n", (unsigned long)gattChangedSent);
        dirty = true;
    }
    keyGWasDown = gDown;
}

// ---------------------------------------------------------------- setup / loop
void setup()
{
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);  // never stall the loop when no one reads the CDC port
    delay(300);
    Serial.printf("[BOOT] MediaJog %s\n", BUILD_TAG);  // (r3) full build id in the log (screen shows SCREEN_TAG)

    // Same display / M5 init as the USB builds.
    M5Cardputer.Display.begin();
    auto cfg = M5.config();
    cfg.clear_display = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    cfg.external_imu = false;
    cfg.external_rtc = false;
    cfg.external_display_value = 0;
    cfg.external_speaker_value = 0;
    cfg.serial_baudrate = 0;           // Serial is already started above
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(128);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    // NVS usage before BLE starts (BLE may add bonds and PHY calibration data).
    readNvsStats(&nvsBoot);
    nvsNow = nvsBoot;
    Serial.printf("[NVS] boot: used %u available %u namespaces %u\n",
                  (unsigned)nvsBoot.used_entries, (unsigned)nvsBoot.available_entries,
                  (unsigned)nvsBoot.namespace_count);

    // HID state starts all-up (static initialization); nothing is sent before
    // the link is READY.
    bleBegin();

    sensor.begin(&Wire, ENCODER_ADDR, PORT_SDA, PORT_SCL, I2C_FREQ);
    unitFound = probeUnit();
    Serial.printf("[U135] probe 0x40: %s\n", unitFound ? "ACK" : "no ACK");
    if (unitFound) {
        unitFound = readInitialState();
    }
    lastProbeMs = millis();
    lastNvsStatsMs = millis();
    drawStatus();
}

void loop()
{
    uint32_t now = millis();

    drainTrace();  // (r2/r3) diagnostics
    pollPageKey();  // (r3) 't' cycles status / trace pages (local only)
    if (now - lastConnScanMs >= 250) {
        lastConnScanMs = now;
        uint8_t n = countConnections();
        if (n != connCount) {
            connCount = n;
            dirty = true;
        }
    }
    updateLinkState(now);
    pollUnit(now);
    serviceReports(now);

    if (now - lastNvsStatsMs >= NVS_STATS_INTERVAL_MS) {
        lastNvsStatsMs = now;
        nvs_stats_t st;
        readNvsStats(&st);
        if (nvsStatsOk && memcmp(&st, &nvsNow, sizeof(st)) != 0) {
            nvsNow = st;
            Serial.printf("[NVS] now: used %u available %u namespaces %u\n",
                          (unsigned)st.used_entries, (unsigned)st.available_entries,
                          (unsigned)st.namespace_count);
            dirty = true;
        }
    }

    if (uiPage != 0) {
        if (pageDirty) {  // (r3) trace pages are frozen: drawn once per page
            pageDirty = false;
            drawTracePage();
        }
    } else if ((dirty || pageDirty) && now - lastDrawMs >= DRAW_INTERVAL_MS) {
        dirty = false;
        pageDirty = false;
        lastDrawMs = now;
        drawStatus();
    }

    uint32_t work = millis() - now;
    if (work > maxWorkMs) {
        maxWorkMs = work;
        dirty = true;
    }
    delay(20);  // same poll period as the USB builds
}
