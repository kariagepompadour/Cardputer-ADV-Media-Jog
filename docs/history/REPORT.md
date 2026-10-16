# Cardputer ADV + Unit Encoder U135 動作検証 — 調査報告（GPTレビュー用）

作成日: 2026-10-11（v2: GPT一次レビュー後の追加調査を反映）／追補: 2026-10-12

**文書の読み方：§1〜§9は2026-10-11時点の「実機テスト前」調査の原記録であり、当時の未検証表示やチェックリストは履歴として残す。最新の実機結果とOpus 5.5の追加調査は末尾の§10〜§12を参照。**

凡例: ✅ 公式資料または公式ソースで確認 / ⚠️ 資料間で矛盾 / ❓ 未確認

---

## 1. 調査報告（第1段階）

### 1-1. U135 の通信仕様

| 項目 | 内容 | 根拠 |
|---|---|---|
| 通信方式 | I2C、アドレス 0x40 ✅ | https://docs.m5stack.com/en/unit/encoder（"I2C @0x40"） |
| 内蔵MCU | STM32F030F4P6 ✅ | 同上 |
| 電源 | DC 5V @ 17mA ✅ | 同上（"Encoder operation (DC 5V@17mA)"） |
| ケーブル配線 | 黒 GND / 赤 5V / 黄 SDA / 白 SCL ✅ | 同上 |
| レジスタ 0x00 | Mode（W）: 0=Pulse, 1=AB、既定 0 ✅ | https://github.com/m5stack/M5UnitEncoder_Firmware の docs/M5UnitEncoder_Register_Map.jpg |
| レジスタ 0x10 | カウンタ（R/W）: 2バイト、L, H の順（int16 リトルエンディアン）✅ | 同上、および Protocol PDF |
| レジスタ 0x20 | ボタン（R）: 0 または 1 ✅ | 同上 |
| レジスタ 0x30 | RGB LED（W）: index, R, G, B ✅ | 同上 |
| レジスタ 0x40 | カウンタリセット（W）✅ | 同上（Firmware Register Map のみに記載） |
| **ボタンの極性** | ⚠️ **矛盾あり** | 下記参照 |
| I2C クロック上限 | ❓ 公式資料に記載なし | — |
| SDA/SCL プルアップ | ✅ **R6/R7 10kΩ → +3.3V**（5V から HT7533 で生成した 3.3V）。詳細は §8 | U135 公式回路図 encoder_sch_01.webp |
| 1回転あたりのカウント数 | 公式ページは「30パルス/回転」と記載。Pulse モードで 1 クリックあたり何カウントになるかは ❓ | docs ページ |

**⚠️ ボタン極性の矛盾**

- Protocol PDF（https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/801/Unit-Encoder_Protocol.pdf、FW V1, 2022/5/13）は「0: Released / 1: Pressed」と記載。
- 公式サンプル（M5Unit-Encoder `Unit_Encoder_M5Core2.ino`）は `if (!btn_status)` で LED を紫に点灯させており、0 を「押下」とみなしているように読める。
- → 本スケッチは極性を決め打ちしない。**生値（raw）の変化だけを表示**し、どちらが押下かは実機で確定する。

### 1-2. Cardputer ADV の外部 Grove 端子

| 項目 | 内容 | 根拠 |
|---|---|---|
| コネクタ | HY2.0-4P、公式表記は "PORT.CUSTOM" ✅ | https://docs.m5stack.com/en/core/Cardputer-Adv |
| ピン | 黒 GND / 赤 5V / 黄 G2 / 白 G1 ✅ | 同上 |
| SDA / SCL の割当 | **G2 = SDA、G1 = SCL** ✅ | M5Unified 0.2.25 `M5Unified.inl:110`（`board_M5CardputerADV` の EX SCL=G1, SDA=G2）、ボード定義 `variants/m5stack_cardputer/pins_arduino.h`（SDA=2, SCL=1）。U135 側の黄=SDA / 白=SCL とも一致 |
| ポート 5V の出力電流上限 | ❓ 公式資料に数値なし（昇圧 IC SY7088 の定格は未確認） | — |
| ポート 5V の供給元 | ⚠️ 回路図では **SW2（SPDT）で +5VOUT（電池昇圧、EN=IN で常時ON）と +5VIN（USB）を切り替える**。ソフトでの有効化は不要。SW2 の物理的な位置や操作方法は ❓。詳細は §8 | Cardputer-Adv 回路図 v1.0 p.1 |

### 1-3. 内部バス（衝突回避のための確認）

| 項目 | 内容 | 根拠 |
|---|---|---|
| 内部 I2C | G8 = SDA、G9 = SCL。キーボード TCA8418（0x34）、IMU BMI270、Codec ES8311 が接続 ✅ | Cardputer-Adv 公式ページ、M5Unified.inl:110、SOUND TERRARIUM のコード |
| LCD | G33 RST / G34 RS / G35 DAT / G36 SCK / G37 CS / G38 BL ✅ | 同上、M5GFX.cpp:2617-2715 |
| ⚠️ ボード定義の注意点 | `pins_arduino.h` の `IN_SCL=8 / IN_SDA=9` は公式表（SDA=G8, SCL=G9）と**逆**。本スケッチはこのマクロを使わない | `variants/m5stack_cardputer/pins_arduino.h:33-34` |

### 1-4. 使用する I2C バスと初期化方法

- Arduino の `Wire`（= I2C_NUM_0）を **G2 = SDA、G1 = SCL** で使う。
- 内部バス（G8/G9）は M5Unified では I2C_NUM_1、SOUND TERRARIUM では `Wire1` が使っている。**本スケッチは I2C_NUM_1 と `Wire1` に一切触れない。**
- 画面の初期化は、この実機で動作実績がある SOUND TERRARIUM の `setup()` と同じ4行に揃えた（`M5Cardputer.begin()` と `M5.begin()` は呼ばない）。
  ```cpp
  M5Cardputer.Display.begin();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  ```
- M5GFX の自動判定（`M5GFX.cpp:2617-2715`）は、ADV では G5/G6/G8/G9 の状態を読み取って ADV と判定する。判定の途中で G2/G3 に PaperColor 用の I2C プローブ（アドレス 0x32/0x44 ほか）を出すが、U135 の SCL（G1）は駆動されない。また 0x40 は G5/G6 側の VAMeter プローブでしか使われない。→ U135 を接続していても誤判定や干渉は起きないと**ソースからは判断できる**（実機では未確認）。

### 1-5. 公式ライブラリの対応機種と制約

- M5Unit-Encoder：`library.properties` の `architectures=esp32`。サンプルは M5Core と M5Core2 のみ。**Cardputer（ADV）向けの公式サンプルは存在しない**（❓ 動作実績も公式には未確認）。
- `begin()` は内部で `Wire.begin(sda, scl, 200000)` を呼ぶだけ。ユニットへは何も書き込まない。
- `readBytes()` は `requestFrom` の戻り値を確認しない。ユニット未接続時は 0xFF（値 -1、ボタン true）が返り、エラーにはならない。→ スケッチ側でアドレス 0x40 の ACK を確認してから読み出す。
- 旧URL github.com/m5stack/UNIT_ENCODER は M5Unit-Encoder へリダイレクトされる（同一リポジトリ）。

### 1-6. Cardputer ADV との組合せで報告されている既知の問題

- ❓ 公式 Issue、フォーラム、検索のいずれでも、ADV と U135 の組合せに関する報告は見つからなかった。
- 参考（非公式）：Reddit r/CardPuter に「Grove は GPIO1=SCL / GPIO2=SDA」との回答あり（https://redlib.hbubli.cc/r/CardPuter/comments/1kwgdb3/sensors_question）。公式ソースの割当と一致する。
- **前回の失敗（画面が点灯しなくなった）の原因は未確定のまま。** 後述 §7 に候補を挙げるが、いずれも検証はしていない。

---

## 2. 採用した既存コードと選定理由（第2段階）

| 項目 | 内容 |
|---|---|
| 元リポジトリ | https://github.com/m5stack/M5Unit-Encoder（commit 8f45080, 2025-11-07） |
| 元ファイル | `examples/Unit_Encoder_M5Core2/Unit_Encoder_M5Core2.ino` |
| 対象ハードウェア | M5Stack Core2（ESP32） |
| ライブラリ | M5Unit-Encoder 0.0.2（ローカル版と GitHub HEAD は `setWorkMode()` の有無だけが違う。本スケッチは未使用なので影響なし） |
| 画面初期化の出典 | SOUND TERRARIUM `SoundTerrarium.ino` の `setup()`（この Cardputer ADV で動作実績あり） |

**選定理由**：U135 用に M5Stack が公開している Arduino ライブラリとサンプルはこれだけ。I2C の読み出し処理（0x10 / 0x20）はライブラリをそのまま使う。

**Cardputer ADV 向けの変更点と理由**

| # | 変更 | 理由 |
|---|---|---|
| 1 | `#include <M5Core2.h>` と `M5.begin(...)` → `M5Cardputer.Display` の4行初期化 | Core2 用 API は ADV では使えない。この実機で実績のある初期化に揃えた |
| 2 | `sensor.begin()` → `sensor.begin(&Wire, 0x40, 2, 1, 200000)` | ピンを明示し、ボード定義のマクロへの依存をなくした（値はマクロと同じ）。速度はライブラリ既定のまま |
| 3 | 起動時と 500ms ごとに 0x40 の ACK を確認 | ライブラリには未接続を検出する手段がないため |
| 3b | （v2）読み出しを `readRegChecked()` に置き換え。手順はライブラリの `readBytes()` と同じで、戻り値の確認だけを追加 | 通信失敗時の 0xFF が正常値として扱われるため（§9） |
| 4 | `setLEDColor()` を全削除 | 今回の対象外。ユニットへの書き込みをゼロにするため |
| 5 | M5Canvas スプライトをやめ、直接描画にした（変化時のみ） | 最小構成にするため。メモリ確保に失敗する経路もなくなる |
| 6 | 回転方向の判定を int16 の差分（ラップ対応）で ± を判定 | 回転の検出に必要。どちらが右回転かは実機で確定する |
| 7 | ボタンは生値の変化だけを検出 | §1-1 の極性矛盾があるため |
| 8 | 各イベントを Serial に出力 | 画面が点かない場合でも状況を確認できるようにするため |

差分の全文：`diff_from_official_example.patch`（公式サンプル → v2）、`diff_v1_to_v2.patch`（前回レビュー版 → v2）

---

## 3. 最小構成スケッチ

`U135_Probe/U135_Probe.ino`（ユニットへの書き込みなし。読み出すのは 0x10 と 0x20 のみ）

画面の表示内容：
- `VAL n`：カウンタの生値
- `BTN raw=0/1`：ボタンの生値
- `+ n - n`：回転イベントの回数
- `BTN chg n`：ボタン生値が変化した回数
- `ERR n`：読み出しに失敗した回数（v2 で追加）
- 最後のイベント：`ROTATE +` / `ROTATE -` / `BTN raw 0->1` など
- 未接続時：赤字で `U135 NOT FOUND` を表示し、1秒ごとに再検出を試みる

## 4. ライブラリ・ツールのバージョン（コンパイルで実際に使われたもの）

| 名前 | バージョン |
|---|---|
| ボードパッケージ m5stack:esp32 | 3.3.9 |
| ボード | M5Cardputer（`m5stack:esp32:m5stack_cardputer`）、メニューは既定値 |
| M5Unit-Encoder | 0.0.2 |
| M5Cardputer | 1.1.1 |
| M5Unified | 0.2.25 |
| M5GFX | 0.2.32 |
| Wire | 3.3.9（コア同梱） |
| arduino-cli | 1.5.1（Arduino IDE 同梱） |

---

## 5. 実機テスト前の検証結果（第4段階）

| 項目 | 結果 |
|---|---|
| ライブラリと API の整合 | ✅ `Unit_Encoder::begin(TwoWire*, uint8_t, uint8_t, uint8_t, uint32_t)`、`getEncoderValue()`、`getButtonStatus()` をソースで確認 |
| コンパイル | ✅ **v2 で再実施・成功**（exit 0）。フラッシュ 455,931 B（34%）、RAM 26,404 B（8%）。警告はコアとバリアントのマクロ再定義のみで、スケッチやライブラリ由来の警告はない |
| GPIO 競合 | ✅ 使用するのは G1/G2（Wire）と LCD（M5GFX が管理）のみ。G8/G9、I2S（G41-46）、SD、G0、G38 には触れない |
| I2C 初期化 | ✅ I2C_NUM_0 のみ使用。M5Unified の In_I2C（I2C_NUM_1）は初期化されない（`M5.begin()` を呼ばないため） |
| 電源仕様 | ⚠️ U135 は 5V で 17mA。ソフトでの有効化は不要（回路図で確認）。電流上限は ❓、SW2 の状態も ❓（§8） |
| I2C の電圧レベル | ✅ 両側とも 3.3V プルアップ。ESP32-S3 の入力定格の範囲内（§8） |
| 実機動作 | ❓ **未テスト**（レビュー完了まで実施しない方針のため） |
| 回転方向（+ が右回転か） | ❓ 実機で確定する |
| ボタン極性 | ❓ 実機で確定する |

---

## 6. 実機テスト前チェックリスト

**書き込み前**
- [ ] GPT の独立レビューが完了している
- [ ] 復旧用の `SoundTerrarium.ino.bin` が SD カードに残っている（Mac 側 `~/Downloads/SoundTerrarium.ino.bin`, SHA-256 `ce45d70a…cfe921`）
- [ ] **USB で書き込むと、現在フラッシュにある SOUND TERRARIUM（またはランチャー）は上書きされる**ことを了承済み。SD 上の .bin とソースは変更されない。SD ランチャーを使う場合は `U135_Probe.ino.bin`（アプリ単体）を SD に置いて起動する方法もある（ランチャーの種類は ❓）
- [ ] Arduino IDE の設定：ボード「M5Cardputer」（M5Stack 3.3.9）、USB Mode = Hardware CDC and JTAG、USB CDC On Boot = Enabled（いずれも既定値）
- [ ] （v2）**接続前に**、本体の外観で SW2（Grove 5V の切替）に相当するスイッチの有無と位置を確認する。分からなければ「未確認」のまま進め、`NOT FOUND` になった場合の切り分け対象にする
- [ ] まず **U135 を外した状態で**書き込み、赤字の `U135 NOT FOUND` が表示されることを確認（画面初期化の切り分け）

**接続後**
- [ ] 電源 OFF の状態で Grove ケーブルを接続（黄=G2、白=G1 の向きは HY2.0 コネクタで固定）
- [ ] 電源 ON → 緑の `VAL` 表示になるか
- [ ] 右に1クリック回す → `ROTATE +` と `ROTATE -` のどちらが出るかを記録
- [ ] 左に1クリック回す → 逆の表示が出ること
- [ ] 押す/離す → `BTN raw` がどう変化するかを記録（押下中の値で極性を確定）
- [ ] シリアルモニタ（115200）でも同じ内容が出ているか

## 7. 想定される失敗原因と復旧手順

**失敗原因の候補（いずれも未検証）**

| 症状 | 候補 |
|---|---|
| 画面が点かない（U135 なしでも） | ボード設定の違い／ライブラリ版の違い／書き込みの不完全 → シリアル出力が出ているかで切り分ける |
| `NOT FOUND` | ケーブルの接触不良、ポート 5V が出ていない（**SW2 が +5VIN 側で、USB 非接続**の可能性。§8）、SDA/SCL の逆配線（非純正ケーブル） |
| `ERR` が増え続ける | 配線の接触不良やノイズ。ケーブルを交換して確認する |
| 値が変化しない | Mode レジスタの状態（本スケッチは書き込まないので既定 0 のはず） |
| 前回の消灯の原因候補 | `IN_SDA/IN_SCL` マクロ（逆定義）の使用、G8/G9 を Wire で再初期化した、G38 を操作した、など。**どれも確認できていない** |

**復旧手順（起動不能時）**
1. 電源スイッチを OFF にする。
2. **G0 ボタンを押したまま** USB を接続し（または電源 ON にし）、その後ボタンを離す → ダウンロードモードに入る（公式 Cardputer-Adv ページの記載）。
3. Arduino IDE でポートを選び、既知の良好なスケッチを書き込む。または SD ランチャーから SoundTerrarium.ino.bin を起動する。
4. フラッシュ全体を書き戻す場合は `SoundTerrarium` をソースから再ビルドし、IDE で書き込む（ソースは `~/Documents/GitHub/SOUND-TERRARIUM`、今回は変更していない）。

---

## 未解決事項（レビューで確認してほしい点）
1. ボタン極性（PDF と公式サンプルの矛盾。回路図のピン部分は「押下 = Low」だが、レジスタ値との対応は ❓）
2. ADV ポートの 5V 出力電流上限、および SW2 の物理的な位置と現在の状態
3. ~~U135 の I2C プルアップ電圧~~ → v2 で解決（3.3V）
4. 回転方向の符号
5. 前回の故障様症状の原因

---

# 追加調査（v2：GPT一次レビュー後）

## 8. U135 の I²C 信号電圧と、Cardputer ADV との電気的互換性

### 8-1. U135 側（根拠：公式回路図 https://static-cdn.m5stack.com/resource/docs/products/unit/encoder/encoder_sch_01.webp、写し：`evidence/U135_schematic.png`）

| 項目 | 回路図の記載 |
|---|---|
| Grove コネクタ J1（HY-2.0_IIC） | 1 = IIC_SCL、2 = IIC_SDA、3 = VCC（**+5V**）、4 = GND |
| SCL のプルアップ | **R6 10kΩ → +3.3V** |
| SDA のプルアップ | **R7 10kΩ → +3.3V** |
| +3.3V の生成 | U1 **HT7533**（LDO）：VIN = +5V → VOUT = +3.3V |
| SDA/SCL の接続先 | STM32F030F4P6 の PA10（SDA）、PA9（SCL）に直結。MCU の VDD は +3.3V。レベル変換 IC はない |
| 5V を使う部品 | SK6812 LED（VDD = +5V）と HT7533 の入力のみ |
| ボタン（参考） | BTN 端子は R4 10kΩ で +3.3V にプルアップされ、スイッチで GND に落ちる → **ピン上では押下 = Low**。ただし STM32 ファームウェアがレジスタ 0x20 に反転して格納するかは ❓（ファームウェアは .hex のみ公開。ソースは未公開） |

→ **U135 の SDA/SCL は 3.3V 系。5V にプルアップされる経路は回路図上にない。** ✅

### 8-2. Cardputer ADV 側（根拠：公式回路図 https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1178/Sch_M5CardputerAdv_v1.0_2025_06_20_17_19_58.pdf の p.1、写し：`evidence/CardputerADV_grove_port.png`）

| 項目 | 回路図の記載 |
|---|---|
| Grove コネクタ J2（HY-2.0_IIC） | 1 = IIC_SCL（ネット "GI"）、2 = IIC_SDA（ネット "GO"）、3 = VCC（+5VGrove）、4 = GND |
| SCL / SDA のプルアップ | **R10 10kΩ → +3.3V**（SCL）、**R3 10kΩ → +3.3V**（SDA） |
| ESD 保護 | SDA/SCL に PESDNC2FD3V3B（3.3V 系の TVS）、VCC に PESDNC2FD5VB |
| ネット名とピン | "GI" / "GO" は Stamp-S3A（M1）の 1 番ピン（G1）と 2 番ピン（G2）につながる。BOOT ボタンのネット "G0"（数字のゼロ）とは別のネット。→ SCL = G1、SDA = G2 で、§1-2 と一致 ✅ |

### 8-3. 電気的互換性の評価

| 確認項目 | 値 | 判定 |
|---|---|---|
| バスの High レベル | 3.3V（両側のプルアップ先が 3.3V。合成すると約 5kΩ、ESP32 内蔵プルアップ 45kΩ も並列に入る） | — |
| ESP32-S3 の入力上限 | VIH max = VDD + 0.3V = 3.6V（VDD = 3.3V のとき） | ✅ 3.3V は範囲内 |
| ESP32-S3 の High 判定 | VIH min = 0.75 × VDD ≈ 2.475V | ✅ 3.3V は満たす |
| ESP32-S3 の Low 判定 | VIL max = 0.25 × VDD ≈ 0.825V | ✅（U135 側 STM32 の VOL は ❓。オープンドレインで GND に引くため通常は満たすと考えられるが、数値は未確認） |
| STM32F030 側の High 判定 | ❓ STM32F030 のデータシートは未確認。両方とも 3.3V 系なので問題ないと考えられるが、数値は見ていない | ❓ |
| 5V が信号線に回り込む経路 | 両方の回路図で見当たらない | ✅ |

根拠：ESP32-S3 Series Datasheet **v2.2**、Table 5-4 "DC Characteristics (3.3 V, 25 °C)"（VIH = 0.75×VDD〜VDD+0.3V、VIL = −0.3〜0.25×VDD、RPU = 45kΩ）、Table 5-1 Absolute Maximum Ratings（電源ピン −0.3〜3.6V）。https://documentation.espressif.com/esp32-s3_datasheet_en.pdf

**結論：SDA/SCL は両側とも 3.3V にプルアップされており、ESP32-S3 の GPIO と電気的に互換。✅**
ただし STM32F030 側の入出力しきい値は数値で確認していない（❓）。

### 8-4. 新たに分かったこと：Grove 5V の供給元を決めるスイッチ SW2

Cardputer ADV 回路図 p.1（写し：`evidence/CardputerADV_grove_port.png`、`evidence/CardputerADV_5V_boost.png`）

- Grove の VCC（+5VGrove）は **SW2（SW-SPDT）** を通っており、次のどちらかに切り替わる：
  - SW2 の 1 番 = **+5VOUT**：SY7088 の昇圧出力（電池側）。SY7088 の EN は IN に直結されているので、電源スイッチが ON なら常時出力される。設定電圧は 1.2V ×（75k/22k + 1）≈ 5.3V、ダイオード SS34 を通った後の値。
  - SW2 の 3 番 = **+5VIN**：USB からの 5V。
- **ソフトで有効にする必要はない**（回路図で確認）。✅
- ❓ **SW2 が外から操作できる物理スイッチなのか、現在どちら側になっているのか**は、公式ページにも記載がなく未確認。
  - +5VIN 側になっている場合、USB を接続していないと U135 に電源が供給されない可能性がある。
  - 公式ページの EXT 14P のピン表にある "5VIN / 5VOUT" との関係も ❓。
- ❓ SY7088 の出力電流上限と、Grove 5V に流せる電流は未確認。

## 9. M5Unit-Encoder 0.0.2 の読み取り失敗時の挙動

### 9-1. 実装（ライブラリのソース `src/Unit_Encoder.cpp`、ローカルの 0.0.2）

```cpp
void Unit_Encoder::readBytes(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t length) {
    uint8_t index = 0;
    _wire->beginTransmission(addr);
    _wire->write(reg);
    _wire->endTransmission();            // 戻り値を見ていない
    _wire->requestFrom(addr, length);    // 戻り値（受信バイト数）を見ていない
    for (int i = 0; i < length; i++) {
        buffer[index++] = _wire->read(); // データがなければ -1 → uint8_t で 0xFF
    }
}
signed short int Unit_Encoder::getEncoderValue() { ... readBytes(_addr, 0x10, data, 2); return data[0] | data[1] << 8; }
bool Unit_Encoder::getButtonStatus() { uint8_t data; readBytes(_addr, 0x20, &data, 1); return data; }
```

### 9-2. 下位層（実際にコンパイルされたコア：m5stack:esp32 3.3.9、ESP-IDF 5.5.4 → `esp32-hal-i2c-ng.c` が使われる）

- `Wire.requestFrom()`：`rxIndex = 0; rxLength = 0;` にしてから `i2cRead()` を呼ぶ（`Wire.cpp:521-526`）。
- `i2cRead()`（ng）：最初に `*readCount = 0`。成功したときだけ `*readCount = size` になる。**全バイト受信か 0 バイトかのどちらか**で、途中までの受信はない。
- `Wire.read()`：`rxIndex < rxLength` でなければ **-1 を返す**（`Wire.cpp:568-578`）。
- タイムアウトは既定で 50ms（`Wire.cpp:44`）。ハングはしない。

### 9-3. 失敗時に返る値と、それが正常値として扱われる可能性

| ケース | getEncoderValue() | getButtonStatus() | 正常値と区別できるか |
|---|---|---|---|
| A. アドレス NACK（抜けた、電源なし、接触不良） | 0xFF,0xFF → **−1** | 0xFF → **true** | **区別できない**。−1 は正当なカウンタ値。true は正当なボタン状態 |
| B. レジスタ指定の書き込みは失敗、読み出しは成功 | **直前のレジスタポインタの内容**が返る可能性（STM32 ファームウェアの動作は ❓） | 同左 | **区別できない** |
| C. 両方とも ACK だが、ノイズでビットが化ける | 化けた値 | 化けた値 | プロトコルに CRC がないため**原理的に検出できない**（v2 でも残る） |

**v1 スケッチへの影響（評価）**
- v1 では 500ms ごとに ACK を確認していたが、**その合間（20ms 周期のループで約 25 回分）**に一時的な失敗が起きると、次のことが起きた：
  - カウンタが −1 に見える → `ROTATE ±` の**誤イベント**。次の正常な読み出しで逆方向の誤イベントがもう一度出る。
  - ボタンが 0xFF → true → raw = 1 に見える → `BTN raw` の**誤イベント**。
- カウンタはユニット側で絶対値として保持されているので、**累積値そのものは次の正常な読み出しで元に戻る**。誤るのはイベントの検出だけ。
- 今回の目的（回転方向とボタンの検出確認）では、**誤った検出結果を実機テストの結論にしてしまうリスク**がある → 修正する価値がある。

### 9-4. 最小限の修正（v2 で実施済み。ライブラリ本体は変更しない）

- スケッチ内に `readRegChecked()` を追加。**手順はライブラリの `readBytes()` と同じ**（STOP を挟んだ書き込みの後に読み出し）で、次の 3 点だけを追加した：
  1. `endTransmission() != 0` なら失敗（ケース B を排除）
  2. `requestFrom() != len` なら失敗（ケース A を排除）
  3. `read() < 0` なら失敗（念のため）
- ボタンは生の値を `uint8_t` で受け取り、**0 / 1 以外は失敗扱い**（プロトコル上の値域は 0 / 1）。
- 失敗した場合はその回の値を**捨てる**（前回値を保持し、イベントは発生させない）。`ERR` カウンタを増やし、Serial に記録する。
- 初回の読み出しに失敗した場合は「未検出」として扱い、1 秒ごとに再試行する。
- 変更しなかったこと：`sensor.begin()`（Wire の初期化）、I2C 速度 200kHz、20ms 周期、ACK による検出、画面初期化。
- 差分：`diff_v1_to_v2.patch`。v2 はコンパイル成功（§5）。

### 9-5. 修正後も残るリスク（未解決）
- ケース C（ACK はあるがデータが化ける）は検出できない。プロトコルに CRC やチェックサムがないため。
- STM32 ファームウェアのクロックストレッチ、レジスタポインタの扱い、200kHz での動作は ❓（ファームウェアのソースが未公開）。


---

# 追補（2026-10-12）：実機結果とOpus 5.5開発計画レビュー

## 10. U135_Probe.ino v2の実機結果（2026-10-11）

以下はユーザーが実機で確認した結果。上記§5・§6の「実機未テスト」は**当時の履歴**であり、現在の状態を示さない。

| 項目 | 実機で確認した結果 |
| --- | --- |
| LCD表示 | 正常 |
| U135未接続 | `U135 NOT FOUND` を表示 |
| U135接続 | 検出成功 |
| 右回転1目盛り | カウンタ `+2` |
| 左回転1目盛り | カウンタ `-2` |
| ボタン押下 | `raw=1` → `raw=0` |
| ボタン解放 | `raw=0` → `raw=1` |
| I²Cエラー | 確認時 `ERR 0` |

**未検証：** Cardputer ADVからMacへのUSB HID送信、BLE HID、Fn同時押し、複数ホスト切替、RetroArchパドル入力。Mac Chromeでは、物理キーボードによるYouTube `.`/`,` のフレーム移動とYouTube Music `Shift+H/L` の1秒移動をユーザーが確認済み。

## 11. Opus 5.5のソース調査・計画レビュー（2026-10-12）

**証拠の区分：** 以下はOpus 5.5がローカルの `m5stack:esp32 3.3.9`、`M5Cardputer 1.1.1`、`M5Unified 0.2.25` のソースを調査したとする**レビュー報告の要約**。本追補作成時にエドガーが当該ローカルソースやADV実機を独立再検証したものではない。公式仕様・実機結果と混同しない。

| 論点 | Opusの報告・推奨 | 状態 |
| --- | --- | --- |
| USB HID | `Hardware CDC and JTAG`（USB Mode=1）ではTinyUSB HID例が無動作となり得る。`USB-OTG (TinyUSB)`（Mode=0）と `USB CDC On Boot=Enabled` を推奨 | ソース調査報告。ADV実機未検証 |
| 復旧 | USBモード変更に伴いJTAGデバッグへの影響、CDC消失の可能性。G0でダウンロードモードに入り復旧する経路を確保 | 実機復旧未検証 |
| 本体キーボード | `M5Cardputer.begin()` / `M5.begin()` によるADVボード判定が必要との指摘。Display単独初期化から変更すると内部周辺機器の初期化経路も変わる | ソース調査報告。画面・U135共存未検証 |
| Fn組み合わせ | Fn状態と文字キーをソース上取得できるとの指摘 | ADV実機未検証 |
| HID送信 | ASCIIの大文字`K`はShift付きになる場合がある。HID Usage ID＋修飾キーで指定する案（`k`=0x0E、`.`=0x37、`,`=0x36） | 推奨設計。JIS/US配列を実機確認 |
| U135イベント | カウンタ差分を±2単位に変換。半クリック残差、高速回転、キュー滞留を要評価 | 変換仕様未決定 |
| BLEボンド数 | ローカルコアの `CONFIG_BT_NIMBLE_MAX_BONDS 3`、`NVS_PERSIST 1` という設定値を報告 | 使用ライブラリと実効上限の再確認が必要 |
| BLE接続先選択 | 登録済み3台から選択した1台だけへ再接続する方式が難所。受付リスト・識別鍵等の検証が必要 | 実装方式未確定 |
| RetroArch | FBNeo・MAME 2003-Plusでマウス／ダイアル系入力の候補がある | 対象ゲームでの動作未検証 |
| U135分解能 | 公式30パルス/回転。アルカノイドの非公式486ステップ/回転との比較は単位・定義に注意 | 操作性未評価、交換未決定 |
| 設定保存 | NVS（Preferences）、バージョン付き保存形式、変更時のみ書き込みを推奨 | 設計候補 |
| Wi-Fi | 標準HIDではない。ESP-NOW受信機＋USB HIDドングルは追加ハードウェアを要する代替案 | 未検証の将来構想 |

関連一次資料（Opusレビューで参照されたもの）：
- Espressif Arduino USB API: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html
- Espressif USB keyboard example: https://github.com/espressif/arduino-esp32/blob/master/libraries/USB/examples/Keyboard/KeyboardMessage/KeyboardMessage.ino
- M5Cardputer: https://github.com/m5stack/M5Cardputer
- M5Unified: https://github.com/m5stack/M5Unified
- Cardputer ADV: https://docs.m5stack.com/en/core/Cardputer-Adv
- FBNeo: https://docs.libretro.com/library/fbneo/
- MAME 2003-Plus: https://docs.libretro.com/library/mame2003_plus/

## 12. レビュー指摘に対する採否と次の確認事項

**採用：** USB-OTG設定の明記、復旧経路の検証、第1a（USB HID）／第1b（本体キー）分離、HID Usage ID＋修飾キーの明示、BLEを単一接続→複数登録→選択接続に分割、設定保存にNVSを第一候補とする。

**保留：** 回転イベントキューを4件固定して超過分を捨てる方式、方向反転時に残差を無条件破棄する方式、パドル用エンコーダの交換。いずれも実測なしに確定しない。基本原則は「1目盛り＝1操作」だが、応答遅延とのトレードオフは測定して決める。

**原本保全：** `U135_Probe.ino` v2と `.bin` は元の場所から移動・上書きせず、別フォルダ等へコピーしてSHA-256を記録する。SOUND TERRARIUMのコードは変更しない。

**旧チェックリストの注意：** §6の `USB Mode = Hardware CDC and JTAG` は**U135_Probe v2の当時の設定**であり、次のUSB HID試作にそのまま適用してはならない。USB HID試作の設定・合格条件は最新版 `DEVELOPMENT_PLAN.md` の第0・第1a・第1b段階を優先する。

**次回実機検証：** USB-OTG＋CDC設定 → 最小HID送信 → U135回転／押下 → Chromeでの6操作 → 復旧経路 → 本体キーボード初期化とFn操作。各段階でLCD・I²Cエラー・USB接続状態を記録する。

---

# 追補（2026-10-12）：第1a段階 USB HID 実機成功と次段階への引き継ぎ

> **最新状況：第1a段階のYouTube用USB HID基本操作は実機で成功。** 上記§10の「USB HID未検証」、§11の「ADV実機未検証」、§12末尾の「次回実機検証」は、それぞれ**記録当時の状態**であり、最新の進捗は本節を優先する。未実施項目がすべて解消されたことを意味しない。

## 13. 第1a段階：実機テスト結果

| 項目 | 実機結果・確認範囲 |
| --- | --- |
| スケッチ | `MediaJog_1a.ino`（第1a成功版。既存ファイルは上書きしない） |
| ビルド | Opus 5.5がビルドしたアプリ単体 `.bin` を使用。Arduino環境は `m5stack:esp32 3.3.9`、ボード `M5Cardputer`、USB Mode=`USB-OTG (TinyUSB)`、USB CDC On Boot=`Enabled` |
| 導入方法 | Cardputer ADVの**LauncherからWi-Fiアップロード**して起動。Launcherを直接上書きせず導入できた |
| 本体表示 | 最終的に `USB HID READY` を確認 |
| ホスト | Mac、Chrome、YouTube |
| U135右回転 | YouTube一時停止中のフレーム送りが動作 |
| U135左回転 | YouTube一時停止中のフレーム戻しが動作 |
| U135押し込み | YouTubeの再生／停止が動作 |
| Macの文字入力 | 日本語入力中のテキストエディタでは `。`／`、`、ABC入力では `.`／`,` が入力され、HID入力の送信を確認。入力ソース依存の表示差であり、HID不具合とは判断しない |
| I²C・回転追従 | 基本操作に支障は認められなかったが、高速回転時の欠落率、1目盛り＝1フレームの厳密な連続計測、長時間安定性は未測定 |

**判定：第1aのYouTube基本3操作は合格。** 旧計画の「YouTube Musicを含む6操作」「復旧経路の実地確認」まで完了したという意味ではない。YouTube Music用の独立プロファイルは今回の必須条件にしない。

**注意：** 初期に `USB NOT MOUNTED` と表示された経緯はあったが、最終的には `USB HID READY` となり、Macでの操作が成功した。初期表示の原因は特定していないため、断定しない。

## 14. 成功版の保全と第1b段階

- `MediaJog_1a.ino` および対応する `.bin` を**変更せず保全**する。`U135_Probe.ino` v2、Launcher、SOUND TERRARIUMも保全対象。
- 第1bは別フォルダ `MediaJog_1b/` で作業する。
- 第1bの最小実装は、Cardputer本体の `Fn+1`／`Fn+2` を検出し、画面に `PROFILE 1`／`PROFILE 2` を表示すること。**いずれを選んでもU135のUSB HID割り当ては第1aと同一**とし、この段階では実際のアプリ別機能切替を実装しない。
- 本体キーボードのキー入力をMacへ送信しない。`M5Cardputer.begin()` の導入に伴うLCD、外部Grove I²C、USB HIDの共存を実機で検証する。
- 同一アプリ内に用途の曖昧な類似プロファイルを増やさない。用途別プロファイルは必要性を確認してから追加する。

## 15. RetroArch／アルカノイドと筐体の将来検証項目

**アルカノイドは未実装・未検証。** U135の回転をUSB HIDマウス相対X移動などに変換し、対象ROM・RetroArchコア（FBNeo／MAME 2003-Plus等）の入力設定を実機で確認する。レーザー取得時の発射操作にはボタンが必要なため、U135の押し込みを候補とし、**押している間は押下状態を維持し、離したら解放する**方式を検証する。回転しながら同じノブを押す操作性も実際に評価する。コア側でマウスボタンを発射に割り当てられるかは未確認。

操作感の課題として、U135の分解能だけでなく、**標準ノブの半径が小さいこと**に着目する。現段階でU135を交換する方針は採用しない。まず既存U135でソフトウェアを検証し、必要に応じて大径ノブによる操作性改善を検討する。大径化してもエンコーダの電気的分解能は変わらない。

**筐体設計はソフトウェア実証後に着手する構想。** Cardputer ADVとU135を一体化した卓上型筐体、Cardputerの着脱、M5StackのLEGO互換取り付け部の活用、大型3Dプリントダイヤルを候補とする。ADVの実際の穴位置・寸法・保持力、USB-Cアクセス、Grove配線、ノブ押し込み機構、軸への荷重は現物測定後に設計する。寸法精度が必要なモデルはCADで作成し、Bambu Studioでスライスする。将来的なジョイスティック等のモジュラー筐体にも展開できる設計余地を残す。

**次回の作業：** 第1a成功版を保全 → 第1bでFnキー検出・表示切替と既存USB HIDの共存を実機確認 → RetroArchの入力対応を検証 → ソフトウェア完成後に一体型筐体と大型ダイヤルを設計。詳細は最新版 `DEVELOPMENT_PLAN.md` を参照。

---

# 追補（2026-10-12）：第1b段階の実装・ソース調査・コンパイル

## 16. 第1b段階：実装とソース確認の要約（実機未インストール）

**証拠の区分：** 以下はOpus 5.5が、ローカルの M5Unified 0.2.25、M5GFX 0.2.32、M5Cardputer 1.1.1、m5stack:esp32 3.3.9 のソースとビルドで確認した内容である。**ADV実機での結果ではない。** ソースの行番号つきの詳細は `docs/STAGE_1B.md` §5 にある。

| 項目 | 内容 | 状態 |
| --- | --- | --- |
| スケッチ | `MediaJog_1b/MediaJog_1b.ino`（第1a r2の成功版をコピーし、本体キーボード部分だけを追加）。U135の読み取りとUSB HID送信は差分なし | ✅ diffで確認 |
| ビルド | 第1aと同じ設定（USB-OTG、CDC On Boot=Enabled）で成功。フラッシュ 595,435 B（45%）、RAM 59,956 B（18%）。アプリ単体 `.bin` は 595,584 B、SHA-256 `9377aba8…c534443` | ✅ ビルド |
| 初期化順序 | `Display.begin()`（第1aと同じ）→ `M5Cardputer.begin(cfg, true)` → `sensor.begin(&Wire, G2, G1)` | 実装 |
| 画面の再初期化 | `M5GFX::init_impl()` はボード判定済みならすぐ戻るため、`M5.begin()` 内で画面は再初期化されない | ✅ ソース。❓実機 |
| GPIO46 | `M5.begin()` は `Display` が未判定のときだけGPIO46をHIGH出力にする（ADVには電源保持ピンがなく、元に戻されない）。`Display.begin()` を先に呼ぶことで回避した | ✅ ソース |
| `M5.begin()` の設定 | IMU、RTC、スピーカー、マイク、外部ディスプレイ、画面消去、`Serial.begin` を無効化。ADVで実際に行うのは、内部I²C（I2C_NUM_1／`Wire1`、G8/G9）の開始、G0入力、電池ADC（G10）の設定だけ | ✅ ソース |
| 外部Grove（U135） | `Ex_I2C` はポート番号を記録するだけで、開始されない。外部ディスプレイの検出コードは該当ヘッダーを読み込まない限りコンパイルされない | ✅ ソース |
| キーボード | ADV判定時にTCA8418（0x34、400kHz、内部I²C、割り込みG11）を使用。`Fn` → `keysState().fn`、数字キー → `word` に `'1'`/`'2'`（Fnは文字を変えない） | ✅ ソース。❓実機 |
| 本体キーの非送信 | HIDレポートを作る経路は、U135のイベント（`pollUnit` → `pushEvent`）だけ。キーボード処理はプロファイル表示用の変数を変えるだけ | ✅ ソース。❓実機 |
| プロファイル | `Fn+1` → `PROFILE 1`、`Fn+2` → `PROFILE 2`（表示のみ）。どちらもU135の割り当ては第1aと同じ。保存はしない | 実装 |

**実機で確認すること**（`docs/STAGE_1B.md` §6）：

- LCDの表示が第1aと変わらないこと。
- `Fn+1`／`Fn+2` が検出されること。
- 本体キーを押してもMacに入力されないこと。
- 第1aのYouTube 3操作が回帰していないこと。
- 連続操作中に `ERR`／`FAIL` が増えないこと。
- `W`（1ループの処理時間）の増分。
- 595 KBのアプリがLauncherの領域に収まること。

**状態：エドガーのコードレビュー待ち。実機にはまだインストールしていない。**（記録当時。最新の状態は§17を参照）

---

# 追補（2026-10-12）：第1b段階 実機成功、USB初回認識の記録、第1c計画

> **最新状況：第1b段階は実機で成功。** §16末尾の「実機未インストール」は記録当時の状態である。

## 17. 第1b段階：実機テスト結果

| 項目 | 実機結果・確認範囲 |
| --- | --- |
| スケッチ／バイナリ | `MediaJog_1b.ino`（SHA-256 `570663df…8b4eef`）、アプリ単体 `MediaJog_1b.ino.bin`（`9377aba8…c534443`） |
| 導入方法 | LauncherのWi-Fiアップロード（Launcherは上書きしていない） |
| `Fn+1`／`Fn+2` | プロファイル表示の切替が動作 |
| PROFILE 1／PROFILE 2でのYouTube | どちらでも、コマ送り・コマ戻し・再生／停止が動作 |
| 本体キーの非送信 | 本体キーがMacに入力されないことを確認 |
| LCD | 表示切替を含めて機能（`M5Cardputer.begin()` の追加後も表示に問題は報告されていない） |
| `ERR`／`FAIL`／`W`／`T` の値、長時間の連続操作 | 未記録 |

**判定：第1bの合格条件を満たした。** §16で「ソース上の判断」としていた次の点は、実機で問題が出なかった範囲で裏付けられた。

- 画面の再初期化が起きないこと
- 内部I²C（キーボード）と外部I²C（U135）が共存すること
- 本体キーがMacに送信されないこと

ただし、エラー数や処理時間の測定値は記録していない。

**保全：** 上記2ファイルを第1b成功版とし、変更しない。SHA-256は `MediaJog_1b/SHA256SUMS.txt` と `docs/STAGE_1B.md` §10に記録した（`shasum -a 256 -c` で照合済み）。

## 18. USB初回認識の不安定（継続中・原因未確定）

- **経緯：** 第1aで、最初は `USB NOT MOUNTED` と表示され、最終的に `USB HID READY` になった（§13）。第1bでも、MacとのUSB初回認識が不安定な現象が続いている。
- **原因：未確定。** 次は切り分け候補であり、いずれも未検証。優先順位は付けていない。
  1. Launcherからアプリへ切り替える際のUSB周辺回路の状態
  2. ケーブル、ハブ、Mac側のポート
  3. 接続の順序（アプリ起動後にUSBを挿す／挿したまま起動する）
  4. macOSでの初回キーボード認識の処理
  5. TinyUSBの列挙のタイミング
- **発生時に記録したい情報：**
  - 接続の順序
  - 画面1行目の表示（`USB NOT MOUNTED`／`USB SUSPENDED`／`USB HID READY`）
  - READYになるまでの時間と操作（抜き差しの回数など）
  - 使用したケーブルとポート
  - Macの「システム情報」→「USB」での表示の有無
- **対応方針：** 成功版（1a・1b）のコードは変更しない。記録がたまってから、原因の調査を別の作業として行う。1cではUSB記述子が変わるため、1c単独の問題と混同しないよう、発生状況を分けて記録する。

## 19. 第1c段階（RetroArch）計画のための技術メモ（未実装）

**証拠の区分：** m5stack:esp32 3.3.9 のソースで確認した事実と、未確認の事項を分けて記載する。計画の本文は `DEVELOPMENT_PLAN.md` の第1c段階にある。

| 項目 | 内容 | 状態 |
| --- | --- | --- |
| マウスのクラス | `USBHIDMouse` は内部にブートプロトコルなしの `USBHID` を持つ（`USBHIDMouse.cpp:28`）。相対マウスの記述子はReport ID 2（`USBHIDMouse.cpp:96`） | ✅ ソース |
| 移動量の範囲 | TinyUSB標準マウス記述子のX／Yは8ビット、範囲は−127〜127（`hid_device.h:244-247`、`hid_mouse_report_t` の `int8_t x`） | ✅ ソース |
| インターフェースのプロトコル | 最初に構築された `USBHID` がプロトコルを決める（`USBHID.cpp:277-287`）。1bではキーボード（ブート）が先 | ✅ ソース |
| ブートプロトコル時の送信 | ホストがブートプロトコルを選ぶと、Report ID 0で送る（`USBHID.cpp:343-345`）。ブートキーボードのインターフェースにマウスを同居させると、この場合にマウスのレポートが正しく解釈されない可能性がある | ✅ ソース。実際の影響は❓ |
| 公式例 | `KeyboardAndMouseControl.ino` はマウスを先に宣言している（ブートプロトコルなし） | ✅ ソース |
| FBNeo | 入力デバイス種別「Mouse (ball only)」「Mouse (full)」がある（https://docs.libretro.com/library/fbneo/）。アルカノイドでの動作はコミュニティ報告のみ | ❓ 実機 |
| MAME 2003-Plus | コアオプション「Mouse Device」、ダイヤル関連の設定がある（https://docs.libretro.com/library/mame2003_plus/） | ❓ 実機 |
| RetroArchのマウス固定 | grab用のホットキーがあるとされる（既定値はコミュニティ情報のみ） | ❓ |
| macOSのポインタ加速 | RetroArchの入力に影響するか | ❓ |

**状態：計画のみ。`MediaJog_1c/` は未作成で、コードはまだ書いていない。**

---

# 追補（2026-10-12）：第1c-1段階 実機試験記録

> **最新状況：第1c-1（キーボード＋マウス複合HID、PROFILE 2＝マウス）は主要機能が実機で動作した。ただし、USB切断後の自動復帰に未解決の問題がある。** §19末尾の「計画のみ・未作成」は記録当時の状態である。実装、HID構成のソース確認、試験手順は `docs/STAGE_1C.md` にある。

## 20. 第1c-1：実機試験記録（2026-10-12、エドガーのレビュー後に実施）

**使用環境：** M5Stack Cardputer ADV、M5Stack Unit Encoder U135、Mac。Launcher経由で第1cのアプリ単体 `.bin` を起動。

**使用したファームウェア：**
- `MediaJog_1c.ino`（SHA-256 `0d44b476…99b920bf`）
- `MediaJog_1c.ino.bin`（`875f073e…69c9ea42`）
- HID構成：ブートプロトコルなしのHIDインターフェース1つに、Report ID 1＝キーボード、Report ID 2＝相対マウス

### 20-1. 機能試験

| 試験項目 | 結果 |
| --- | --- |
| PROFILE 1：YouTube操作 | 成功（第1bと同様に正常動作との報告） |
| PROFILE 2：U135右回転によるポインタ移動 | 成功 |
| PROFILE 2：U135左回転によるポインタ移動 | 成功 |
| PROFILE 2：U135を押しながら回転してファイルをドラッグ | 成功 |
| ドラッグ中に `Fn+1` へ切替 | ドラッグが終了した |
| USBケーブルを抜いた際のMac側のドラッグ解除 | 成功 |
| Launcherの維持 | 確認できた |

### 20-2. USB切断・再接続試験（時系列）

**試験A：USB切断後にLauncherが起動したケース**

1. PROFILE 2でドラッグ。
2. USBケーブルを抜く。
3. USBケーブルを再接続。
4. Launcherが起動。
5. Mac側ではドラッグが解除されていた。

→ MediaJogが動作を続けていないため、**MediaJog自身の再接続処理が成功したとは判定しない。**

**試験B：Cardputer ADVの物理電源スイッチをONに維持したケース**

1. PROFILE 2でドラッグ。
2. USBケーブルを抜く。
3. 画面表示が `USB SUSPENDED` に変化。
4. USBケーブルを再接続。
5. 表示は `USB SUSPENDED` のまま。
6. Mac側でドラッグは再開されなかった。
7. Cardputer ADVの物理電源スイッチをOFF→ON。
8. 表示が `USB HID READY` に復帰。
9. `PROFILE 1 YOUTUBE` に戻り、ドラッグ状態も解除されていた。

**判定：**
- USB切断時にMac側のドラッグが解除されることは、実機で確認できた。
- USBの再接続だけでは `USB HID READY` に復帰しなかった。
- 電源の再投入による復帰は確認できた。
- MediaJogが再接続を検知して解放レポートを送る処理の成功は、確認できていない。
- USBの自動復帰不良の原因は未特定。

## 21. 未解決課題：USB切断後の自動復帰（原因未特定）

- **現象：** 電源スイッチONのままUSBケーブルを抜いて挿し直すと、表示が `USB SUSPENDED` のまま `USB HID READY` に戻らない（試験B）。物理電源のOFF→ONで復帰する。
- **原因：未特定。推測で断定しない。**
- **ソースから言える表示の意味（原因の説明ではない）：**
  - `USB SUSPENDED` は、スケッチの `usbSuspended` フラグが真のときに表示される。
  - このフラグは、レジューム、マウント（STARTED）、アンマウント（STOPPED）のいずれかのイベントで下りる。
  - したがって試験Bの手順5の時点では、そのいずれのイベントもスケッチに届いていなかった。
  - なぜ届かなかったかは未調査（詳細は `docs/STAGE_1C.md` §10-2）。
- **未記録の情報：** 再接続後にMacの「システム情報」→「USB」にデバイスが表示されていたか、CDCシリアルのポートが現れたか、抜いていた時間、ケーブルとポート。
- **§18（USB初回認識の不安定）との関係：** 未確認。別の課題として記録する。
- **扱い：** 第1c-1成功版のファームウェアは修正・再コンパイルしない。独立した未解決課題として `DEVELOPMENT_PLAN.md` に残す。

## 22. 第1c-1成功版の保全と次の段階

- `MediaJog_1c/MediaJog_1c.ino` と `MediaJog_1c/build/MediaJog_1c.ino.bin` を第1c-1成功版として保全する。
  - SHA-256を `MediaJog_1c/SHA256SUMS.txt` に記録し、照合済み（2026-10-12、両方OK）。
  - 第1a・第1bの成功版と `U135_Probe` v2 も、同日にSHA-256の一致を確認した。
  - LauncherとSOUND TERRARIUMは変更していない。
- **次の段階は「第1c-2：RetroArch／Arkanoidの入力設定・動作検証」。** 第1c-1成功版のファームウェアをそのまま使い、まずRetroArch側の設定だけで、次の点に対応できるか確認する。
  - 回転 → パドルの左右移動
  - 押し込み → 発射
  - コアによる入力方式の違い
- RetroArchの設定確認が済むまで、ファームウェアは変更しない。


## 23. 2026-10-12 方針整理：汎用スピナーと感度調整（計画更新・実機結果ではない）

**この節はユーザーとの設計協議による方針更新であり、追加のコンパイル・書き込み・ゲーム実機試験は行っていない。§1〜§22の調査・試験結果は変更しない。**

- **目的の再整理：** ArkanoidはU135の回転操作が生きる例として挙げられた。RetroArchはエミュレーター全般の入力方式として使われているとユーザーが想定していたため候補になったもので、現時点では未導入。RetroArch／Arkanoid専用化を目指すわけではない。
- **汎用性：** 現行PROFILE 2はUSB HID相対マウスX移動と左ボタンを送る。Mac Finderでのポインタ移動・ドラッグは確認済みだが、ゲーム内のパドル／スピナー入力は未検証。RetroPieを含むすべてのエミュレーターで自動的に動くという意味ではない。
- **操作性の背景：** ユーザーはイーグレットツーミニ用パドル＆トラックボールコントローラーを以前使用し、「ダイヤルが小さい」「小さな操作でゲーム内の動きが大きい」と感じた。感度設定は変更せず使用を中止した。製品一般への評価ではなく個人の使用経験として記録。
- **次の実験：** 第1c-1成功版を変更せず、Macでマウス対応の簡単なブロック崩し等を試す。ゲーム側の絶対位置入力と相対マウス入力の違いを記録し、必要ならRetroArch／他のエミュレーターを次の候補とする。ゲーム動作の成否は未確認。
- **次期機能候補：** Cardputer本体のキーでPROFILE 2の感度をリアルタイムに増減し、LCDに現在値を表示する。現行の1目盛り8カウントは仮値。操作キー、調整範囲、端数処理、保存方法は未決定。速度依存の加速は初期実装に含めない。
- **保全：** 第1a／第1b／第1c-1の成功版、U135_Probe v2、Launcher、SOUND TERRARIUMは変更しない。USB自動復帰問題も未解決のまま継続管理する。

---

# 追補（2026-10-12）：ブラウザ版ブロック崩しの試験と、第1d段階（スピナー感度調整）の実装・ビルド

## 24. ブラウザ版ブロック崩しの試験結果と、第1dの実装・ビルド

### 24-1. ブラウザ版ブロック崩しの試験（2026-10-12、ユーザーの実機試験）

| 条件 | 結果 |
| --- | --- |
| MacのChrome、通常のマウス | 快適に遊べる |
| 同じゲーム、U135（第1c-1成功版、PROFILE 2、1目盛り＝8 HIDカウント） | パドルは動くが、**移動量が極端に小さく、ゲームとして遊べない** |

**原因は断定しない。** 次の3つが候補で、いずれも切り分けていない。

1. 倍率の不足
2. macOSのマウス加速
3. ブラウザゲーム側の入力の読み方

→ 実機で倍率を変えて確かめられるよう、第1dで感度調整を実装した。RetroArch専用ではなく、汎用スピナー機能の改善として位置づける。

### 24-2. 第1dの仕様（ユーザー・エドガーとの協議で確定）

- **感度の操作（PROFILE 2のみ）：**
  - `Fn+;`（ADVの刻印 ↑）で感度を上げる。
  - `Fn+.`（刻印 ↓）で感度を下げる。
  - M5CardputerライブラリのFnフラグと基本文字で判定する。
  - ユーザー提供のキーボード画像の物理配置と、ライブラリのキー配置（`_key_value_map[2][11]=';'`、`[3][11]='.'`、`[2][0]=KEY_FN`）を照合した。
- **感度：** 1目盛りあたりのHIDカウント。14段階（1／2／3／4／6／8／12／16／24／32／48／64／96／128）、初期値8（第1cと同じ）。NVSには保存しない。
- **協議で決めた方針：**
  - 高速回転時に取りこぼしがないとは断定しない（実機未検証）。
  - 方向反転時に未送信の移動量を捨てる処理は追加しない。
  - 上限128で足りるかは実機で評価する。
  - 第1c成功版、PROFILE 1、Launcher、既存のUSB処理は保全する。
- **実装の要点：**
  - 半カウント単位で計算し、余りを繰り越す（奇数の感度でも正確になる）。
  - 1レポート±127で分割する。
  - 未送信の上限は±1,016（第1cと同じ）。
  - 感度を変えたときは、未送信の移動量を捨てる。
  - PROFILE 2の画面は専用にし、感度の数字と診断行だけを部分的に描き直す（回転だけでは全画面を描き直さない）。
  - 詳細は `docs/STAGE_1D.md`。

### 24-3. ビルド結果（実機結果ではない）

| 項目 | 結果 |
| --- | --- |
| スケッチ | `MediaJog_1d/MediaJog_1d.ino`（33,566 B、SHA-256 `3b8c5667…75ac30c6`）。第1c-1成功版からコピーして作成 |
| アプリ単体 `.bin` | `MediaJog_1d/build/MediaJog_1d.ino.bin`（598,592 B、SHA-256 `2692467b…377b3d6f`）。`MediaJog_1d/SHA256SUMS.txt` で照合済み |
| ビルド | 成功。USB-OTG、CDC On Boot=Enabled。フラッシュ 598,439 B（45%）。スケッチ由来の警告0件 |
| 第1cとの差分 | USBの処理、キーボード送信、マウスボタン処理に変更がないことをdiffで確認 |
| 保全 | 第1a・第1b・第1c-1の成功版と `U135_Probe` v2 のSHA-256一致を確認。Launcher、SOUND TERRARIUMは変更なし |

**実機試験は未実施。** 次の点は、すべて実機未確認である。

- 感度の変更と表示
- ポインタやパドルの移動量の変化
- 高速回転時の挙動
- 本体キーの非送信
- PROFILE 1の維持

USBの再接続後の自動復帰問題（§21）は、第1dでUSBの処理を変えていないため、未解決のまま扱う。

---

# Addendum (2026-10-13, English): Stage 1d hardware observation and documentation corrections

> **Latest status:** Stage 1d has been used on real hardware by the project author with a positive, subjective result (§25). §24-3's "実機試験は未実施" (hardware test not done) was correct at the time of the build and is kept as written. From this addendum on, new records are written in English. Earlier sections stay in Japanese as the historical record.

## 25. Stage 1d: hardware observation (reported by the project author)

| Item | Record |
| --- | --- |
| Firmware | Stage 1d (`MediaJog_1d`), installed through the Launcher's Wi-Fi upload. **The SHA-256 of the uploaded file was not recorded.** The repository files still match `MediaJog_1d/SHA256SUMS.txt` (sketch `3b8c5667…75ac30c6`, app-only binary `2692467b…377b3d6f`) |
| Host | macOS, Chrome, a mouse-controlled browser Breakout game |
| Date | Between the 2026-10-12 build and 2026-10-13; exact date not recorded |
| Result | The spinner (Profile 2) was used successfully in the game |
| Sensitivity | **48–64 mouse counts per detent** felt comfortable (default 8 was too small; §24-1) |
| Fast movement | Rapid left–right paddle movement felt smooth, without noticeable catching or lag |

**Interpretation limits:**
- This is a subjective observation for one game on one host. It does not show compatibility with other games, emulators or operating systems.
- It is not a measurement of encoder-count accuracy, and not proof of zero count loss at high speed. No `clip`/`fail`/`ERR`/`W` values were recorded.
- 48–64 is the author's preference for this game, not a recommended default. The firmware default remains 8.
- The Stage 1d checklist (`docs/STAGE_1D.md` §6) was **not** executed item by item. Per-item status is in `docs/STAGE_1D.md` §9-3.

**What this does and does not answer from §24-1:** Raising the gain made the game playable. This is consistent with "insufficient gain" being at least a major factor. It does **not** separate the effects of macOS pointer acceleration or of how the game reads mouse input; that remains unisolated.

## 26. Repository and documentation changes (2026-10-13)

1. **Probe sketch moved into a folder (by 2026-10-13):**
   - `U135_Probe.ino` → `U135_Probe/U135_Probe.ino`
   - `U135_Probe.ino.bin` → `U135_Probe/U135_Probe.ino.bin`
   - SHA-256 checked after the move: unchanged (`14aaca87…`, `0323b74b…`).
   - Earlier sections that cite the root path are kept as written.
   - A sketch folder of the same name is also the layout the Arduino IDE expects.
2. **English public documents:** `README.md` was added, and `docs/DEVELOPMENT_PLAN.md` was rewritten in English by the project author. No in-repository backup of the previous Japanese plan was found on 2026-10-13. The key technical decisions from it were restored in the English plan (§2.1 "Design invariants" and §2.2 "Historical decision index"). The full discussion remains in this report (§1–§24) and in `docs/STAGE_1A.md`–`STAGE_1D.md`.
3. **License:** `LICENSE` (MIT, `Copyright (c) 2026 kariagepompadour`) was added. Scope, as stated in `README.md`:
   - It covers the project's original firmware and original 3D-print design files.
   - Third-party code and materials keep their own licenses:
     - M5Stack libraries: MIT (M5Unified, M5GFX, M5Unit-Encoder: "Copyright (c) 2021 M5Stack"; M5Cardputer: SPDX MIT).
     - Arduino-ESP32 core: mixed. Some files carry LGPL-2.1 headers (e.g. `cores/esp32/Arduino.h`) and others Apache-2.0 (e.g. `libraries/USB/src/USBHID.cpp`).
     - TinyUSB: MIT.
     - Third-party schematics in `evidence/`.
   - `U135_Probe` was derived from M5Stack's MIT-licensed `Unit_Encoder_M5Core2.ino` example, and M5Stack's notice applies to the derived portions.
   - Licensing of photographs and documentation was not specified by the author and is left open.
4. **Publication filter:** `.gitignore` was added. The repository is not yet a git repository, so nothing was untracked or deleted.
   - It excludes merged/bootloader/partition images, ELF/map files and build caches under each `build/`, the third-party evidence images, and `.DS_Store`.
   - Sketches, app-only `.bin` files and `SHA256SUMS.txt` files remain publishable.
5. **Firmware:** unchanged. No source, build setting or binary was modified.

## 27. Follow-up corrections after Edgar's review (2026-10-13)

§26 is kept as written; the items below update it.

1. **Checksum instructions verified:**
   - `shasum -a 256 -c MediaJog_1d/SHA256SUMS.txt` was run from the repository root and passed: both files `OK`, exit status 0.
   - The paths inside the 1b, 1c and 1d `SHA256SUMS.txt` files are relative to the repository root, so the same command fails if run from inside the stage folder. The checksum files were left unchanged; the README now states "run from the repository root" and shows the expected output.
   - All recorded hashes still match: 1b, 1c and 1d via their `SHA256SUMS.txt`; 1a and `U135_Probe/` against the values in the stage docs.
2. **`.gitignore` wording:** The README now says that `.gitignore` excludes specified build artifacts and temporary files but does **not** guarantee that every unwanted file is excluded. It also says to inspect staged files with `git status` and `git diff --cached --name-only` before publishing. The project folder is still not a Git repository; Git was not initialized and nothing was staged.
3. **Japanese plan backup:** searched on 2026-10-13 by file name, by the plan's Japanese title ("汎用ロータリーHIDコントローラー") and by a section heading ("第0段階：原本保全"). **No original copy exists in the project directory.** `docs/archive/` was not created and the plan was not reconstructed.
4. **License scope decided by the author** (supersedes "photographs and documentation left open" in §26 item 3):
   - MIT, `Copyright (c) 2026 kariagepompadour`, covers all original project materials: firmware source, 3D-print designs, documentation, and photographs/images.
   - Third-party libraries, example code, schematics and other externally sourced materials keep their original licenses.
   - The `LICENSE` text is the unmodified standard MIT text.
5. **Enclosure status (author report, 2026-10-13):**
   - The flat centered enclosure has been printed and photographed.
   - The 15° sloped enclosure has been designed and is **currently being 3D-printed**.
   - Its fit, stability, ergonomics and usability are not yet verified.
6. **Firmware:** unchanged.

## 28. License-compliance correction for `U135_Probe` (2026-10-13)

**Reason (Edgar's review):**
- `U135_Probe.ino` is derived from M5Stack's `Unit_Encoder_M5Core2.ino` example (§2).
- The MIT License requires the original copyright and permission notice to be kept.
- The sketch did not carry M5Stack's notice, and the root `LICENSE` names only the project author.

**Upstream notices checked:** `m5stack/M5Unit-Encoder`, `master` branch, fetched 2026-10-13. Both match the locally installed library 0.0.2 exactly.
- Repository `LICENSE`: MIT License, "Copyright (c) 2021 M5Stack".
- Example header (`examples/Unit_Encoder_M5Core2/Unit_Encoder_M5Core2.ino`): "Copyright (c) 2021 by M5Stack". The example file has no separate license line; the MIT terms come from the repository `LICENSE`.

**Changes:**

| File | Change |
| --- | --- |
| `archive/U135_Probe/U135_Probe.ino` | **New.** Byte-identical copy of the original, hardware-tested source (2026-10-11), kept as the historical reference. SHA-256 `14aaca87d57e018e2eab64a2e183e38c95f4cfee14f29fe597b6b6a0cbbe80fa`, recorded in `archive/SHA256SUMS.txt` |
| `U135_Probe/U135_Probe.ino` | **Comments only:** three lines added to the header comment (M5Stack copyright, pointer to `LICENSES/M5Stack-MIT.txt`, project copyright for the modifications). Everything from the closing `*/` onward is byte-identical to the original (checked with `diff`). New SHA-256 `3bb64b7267cdac6d4e1ce2c66b115189c338e773061f97bd9b9e8531bff6f1e9` (6,883 bytes; original 6,672 bytes) |
| `U135_Probe/U135_Probe.ino.bin` | **Unchanged.** SHA-256 `0323b74b4e57a826278de6c32be93fbde1ac45dca8bc4b510f5eddaafbcbd375`. Not rebuilt |
| `LICENSES/M5Stack-MIT.txt` | **New.** Byte-identical copy of the upstream M5Unit-Encoder `LICENSE` (checked with `diff`) |
| `LICENSE` | Unchanged |
| `README.md` | License section and layout updated to reference `LICENSES/M5Stack-MIT.txt` and `archive/` |

**Which source is which:**
- **Tested source:** `archive/U135_Probe/U135_Probe.ino` (`14aaca87…`). This is the source compiled into `U135_Probe.ino.bin` (`0323b74b…`) and run on hardware on 2026-10-11 (§10). All earlier references to the probe source hash (§1–§27, `STAGE_1A.md`–`STAGE_1D.md`) mean this file.
- **Attribution-corrected source:** `U135_Probe/U135_Probe.ino` (`3bb64b72…`). It differs only in comments. It has **not** been compiled or run, and no binary is recorded for it. The executable code is identical, but no claim is made that it would produce a byte-identical binary.

**Firmware behavior:** unchanged. No code, build setting or binary was modified, and nothing was rebuilt.

**`.gitignore`:** checked against an empty mirror of the file tree in a temporary Git repository. `LICENSES/M5Stack-MIT.txt`, `archive/U135_Probe/U135_Probe.ino` and `archive/SHA256SUMS.txt` are **not** excluded.

## 29. Development entry — 2026-10-13: 15° base completed, agreed Stage 1e direction, `SendReport()` wording review

Earlier sections are unchanged. §27 item 5 recorded the 15° base as "currently being 3D-printed"; that was correct when written, and this entry updates it.

### 29-1. Hardware: two printed base variants (author report)

| Item | Record |
| --- | --- |
| Flat base | Printed and assembled with the Cardputer ADV and U135 (as recorded earlier; photographed) |
| 15° angled base | **Printed and assembled** with the Cardputer ADV and U135 |
| Evaluation | The author operated the knob on both and reported **no substantial difference in operating feel** |
| Interpretation | Subjective impression only. No ergonomic advantage of the angled base is claimed. Stability, slipping, push force and long-term fatigue were **not** systematically evaluated |
| Decision | The two bases are **alternatives**, not original vs improved. Both are intended for eventual publication so users can choose |

### 29-2. Agreed future direction: Stage 1e (planned, not implemented)

**Concept:** "Set it once. Use it your way." The controller should act as a dedicated device for the user's preferred application while remaining configurable. All configuration happens locally on the Cardputer keyboard and display; no PC-side configuration application.

**Agreed scope (requirements, not results):**
- **Four profiles:** 1 YouTube, 2 Spinner, 3 Video Editing, 4 Custom. Proposed keys `Fn+1`–`Fn+4`; `Fn+3`/`Fn+4` must be verified on the ADV before they are final.
- **Video Editing**, for Adobe Premiere Pro and Apple Final Cut Pro. Proposed defaults, based on common editing shortcuts: clockwise = Right Arrow, counterclockwise = Left Arrow, press = Space. **Not tested in either application; no compatibility claimed.**
- **Custom:** clockwise, counterclockwise and button each configurable on the device. Ordinary HID keys and modifier combinations where feasible; TAP and HOLD button behaviors to be considered. Configuration UI separate from the operating UI. **Configuration keystrokes must never reach the USB host.**
- **Persistent settings** (ESP32-S3 NVS or another mechanism, subject to review): last profile, Custom mappings, Custom button behavior, spinner sensitivity. Restore at boot; validate stored data with safe defaults; minimize flash writes; **never restore an active key-down or mouse-button-down state**.
- **Compatibility:** preserve Stage 1d behavior until Stage 1e is implemented and tested; keep the Launcher-compatible app-only workflow; investigate how NVS persistence interacts with the Launcher and other apps. The USB replug / `USB SUSPENDED` issue (§21) remains unresolved.

**Design findings recorded during planning** (source review only; details in `docs/DEVELOPMENT_PLAN.md` §6):
1. **HOLD needs a key-state model.** The Stage 1a–1d tap path sends single-key reports followed by an all-keys-up report. With a HOLD key down, such a tap would silently release the held key. Stage 1e must build every keyboard report from a model of held keys plus the tap key, and must re-test Profile 1.
2. **NVS risk in the Arduino core.** `initArduino()` erases the **entire** NVS partition if `nvs_flash_init()` returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` (`cores/esp32/esp32-hal-misc.c:331-337`, m5stack:esp32 3.3.9).
   - This already applies to every Arduino build, including Stage 1a–1d.
   - Under a Launcher, the partition is presumably shared with the Launcher and other apps (unverified).
   - Whether the Launcher's partition table provides a usable `nvs` partition is **unverified**.
3. **M5GFX already writes NVS:** it stores its board-detection result (namespace `M5GFX`, key `AUTODETECT`) when it changes (`M5GFX.cpp:1109-1111`). Not observed on hardware.
4. **Keys:**
   - `Fn+3`/`Fn+4` produce `fn=true` plus `'3'`/`'4'` in the library, the same mechanism as the hardware-verified `Fn+1`/`Fn+2`; still unverified on hardware.
   - The library defines `KEY_OPT` as `0x00` (`Keyboard_def.h:18`), which may complicate capturing Opt as a modifier.

### 29-3. Documentation review: what `SendReport()` = true means

- **Previous wording** (`DEVELOPMENT_PLAN.md` §2.1-7, 2026-10-13): "`SendReport()` = true means the host read the report." Earlier Japanese records (`STAGE_1A.md` §5③) said the equivalent.
- **What the installed implementation shows** (m5stack:esp32 3.3.9):
  - `USBHID::SendReport()` returns true after `tud_hid_n_report()` succeeds and a semaphore is taken. That semaphore is given in `tud_hid_report_complete_cb()` (`libraries/USB/src/USBHID.cpp:326-369`).
  - TinyUSB documents that callback as "Invoked when sent REPORT successfully to host" (`class/hid/hid_device.h:150`).
  - So true means the HID IN transfer **completed at the USB level**: the host controller polled the endpoint and received the report.
  - It does **not** show that the host OS, the HID driver or the focused application processed the report.
  - The library clears a possibly stale completion before each send (`USBHID.cpp:355`). With one sending task, as in these sketches, the remaining ambiguity is small but not formally zero.
- **Correction:** `DEVELOPMENT_PLAN.md` §2.1-7 now uses this precise wording. The earlier Japanese records are left as written; read them with this clarification.
- **Firmware:** no change. The design decisions built on this result (retry when not ready, do not resend on an unknown outcome, resend button state until confirmed) remain valid under the precise meaning.

### 29-4. Files

- Edited (documentation only): `README.md`, `docs/DEVELOPMENT_PLAN.md`, this report.
- **Not modified:** firmware sources, binaries, checksum files, `LICENSE`, `LICENSES/`, hardware design files, images.

## 30. Development entry — 2026-10-13: Stage 1e-1 implemented and built (not hardware-tested)

| Item | Record |
| --- | --- |
| Scope | Stage 1e-1 only. Four profiles: 1 YouTube, 2 Spinner (both unchanged from 1d), 3 Video Editing (Right Arrow / Left Arrow / Space), 4 Custom (placeholder, no HID output, shows `NOT CONFIGURED`). Selection with `Fn+1`…`Fn+4`. No configuration UI, no NVS persistence |
| Source | `MediaJog_1e_1/MediaJog_1e_1.ino`, derived from `MediaJog_1d/MediaJog_1d.ino`; 97 insertions, 14 deletions. SHA-256 `38a695350d8ce169162db8a49e49f83cabe64049f13c7e6953dc03cb08b6ff3e` |
| App-only binary | `MediaJog_1e_1/build/MediaJog_1e_1.ino.bin`, 598,880 B, SHA-256 `31374dc0a292a5780718fc63c95cc360cb242a02e9077e11e3feb659933bbd8b` (`MediaJog_1e_1/SHA256SUMS.txt`) |
| Build | arduino-cli 1.5.1, `m5stack:esp32` 3.3.9, USB-OTG + CDC On Boot; libraries as in 1d. Exit 0; no sketch warnings; `#error` guards still effective |
| Unchanged vs 1d (byte-identical bodies) | `setup`, `loop`, `onUsbEvent`, `updateUsbState`, `sendKeyReport`, `sendRelease`, `tapKey`, `sendQueued`, `pushEvent`, `sendMouse`, `setMouseButton`, `setProfile`, `changeSensitivity`, `readRegChecked` |
| Hardware | **Not installed, not tested.** Awaiting GPT review. Checklist: `docs/STAGE_1E_1.md` §6 |
| Unverified | `Fn+3`/`Fn+4` on the ADV keyboard hardware; Profile 3 behavior in Premiere Pro / Final Cut Pro (no compatibility claimed) |
| Plan changes | The key-state model planned for 1e-1 is deferred to 1e-3 (HOLD), where it is first needed. The agreed **persistence timing policy** (profile: immediate on a confirmed switch; Custom: only on explicit Save; sensitivity: after a debounce interval) and acceptance tests AT-16–AT-21 are recorded in `DEVELOPMENT_PLAN.md` §6.8–§6.9 for later sub-stages |
| Preserved | `MediaJog_1a`–`1d`, `U135_Probe/`, `archive/`, `LICENSE`, `LICENSES/`, hardware and image files: SHA-256 unchanged. README not changed (Stage 1d stays the documented current firmware). No flashing, no Git, no publication |

## 31. Development entry — 2026-10-13: Stage 1e-1 hardware test results

§30's "not installed, not tested" was correct when written; this entry records the later test.

| Item | Record |
| --- | --- |
| Firmware | Stage 1e-1 (`MediaJog_1e_1`), installed through the Launcher. **SHA-256 of the uploaded file not recorded** (repository files: §30) |
| Host | Mac |
| PASS | Boot into Profile 1 · Profile 2 Spinner operation · `Fn+3` selects Profile 3 · Profile 3 sends Right Arrow, Left Arrow and Space · `Fn+4` selects Profile 4 · Profile 4 produces no HID output · Spinner → Profile 3 releases an active drag · Spinner → Profile 4 releases an active drag · Cardputer keyboard input does not leak to the Mac · Power-cycling from Profile 4 returns to Profile 1 (expected without persistence) |
| INCONCLUSIVE | Fast rotation followed by a profile switch: the author could not clearly tell whether additional key events remained after switching. **Neither PASS nor FAIL** |
| Not recorded / not tested | Launcher still present after install; Profile 1 regression in this build; Spinner → Profile 1 drag release in this build; re-selecting the current profile; Premiere Pro / Final Cut Pro (no compatibility claimed) |
| Newly confirmed on hardware | `Fn+3` and `Fn+4` are detected on the Cardputer ADV (previously source-checked only, `DEVELOPMENT_PLAN.md` §6.4) |

**Not every Stage 1e-1 test passed or was run.** Per-item detail: `docs/STAGE_1E_1.md` §10.

## 32. Development entry — 2026-10-13: Stage 1e-2 implemented and built (not hardware-tested)

| Item | Record |
| --- | --- |
| Scope | On-device configuration UI for Profile 4 (Custom). Three actions (CW, CCW, BTN), each one ordinary keyboard key with optional Ctrl/Shift/Alt(Option)/GUI(Command). **TAP only** (HOLD = 1e-3). **RAM only** (NVS = 1e-4). Profiles 1–3 unchanged |
| UI | In Profile 4, `Fn+S` opens CUSTOM SETUP. `1`/`2`/`3` or the knob select an action. In EDIT: type a key, or turn the knob through a 28-key list; `Fn+C/S/A/G` toggle modifiers; del clears; ok/knob press = done; esc = undo. On the overview: ok (Enter) = **Save**, esc (`` ` ``) = **Cancel** (discards the draft). Details: `docs/STAGE_1E_2.md` §2 |
| Library findings that shaped the design | `keysState()` is a state, not an event stream, so key presses are derived by comparing loops. With Shift/Ctrl/Caps, `word` holds the second legend; Fn does not change it. Tab/Backspace/Enter appear only as flags. **`KEY_TAB`/`KEY_BACKSPACE` are defined differently by M5Cardputer (`0x2b`/`0x2a`) and the core's `USBHIDKeyboard.h` (`0xB3`/`0xB2`)**, so the sketch uses the library's flags and never these macros. `KEY_OPT` is `0x00` |
| Source | `MediaJog_1e_2/MediaJog_1e_2.ino`, derived from `MediaJog_1e_1`; 591 insertions, 17 deletions. SHA-256 `cbde987a2325a001c6c5d468e6cf85da2c7a69f4c8c8ac7c9ff0695501e59a43` |
| App-only binary | `MediaJog_1e_2/build/MediaJog_1e_2.ino.bin`, 603,472 B, SHA-256 `65e95432a20dedf8202f587feb84d4d1ad389314f36133a29818128e4ee222ef` (`MediaJog_1e_2/SHA256SUMS.txt`) |
| Build | Same environment as 1d/1e-1. Exit 0 on the first attempt; no sketch warnings; `#error` guards effective |
| Host test (not hardware) | `baseChar`, `usageForChar`, `keyName` and `formatKey` were extracted verbatim and compiled on the Mac against the real library key table and TinyUSB constants, with signed and with unsigned `char`. All 48 printable keys passed and the longest label fits its buffer |
| Unchanged vs 1e-1 (byte-identical) | `setup`, `onUsbEvent`, `updateUsbState`, `sendKeyReport`, `sendRelease`, `tapKey`, `sendQueued`, `pushEvent`, `sendMouse`, `setMouseButton`, `setProfile`, `changeSensitivity`, `readRegChecked`, and the three spinner screen functions. Other edits are guards that apply only while the setup UI is open, plus the Profile 4 branches |
| Hardware | **Not installed, not tested.** Awaiting GPT review. Checklist: `docs/STAGE_1E_2.md` §6 |
| Known limitation found in review | For the `Fn+C/S/A/G` modifier toggles, Fn must be pressed first; pressing the letter first assigns that letter |
| Preserved | `MediaJog_1a`–`MediaJog_1e_1`, `U135_Probe/`, `archive/`, licenses, hardware and image files: SHA-256 unchanged. README unchanged. No flashing, no Git, no publication |

## 33. Development entry — 2026-10-13: Stage 1e-2 hardware test FAILED; investigation; revision r2 built

§32's "not installed, not tested" was correct when written.

| Item | Record |
| --- | --- |
| Hardware test 1 (author) | `Fn+4` selected `PROFILE 4 CUSTOM` and the screen showed `NOT CONFIGURED`. **`Fn+S` did not open CUSTOM SETUP; no configuration screen appeared.** SHA-256 of the uploaded binary not recorded. Result: **FAILED** |
| Library source (M5Cardputer 1.1.1) | `updateKeysState()` (`Keyboard.cpp:90-210`) has no Fn layer. With Fn held, `word` contains the key's first legend; only Ctrl/Shift/CapsLock select the second legend. `Fn+S` therefore yields `fn=true` + `'s'` by the same path as the hardware-verified `Fn+4`. The reader does not treat letters differently. **The library does not explain the failure** |
| Sketch source (1e-2) | `Fn+S` → `CHORD_SETUP` → `enterSetup()` (Profile 4 only) → `uiMode = UI_SETUP` → setup screen on the next loop. No exit path without Enter or `` ` ``. No conflict with other `Fn+S` handling. **No defect found in this path** |
| Defect found | The 1e-2 build labels itself **`MEDIA JOG 1e-1`** on the Spinner screen (stale label in an intentionally unchanged function; confirmed in the binary). The running build could not be identified on the device |
| Hypotheses (not established) | **A:** the device ran 1e-1 (consistent with `Fn+4` working and `Fn+S` doing nothing; the 1e-1 and 1e-2 Profile 4 screens differ, but the report does not say which lines were shown). **B:** on this unit, `S` with Fn is not delivered as `'s'` (not supported by the source; not excluded) |
| Correction | **Revision r2** (`MediaJog_1e_2_r2/`), identification and diagnostics only: build tag `1e-2r2` on every screen; fixed Spinner label; live key state (`key:Fn+s`) on the Profile 4 screen; serial log of recognised chords. The `Fn+S` logic and all HID functions are byte-identical to 1e-2. 37 insertions, 3 deletions |
| r2 build | Exit 0, no sketch warnings, guards effective. Source SHA-256 `ae8d062d6c11945a1bbf8c4861514268623ed2d9657ae82a4038df270c1bd16f`; app-only binary (603,616 B) `d657f465951f3eb4e2522443011c4e2486f77a6da3cda5fd79855fee026e30ae` (`MediaJog_1e_2_r2/SHA256SUMS.txt`) |
| Next | GPT review, then the r2 test in `docs/STAGE_1E_2.md` §10-3, which decides between A and B. Record the uploaded hash. `MediaJog_1e_2/` is kept unchanged as the build used in test 1 |

## 34. Development entry — 2026-10-13: Stage 1e-2 r2 hardware results; revision r3 built

| Item | Record |
| --- | --- |
| r2 hardware test (author) | **Hardware-verified:** build tag `1e-2r2` visible; `Fn+4` selects Profile 4; **`Fn+S` opens CUSTOM SETUP**; CW assigned A and saved; turning CW sends `a`; CW/CCW/BTN independently assigned A/B/C, all with the expected HID output; CANCEL discards unsaved changes; `Fn+G` adds GUI/Command; Command+A selects text on macOS; no unintended host typing during setup; the knob selects special keys; assignments survive switching between Profiles 1–4 while powered; after a power cycle the device starts in Profile 1 with the Custom assignments cleared (expected before 1e-4). SHA-256 of the uploaded file not recorded |
| Checklist status | Of the 19 items in `STAGE_1E_2.md` §6: 6 PASS, 1 PASS for typing only, 7 partial, 5 not tested (2 Profiles 1–3 regression, 4 `Fn+S` in Profiles 1–3, 7 release on entry, 12 clear/undo/done, 19 fast rotation). **Not passed as a whole.** Per item: `STAGE_1E_2.md` §11-2 |
| Test-1 failure (§33) | `Fn+S` works with r2, whose `Fn+S` code is byte-identical to 1e-2. The most consistent explanation is hypothesis A (a different build ran in test 1). This is an inference; the test-1 binary was never identified |
| Host-side note | With macOS Japanese input active, letter keys from the Custom profile may start a partial Japanese composition. This is expected IME behavior for HID key usages, **not a confirmed firmware defect**. The firmware does not change the host input method |
| Source/build verification (unchanged) | `Fn+S` path, release on entering setup, leak-proof design (source); key-mapping functions host-tested; r2 HID functions byte-identical to 1e-2 |
| Revision r3 | `MediaJog_1e_2_r3/`: on the CUSTOM SETUP **overview** only, assigned key names are drawn at the largest of text sizes 2 / 1.5 / 1 whose measured width fits (x 72–236). The longest possible name (25 characters) fits at size 1. The row highlight is preserved. Only `drawSetupScreen()` changed (overview-row loop); the EDIT screen, Profile 4 normal screen, key handling, HID and Profiles 1–3 are unchanged. Build tag `1e-2r3`. 29 insertions, 4 deletions vs r2 |
| r3 build | Exit 0, no sketch warnings, guards effective. Source SHA-256 `37f0698a2469e21b34b561587f47aacc036bb8453a2148754f02d37b160fc06c`; app-only binary (604,016 B) `a448c9574f9d2282bd15d6f1bdbd1b132678594786196528347a191ed35f447e` |
| Next | GPT review, then the r3 overview-screen test (`STAGE_1E_2.md` §12-4) and the untested checklist items. Record the uploaded hash |

## 35. Development entry — 2026-10-13: Stage 1e-3 (Custom HOLD) implemented and built (not hardware-tested)

| Item | Record |
| --- | --- |
| r3 overview (author) | The enlarged key labels on the CUSTOM SETUP overview are appropriately sized (author confirmation). The r3 design is kept unchanged in 1e-3 |
| Scope | `MediaJog_1e_3/` (from r3, unchanged): Profile 4 BTN can be TAP or HOLD (`Fn+H` in EDIT BTN); CW/CCW stay TAP only; RAM only. Build tag `1e-3` |
| Architecture | Keyboard reports are built from a base state (held Custom key/modifiers) plus the tap, and return to the base after a tap. HOLD down/up are queued in order with taps; a HOLD up is never dropped. Safety release, immediate and outside the queue, on profile change, setup entry, U135 loss and USB not ready; no auto-resume. Modifier-only actions are now valid. Details: `docs/STAGE_1E_3.md` |
| Verification (source/build/host only) | Per-function comparison against r3: 6 new functions, 14 changed (4 of them only gain a `releaseHoldNow()` call, 1 comment only), 36 unchanged. Host test of the real report functions: 16/16, including HOLD Shift + Right → Shift+Right and HOLD Command + Right → Command+Right, and byte-identical reports with nothing held. **Not hardware-tested** |
| Build | Exit 0, 0 sketch warnings (known library `KEY_TAB` warning only), guards effective. Flash 604,827 B (46%), RAM 60,196 B (18%). Source SHA-256 `23054acf8b6721210d6ab65d96d59ab326de90819bf33f01cd5fbf3139241486`; app-only binary (604,976 B) `0d982651cbe1d5994e06619e654380ce97e3d0586d50714d69ca8ed220927506` |
| README | Added note (Japanese) that SETUP shows letters as uppercase key names and that Shift must be added to type an uppercase letter |
| Next | GPT (Edgar) independent review, then `STAGE_1E_3.md` §7 on hardware. Record the uploaded hash |

## 36. Development entry — 2026-10-13: Stage 1e-3 hardware results (prioritized tests)

| Item | Record |
| --- | --- |
| Setup | Cardputer ADV + U135 on a Mac; Stage 1e-3 installed by the author. Uploaded binary SHA-256 not recorded |
| Hardware-verified | (1) TAP/HOLD selector in CUSTOM SETUP works. (2) BTN = HOLD Shift, CW = Right: holding the knob and turning clockwise selects text character by character (combined HOLD + TAP report). (3) Releasing the knob restores normal cursor movement without selection. (4) A profile switch while HOLD Shift is active releases Shift; no stuck modifier observed |
| Scope | These are the **prioritized tests requested by Edgar**, not the full H1–H19 checklist. Status per item: `STAGE_1E_3.md` §10-2 (H6, H8 PASS; H11 partial; the rest not tested) |
| Not hardware-verified | Command+Right; release on `Fn+S`, USB unplug and U135 loss; no auto-resume after reconnect; Profiles 1–3 regression; Cancel/undo/`del`; fast rotation. These remain source-reviewed and host-tested only |

## 37. Development entry — 2026-10-13: Stage 1e-4 (NVS persistence) implemented and built (not hardware-tested)

| Item | Record |
| --- | --- |
| Scope | `MediaJog_1e_4/` (from 1e-3, unchanged): last profile, Custom mappings incl. BTN TAP/HOLD, and Spinner sensitivity stored in NVS (namespace `mediajog`, key `cfg`), restored at power-on. Build tag `1e-4`. Details: `docs/STAGE_1E_4.md` |
| Record | 21 bytes: magic `MJCF`, version 1, length, profile, sensitivity index, 3 × (modifiers, usage, flags), CRC-32. Applied only if every check passes; otherwise all defaults. No HID state, queue, USB/U135 state or setup draft is stored |
| Write policy | Profile at once after a confirmed switch; Custom only on Save; sensitivity 2 s after the last step. Each trigger changes only its own fields; identical records are skipped; writes run after the HID sends of the loop. Failed writes are shown (`NV WFAIL` / `NV FULL`), not retried automatically, and never reported as stored |
| NVS safety (source review) | Only `nvs_open/get_blob/set_blob/commit/get_stats/close` on the own namespace; no erase call. Boot only reads. A newer-version record or a read error makes the session read-only. A write needs ≥ 32 available entries. **Pre-existing risk recorded, not removed:** the Arduino core erases the whole NVS partition before `setup()` on `NO_FREE_PAGES`/`NEW_VERSION_FOUND` (`esp32-hal-misc.c:331-344`), in every Arduino app including 1a–1e-3; it cannot be disabled from the sketch (`STAGE_1E_4.md` §6.3). For Edgar and the author to accept before the hardware test |
| Verification (source/build/host only) | Per-function comparison against 1e-3: 15 new, 11 changed (trigger lines, two calls, status drawing), 45 unchanged incl. all HID/HOLD/USB/mouse functions. Host test with a mock NVS backend: 47/47 (19 corruption cases, all 168 single-bit flips, read/write failures, policy). **Not hardware-tested**; device NVS was not touched |
| Build | Exit 0, 0 sketch warnings (core/library warnings identical to 1e-3), guards effective. Flash 608,179 B (46%), RAM 60,228 B (18%). Source SHA-256 `04d0f590db626700b5c6578096d81169d52832a582874dda2f484e88c89018f5`; app-only binary (608,320 B) `8db62fc43dfce3592b510ac232c3588a5a5fb6580c0a3b7d20105c465195d61c` |
| Next | GPT (Edgar) independent review, including the residual NVS risk; optional flash backup by the author; then `STAGE_1E_4.md` §9 on hardware. Record the uploaded hash |

## 38. Development entry — 2026-10-13: Stage 1e-4 hardware results (author report); USB release candidate

| Item | Record |
| --- | --- |
| Reported as working (author) | YouTube, Spinner movement and drag, Video Editing, CUSTOM CW/CCW/BTN, HOLD Shift press/release, profile switch during HOLD releasing the key, CUSTOM Cancel, **NVS save and restore after restart**, **Launcher Wi-Fi settings kept**, **SOUND TERRARIUM works normally**. No known functional defects |
| Precaution | Whole 8 MB flash backed up with esptool before installing 1e-4 (author). The backup is not a reason to allow destructive operations |
| Scope | Summary report; no per-item mapping to `STAGE_1E_4.md` §9 (N1–N15) was given, so no item is individually marked. Uploaded binary SHA-256 not reported |
| Decision (author) | **Stage 1e-4 is fixed as the USB release candidate.** BLE is explored in separate firmware |

## 39. Development entry — 2026-10-13: BLE-1 (minimal BLE HID prototype) implemented and built (not hardware-tested)

| Item | Record |
| --- | --- |
| Scope | `MediaJog_BLE_1/`: separate BLE-only firmware; 1e-4 unchanged. Core BLE library 3.3.9 (NimBLE configuration), `BLEHIDDevice` + `BLESecurity`, no third-party library. Keyboard (Report ID 1) + mouse (Report ID 2, prepared; sends only the released state). CW Right, CCW Left, press Space (TAP). No USB HID; USB-C for power and the CDC log. Details: `docs/BLE_1.md` |
| Pairing | Just Works with bonding and LE Secure Connections (as the core's `Server_Gamepad` example); bonds stored by NimBLE (`nimble_bond`); advertising restarts after a disconnect |
| HID safety | Reports only when connected, encrypted and keyboard-subscribed; inputs dropped otherwise; all-up at every link-up, repeated once after 100 ms; full state clear on any link loss (incl. a reconnect between loops); `notify()` outcome read from `onStatus()` (accepted by the stack, not delivery-confirmed) |
| NVS | Sketch only reads `nvs_get_stats()`. NimBLE store and `BLEDevice::init()` (NimBLE path) contain no partition erase (source and symbol check). PHY calibration storage is enabled; growth is shown on screen for measurement |
| Verification (source/build/host only) | Host test of the real link/report functions with mocks: 21/21. **Not hardware-tested** |
| Build | Clean build exit 0, 0 sketch warnings, guards effective. Flash 862,191 B (65%; +254,012 B vs 1e-4), RAM 65,412 B (19%). Source SHA-256 `1abae1f95da45d951d8192c37a599802c743b2c25c3ee607b9136b19fb656ad3`; app-only binary (862,336 B) `faaaa40fdc056882fe03bdb2065e3ce578070a07f33f6a17d6fa2bee7de459bb` |
| Next | GPT (Edgar) independent review, then `BLE_1.md` §8 on hardware. Record the uploaded hash and the NVS line |

## 40. Development entry — 2026-10-13: BLE-1 fixed after Edgar's review 1 (not hardware-tested)

| Item | Record |
| --- | --- |
| Findings (Edgar) | (1) `sendMouse()` subtracted movement before the report was accepted. (2) The 100 ms all-up repeat after connection ran only with an empty queue and was not retried, inconsistent with the design text |
| Fix (in place; BLE-1 never installed) | (1) Movement subtracted only after `SUCCESS_NOTIFY`. (2) The repeat sets `releasePending` whatever is queued, so it is sent before any further tap and retried until accepted; the block moved unchanged into `serviceReports()`. Reviewed source archived in `archive/MediaJog_BLE_1_review1/` (SHA-256 `1abae1f9…56ad3`) |
| Verification | Clean build exit 0, 0 sketch warnings. Host test 36/36 (15 new: connection-time input, repeat with queued taps, rejected repeat and initial all-up, mouse movement on rejection); the new tests fail 7 checks on the pre-review source |
| Hashes | Source `40bde390f0431dd446cc5b62c67bea907dc623797ae3d2df725f8bcbe64898c0`; app-only binary (862,336 B) `3196bffdf19fd4902ae3247313f95ca661a76e5190eb07483f3fadb2a47730e0`. The pre-review binary (`faaaa40f…`) is superseded |
| Next | Edgar's confirmation, then `BLE_1.md` §8 on hardware |

## 41. Development entry — 2026-10-13: BLE-1 hardware results; B7 investigation; BLE-1 r2 (diagnostics) built

| Item | Record |
| --- | --- |
| Hardware (author) | BLE-1 (`3196bffd…`): **B1, B2, B3, B4, B6 PASS** (advertising, first pairing, Right/Left over BLE, one space per press without repeat, reconnection after Cardputer restart). **B7 FAIL**: after Mac Bluetooth OFF → ON the Mac shows "connected" but the Cardputer stays at `BLE SUBSCRIBING` (`SKIP (no link)`); a Mac-side "Disconnect" leads to an automatic reconnect into the same state. Uploaded SHA-256 not reported |
| Facts (source) | `BLE SUBSCRIBING` = connected + encrypted + no keyboard CCCD subscribe event for the current connection; BLE-1 has no other path to READY and no timeout. NimBLE (esp-nimble 5c1a43c): RESTORE events only when security is restored from a stored bond; CCCD records keyed by the connection's peer address; a CCCD write of 0 on a bonded link deletes the record; TERM events precede DISCONNECT. Library: same advertising/GAP path at boot and after disconnect; `resetGATT()` not triggered in BLE-1; repeat pairing deletes the old bond with its CCCDs. `SKIP (no link)` did not distinguish a missing CCCD |
| Hypotheses (open) | H1 CCCD record not found under the reconnection key; H2 the Mac unsubscribed when Bluetooth was turned off (record deleted); H5 the Mac re-paired (bond replaced, no RESTORE); H3 subscribe for another handle. Not established; no behavior-changing fix made |
| BLE-1 r2 | `MediaJog_BLE_1_r2/` (BLE-1 unchanged): read-only GAP event trace (`EV` line + Serial), CCCD/bond snapshot at encryption (`ENC` line), handle invalidation on disconnect with a foreign-subscribe counter, SKIP texts by cause. Security, bonding, NVS and HID behavior unchanged |
| Verification | Clean build exit 0, 0 sketch warnings; flash 864,287 B, RAM 66,044 B. Host test 48/48 (36 earlier + 12 new). Source SHA-256 `8e5f824bb27de0f8a497a76a08c8795b26ed57f6be34a6e74e45a6721c7943a8`; app-only binary (864,432 B) `69962f8268eedf233268b7ff5b2e721420d42e99e9504e239cbe801682af488c` |
| Next | Edgar's review; then the r2 procedure (`BLE_1.md` §11-6) to decide between the hypotheses; then a targeted fix |

## 42. Development entry — 2026-10-13: r2 diagnosis of B7; BLE-1 r3 (connection-resolved diagnostics) built

| Item | Record |
| --- | --- |
| Hardware with r2 (author, photos) | Start: `BLE READY`, `EV C1 E0 SkR1 SkR1`, `ENC … cc3/1/1 b1`. After Mac Bluetooth OFF → ON: Mac "connected"; Cardputer `BLE SUBSCRIBING`, `SKIP (no CCCD)`, kb=0, ms=0; `EV E0 SkR0 SkR1 SkR1 C1 E!0D`; `ENC id0..D0B0 ota0..D0B0 cc3/1/1 b1` |
| Findings (source) | `E!0D` = `ENC_CHANGE` with a non-zero status whose low byte is 0x0D; r2 dropped the high byte (candidates `0x00D` `BLE_HS_ETIMEOUT`, `0x20D` HCI, `0x50D`/`0x40D` SM). The `SkR1` tokens precede the last `C1`, whose connect cleared `kbSubscribed`; no subscribe event followed, and NimBLE restores CCCDs only after a successful restored encryption. The `ENC` snapshot comes from the last successful encryption and shows a public peer address (H1 weakened). Unexplained: encrypted although the last encryption failed; `C1` without a disconnect in between; `SkR0` |
| Status | **Root cause not established.** Most supported chain: failed security procedure on reconnection → no restore → no keyboard subscription. Source fact for follow-up: the BLE library sends a peripheral Security Request at every connection (`m_forceSecurity` default true; 30 s SM timeout) |
| BLE-1 r3 | `MediaJog_BLE_1_r3/` (r2, BLE-1 unchanged). Diagnostics only: records with sequence, time, connection generation, handle, full code, link flags; more NimBLE events (link established, pairing complete, MTU, update); app-level records (connect/disconnect/subscribe as handled, ready/lost); 64-record frozen trace pages on the `t` key; open-connection count. Top-right tag `B1r3` right-aligned; `BLE-1r3` kept as build id (Serial at boot) |
| Verification | Clean build exit 0, 0 sketch warnings; flash 869,735 B, RAM 68,692 B. Host test 53/53. Source SHA-256 `0c63dcc25cbb123c42b65a13145e80c9e44311cb6c3e8ffd7b01e59cc468662c`; app-only binary (869,888 B) `2c6a0cd718da062fc29a4a2f11d860d70595b7fd42a49fdbb3d9ed556354abab` |
| Next | Edgar's review; one-shot r3 run (`BLE_1.md` §12-6); then a fix chosen from §12-7 only if the trace supports it. B7 is not fixed |

## 43. Development entry — 2026-10-13: B7 cause found with the r3 trace; BLE-1 r4 built (not hardware-tested)

| Item | Record |
| --- | --- |
| r3 hardware (author) | Already at start: `BLE SUBSCRIBING`, kb=0. Trace: `ENC ok`, `SUB … R0`, `SUB … R1`, `aSUB k1 FGN`, `aCON` (≈1.7–1.8 s), `MTU 256`, `UPD ok`, `ENC !D` at ≈31.8 s; n1, lost0 |
| Cause (source + host test) | NimBLE (esp-nimble 5c1a43c) posts CONNECT for a peripheral link only after the remote version and feature reads (`ble_gap_rx_rd_rem_sup_feat_complete`), while the connection callback already receives `ENC_CHANGE` and the CCCD RESTORE. BLE-1 learned the handle only in `onConnect()`: the RESTORE was rejected as foreign (`bleConnHandle` still `0xFFFF`) and `onConnect()` then cleared the flags. Reproduced with r3's functions. Explains the r2 B7 photo as well |
| `ENC !D` | 0x00D `BLE_HS_ETIMEOUT`, 30 s after the late CONNECT: the library's Security Request (`m_forceSecurity` default) on an already-encrypted link times out. Not an encryption failure; no effect on BLE-1's state. Not changed |
| BLE-1 r4 | `MediaJog_BLE_1_r4/`: adopt the handle of an early subscribe event if no live connection is registered and NimBLE knows the handle as open; keep its flags at CONNECT; disconnect affects only the current link; a failed connect drops an adoption. Handle check kept (other handles still rejected). Security, bonding, NVS, HID unchanged. Tag `B1r4`, build id `BLE-1r4` |
| Verification | Clean build exit 0, 0 sketch warnings; flash 870,083 B, RAM 68,692 B. Host test 70/70 (17 new). Source SHA-256 `28c8c5ade351c56cda8ef98ba95588ae994c0e3baba7901a16b41a233760ae52`; app-only binary (870,224 B) `02be4af083defdec297bad2375d5d010bb2a1aa43963ffc5755696c87e4e13f6` |
| Next | Edgar's review; hardware acceptance `BLE_1.md` §13-6 (start, Mac Bluetooth OFF→ON, HID). B7 is not declared fixed before that |

## 44. Development entry — 2026-10-13: BLE-1 r4 passed on hardware; Stage 2a (USB/BLE integration) design

| Item | Record |
| --- | --- |
| r4 hardware (author) | Start → `BLE READY`; Mac Bluetooth OFF→ON 3× automatic reconnection; rotation after reconnection; Cardputer restart → automatic connection, rotation; one Space per press. **B7 fixed on hardware** for these cases. Not reported: start repetitions, Mac "Disconnect", uploaded SHA-256 |
| Stage 2a | Design only (`docs/STAGE_2A_DESIGN.md`); no integration code. Facts: identical keyboard/mouse report layouts on both transports; 1e-4's USB dependence limited to three send calls, `updateUsbState()`/`onUsbEvent()`, 8 `usbReady` reads and two display lines; the USB and BLE delivery results map onto one SENT / NOT_SENT / UNKNOWN rule that reproduces both existing behaviors. Probe build (scratchpad) of 1e-4 + BLE stack: exit 0, no new warnings, flash 882,595 B (67%), RAM 65,828 B |
| Proposals | Runtime switch with lazy BLE start (no restart); `Fn+B` CONNECTION screen; one active backend; all-up on the old backend before switching; transport mode in a separate NVS key `link` (cfg v1 unchanged); r4 BLE functions moved unchanged. Decisions D1–D8 open for Edgar and the author |
| Next | Edgar's design review; implementation only after approval |

## 45. Development entry — 2026-10-13: Stage 2a (USB/BLE integrated) implemented, built and host-tested (not hardware-tested)

| Item | Record |
| --- | --- |
| Design review | Edgar: conditional approval, D1–D8 adopted. Conditions applied in `STAGE_2A_DESIGN.md`: release on the old transport within 200 ms (BLE + 100 ms flush), warning, priority release when READY again, no carry-over, no host-side guarantee; BLE mode shows `USB HID INACTIVE`. Added during implementation: a release is needed only when the old transport has a host link (no false warning on battery) |
| Implementation | `MediaJog_2a/` from 1e-4 (54 of 71 functions byte-identical; 17 intended changes) plus the BLE backend cut out of BLE-1 r4 (27 functions/classes byte-identical, including the CCCD-before-CONNECT handling). New: SENT / NOT_SENT / UNKNOWN backends, one active transport, common link up/down, switch state machine, owed release, lazy BLE start, `link` NVS record (cfg v1 unchanged), CONNECTION screen (`Fn + B`), tag `USB 2a` / `BLE 2a` with `!` |
| Host tests | End to end on the Arduino-preprocessed sketch with stubs: **66/66** (18 scenarios); mutation check (r3 behavior → 6 failures, no adoption → 9, double send → 3, …); 1e-4 NVS suite 47/47; 1e-3 HOLD suite 16/16 |
| Build | Clean build exit 0, 0 sketch warnings, guards effective. Flash 896,127 B (68%), RAM 69,228 B. Source SHA-256 `819b2e484f5a972ca25a5e15533ece9bf31d90d381ec9cd6c2f6a24436f8ce39`; app-only binary (896,272 B) `ddd52d997e57e000ea35eca6e54f6aa7cd0852153589b9eb5069b4ff58a11c33`. 1e-4 and BLE-1 r4 unchanged (checksums OK) |
| README | Updated to the current state (USB release candidate 1e-4, BLE-1 r4 prototype, Stage 2a not hardware-tested); the Japanese SETUP note kept verbatim in the CUSTOM section |
| Next | Edgar's code review of `MediaJog_2a/` and the diff; then `STAGE_2A.md` §7 on hardware |

## 46. Development entry — 2026-10-13: Stage 2a code review 1 — owed-release completion fixed

| Item | Record |
| --- | --- |
| Review (Edgar) | Minor fix, then hardware testing. Approved: integration design, common backends, r4 port, NVS compatibility, and the "no warning without a host link" change |
| Finding | `noteReleaseSent()` was also reached from `sendRelease()` (base report, may carry a CUSTOM HOLD) and `sendMouse()` (may carry the drag button), so a non-release report could complete an owed release |
| Fix (in place; never installed) | Counted only for an all-keys-up base (`kbHoldMods == 0 && kbHoldUsage == 0`) and a mouse report with `buttons == 0`. HOLD behavior and send order unchanged. Reviewed source archived in `archive/MediaJog_2a_review1/` (`819b2e48…`) |
| Tests | End to end 85/85 (19 new: re-activation of the owed transport with release before input, HOLD base not a release, drag not a release, warning clears only after keyboard and mouse, BLE owed with mouse CCCD); the pre-fix source fails exactly the 2 targeted checks. NVS 47/47, HOLD 16/16 |
| Build | Exit 0, 0 sketch warnings, guards effective; flash 896,179 B. Source SHA-256 `eb42955bffba28afcc46c232db3c02d31b40bc8efbd33d69da781d1a2f0c7fc7`; app-only binary (896,320 B) `0b0dfc44cf5cd5da20304e650750308de5bace475d2fd31a5d728981d347deb1` |
| Next | Hardware test, `STAGE_2A.md` §7 (install `0b0dfc44…`) |

## 47. Development entry — 2026-10-13: Stage 2a hardware test (BLE mouse fails); cause; BLE-M1 test build

| Item | Record |
| --- | --- |
| Stage 2a hardware (author) | USB Profiles 2/3, BLE Profile 3, BLE↔USB switching without restart, transport restore after restart: PASS. **BLE Profile 2: FAIL** (`kb=1 ms=0`, pointer still, `rep` 0; also after Mac Bluetooth OFF→ON). Trace `… ENC ok, SUB x R0, SUB k R1, aSUB k1 ok, SUB x R1, RDY` |
| Cause (source) | The 3.3.9 BLE library (NimBLE) keeps only the first characteristic of a UUID per service; `BLEHIDDevice`'s keyboard output and mouse input reports (0x2A4D) are never registered (handle 0xFFFF). Also in upstream master. BLE-1 r4 has the same construction (it never sent mouse reports on hardware). The 2a host-test stub did not model this; corrected |
| Decision (Edgar) | Proposal A first (second HID service for the mouse) as a separate test build; Stage 2a unchanged until the result |
| BLE-M1 | `MediaJog_BLE_M1/` from BLE-1 r4: keyboard via `BLEHIDDevice` (keyboard-only map; B7 functions byte-identical), mouse in its own HID service (instance 1) with HID Information, Report Map (Report ID 2), Control Point, Report + Report Reference; `m` = KEYS/MOUSE, `g` = Service Changed indication; handles and CCCDs on screen. Tag `BM1` |
| Verification | Stub models the duplicate drop and reproduces `mouse handle 0xFFFF` for BLE-1 r4 and 2a. BLE-M1 end to end 33/33; mutant with the mouse in the keyboard's service fails 8. Clean build exit 0, 0 sketch warnings; flash 871,427 B. Source SHA-256 `7e98f370c742a8e99792c147dadf0f31e1443df612aed9d3130ff07baacbf13b`; app-only binary (871,568 B) `90bc4e5510af1f36c152e7b5b482b3b0d0365457df5cf69b7a68d274cc1c0861` |
| Next | Hardware procedure `BLE_M1.md` §5 (no bond deletion; Mac-side re-pairing only with approval); report the outcome to Edgar before integration or proposal B |

## 48. Development entry — 2026-10-14: BLE-M1 hardware result 1 (`ms hFFFF`); cause analysis

| Item | Record |
| --- | --- |
| Hardware (author, checked by Edgar) | BLE-M1 installed. Status line at start: `kb h10 c0`, `ms hFFFF c0`. No re-pairing, no bond or NVS change |
| Cause (source) | `BLEHIDDevice::startServices()` also calls `BLEServer::start()` → `ble_gatts_start()`, the only point where NimBLE assigns handles. BLE-M1 creates and starts the mouse service after that, so its definition is queued but never registered (handle 0xFFFF). Side effect: `m_svcChanged` is set, so the first disconnect would trigger the library's `resetGATT()`, a runtime GATT rebuild; the screen's handles are a boot snapshot |
| Test gap | The host stub assigned handles per service start and did not model the server-start latch |
| Proposed minimal fix (not implemented) | Start device info, keyboard HID and battery services, then the mouse service, then `bleServer->start()` (keyboard handles unchanged); live handles in the status line; stub models the latch and must reproduce `ms 0xFFFF` on BLE-M1. New folder `MediaJog_BLE_M1_r2` |
| Next | Edgar's review of the fix plan (`BLE_M1.md` §7); no code or build before that |

## 49. Development entry — 2026-10-14: BLE-M1 r2 (mouse service before the GATT server start)

| Item | Record |
| --- | --- |
| Review (Edgar) | Cause analysis and minimal fix approved; implement as a separate test build |
| Library check | `BLEHIDDevice::startServices()` = device info / HID / battery `start()` + `BLEServer::start()`; r2 makes the same calls with the mouse service queued before the server start. No initialization lost |
| Change | `bleBegin()` order; `createMouseService()` returns its start result; status line reads handles live and shows `s<mouse start>`; tag `BM1r2`, build id `BLE-M1r2`. Everything else as BLE-M1 |
| Tests | Stub models handle assignment at the single server start. BLE-M1 reproduces mouse 0xFFFF and "service changed"; r2 gives the mouse a handle and keeps the keyboard handle as BLE-1 r4. r2 end to end 43/43; BLE-M1 on the corrected stubs fails 8; mutants: mouse queued first fails 3 (keyboard handle moved), BLE-M1 order fails 12 |
| Build | Exit 0, 0 sketch warnings, guards effective; flash 871,591 B. Source SHA-256 `315cc7cf3bb5f97fbe668426229fb64d750876c157b62b4488d00792ff7dd6ff`; app-only binary (871,744 B) `252d61419a715b053b886d69fd46ba0a96ee53c2e317951540e0ebb63cf8e47e` |
| Next | Edgar's review; then hardware per `BLE_M1.md` §8.5 |

## 50. Development entry — 2026-10-14: BLE-M1 r2 hardware — Mac drops the link before encryption (analysis)

| Item | Record |
| --- | --- |
| Hardware (author) | `ms h2D` (mouse registered). Repeated `CONN ok`, `aCON new`, `LEST ok`, `ENC !7`, `DISC 213`, `aDIS cur 1` |
| Codes (hex) | `ENC !7` = `BLE_HS_ENOTCONN`: the pending security procedure was cancelled by the disconnect. `DISC 213` = 0x213 = HCI 0x13 Remote User Terminated: the Mac closed the link. No key error, no pairing: the Mac never started encryption |
| B7 | Not involved (CONNECT first, nothing restored; B7 functions unchanged and behaving as designed) |
| Hypothesis | r2 is the first build that changes the GATT table cached by the bonded Mac (no Database Hash in this NimBLE build); alternatively macOS rejects two HID services. Not distinguishable from this trace |
| Proposed next steps | Trace times, `DISC` E/B flags, ENC line (`b`, `i`/`o`), `kb` handle; reinstall BLE-1 r4 to check that the bonds are intact; Mac re-pairing with r2 only with approval. No code change (`BLE_M1.md` §9) |

## 51. Development entry — 2026-10-14: BLE-1 r4 reinstalled — `BLE ADVERTISING`, `bond=0`; bond store analysis

| Item | Record |
| --- | --- |
| Hardware (author) | r4 stays advertising, `bond=0`; Mac lists "MediaJog BLE" as known, not connected |
| `bond=0` | Bonded flag of the **current connection** (`sec_state.bonded`); always 0 without a connection. Not the bond count. The stored count (`b<n>` on the ENC line) is read only at an encryption event |
| Bond deletion paths (source) | Repeat pairing, store-full overflow on a new bond/CCCD write, client repeat pairing; no `ble_store_clear` / unpair / NVS erase anywhere. None was triggered (no pairing, no encryption, no CCCD write). The app install does not touch NVS |
| Assessment | No fault shown on the Cardputer side. The Mac does not attempt to connect (Mac-side state after the r2 failures) |
| Next | Mac Bluetooth OFF → ON, then **Connect** (not Forget) with r4; decide re-pairing from the trace (`BLE_M1.md` §10). Re-pairing only with approval, and with the build the next test needs |

## 52. Development entry — 2026-10-14: BLE-M1 r2 hardware 2 (`ms c0`); proposal B design

| Item | Record |
| --- | --- |
| Hardware (author) | `BLE READY`, `bond=1`, `kb h1D c1`, `ms h2D c0`; no pointer movement in MOUSE mode; `t` reported as showing no change |
| Edgar | Proposal A does not work as implemented; proposal B next (design first) |
| Analysis | Mac never subscribed the mouse report (`c0`), so nothing is sent. No HIDS/HOGP error found in the second service; likely macOS does not use a second HID instance, or it works from a stale cache. New: the library orders characteristics and services by object pointer, so handles depend on heap layout (`kb h10` on M1, `h1D` on r2). `t`: code unchanged from r3/r4 and local only; the trace page is a frozen snapshot by design; no conflict with HID sending (`BLE_M1.md` §11) |
| Design | `docs/BLE_B1_DESIGN.md`: static NimBLE C table (DIS, one HID service with kb in/out + mouse in, battery), subscribe via the existing GAP listener into the unchanged `handleSubscribe()`, notify via `ble_gatts_notify_custom()`; B7 functions byte-identical; independent BLE-only build `MediaJog_BLE_B1/`; one Mac re-pairing expected (approval) |
| Next | Edgar's review of the design; no code or build before that |

## 53. Development entry — 2026-10-14: BLE-B1 (proposal B) implemented — one HID service, static NimBLE table

| Item | Record |
| --- | --- |
| Approval (Edgar) | Implement B1 after two source checks |
| Source checks | (1) Table queued after `createServer()` (which resets GATT), registered by the single `ble_gatts_start()` in `BLEServer::start()`; the library's start loop, SUBSCRIBE and NOTIFY_TX paths only touch library objects (none); `resetGATT()` cannot trigger. (2) NimBLE calls all GAP listeners (newest first) for every SUBSCRIBE reason, before the library handler; the B1 listener applies the library's `conn_find` check and value and calls the byte-identical `handleSubscribe()` with the event's connection handle. No conflict |
| Build | `MediaJog_BLE_B1/` (tag `BB1`): DIS + one HID service (info, r4 map, control point, protocol mode, kb in `{1,1}`, kb out `{1,2}`, mouse in `{2,1}`) + battery; values and flags as r4 effectively registered; notify via `ble_gatts_notify_custom()`. 46 functions identical to r4, including all B7 functions. Clean build exit 0, 0 sketch warnings, guards effective; flash 864,379 B |
| Tests | New C-API stubs. B1 end to end 55/55 (table, B7 order with both CCCDs, first pairing, mouse, release, disconnect/reconnect, FGN, trace order, conn_find check, host error and mbufs, host writes). r4 / 2a still reproduce mouse 0xFFFF. 7 mutants: 15, 15, 2, 30, 1, 30, 9 failures |
| Checksums | Source `7f5ab35dec573d51e9b373946b96654d1715fa60e14278248872ff60de32c95b`; app-only binary (864,528 B) `d550cc5534aea4f199e27e9d225aaa69753ad5e4953a697c4694c8525660812e` |
| Deviations for review | No `READ_ENC` / no `startSecurity` (as r4 effectively), PnP bytes as r4, read-only descriptors (`BLE_B1_DESIGN.md` §6) |
| Next | Edgar's review; then hardware per `BLE_B1.md` §5 (re-pairing only with approval) |

## 54. Development entry — 2026-10-14: BLE-B1 hardware PASS; Stage 2a-2 integration design

| Item | Record |
| --- | --- |
| BLE-B1 hardware (author; Edgar: PASS) | Three reports in one HID service registered; new pairing; `ki c1`, `mi c1`; pointer and drag; Mac Bluetooth OFF→ON 3/3; restart with automatic reconnection and both CCCDs restored |
| Design | `docs/STAGE_2A_2_DESIGN.md`: new folder (proposed `MediaJog_2a_2`); B1's table, access callbacks, subscribe listener and `notifyReport` copied verbatim into a copy of 2a; `bleBegin` as B1; USB, transport, profiles, HOLD, NVS unchanged; B7 functions identical (r4 = 2a = B1) |
| Main risks | BLE mouse paths in switching and owed release run on hardware for the first time; NimBLE CCCD store limit (8) with 3 records per host, so a third bonded host could evict the oldest peer's bond (decision needed); old builds must not be reinstalled after pairing with the new table |
| Next | Edgar's review of the design and decisions (§8); no code or build before that |

## 55. Development entry — 2026-10-14: Stage 2a-2 implemented (BLE-B1 table in Stage 2a)

| Item | Record |
| --- | --- |
| Decisions (Edgar) | `MediaJog_2a_2` / `2a-2` / `USB 2a2`, `BLE 2a2`; CCCD option (a): at most 2 BLE hosts (a third bonded host can make NimBLE delete the oldest other bond: known limitation); B1 permissions, PnP and descriptors kept; B7, registration order, map and CCCD handling identical to B1; Mac's B1 bond reused, no re-pairing |
| Changes | From Stage 2a: B1's `GattValue` objects, table, access callbacks, `registerGattTable`, `onSubscribeEvent`, `notifyReport`, `bleBegin` verbatim; `ReportCallbacks` / `BLEHIDDevice` removed; screen tag and CONNECTION handle line. 130 functions identical to 2a; B7 functions identical to r4 and B1 |
| Verification | GATT identity script: all blocks identical to B1; stub handles identical to B1. Stage 2a suite ported 85/85; new scenarios 32/32 (lazy single registration, B7 with both CCCDs, switch while dragging, owed mouse release, HOLD on BLE, listener checks, registration error, NVS writes); NVS 47/47; HOLD 16/16; r4/2a still reproduce mouse 0xFFFF; 8 mutants fail 1–43 checks |
| Build | Exit 0, 0 sketch warnings, guards effective; flash 889,511 B. Source SHA-256 `9ec07fa4ad6bea1053d7bd4b2b15ce46d4e62e27e2755a62892bc19a112fbbd9`; app-only binary (889,664 B) `4339eed644f01468548f6444501c287e74c14e5bd1d1b5de1448abdd42a8019b` |
| Next | Edgar's review; then hardware per `STAGE_2A_2.md` §5 (no re-pairing; do not reinstall older BLE builds afterwards) |

## 56. Development entry — 2026-10-14: Stage 2a-2 H0 — no BLE connection (analysis)

| Item | Record |
| --- | --- |
| Hardware (author) | 2a-2 in BLE: `BLE ADVERTISING`, `ki10 ko21 mi24` (= B1), `kb=0 ms=0`; Mac lists the device, not connected; Mac Bluetooth OFF→ON and Cardputer restart: no change. No unpairing, erase or re-pairing |
| Source comparison | `bleBegin`, all GAP / B7 / security / subscribe functions and `updateLinkState` byte-identical to B1; the transport layer touches GAP only through `bleStop()` / `bleStart()` at a user switch. No difference in the path from boot to an incoming connection |
| Screen | `CFG RESTORED` = boot settings load; `SKIP (no link)` = knob input while not connected; `BLE ADVERTISING` = no connection registered, does not prove advertising (start result not shown) |
| Assessment | Most consistent: the Mac does not start a connection (same symptom once before with r4, `BLE_M1.md` §10); advertising not on air not excluded. Not confirmed |
| Proposed test | Explicit Mac **Connect** (not Forget) with the trace before and after; optional Mac Bluetooth log. Judgment table in `STAGE_2A_2.md` §6.5 |

## 57. Development entry — 2026-10-14: Stage 2a-2 hardware test 1; evaluation of two open issues

| Item | Record |
| --- | --- |
| Passed (author) | BLE new pairing, BLE READY / LINK READY, YouTube over BLE, Bluetooth OFF→ON reconnection (1/1), restart reconnection (1/1), USB HID READY and System Information, YouTube over USB, USB→BLE and BLE→USB switching with YouTube afterwards |
| Not yet tested | BLE mouse (movement, drag, switching while dragging), repeated switching, B7 repetitions, HOLD / CUSTOM on BLE, `!` path, transport restore, long-time stability |
| Open 1 | BLE-B1 bond not taken over: the Mac did not reconnect (ADVERTISING); fixed only by Mac unpairing and new pairing. Source: identity, advertising, GATT, map, security, bond / CCCD handling and start order give no explanation; no code path touched the bond. Trace before the unpairing not captured. Hypothesis: Mac-side; same symptom once before (r4 after M1 r2). Proposal: more verification (Launcher round trip, then same-binary re-install) |
| Open 2 | USB first `NOT MOUNTED`, not in System Information, also after restart; recovered after a power operation. Source: USB is started on every boot in both modes and never re-enumerated by a switch; `NOT MOUNTED` = host never configured the device (`tud_mount_cb`). Candidates: software restart without a clean USB reconnect, or external (cable, port, macOS accessory prompt). Proposal: firmware unchanged; one cold-start vs software-restart test |
| Next | Edgar's decision on `STAGE_2A_2.md` §7.3 / §7.4; no code change |

## 58. Development entry — 2026-10-14: Stage 2a-2 — main functions hardware-verified (final record)

| Item | Record |
| --- | --- |
| New hardware results (author with Edgar) | BLE mouse movement (SPINNER, `Fn+2`) PASS; BLE mouse drag (Finder window; knob release ends it) PASS; BLE → USB switch while dragging PASS (the drag ended at the `Fn+B` operation, before the switch completed; the direct cause was not determined on hardware); restart in USB mode with USB HID reconnecting PASS |
| Consolidated | With hardware test 1 (§57): BLE pairing, READY, YouTube over BLE, reconnection after Bluetooth OFF→ON and restart (1/1 each), USB HID and System Information, YouTube over USB, both switch directions. Table in `STAGE_2A_2.md` §8.1 |
| Status | **Stage 2a-2: main functions hardware-verified.** Not an exhaustive verification, no long-time stability or compatibility guarantee; unverified items in `STAGE_2A_2.md` §8.3 |
| Known open issues | (1) BLE-B1 bond not reused, new pairing needed; cause not determined; not established that every update needs it. (2) First USB recognition failed (`NOT MOUNTED`, not listed); recovered after a power operation; cause and exact recovery condition not determined (`STAGE_2A_2.md` §8.4) |
| Files | Documentation only; no source change, no build, no new binary |

## 59. Development entry — 2026-10-14: Stage 2a-2 on Windows and ChromeOS (USB)

| Item | Record |
| --- | --- |
| Hardware (author with Edgar) | Stage 2a-2 over USB HID. Windows: YOUTUBE PASS, SPINNER pointer movement PASS. ChromeOS: YOUTUBE PASS, SPINNER pointer movement PASS |
| Matrix | macOS USB: YOUTUBE / SPINNER PASS; macOS BLE: YOUTUBE / SPINNER (movement, drag) PASS; Windows USB and ChromeOS USB: YOUTUBE / SPINNER (pointer) PASS (`STAGE_2A_2.md` §9) |
| Not verified | BLE on Windows and ChromeOS; drag on Windows / ChromeOS; OS versions not recorded; ChromeOS is not taken as evidence for general Linux distributions |
| Files | Documentation only |

## 60. Development entry — 2026-10-16: Stage 2a-3 design and step 1 (KEY STEP, ext record)

| Item | Record |
| --- | --- |
| Decisions (Edgar) | KEY STEP (key inputs per detent, 1–10, per profile 1/3/4) and SPINNER SENS separated; Fn+5 `C-SPINNER` (own SENS, direction, custom button, default mouse left HOLD); FIFO on reversal; up to 10 taps per loop provisionally; backlog limit decided after USB / BLE measurement; three steps, folder `MediaJog_2a_3`, build id `2a-3`. Tap counts are guaranteed for normally processed detents only (`STAGE_2A_3_DESIGN.md`) |
| Step 1 | KEY STEP with per-entry tap counts (KEY STEP 1 = the 2a-2 path), `Fn+;` / `Fn+.` per current key profile, new NVS record `mediajog/ext` (`cfg` unchanged), display of KEY STEP and measurement counters |
| Tests | 2a suite 85/85, 2a-2 scenarios 32/32, NVS 47/47, HOLD 16/16, KEY STEP 35/35; differential KEY STEP 1 vs 2a-2 identical (1,711 USB / 1,725 BLE reports); rollback to 2a-2 restores settings; 10 mutants detected; GATT identical to B1 |
| Build | Exit 0, 0 sketch warnings; source `a9313f27…`, app-only binary (891,744 B) `9f222fc42248f3235eabdc013c8db2455696553b119d6dfe783ff4287e48c7e6` |
| Next | Edgar's review, then the step-1 hardware check (`STAGE_2A_3.md` §4). Step 2 only after that |

## 61. Development entry — 2026-10-16: Stage 2a-3 step 1 — BLE at KEY STEP 10 lags after fast rotation (analysis)

| Item | Record |
| --- | --- |
| Hardware (author) | USB KEY STEP 10: fine. BLE: KEY STEP 1 fine; KEY STEP 10 at fast rotation: frame stepping continues after stopping, YouTube spinner. Screen: `P100 D428`, `W298 T22`, `FAIL0`, `fail10`, `.4067 ,2452` |
| Analysis | One BLE tap took up to 22 ms (`T22`), so sending blocks for about one connection interval per notification. Estimated BLE rate about 40–50 taps/s versus about 250 over USB. The delay comes mainly from the 100-tap firmware queue (about 2 s). Drops were whole detents, as specified. `fail10` = USB mouse reports with unknown outcome, unrelated. The loop was blocked up to 298 ms |
| Proposal | Diagnostic build first (rate, delay, `notifyFails`); then a BLE-only time-based backlog limit and a BLE-only taps-per-loop limit. USB, KEY STEP 1 and BLE GATT unchanged (`STAGE_2A_3.md` §5) |
| Next | Edgar's decision; C-SPINNER (step 2) remains on hold |

## 62. Development entry — 2026-10-16: Stage 2a-3 diagnostic build d1 (BLE send measurement)

| Item | Record |
| --- | --- |
| Request (author / Edgar) | Diagnostic build first: BLE tap rate, press / release notify times, where sending waits, pending taps, tail after stopping; minimal influence of the measurement; no change to USB, KEY STEP 1, BLE connection, NVS. Goal: drop as little as possible, accurate rotation, low delay. B + C later; C-SPINNER on hold |
| Build | `MediaJog_2a_3_d1/` (`2a-3d1`, tag `2a3d`): `micros()` timing of allocation and notify (press / release separately), min free msys, BLE rejects, connection interval, 1 s and burst tap rates, pending taps, tail, draw time; two status lines in BLE PROFILE 1 / 3; `[DIAG]` Serial line per burst |
| Tests | 2a suite 85/85, 2a-2 scenarios 32/32, KEY STEP 35/35, diagnostics 12/12; differential d1 vs 2a-3 identical (31,086 reports, random KEY STEP changes); GATT identical to B1 |
| Build | Exit 0, 0 sketch warnings; source `d2f20561…`, app-only binary (893,152 B) `714f034c22310f567b4ad9c3108338d6e113321162b8835e2647276636d231a9` |
| Next | Measurement on hardware (`STAGE_2A_3.md` §6.4), then the decision on B + C and on the connection interval |

## 63. Development entry — 2026-10-16: d1 BLE results analysed

| Item | Record |
| --- | --- |
| Hardware (author / Edgar) | BLE KEY STEP 10: about 70 taps/s peak, `ci` 15 ms, tail max 1,554 ms, `P100 D632 n1250`, notify times about 0 ms; felt delay after stopping similar to USB; YouTube spinner more frequent on BLE |
| Analysis | Notify call is non-blocking (the §5 "blocking" hypothesis is refuted). `n` counts the stack's flow-control rejections, about one per loop under a backlog; every rejection is retried, no input lost. About 2.1 notifications per connection event is the link rate. The tail is the firmware queue draining at that rate, plus an estimated ≤ 0.25 s in the host pool; it is bounded by the 100-tap limit (`STAGE_2A_3.md` §7) |
| Proposal | Keeping 2a-3 is acceptable. If improved: stack-aware pacing for BLE / KEY STEP ≥ 2 and rejection counting without printing (no drops, low risk); PHY / connection interval only as a separate experiment; a shorter queue limit last |
| Next | Edgar's decision; no code change |

## 64. Development entry — 2026-10-16: direction reversal during fast rotation — design review

| Item | Record |
| --- | --- |
| Observation (author) | BLE, KEY STEP 10: after fast right rotation and an abrupt left turn, right frame stepping continues for a noticeable time |
| Cause (source) | FIFO queue plays all queued old-direction detents first (up to 100 taps ≈ 1.4 s) and, at the limit, drops the new direction's detents; accepted notifications in the BLE stack (estimated ≤ 0.25 s) cannot be recalled |
| Recommendation | C: reversal purge of queued old-direction rotation (KEY STEP ≥ 2; keeps PRESS / HOLD order; new detent then admitted) + BLE-only stack-aware pacing (keeps the stack backlog to about 3 taps). KEY STEP 1, USB at KEY STEP 1, HOLD, NVS, GATT unchanged (`STAGE_2A_3_DESIGN.md` §7) |
| Next | Author / Edgar decide; no code change |

## 65. Development entry — 2026-10-16: Stage 2a-3 r2 — reversal purge (method A)

| Item | Record |
| --- | --- |
| Decision (author / Edgar) | Method C as the goal; this round method A only (purge waiting old-direction rotation at a reversal), KEY STEP 2–10, all key profiles incl. CUSTOM, USB and BLE; KEY STEP 1 and everything else unchanged. Pass criterion on hardware: "fast right, then left: it moves left" |
| Implementation | `MediaJog_2a_3_r2/` (`2a-3r2`, tag `2a3r`): `newestQueuedRotation()` + `purgeRotation()` called in `pushDetent()` before the CUSTOM / limit checks; PRESS / HOLD kept in order; taps stay atomic; diagnostics V / X / q. Only `pushDetent`, `drawStatus`, `drawCustomStatusBody` changed |
| Tests | 2a 85/85, 2a-2 32/32, KEY STEP 34/35 (K05 = the replaced FIFO-on-reversal expectation), KEY STEP 1 differential identical (10,531 reports), reversal tests 21/21, 6 mutants detected; GATT identical to B1 |
| Build | Exit 0, 0 sketch warnings; source `579dd306…`, app-only binary (892,096 B) `a4ea24fd3f01243e62e34b1f30c50d101b06445d61d1fde5023a94bb4fe8e6af` |
| Next | Edgar's review, then the hardware evaluation (`STAGE_2A_3_DESIGN.md` §8); method B only if the residual old-direction movement on BLE still feels wrong |

## 66. Development entry — 2026-10-16: r2 — no BLE input on hardware (investigation)

| Item | Record |
| --- | --- |
| Hardware (author) | r2 BLE: Mac connected, `BLE READY`, rotation value changes, but no key reaches the Mac (rotation and knob press); re-pairing did not help. USB works |
| Source | Only `pushDetent()` (KEY STEP ≥ 2 rotation) and two display functions changed; the BLE send path, input path, link / switch logic and GATT are byte-identical to 2a-3. The knob press (`pushEvent(EV_PRESS)`) does not touch the new code, and the purge sets no flag that blocks sending. `BLE READY` = `linkState` (connected + encrypted + keyboard CCCD); sending also needs `hidReady`; `txCount` shows accepted reports |
| Judgment | Not determined; no r2-specific path found. Candidates: `hidReady` false, all notifications refused, or reports not used by the Mac |
| Next | Status-screen photo after KEY STEP 1 rotation + knob press in BLE; CONNECTION screen photo; if needed, the same test with 2a-3 (`STAGE_2A_3_DESIGN.md` §9) |

## 67. Development entry — 2026-10-16: r2 BLE investigation — photos show reports accepted on the device

| Item | Record |
| --- | --- |
| Photos (author) | Status: `BLE READY`, `.760 ,811 k9`, `TX k`, `SKIP0 q0`, no `REL!`, `V8 X399`, KEY STEP 1. CONNECTION: `bond=1 kb=1 ms=1 h1 fs0`, `ki10 ko21 mi24`, USB mounted (inactive) |
| Analysis | `TX` / counters are set only for `TAP_SENT` = BLE host accepted the notification (incl. the knob press). The firmware path works; the loss is after the stack accepts (link or Mac). This path is identical to 2a-3 / d1. The written summary (counters 0, V0) differs from the photo; counters are cumulative since boot |
| Next | Install d1 (or 2a-3) and repeat the TextEdit test; if it also fails, Mac Bluetooth off / on, then Mac restart, without unpairing (`STAGE_2A_3_DESIGN.md` §9.1) |

## 68. Development entry — 2026-10-16: d1 works, r2 does not (same Mac) — candidates

| Item | Record |
| --- | --- |
| Observation | d1: no input at first, works after Mac re-pairing. r2: re-paired, `BLE READY`, no input |
| Source / build | Init order and all BLE code identical in 2a-3 / d1 / r2; r2 adds two counters and longer tags (no overflow); d1 changes more and works |
| Candidates | A Mac / link state (d1 showed the same symptom before re-pairing); B r2-specific runtime effect (no source evidence); C reports not sent on the link; D wrong upload |
| Tests | Fresh link via USB → BLE switch without unpairing (+ trace photo); Mac Bluetooth off / on, Mac restart; repeatability 2a-3 vs r2; diagnostic r2 only if B is likely (`STAGE_2A_3_DESIGN.md` §9.2) |

## 69. Development entry — 2026-10-16: r2 (method A) passes on hardware; method B not implemented

| Item | Record |
| --- | --- |
| Hardware (author) | BLE YouTube: fast rotation with an abrupt reversal feels good; BLE TextEdit follows reversals; USB KEY STEP 10: `。` switches to `、` correctly on reversal |
| Decision | Method A meets the pass criterion; method B (BLE pacing) is not implemented; `MediaJog_2a_3_r2` is kept (`579dd306…` / `a4ea24fd…`); 2a-3 and d1 preserved |
| Open issue | "`BLE READY` without input" (seen with d1 before re-pairing and once with r2): cause not determined, kept as an open issue with the capture procedure (`STAGE_2A_3_DESIGN.md` §10) |
| Files | Documentation only; no code change, no build |

## 70. Development entry — 2026-10-16: r2 additional hardware tests

| Item | Record |
| --- | --- |
| Hardware (author) | BLE KEY STEP 1 (YOUTUBE): right / left / press as before — PASS. BLE CUSTOM (CW Right, CCW Left, BTN Shift HOLD): turning left and right while holding — PASS |
| Not verified | A new knob press during a reversal; long-term continuous use |
| Status | r2 current; method A verified; method B not implemented; "`BLE READY` without input" open. No code change |

## 71. Development entry — 2026-10-16: web MAME Arkanoid works with the existing SPINNER

| Item | Record |
| --- | --- |
| Setup | Cardputer ADV + U135, `MediaJog_2a_3_r2` (unchanged), SPINNER (Fn+2), BLE, Mac, Google Chrome; Internet Archive web MAME, Arkanoid Tournament (US) (https://archive.org/details/arcade_arkatour); MAME: Dial Analog = Mouse X, Inc = RIGHT, Dec = LEFT |
| Hardware (author) | Mouse output OK; paddle moves left / right; speed: "no problem at all"; responsiveness: no practical problem; smoothness: slightly stepped; no firmware change needed |
| Mouse input | At first the Mac pointer moved but the paddle did not. Working method: reload the page, then before starting the game run `javascript:MAMELoader.extraArgs=(z)=>({extra_mame_args:['-mouse']});AJS.emulate()` from the address bar (in Chrome, `javascript:` typed by hand, the rest pasted). Depends on the Internet Archive's page code; may stop working. Reference: Alexander Pruss, "Two tweaks for Internet Arcade games on archive.org" (2023) |
| Pointer | After the game the Mac pointer stayed hidden; **Esc once restores it** (hardware-verified). Must be part of any published procedure |
| Stepped movement | Cause not determined (encoder resolution, firmware movement conversion, web MAME input handling are candidates only). No firmware change now: speed and responsiveness have priority |
| Decisions | SPINNER SENS kept; Fn+2 settings and behavior unchanged; C-SPINNER stays a general customizable SPINNER (not Arkanoid-specific), not started (`STAGE_2A_3_DESIGN.md` §12) |
| Files | Documentation only; no code change, no build |

## 72. Development entry — 2026-10-16: Arkanoid test — SPINNER SENS 64

| Item | Record |
| --- | --- |
| Hardware (author) | The successful web MAME Arkanoid test (§71) used SPINNER SENS **64**: speed no problem, responsiveness sufficient, slightly stepped movement |
| Use | Baseline for later comparison tests; firmware default unchanged (`STAGE_2A_3_DESIGN.md` §12.5) |
| Files | Documentation only; no code change, no build |

## 73. Development entry — 2026-10-16: Stage 2a-4 — C-SPINNER (PROFILE 5) implemented; not hardware-tested

| Item | Record |
| --- | --- |
| Request | Author: Stage 2A-4, C-SPINNER on Fn+5 (general, not Arkanoid-specific); Fn+1–4, KEY STEP / reversal purge, USB / BLE and NVS data unchanged |
| Review | Design, plan, report, r2 source and NVS checked. 64 = step 12 of the 14-step table (consistent). Default = PROFILE 2's default 8, not 64. Design §4 "ok toggles DIR" conflicts with ok = SAVE: kept ok = SAVE, DIR toggled with `2` / knob press (to confirm). SENS and MODE also added as SETUP rows (request) |
| Build | `MediaJog_2a_4/`, tag `2a4`: no errors, no warnings; source `fc17bf73…`, bin `4e1dc5ed…`. r2 checksums verified unchanged |
| Code | 132 of 148 functions identical to r2; 16 extended for PROFILE 5; 19 new. `ext` v1 format unchanged; `cfg` never holds 5; PROFILE 5 restored at boot only with the `cfg` check. GATT identity PASS |
| Host tests | New suite 80/80 (incl. sensitivity 64 producing exactly the PROFILE 2 reports at 64); earlier suites as r2; 10/10 mutants detected |
| Not verified | All hardware behaviour of 2a-4 (`STAGE_2A_4.md` §9); suggested checks in §10 |
| Files | `MediaJog_2a_4/` (new), `docs/STAGE_2A_4.md` (new); appended: REPORT, DEVELOPMENT_PLAN, STAGE_2A_3_DESIGN |

## 74. Development entry — 2026-10-16: Stage 2a-4 hardware tests (author)

| Item | Record |
| --- | --- |
| Setup | `MediaJog_2a_4` (`4e1dc5ed…`), Cardputer ADV, Mac |
| BLE | Fn+2 and Fn+5 pointer PASS; Fn+5 at 8 and 64 PASS; sensitivities independent PASS (Fn+5 8 → 64 left Fn+2 at 64; at 64 both moved about the same by feel) |
| Direction | NORMAL → REVERSE, saved, reversed, kept after profile switch and power cycle: PASS |
| Mouse button | L → R, right click, TAP / HOLD, three TAP right clicks, TAP L no drag, HOLD L drag: PASS |
| Keys | A, HOLD A (macOS accent menu in English input, "あああああ" in Japanese input: the computer's normal held-key behaviour), TAP A held = one character, Shift+A = capital A, NONE: PASS |
| NVS / USB | REVERSE and 64 kept after power cycle; Fn+5 pointer over USB: PASS |
| Not covered | Arkanoid with Fn+5; Windows / ChromeOS; Fn+5 button / keys over USB; long-term use; "`BLE READY` without input" still open (`STAGE_2A_4.md` §11) |

## 75. Development entry — 2026-10-16: Stage 2a-5 — colour-coded operation guides; not hardware-tested

| Item | Record |
| --- | --- |
| Change | Display only: operation cyan, description white in every guide (SPINNER, CUSTOM, C-SPINNER, both SETUPs and their edit screens, CONNECTION); status, values, selections and warnings unchanged |
| Code | 162 of 167 functions identical to 2a-4; 5 drawing functions changed, `printGuide()` new; GATT identity PASS |
| Build | `MediaJog_2a_5/`, tag `2a5`: no errors, no sketch warnings; source `20a3659b…`, bin `4093c047…`. 2a-4 checksums verified unchanged |
| Host tests | All earlier suites as on 2a-4; display comparison 270/270 (same characters and positions, colour changes only on guide lines) |
| Not verified | The display on the device (`STAGE_2A_5.md` §7–§8) |
| Files | `MediaJog_2a_5/` (new), `docs/STAGE_2A_5.md` (new); appended: STAGE_2A_4 §11, REPORT, DEVELOPMENT_PLAN. Public repository prepared, not committed |
