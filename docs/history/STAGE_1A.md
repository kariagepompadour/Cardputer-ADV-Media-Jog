# 第1a段階 USB HID最小試作 — 実装・ビルド・試験手順

作成日：2026-10-12／作成：Claude Opus 5.5
改訂：2026-10-12 r2（エドガーの最終レビュー①〜⑤を反映）
状態：実機動作確認済み（2026-10-12、Mac Chrome／YouTube）

凡例：✅ ソースまたはビルドで確認 / ❓ 未検証（実機で確認が必要）
ソースの行番号は m5stack:esp32 **3.3.9**（`~/Library/Arduino15/packages/m5stack/hardware/esp32/3.3.9/`）のもの。

## 1. 納品物

| ファイル | 内容 |
|---|---|
| `MediaJog_1a/MediaJog_1a.ino` | 新規スケッチ（U135_Probe v2を基に作成。v2本体は未変更） |
| `MediaJog_1a/build/MediaJog_1a.ino.bin` | アプリ単体のバイナリ。**Launcherから起動する際に使う** |
| `MediaJog_1a/build/MediaJog_1a.ino.merged.bin` ほか | 直接書き込み用のイメージ。**今回は使わない**（Launcherを上書きするため） |

| ファイル | SHA-256 |
|---|---|
| `U135_Probe.ino`（原本・未変更） | `14aaca87d57e018e2eab64a2e183e38c95f4cfee14f29fe597b6b6a0cbbe80fa` |
| `U135_Probe.ino.bin`（原本・未変更） | `0323b74b4e57a826278de6c32be93fbde1ac45dca8bc4b510f5eddaafbcbd375` |
| `MediaJog_1a.ino.bin`（r2） | `57190de1778316665c7b94b92e00c8d77c36b3f59b4f04c999987336031aa8c7` |

r1の `.bin`（`46bc8f2e…`）は r2 で置き換えた。

## 2. 動作仕様

| 入力 | 送信するHIDレポート（Report ID 1、8バイト） |
|---|---|
| 右1目盛り（カウンタ +2、v2で実測） | 修飾 `0x00`、Usage `0x37`（`.`）→ 全キー解放 |
| 左1目盛り（カウンタ −2、v2で実測） | 修飾 `0x00`、Usage `0x36`（`,`）→ 全キー解放 |
| 押し込み（raw 1→0、v2で「押下中 raw=0」を実測） | 修飾 `0x00`、Usage `0x0E`（`k`）→ 全キー解放 |

Usage値はTinyUSBの `HID_KEY_PERIOD`／`HID_KEY_COMMA`／`HID_KEY_K` と一致することを `static_assert` で確認している。✅

- 本体キーボード、BLE、設定保存、プロファイル切替、`M5Cardputer.begin()`／`M5.begin()` は使わない。LCDの初期化はv2と同じ4行。
- カウンタの余り（±1）は保持し、画面に `R+1` などと表示する。扱いは実測後に決める（計画 §7-2）。
- イベントは順序を保って最大32件まで保持し、1ループで最大4回送信する（④参照）。

### 画面表示（文字サイズ2）

```
USB HID READY        ← 緑。赤＝USB NOT MOUNTED、黄＝USB SUSPENDED
VAL 124 R+0          ← カウンタの生値、余り
BTN raw=1
.12 ,8 k3            ← 押下レポートの送信が確認できた回数
Q0/3 OVF0            ← 現在のキュー件数／起動後の最大件数、溢れた回数
FAIL0 SKIP0          ← 「REL!」が付いたら解放レポートの再送待ち
ERR0 W14 T3          ← I²Cエラー、ループ処理時間の最大値[ms]、1回のキー操作時間の最大値[ms]
TX .                 ← 最後のイベント
```

`W`（delay(20)を除くloop本体の時間）と `T`（押下＋解放にかかった時間）は、④の実測用に追加した。いずれも起動後の最大値。

## 3. Arduino IDEの設定

| 項目 | 値 | 備考 |
|---|---|---|
| ボード | M5Cardputer（m5stack:esp32 3.3.9） | v2と同じ |
| **USB Mode** | **USB-OTG (TinyUSB)** | v2（Hardware CDC and JTAG）とは異なる。違う値だと `#error` で停止する ✅ |
| **USB CDC On Boot** | **Enabled** | 無効だとSerialがUART0（TX=GPIO43。ADVでは音声I²Sが使用）になるため、`#error` で停止する ✅ |
| その他 | 既定値 | |

```bash
"/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile --fqbn "m5stack:esp32:m5stack_cardputer:USBMode=default,CDCOnBoot=cdc" --output-dir MediaJog_1a/build MediaJog_1a
```

**IDEの「書き込み」ボタンは使わない。** 使うのは「スケッチ → コンパイル済みバイナリをエクスポート」または上記コマンドで作った `.bin` だけ（§5）。

## 4. コンパイル結果（r2、2026-10-12）

| 項目 | 結果 |
|---|---|
| USB-OTG ＋ CDC On Boot=Enabled | ✅ 成功（exit 0）。フラッシュ 523,239 B（39%）、RAM 58,852 B（17%） |
| スケッチ由来の警告・エラー | なし（r1と同じく、コアとバリアントのマクロ再定義警告のみ） |
| Hardware CDC and JTAG で試しにビルド | ✅ `#error`（USB Mode）で停止 |
| CDC On Boot=Disabled で試しにビルド | ✅ `#error`（CDC On Boot）で停止 |

---

## 5. エドガーの最終レビューへの回答

### ① USB HIDインスタンス（2インスタンスで送信しても安全か）

**結論：安全。登録されるHIDインターフェースは1つで、`hidOut` は送信口として同じインターフェースを使うだけ。** ✅（ソース確認。実機での列挙は❓）

**初期化の順序**

1. **静的初期化（`setup()` より前）。** グローバルオブジェクトは同じソースファイル内では宣言順に構築される（C++の規則）。このビルドで `USBHID` を生成するのは本スケッチの2つだけ（M5Cardputer、M5Unified、M5GFX、M5Unit-Encoder のソースを検索して確認）。
   1. `USBHIDKeyboard UsbKeyboard`
      - メンバー `hid(HID_ITF_PROTOCOL_KEYBOARD)` が最初の `USBHID` として構築される。ファイル内static変数 `tinyusb_hid_devices_is_initialized` が false なので、プロトコル＝KEYBOARD を記録し、`tinyusb_enable_interface2(USB_INTERFACE_HID, …, reserve_endpoints=true)` でHIDインターフェースを**1つ**予約する（`USBHID.cpp:277-287`）。
      - 続いて `addDevice(this, sizeof(report_descriptor))` でキーボードのレポート記述子を登録する（`USBHIDKeyboard.cpp:35-42`）。記述子は `TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD))`、つまり **Report ID 1**（`USBHIDKeyboard.cpp:33`、`USBHID.h` の enum）。
   2. `USBHID hidOut(HID_ITF_PROTOCOL_KEYBOARD)`
      - フラグがすでに true なので**何もしない**。インターフェースの追加も `addDevice` もしない。
      - 仮に構築順が逆になっても、同じKEYBOARDプロトコルを渡しているため結果は同じ（公式例 `CompositeDevice.ino` は `USBHID HID;` を `USBHIDKeyboard Keyboard;` より先に宣言する、同じ構成）。
2. **`app_main`（CDC On Boot=Enabled）。** `Serial.begin()` と `USB.begin()` が `setup()` より前に呼ばれる（`main.cpp:99-110`）。
   - 構成記述子の生成時に `tusb_hid_load_descriptor()` が1回だけ実行され、HIDインターフェース1つ（キーボード用に予約されたEP IN/OUT 1、`bInterval`=1）を出力する（`USBHID.cpp:194-216`）。
   - これ以降 `tinyusb_hid_is_initialized` は true になり、デバイスの追加はできない（`USBHID.cpp:51-55`）。本スケッチは追加しない。
3. **`setup()`。** `UsbKeyboard.begin()` が `hid.begin()` を呼び、送信用のセマフォとミューテックス（どちらもファイル内static）を作る（`USBHID.cpp:289-296`）。
   - `hidOut.SendReport()` は同じstatic変数を使う。`begin()` より前に呼ぶと false を返すだけ（`USBHID.cpp:333-336`）。本スケッチは `setup()` の後にしか送信しない。

**送信するレポート**

- `hidOut.SendReport(HID_REPORT_ID_KEYBOARD=1, hid_keyboard_report_t, 8バイト)`。`USBHIDKeyboard::sendReport()` と同じ構造体・同じIDで、違いは戻り値を受け取ることだけ（`USBHIDKeyboard.cpp:73-79`）。
- ホストがブートプロトコルを選んだ場合は、ライブラリがReport ID 0に切り替える（`USBHID.cpp:343-345`）。
- `UsbKeyboard.press()`／`write()` は一切呼ばない。そのため、`USBHIDKeyboard` 内部の `_keyReport` は常に空で、古いキー状態が送られる経路はない。
- 送信するのは `loop()` のタスクだけ。USBイベントのコールバックはフラグを立てるだけで、送信しない。

### ② USB切断・再接続時の状態

**r1の挙動（ソース上）：** 未接続中は送信しない。キューは破棄する。`releasePending` は保持され、再接続後の最初の処理で全キー解放を再送していた。押しっぱなしを増やす経路はなかった。

**r2の修正：**「USB準備完了」になるたびに `releasePending = true` とし、**どのキー操作よりも先に全キー解放レポート（キーなし）を送る**ようにした。r1で残っていた次の場合も同じ処理で吸収できる。

- 切断直前に解放が失敗していた。
- ホストのスリープ（サスペンド）中に解放が送れなかった。
- 別のホストに挿し替えた。

全キー解放レポートはキーを含まないため、何度送っても押しっぱなしの原因にはならない。

**未検証事項：**

- ❓ **電池駆動中にUSBを抜いたとき、画面が `USB NOT MOUNTED`（unmount）になるか `USB SUSPENDED`（suspend）になるか。** ESP32-S3側でVBUS（USB電源）を検出できるかは、事前ビルド済みのTinyUSBからは確認できなかった。どちらの場合も「準備未完了」と判定されて送信は止まる（`USB.cpp:99-130` のmount／umount／suspend／resumeコールバック、`USBHID::ready()`＝`tud_hid_n_ready()`）。
- ❓ 抜いた時点でmacOSが押下中のキーを解放するか（HIDホストとしては一般的な挙動だが、この構成では未確認）。

### ③ 送信失敗時の処理

**`SendReport()` の戻り値が保証すること**（`USBHID.cpp:332-369`）：

| 戻り値 | ライブラリ内の経路 | 意味 |
|---|---|---|
| true | `tud_hid_n_report()` で送信予約 → `tud_hid_report_complete_cb()` でセマフォを受け取る | **ホストがUSB上でそのレポートを読み取った**（IN転送完了）。アプリがキーを処理したことまでは保証しない |
| false（not ready） | `tud_hid_n_ready(0)` が偽 | **何も送っていない** |
| false（report failed） | `tud_hid_n_report()` が偽 | **何も送っていない** |
| false（wait failed） | 送信予約後、100ms以内に完了しない | **あとでホストに届く可能性がある**（結果不明） |

**r2の修正：** 送信前に `hidOut.ready()` で確認し、「何も送っていない」場合と「結果不明」の場合を分けた。

| 状況 | r2の処理 | 重複 | 取りこぼし |
|---|---|---|---|
| 押下前に準備未完了 | 送らずキューに残し、次のループで再試行（順序は保持） | なし | なし |
| 押下 true | 成功として数え、続けて解放を送る | なし | なし |
| 押下 false（結果不明） | **再送しない**。`FAIL` に数え、それでも解放は送る | なし | あり得る（`FAIL` で見える） |
| 解放 false | `REL!` を表示し、成功するまで毎ループ再送する。その間は新しい押下を送らない | なし | — |

r1との違い：r1は準備未完了の場合も `FAIL` として捨てていた（不要な取りこぼし）。また、押下が成功しても解放が失敗すると `FAIL` と数えていた。r2はこの2点を直した。

残るリスク：結果不明の押下が後で届いた場合、実際には入力されたのに `FAIL` と数えられる（カウントのずれ）。テキストエディットでの文字数と画面のカウントを比べて確認する。

### ④ キューの処理能力（32件、1ループ最大4回）

**ソースから確定できる値：**

- HIDエンドポイントの `bInterval`=1（`USBHID.cpp:210`）。Full Speedでは、ホストのポーリング間隔は最短1ms。
- `SendReport()` は完了を待つため、1レポート＝最低1フレーム、1回のキー操作（押下＋解放）＝最低約2ms。理論上の上限は約500回/秒。
- 回転の読み取りは20ms周期だが、カウンタはユニット側が保持する絶対値なので、**読み取り間隔中の回転は取りこぼさない**（int16が折り返すには、20msの間に32,768カウントが必要）。

**見積もり（Wとホスト側の応答が未実測なので❓）：**

| loop本体時間 W | 1ループの周期 ≈ 20+W ms | 送信できる上限（4回/ループ） | 30目盛り/回転換算 |
|---|---|---|---|
| 10 ms | 30 ms | 約130回/秒 | 約4.4回転/秒 |
| 30 ms（再描画を含む場合） | 50 ms | 約80回/秒 | 約2.7回転/秒 |

- Wには、4回分のキー操作（各 `T` ms）と100ms間隔の再描画が含まれる。
- 32件のキューは約1回転分（30目盛り）のバッファ。回転速度が送信能力を超えている時間が続くと溢れ、`OVF` に数えられる。
- 手を止めてから送信が終わるまで：最大 32÷4＝8ループ ≈ 0.25〜0.4秒（上表のWの場合）。

**未検証（実測が必要）：**

- ❓ 実際の `W`、`T`（画面に表示される）
- ❓ U135が高速回転を正しく数えるか（STM32のファームウェアは非公開）
- ❓ 人の手による最大回転速度
- ❓ Chrome／YouTubeが短い間隔のキー入力をすべて処理するか（フレーム送りの処理速度）

32件と4回は暫定値のまま変更していない（計画 §7-2：実測後に決定）。

### ⑤ Launcher

Launcherからの起動を前提とした手順に一本化した。**直接書き込み（IDEの「書き込み」、merged.bin、esptool）は行わない。** IDEからの書き込みは、ブートローダ（0x0）・パーティション表（0x8000）・boot_app0（0xe000）・アプリ（0x10000）を上書きし、Launcherを消す（`platform.txt:345`）。✅

**確認できた事実**

- USBモードはアプリのビルド時の定義で決まり、TinyUSBの初期化はアプリ自身の `app_main` が行う（`main.cpp:99-110`）。✅
- v2（アプリ単体の `.bin`）は、この実機のLauncherから起動できた（ユーザーの実機結果）。
- 公開情報：bmorcelli/Launcher はESP32-S3でUSB Mass Storage機能を持つ（https://github.com/bmorcelli/Launcher）。この実機に入っているLauncherの種類とバージョンは未確認。

**未検証事項**

- ❓ Launcherから起動したとき、MacがUSB HIDキーボードとして認識するか（Launcherがアプリに切り替える際のUSB周辺回路の状態、再列挙の可否）。
- ❓ 523 KBのアプリがLauncherのアプリ領域に収まるか（v2の456 KBは動作した）。
- ❓ Launcher経由でCDCシリアル（ログ）がMacから見えるか。
- ❓ Launcherへ戻る操作の方法。

**Launcher経由でHIDとして認識されなかった場合は、その結果を記録して終了する。** 直接書き込みでの切り分けを行うかどうかは、別途判断する（Launcher本体の再書き込み用ファイルの準備が前提）。

---

## 6. 実機テスト手順（Launcher経由のみ）

### 事前準備
- [ ] エドガーのレビューが完了している。
- [ ] SDカードに `U135_Probe.ino.bin`（v2）と SOUND TERRARIUM の `.bin` が残っている（上書きしない）。
- [ ] Mac側で、テキストエディット（新規書類）とChromeのYouTubeを開いておく。
- [ ] Cardputerは**USBケーブルを挿さずに**電池で起動できる状態にしておく。

### 手順
1. `MediaJog_1a/build/MediaJog_1a.ino.bin` をSDカードに**別名のまま**コピーする。
2. U135を**外した状態**でLauncherからMediaJog_1aを起動する。
   - 期待：1行目に `USB NOT MOUNTED`（赤）、2行目に `U135 NOT FOUND`（赤）。
   - **画面が点かない場合は中止**して§7へ。
   - Launcherのアプリ領域に入らないというエラーが出た場合も、内容を記録して中止する。
3. 電源をOFFにし、U135を接続して電源ON → Launcherから再起動する。
   - 期待：`VAL` 表示、`ERR0`。
4. MacとCardputerをUSBケーブルで接続する。
   - 期待：1行目が緑の `USB HID READY`。
   - macOSの「キーボード設定アシスタント」が表示されたら、内容を記録して閉じる。
   - 赤や黄のままなら§5⑤の未検証事項に該当するので、結果を記録して終了する。
5. **テキストエディット**にフォーカスし、右・左・押し込みを各1回行う。
   - 期待：`.` `,` `k` が1文字ずつ入力される（`>` `<` `K` ではないこと＝JIS/US配列の確認）。
6. 右に10目盛りゆっくり回す → `.` が10個入力され、画面も `.10` になる。左も同様に確認する。
7. 速く回す（目安：1秒に1回転、2回転、と段階的に）。次を記録する。
   - 入力された文字数と画面のカウント
   - `Q` の最大値、`OVF`、`FAIL`
   - `W`、`T`
   - 手を止めてから入力が止まるまでの時間
8. 半目盛り回して戻す → `R` の表示と、誤送信の有無を記録する。
9. ボタンを10回押す → `k` が10個入力されること（二重入力がない＝チャタリングなし）。
10. **YouTube（Chrome）**：一時停止中に、右で1フレーム進む、左で1フレーム戻る、押し込みで再生/停止。
11. USBケーブルを抜く → 1行目が赤（NOT MOUNTED）か黄（SUSPENDED）のどちらになるかを記録する（②の未検証事項）。
    - 抜いたまま回すと `SKIP` が増えること。
    - 挿し直したときに過去の回転が入力されず、`REL!` が残らないこと。
12. 各画面の写真と、`ERR`／`FAIL`／`OVF`／`SKIP`／`W`／`T` の値を記録する。

YouTube Musicの `Shift+L`／`Shift+H`／`Space` は今回の範囲外（プロファイルはコンパイル時に固定するため、別ビルドで実施する）。

## 7. 復旧手順（Launcherを上書きしない範囲）

| 症状 | 対処 |
|---|---|
| Macでキーが押しっぱなしになる（同じ文字が連続入力される） | **USBケーブルを抜く**。画面に `REL!` が出ていたか記録する |
| 意図しない入力が続く | ケーブルを抜く、またはCardputerの電源をOFFにする |
| 起動後に画面が点かない、または操作できない | 電源OFF→ON。Launcherの起動画面、または設定済みのボタンでLauncherに戻る（Launcherの種類によって操作が異なる。❓未確認） |
| Launcherに戻れない | **作業を止めて相談する。** 最終手段はG0ダウンロードモード（電源OFF → G0を押したまま電源ON → 離す）でLauncher本体を再書き込みすることだが、Launcherの上書きにあたるため、今回の作業範囲外 |

## 8. 変更履歴

| 版 | 変更 |
|---|---|
| r1 | 初版 |
| r2 | ① 2インスタンス構成の根拠をコメントと本書に明記（動作変更なし）／② USB準備完了ごとに全キー解放を先に送る／③ 送信前に `ready()` を確認し、未送信は再試行、結果不明は再送しない。押下成功を送信成功として数える／④ `W`・`T` の計測表示を追加／⑤ テスト・復旧手順をLauncher経由のみに変更 |
