# 第1b段階 本体キーボード追加 — 実装・ビルド・試験手順

作成日：2026-10-12／作成：Claude Opus 5.5
状態：実機動作確認済み（2026-10-12、Mac Chrome／YouTube）
**第1b成功版として保全（ソース・バイナリは変更しない）。** 実機結果は§9、保全は§10。USB初回認識の不安定は継続中で、原因は未確定（§9-2）。

凡例：✅ ソースまたはビルドで確認 / ❓ 未検証（実機で確認が必要）

ソースの場所と版：
- M5Unified 0.2.25：`~/Documents/Arduino/libraries/M5Unified/src/`
- M5GFX 0.2.32：`~/Documents/Arduino/libraries/M5GFX/src/`
- M5Cardputer 1.1.1：`~/Documents/Arduino/libraries/M5Cardputer/src/`
- m5stack:esp32 3.3.9

## 1. 納品物

| ファイル | 内容 |
|---|---|
| `MediaJog_1b/MediaJog_1b.ino` | 新規スケッチ（第1a r2の成功版をコピーして追加実装） |
| `MediaJog_1b/build/MediaJog_1b.ino.bin` | **アプリ単体の `.bin`（LauncherのWi-Fiアップロードで使う）** |
| `MediaJog_1b/build/` のその他のファイル | 直接書き込み用。**使わない** |

| ファイル | SHA-256 |
|---|---|
| `MediaJog_1b.ino.bin`（第1b成功版・保全） | `9377aba837d69677c5d116368bae606a4b8a82eb309f04bbca60ecab8c534443` |
| `MediaJog_1b.ino`（第1b成功版・保全） | `570663df1b478155252e92947e1d7e3258190aa2f04f796c81dc43c46f8b4eef` |
| `MediaJog_1a.ino`（成功版・未変更） | `1578b83a91d5a530511c0d132844740c6a13831421ff50b0c54b5af591349807` |
| `MediaJog_1a.ino.bin`（成功版・未変更） | `57190de1778316665c7b94b92e00c8d77c36b3f59b4f04c999987336031aa8c7` |
| `U135_Probe.ino`（原本・未変更） | `14aaca87d57e018e2eab64a2e183e38c95f4cfee14f29fe597b6b6a0cbbe80fa` |
| `U135_Probe.ino.bin`（原本・未変更） | `0323b74b4e57a826278de6c32be93fbde1ac45dca8bc4b510f5eddaafbcbd375` |

## 2. 動作仕様

| 操作 | 動作 | Macへの送信 |
|---|---|---|
| `Fn` を押しながら `1` | 画面2行目が `PROFILE 1` になる | **送信しない** |
| `Fn` を押しながら `2` | 画面2行目が `PROFILE 2` になる | **送信しない** |
| その他の本体キー | 何もしない（画面下部の `KB keys=` の数だけ変わる） | **送信しない** |
| U135右1目盛り／左1目盛り／押し込み | 第1aと同じ。`.`（0x37）／`,`（0x36）／`k`（0x0E）。**PROFILE 1と2で違いはない** | 第1aと同じ |

- 起動時は `PROFILE 1`。電源を切ると選択は保持されない（保存は第2段階の範囲）。
- 同時押しを検出した瞬間に1回だけ切り替える。押し続けても繰り返さない。
- `Fn+1` と `Fn+2` を同時に押した場合は、どちらとも判定しない。
- `Fn+Shift+1` は `!` として読まれるため反応しない（ライブラリの仕様。§5-2）。
- U135の読み取りとUSB HID送信のコードは、第1a r2から**一切変更していない**（`diff` で確認）。

### 画面表示

```
USB HID READY            ← 文字サイズ2（第1aと同じ色分け）
PROFILE 1                ← 文字サイズ2、シアン（1bで追加）
VAL 124 R+0
.12 ,8 k3
TX .                     ← 最後のイベント（黄）
BTN raw=1  Q0/3 OVF0     ← ここから文字サイズ1（1aでは文字サイズ2）
FAIL0 SKIP0 ERR0
W14 T3
KB keys=0 fn=0 chg=0     ← 本体キーの押下数、Fnの状態、プロファイル切替の回数（1bで追加）
```

`KB` 行は、キーボードが応答しているかを実機で切り分けるための表示。ライブラリは初期化失敗を戻り値で返さない（§5-2）。

## 3. Arduino IDEの設定とインストール方法

第1aと同じ設定：ボード `M5Cardputer`（m5stack:esp32 3.3.9）、**USB Mode = USB-OTG (TinyUSB)**、**USB CDC On Boot = Enabled**。違う値にすると `#error` でビルドが止まる（確認済み）。

```bash
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile --fqbn "m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc" --output-dir MediaJog_1b/build MediaJog_1b
```

**インストールは第1aで成功した方法を維持する：** LauncherのWi-Fiアップロードで `MediaJog_1b.ino.bin`（アプリ単体）を転送する。IDEの「書き込み」やmerged.binは使わない（Launcherを上書きするため）。

## 4. コンパイル結果（2026-10-12）

| 項目 | 結果 |
|---|---|
| USB-OTG ＋ CDC On Boot=Enabled | ✅ 成功（exit 0）。フラッシュ 595,435 B（45%）、RAM 59,956 B（18%） |
| `--warnings all` でのスケッチ由来の警告 | ✅ 0件 |
| Hardware CDC and JTAG で試しにビルド | ✅ `#error` で停止 |
| CDC On Boot=Disabled で試しにビルド | ✅ `#error` で停止 |
| アプリ単体 `.bin` のサイズ | 595,584 B（1aは523,392 B。`M5.begin()` とキーボードのコード分、約72 KB増） |

❓ Launcherのアプリ領域に収まるか（1aの523 KBは動作した）。

---

## 5. ソース確認：キーボード初期化の影響

### 5-1. 初期化の順序と、`M5.begin()` が実際に行うこと

`setup()` の順序は次のとおり。

1. `Serial.begin(115200)`（第1aと同じ）
2. **`M5Cardputer.Display.begin()`**（第1aと同じ。ここでボード判定と画面初期化が行われる）
3. **`M5Cardputer.begin(cfg, true)`**（1bで追加。内部で `M5.begin(cfg)` と `Keyboard.begin()` を呼ぶ）
4. `setRotation(1)`、`setBrightness(128)`、`fillScreen`（第1aと同じ）
5. USB HIDの開始（第1aと同じ）
6. `sensor.begin(&Wire, 0x40, SDA=G2, SCL=G1, 200kHz)`（第1aと同じ。`M5.begin()` の**後**）

**手順2を手順3より先に置いた理由（ソースで確認）：**

| 項目 | ソース | 結果 |
|---|---|---|
| 画面の二重初期化 | `M5GFX.cpp:1029-1034`（`init_impl`）：ボードが判定済みなら即 `return true` | `M5.begin()` 内の `Display.init()` は**何もしない**。画面初期化は第1aと同じ1回だけ ✅ |
| GPIO46 | `M5Unified.hpp:362-367`：`Display` が未判定のときだけGPIO46をHIGH出力にする。ADVには電源保持ピンがなく（`M5Unified.inl:285-293` の表）、元に戻す対象にも入っていない | 手順2を先にすることで、**GPIO46（ADVではマイクのデータ入力）を駆動しない** ✅ |
| 輝度 | `M5Unified.hpp:371-372, 684`：一時的に0にし、最後に元へ戻す | 手順4で128に設定し直す。`M5.begin()` の間、画面が一瞬暗くなる可能性がある（❓見た目のみ） |

**`M5.begin(cfg)` がADVで行う処理**（cfgは、本スケッチが使わない機能をすべて無効にしたもの）：

| 処理 | ソース | ADVでの内容 | U135／LCD／USBへの影響 |
|---|---|---|---|
| ボード判定 | `M5Unified.hpp:378-383` | `Display.getBoard()` からADVと判定 | なし |
| `_setup_i2c` | `M5Unified.inl:2125-2165`、ピン表 `:110` | **内部I²C：`In_I2C.begin(I2C_NUM_1, SDA=G8, SCL=G9)`**。外部：`Ex_I2C.setPort(I2C_NUM_0, G2, G1)` は**設定の記録だけで、開始しない** | port 0（U135）には触れない ✅ |
| In_I2Cの実体 | M5GFX `common.inl:2079-2085` | Arduino環境では `(i2c_port == 0) ? &Wire : &Wire1` → **`Wire1.begin()`（G8/G9）** | `Wire`（U135）とは別のI²Cコントローラ ✅ |
| `Power.begin()` | `Power_Class.inl:575-582` | 電池電圧のADC（G10）を設定するだけ | なし |
| `setExtOutput` | `Power_Class.inl` | Cardputer／ADV用の処理なし | なし |
| `clear_display` | `M5Unified.inl:2322-2325` | `false` に設定 → 画面を消去しない | なし |
| ボード別のピン設定 | `M5Unified.inl:2449-2451` | **G0を入力にするだけ** | なし |
| `serial_baudrate` | `M5Unified.inl:2589-2593` | 0 → `Serial.begin()` を呼ばない | CDCシリアルは第1aのまま |
| 音声 `_begin_audio` | `M5Unified.inl:2599-3307` | `internal_spk`／`internal_mic`=false。もともと設定値を記録するだけで、`Speaker.begin()`／`Mic.begin()` を呼ばない限りI²Sもコーデックも動かない（本スケッチは呼ばない） | なし |
| `update()` | `M5Unified.inl`（`update` のADV分岐） | **G0を読むだけ** | なし |
| RTC／IMU `_begin_rtc_imu` | `M5Unified.inl:3377-3410` | 内部・外部ともfalse → 何もしない。`Ex_I2C.begin()` は外部RTC／IMUが有効なときだけ | port 0に触れない ✅ |
| 外部ディスプレイの検出 | `M5Unified.hpp:500-600` | `#if defined(__M5GFX_M5UNITOLED__)` などで囲まれており、対応するM5GFXのヘッダーを読み込んだときだけコンパイルされる。本スケッチは読み込まず、`external_display_value=0` も指定 | Port A（G2/G1）を検出に使わない ✅ |

**`M5.begin()` とキーボードが触るGPIO（ADV）：** G0（入力）、G8/G9（内部I²C）、G10（ADC）、G11（キーボードの割り込み入力）。U135（G1/G2）、LCD（G33〜G38）、USB（G19/G20）とは重ならない。✅

### 5-2. `Fn+1`／`Fn+2` の検出方法（ADVでの実装）

| 段階 | ソース | 内容 |
|---|---|---|
| 読み取り方式の選択 | `Keyboard.cpp:23-24` | `M5.getBoard() == board_M5CardputerADV` のとき `TCA8418KeyboardReader` を使う。**このために `M5.begin()` が必要** |
| TCA8418 | `Adafruit_TCA8418.h:74`、`TCA8418.cpp:13, 39, 45` | `m5::In_I2C`（I2C_NUM_1）、アドレス0x34、400kHz、`matrix(7, 8)`、割り込みはG11（CHANGE） |
| イベント処理 | `TCA8418.cpp:52-75` | 割り込みフラグが立っているときだけ読み、**1回の `update()` で1イベント**を処理する。押されているキーの一覧を更新する |
| 座標変換 | `TCA8418.cpp` の `remap()` | ADVの7×8マトリクスを、初代Cardputerと同じ4×14配置に変換する |
| Fnの判定 | `Keyboard.cpp:119`（1回目の走査） | 値が `KEY_FN`（0xFF）のキーがあれば `keysState().fn = true` |
| 文字の判定 | `Keyboard.cpp:202-207`（2回目の走査） | Ctrl／Shift／CapsLockのときだけ第2文字（`!`、`@`）、それ以外は第1文字（`1`、`2`）を `word` に入れる。**Fnは文字を変えない** |
| キー配置 | `Keyboard.h:18-20, 46` | 1行目に `'1'`、`'2'`、3行目の先頭に `KEY_FN` |

**本スケッチの判定：** `fn == true` かつ `word` に `'1'` だけが含まれる → 1、`'2'` だけが含まれる → 2、それ以外 → 0。値が変化して1または2になった瞬間に切り替える（`pollKeyboard()`）。

**限界（ソースから）：**

- `Fn` と `1` は2つのイベントなので、反映までに最低2ループ（約40ms）かかる。
- TCA8418のハードウェアFIFO（イベント10件）はキーを多数押した場合に溢れる可能性があるが、2キー操作では問題にならないと考えられる（❓）。
- 初期化に失敗してもライブラリは `printf` するだけで、戻り値はない。そのため、画面の `KB keys=` が反応するかで確認する。

### 5-3. 本体キーがMacへ送られないこと

- 本スケッチでUSB HIDのレポートを作るのは、`pushEvent(EV_CW／EV_CCW／EV_PRESS)` からの経路だけ。`pushEvent` を呼ぶのは `pollUnit()`（U135）だけ。✅
- `pollKeyboard()` は `profile` などの表示用変数を変えるだけで、`pushEvent`、`hidOut`、`UsbKeyboard` を一切呼ばない。✅
- M5Cardputer公式の `usbKeyboard.ino` は本体キーをHIDに転送する例だが、本スケッチはその転送処理を含まない。
- USB HIDの構成（インターフェース1つ、Report ID 1）は第1aと同じ（`STAGE_1A.md` §5①）。

### 5-4. I²Cの共存

| バス | コントローラ | ピン | 使用者 | 速度 |
|---|---|---|---|---|
| 外部（Grove） | I2C_NUM_0、Arduino `Wire` | G2/G1 | U135（本スケッチ） | 200kHz |
| 内部 | I2C_NUM_1、Arduino `Wire1`（M5GFXの `lgfx::i2c` 経由） | G8/G9 | TCA8418（M5Cardputer） | 400kHz |

- 別々のコントローラと別々のピンなので、バス上では干渉しない（ソース上）。✅
- どちらも `loop()` のタスクから順番に使われ、並行アクセスはない。
- 本スケッチは `Wire1` を直接使わない。SOUND TERRARIUMが `Wire1` を使っていたこととは無関係（別のアプリ）。

### 5-5. 未検証事項（実機で確認）→ 2026-10-12の実機テスト後の状態

| 項目 | 状態 |
|---|---|
| `M5Cardputer.begin()` を追加しても、LCDが第1aと同じように表示されるか | ✅ 実機で `PROFILE 1`／`PROFILE 2` の表示切替を確認（表示は正常に機能） |
| キーボードを使っている最中に、U135の `ERR` が増えないか | 未記録（`ERR` の値は報告されていない。両プロファイルでのYouTube操作は成功） |
| `Fn+1`／`Fn+2` がADV実機で検出されるか | ✅ 実機で確認 |
| 本体キーを押してもMacに文字が入力されないこと | ✅ 実機で確認 |
| `W`（1ループの処理時間）の増分 | 未記録 |
| Launcherのアプリ領域に595 KBが収まるか | ✅ LauncherのWi-Fiアップロードで導入・起動できた |

---

## 6. 実機テスト手順（エドガーのレビュー後。LauncherのWi-Fiアップロード）

### 事前準備
- [ ] エドガーのレビューが完了している。
- [ ] 第1aの成功版（`MediaJog_1a.ino.bin`）を、すぐに戻せる状態でLauncher側またはSDに残しておく（上書きしない）。

### 手順
1. LauncherのWi-Fiアップロードで `MediaJog_1b/build/MediaJog_1b.ino.bin` を転送し、起動する。
   - **画面が点かない場合は中止**し、§7へ。
2. **USBを挿す前に**画面を確認する。
   - 期待：`USB NOT MOUNTED`、`PROFILE 1`、`VAL`、`ERR0`。
   - U135を外している場合は `U135 NOT FOUND`。
3. **キーボードの単体確認（USB未接続のまま）**
   - 本体の任意のキーを押す → 下部の `KB keys=1` になる。離すと0。
   - Fnを押す → `fn=1`。
   - `Fn+2` → `PROFILE 2`、`chg=1`。`Fn+1` → `PROFILE 1`、`chg=2`。
   - Fnを押したまま `2` を押し続ける → 切替は1回だけ。
4. MacとUSB接続する → `USB HID READY`。
5. **第1aの回帰確認：** ChromeのYouTubeで、右（コマ送り）、左（コマ戻し）、押し込み（再生／停止）を、`PROFILE 1` と `PROFILE 2` のそれぞれで確認する。
6. **本体キーがMacに送られないことの確認：** Macのテキストエディットにフォーカスし、本体の文字キー、`Fn+1`、`Fn+2`、Enter、Space、Tabなどを押す。
   - 期待：**何も入力されない**。
   - 続けてU135を回すと `.`／`,` が入力される（ABC入力の場合）。
7. **共存の確認：** U135を回しながら本体キーを押す、`Fn+1`／`Fn+2` を交互に20回、などを数分間続ける。
   - 期待：`ERR0`、`FAIL0`、画面が乱れない、Macに余計な入力がない。
   - 終了時の `W`／`T` を記録する。
8. 画面の写真と、`ERR`／`FAIL`／`OVF`／`SKIP`／`W`／`T`／`chg` を記録する。

## 7. 復旧手順（Launcherを上書きしない範囲）

| 症状 | 対処 |
|---|---|
| 画面が点かない、または操作できない | 電源OFF→ON。Launcherに戻り、**第1a成功版を起動**する |
| Macでキーが押しっぱなしになる | USBケーブルを抜く |
| 本体キーでMacに文字が入力された | **直ちにテスト中止**（設計上は起きないはず）。状況を記録する |
| Launcherに戻れない | 作業を止めて相談する（G0ダウンロードモードでのLauncher再書き込みは今回の範囲外） |

## 8. レビューで確認してほしい点

1. 初期化順序：`Display.begin()` → `M5Cardputer.begin(cfg)` → `sensor.begin(&Wire…)`。特に、`Display` を先に初期化してGPIO46を駆動させない判断（公式例は `M5Cardputer.begin()` だけを呼ぶ）。
2. `cfg` で無効にした項目（IMU、RTC、スピーカー、マイク、外部ディスプレイ、`clear_display`、`serial_baudrate`）が、キーボードの動作に不要であること。
3. 同時押しを検出した瞬間だけ切り替える判定と、`Fn+1`／`Fn+2` の同時押しを無視する扱い。
4. 診断表示を文字サイズ1に変えた画面レイアウト（第1aでは文字サイズ2）。

---

## 9. 実機テスト結果（2026-10-12）

### 9-1. 結果

| 項目 | 結果 |
|---|---|
| 導入方法 | LauncherのWi-Fiアップロード（第1aと同じ。Launcherは上書きしていない） |
| `Fn+1`／`Fn+2` によるプロファイル切替 | ✅ 成功（`PROFILE 1`／`PROFILE 2` の表示が切り替わる） |
| PROFILE 1でのYouTube操作（コマ送り・コマ戻し・再生／停止） | ✅ 成功 |
| PROFILE 2でのYouTube操作（同上） | ✅ 成功 |
| 本体キーがMacに入力されないこと | ✅ 確認 |
| `ERR`／`FAIL`／`OVF`／`SKIP`／`W`／`T` の値 | 未記録 |
| 長時間の連続操作（§6 手順7） | 未記録 |

**判定：第1bの合格条件（画面・U135・HIDの共存、Fn操作で表示上の選択状態が切り替わる、YouTube操作の維持、本体キーの非送信）を満たした。**

### 9-2. 継続している問題：USB初回認識の不安定（原因未確定）

- **現象：** MacとのUSB接続で、最初の認識が安定しない。第1aでも、最初は `USB NOT MOUNTED` と表示され、最終的に `USB HID READY` になった経緯がある（REPORT.md §13）。第1bでも継続している。
- **原因：未確定。** 以下は切り分け候補であり、どれも検証していない。
  - Launcherからアプリへ切り替える際のUSB周辺回路の状態
  - ケーブル、ハブ、Mac側のポート
  - 接続の順序（アプリ起動後にUSBを挿す／挿したまま起動する）
  - macOSでの初回キーボード認識の処理
  - TinyUSBの列挙のタイミング
- **次回、発生時に記録したい情報：**
  - 接続の順序
  - 画面1行目の表示（`USB NOT MOUNTED`／`USB SUSPENDED`／`USB HID READY`）
  - READYになるまでの時間と、そのために行った操作（抜き差しの回数など）
  - 使用したケーブルとポート
  - Macの「システム情報」→「USB」にデバイスが表示されているか
- この問題のためのコード変更は、第1b成功版には行わない。

## 10. 第1b成功版の保全

次の2ファイルを第1b成功版とし、**変更・上書き・再ビルドで置き換えない。** 再ビルドは別フォルダで行う。

| ファイル | サイズ | SHA-256 |
|---|---|---|
| `MediaJog_1b/MediaJog_1b.ino` | 19,292 B | `570663df1b478155252e92947e1d7e3258190aa2f04f796c81dc43c46f8b4eef` |
| `MediaJog_1b/build/MediaJog_1b.ino.bin`（アプリ単体） | 595,584 B | `9377aba837d69677c5d116368bae606a4b8a82eb309f04bbca60ecab8c534443` |

- 同じ値を `MediaJog_1b/SHA256SUMS.txt` にも記録した。照合は、プロジェクトのフォルダで `shasum -a 256 -c MediaJog_1b/SHA256SUMS.txt` を実行する。
- 上記の値は、2026-10-12 11:56にビルドしてリポジトリ内に置いたファイルのもの。実機へアップロードしたファイルが別の場所のコピーだった場合は、そのコピーのSHA-256もこの値と一致するか確認する。
- `MediaJog_1b/build/` のその他のファイル（`.elf`、`.map`、`merged.bin` など）は保全対象外（ビルドの副産物）。
