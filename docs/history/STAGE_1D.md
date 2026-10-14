# 第1d段階 スピナー感度調整 — 実装・ビルド・試験手順

作成日：2026-10-12／作成：Claude Opus 5.5
状態：**ビルド成功。実機未インストール・実機未確認**（エドガーのコードレビュー待ち）

> **Status update (2026-10-13, appended; the line above is the status at build time):** The project author has since used this stage on real hardware and reported a positive, **subjective** result in a browser Breakout game (48–64 mouse counts/detent felt comfortable). See **§9** for exactly what was and was not observed. The §6 checklist was **not** executed item by item, and the SHA-256 of the binary that was uploaded to the device was **not** recorded. §10 records a repository path change.

凡例：✅ ソースまたはビルドで確認 / ❓ 実機未確認

**位置づけ：** RetroArch専用ではなく、汎用スピナー（PROFILE 2）の改善。

## 1. 背景（2026-10-12 ユーザーの実機試験）

第1c-1成功版で、MacのChromeのブラウザ版ブロック崩しを試した。

- Macの通常のマウスでは快適に遊べる。
- U135でもパドルは動く。
- **ただし、U135ではパドルの移動量が極端に小さく、ゲームとして遊べない。**
- 当時の感度は1目盛りあたり8 HIDカウント。

**原因は断定しない。** 候補は次の3つで、どれも切り分けていない。

1. 倍率の不足
2. macOSのマウス加速（小さく遅い入力がさらに小さく扱われる可能性）
3. ブラウザゲーム側の入力の読み方（カーソル位置を読むか、移動量を読むか）

第1dは、倍率を実機で変えて確かめられるようにするもの。

## 2. 納品物

| ファイル | サイズ | SHA-256 |
|---|---|---|
| `MediaJog_1d/MediaJog_1d.ino` | 33,566 B | `3b8c5667a551663912639d2cfd7c7dc5e7e187f45cf4313806aae1dd75ac30c6` |
| `MediaJog_1d/build/MediaJog_1d.ino.bin`（アプリ単体、LauncherのWi-Fiアップロード用） | 598,592 B | `2692467b9716ba6981947353256da79acf9b1e59a42f038e4af04640377b3d6f` |

- 同じ値を `MediaJog_1d/SHA256SUMS.txt` に記録し、`shasum -a 256 -c` で照合した（OK）。
- `MediaJog_1d/build/` のその他のファイル（merged.binなど）は使わない。
- 第1a・第1b・第1cの成功版と `U135_Probe` v2 のSHA-256は、作業後も一致した（変更なし）。Launcher、SOUND TERRARIUMには触れていない。

## 3. 動作仕様

| 操作 | PROFILE 1（YouTube） | PROFILE 2（SPINNER） |
|---|---|---|
| `Fn+1`／`Fn+2` | プロファイル切替（1cと同じ） | 同左 |
| **`Fn+;`（↑の刻印）** | 何もしない | **感度を1段階上げる** |
| **`Fn+.`（↓の刻印）** | 何もしない | **感度を1段階下げる** |
| U135右回転／左回転 | `.`／`,`（1cと同じ） | マウス相対移動 ＋X／−X（**感度＝1目盛りあたりのHIDカウント**） |
| U135押し込み | `k`（1cと同じ） | 左ボタン：押している間は押下、離すと解放（1cと同じ） |

- **感度の段階（案B、エドガーとの協議で決定）：** 1／2／3／4／6／8／12／16／24／32／48／64／96／128（14段階）。**初期値は8**（第1cと同じ）。
- 押した瞬間に1段階変わる。押し続けても繰り返さない。上限・下限ではそれ以上変わらない。
- 電源を切ると初期値（8）に戻る。NVSには保存しない。将来保存する場合は、段階の番号 `sensIndex` だけを保存すればよい構造にしてある。
- 本体キーはMacに送らない（1b以降と同じ）。
- 加速・慣性・自動感度変更は実装しない。

### 3-1. キーの判定（物理配置とライブラリの照合）

| 刻印（ユーザー提供のADVキーボード画像） | 物理位置 | ライブラリのキー配置（`Keyboard.h`） | 本スケッチの判定 |
|---|---|---|---|
| `fn` | 3段目の左端 | `_key_value_map[2][0] = KEY_FN` | `keysState().fn == true` |
| `;` とオレンジの ↑ | 3段目、`L` の右 | `_key_value_map[2][11] = ';'` | Fnありで `word` に `';'` → 感度アップ |
| `.` とオレンジの ↓ | 4段目、`,` と `/` の間 | `_key_value_map[3][11] = '.'` | Fnありで `word` に `'.'` → 感度ダウン |

- ライブラリには矢印専用のキーコードはなく、Fnを押しても文字は変わらない（`Keyboard.cpp:202-207`。変わるのはShift／Ctrl／CapsLockのときだけ）。そのため、「Fnフラグ＋基本文字」で判定している。
- 画像の4段目の `,`（←）と `/`（→）は、今回は使わない。
- Fnと同時に押された認識対象の文字（`1`、`2`、`;`、`.`）が**ちょうど1つ**のときだけ反応する。2つ以上同時なら無視する。
- `Fn+Shift+;` は `':'` として読まれるため反応しない。
- ADVのキーマトリクスから4×14配置への変換（`TCA8418.cpp` の `remap()`）が、`;`／`.` の位置でも正しいかは ❓（`1`／`2` は第1bで実機確認済み）。画面の `key:` 表示で確認する（§6）。

### 3-2. 移動量の計算

- U135は1目盛り＝カウンタ±2なので、カウンタ1つあたり「感度÷2」HIDカウントになる。
- **半カウント単位で計算する：** `half = 差分 × 感度`、`送る量 = (繰り越し + half) ÷ 2`（0方向に切り捨て）。余り（−1、0、+1の半カウント）は次に繰り越す。これで感度1や3のような奇数でも、1目盛りあたりの量が正確になる。例：感度3なら1目盛り＝3、半目盛り＝1（余り1を繰り越し）。
- 計算はすべて `int32_t` で行う。感度128、差分±32,767でも約±420万で収まり、オーバーフローしない。
- 1レポートの移動量は±127に切り詰めてから `int8_t` に変換する（TinyUSB標準マウス記述子は−127〜127。第1cと同じ）。
- 1ループ最大4レポート（±508）。未送信の移動量の上限は±1,016（2ループ分。第1cと同じ値）で、超えた分は捨てて `clip` に数える。
- **方向反転時の特別な処理はしない**（エドガーとの協議による）。逆方向の差分は符号どおりに加算されるだけで、第1cと同じ。
- **感度を変えたとき**は、未送信の移動量と繰り越しを捨てる（古い倍率の移動を新しい倍率と混ぜないため）。通常は毎ループ送り切っているので、捨てる量は通常ほぼない。
- 送信に失敗したときの扱い（移動は再送しない、ボタン状態は再送する）は第1cと同じ。

### 3-3. 高速回転時の見積もり（**実機未検証**）

| 項目 | 値 |
|---|---|
| 送信能力の上限 | 1ループ508 HIDカウント（4レポート×127） |
| 1ループの周期 | 20ms＋処理時間W（**Wは未測定**） |
| 感度128のとき | 1ループで約4目盛り分。Wを5msと仮定すると約160目盛り/秒、つまり約5回転/秒（1回転30目盛り） |
| それを超えたとき | 最大2ループ分まで溜め、それ以上は `clip` |

- カウンタは絶対値なので、読み取り間隔の間に回した分も数値上は差分に含まれる。
- ただし、**U135自体が高速回転を正しく数えられるか**（STM32のファームウェアは非公開）、送信やホスト側の処理が追いつくかを含め、**高速回転時に取りこぼしがないとは断定しない。実機未検証。**
- 感度の上限128で足りるかは、実機で評価する。

### 3-4. 画面

**PROFILE 2（U135接続時）は専用の画面**（240×135）：

```
MEDIA JOG 1d               USB HID READY   ← 文字サイズ1（USB状態は色分け）
───────────────────────────────────
PROFILE 2:SPINNER                          ← 文字サイズ2、シアン
SENSITIVITY                                ← 文字サイズ2
              8          6/14              ← 数字は文字サイズ4、右に段階番号
Fn+. -   Fn+; +                            ← 文字サイズ2、黄
BTN UP X+1234 rep120 fail0                 ← 診断（文字サイズ1）
clip0 ERR0 W12 key:Fn+;
```

- **全画面の描き直し**は、画面の種類が変わったとき（プロファイル切替、U135の検出・喪失）とUSBの状態が変わったときだけ行う。
- **感度の数字**は、変更したループでその部分だけ上書きする（全画面消去なし）。
- **診断の2行**は最大4回/秒、その2行だけ上書きする。**回転だけでは全画面の描き直しは起きない**（第1cでは、回転中に約100msごとに全画面を描き直していた）。
- `key:` は、最後に押した本体キーの基本文字（Fnを押していれば `Fn+` が付く）。キー判定の確認用。
- PROFILE 1の画面、およびU135未接続時の画面は第1cと同じ。表示名だけ「PROFILE 2 MOUSE」→「PROFILE 2 SPINNER」に変えた。
- 描画にかかる時間は未測定。`W`（1ループの処理時間の最大値）で観察する。

## 4. 第1cからの変更点と保全

| 変更あり | 変更なし（第1cと同じ） |
|---|---|
| 固定倍率 `MOUSE_GAIN` を、感度の表 `SENS_STEPS` と `sensIndex` に置き換え | U135の読み取り（v2由来） |
| 半カウントの繰り越し `mouseHalfRem`。移動量を捨てる既存の箇所（USB未準備、プロファイル切替、U135喪失）でも、あわせて0にする | USBの処理（HID構成、`onUsbEvent`、`updateUsbState` の判定、`SendReport` の呼び方） |
| Fnの判定で `;`／`.` も扱う（`Chord` の列挙） | キーボード送信（`tapKey`、`sendQueued`、解放の再送） |
| `changeSensitivity()` を追加 | マウスボタンの押下・解放、押しっぱなし防止、`sendMouse()` |
| PROFILE 2の専用画面と、部分的な描き直し | PROFILE 1の動作と画面 |
| `updateUsbState()` 内で、繰り越しのクリアと画面の描き直し要求を追加（USBの判定そのものは変えていない） | `M5.begin()` の設定、初期化順序 |

ビルド設定は第1cと同じ（USB Mode = USB-OTG (TinyUSB)、USB CDC On Boot = Enabled）。違う値にすると `#error` で止まることを確認した。

## 5. ビルド結果（2026-10-12）

| 項目 | 結果 |
|---|---|
| USB-OTG ＋ CDC On Boot=Enabled | ✅ 成功（exit 0）。フラッシュ 598,439 B（45%）、RAM 60,028 B（18%） |
| `--warnings all` でのスケッチ由来の警告 | ✅ 0件（ライブラリ由来の `KEY_TAB` 再定義の警告は、第1b・第1cと同じく出る） |
| 設定違いでのビルド | ✅ USB Mode、CDC On Bootのどちらも `#error` で停止 |
| 第1cとの差分 | ✅ USBの処理、キーボード送信、マウスボタン処理に変更がないことをdiffで確認 |

**実機での動作は一切確認していない。** 以下の§6はすべて未実施。

## 6. 実機試験手順（LauncherのWi-Fiアップロード。IDEからの直接書き込みとmerged.binは使わない）

**準備：**
- Launcherで第1c成功版をすぐに起動し直せる状態にする。
- ドラッグの試験は、試験用フォルダの中だけで行う。

| # | 手順 | 確認内容 | 結果 |
|---|---|---|---|
| 1 | Launcherから `MediaJog_1d.ino.bin` を起動 | Launcherが維持されている | ❓ |
| 2 | PROFILE 1のまま、YouTubeでコマ送り・コマ戻し・再生／停止 | 第1cと同じように動く | ❓ |
| 3 | `Fn+2` | 専用画面「PROFILE 2:SPINNER」「SENSITIVITY 8」「6/14」 | ❓ |
| 4 | `Fn+;` を数回、`Fn+.` を数回 | 感度が1段階ずつ変わる。`key:Fn+;`／`key:Fn+.` と表示される。1と128で止まる | ❓ |
| 5 | 同上 | 数字がすぐに切り替わる（画面全体がちらつかない） | ❓ |
| 6 | Finderで、感度8と32で同じ目盛り数だけ回す | 感度を上げるとポインタの移動量が増える（比例するかどうかも観察。macOSの加速の影響あり） | ❓ |
| 7 | ブロック崩し（Chrome）で感度を変えながら試す | パドルの移動量が増える。**遊べる感度があれば、その値を記録** | ❓ |
| 8 | 感度を下げる | 細かな位置調整ができる | ❓ |
| 9 | 素早く回す（感度32、128など） | 極端な遅延がない。`clip`、`W` の値を記録 | ❓ |
| 10 | 押したまま回す（Finderの試験用フォルダ） | ドラッグできる | ❓ |
| 11 | ドラッグ中に `Fn+1` | ドラッグが解除される | ❓ |
| 12 | テキストエディットにフォーカスして `Fn+;`、`Fn+.`、`;`、`.`、その他のキー | Macに何も入力されない | ❓ |
| 追加 | PROFILE 1で `Fn+;`／`Fn+.` | 何も起きない（PROFILE 1の動作に影響しない） | ❓ |

**記録してほしい値：**
- 遊べた感度（なければ「128でも不足」など）
- 試験6での見た目の比例関係
- `clip`／`fail`／`ERR`／`W`
- USB初回認識の状況（REPORT.md §18の項目）

**USBの再接続後の自動復帰が未解決（第1c）であることは変わらない。** 第1dでUSBの処理は変えていないので、改善したとは扱わない。

## 7. 復旧手順（Launcherを上書きしない範囲）

| 症状 | 対処 |
|---|---|
| ポインタが動き続ける、ボタンが押されたまま | USBケーブルを抜く |
| 画面が点かない、または操作できない | 電源OFF→ON。Launcherに戻り、**第1c成功版**を起動する |
| USBを挿し直した後も `USB SUSPENDED` のまま | 物理電源をOFF→ON（第1cと同じ既知の課題。再起動するため、PROFILE 1、感度8に戻る） |
| 本体キーでMacに入力が起きた | 直ちに中止して記録する（設計上は起きないはず） |
| Launcherに戻れない | 作業を止めて相談する |

## 8. レビューで確認してほしい点

1. 半カウントの繰り越し（§3-2）と、感度変更時に未送信分を捨てる扱い。
2. 感度128時の送信能力（1ループ4レポート、未送信上限±1,016は第1cのまま）。
3. PROFILE 2の画面だけ部分的に描き直す方式（§3-4）。PROFILE 1の画面は第1cのまま。
4. Fn判定で「認識対象の文字がちょうど1つ」のときだけ反応する条件。

---

## 9. Hardware observation (appended 2026-10-13, in English)

**Source:** report from the project author. The exact date of the session was not recorded (it took place between the build on 2026-10-12 and this note on 2026-10-13). Host: macOS, Chrome, a browser-based Breakout game controlled with the mouse.

### 9-1. What the author reported

- The Stage 1d spinner (Profile 2) was **used successfully** to play the browser Breakout game.
- Sensitivity values of **48–64 mouse counts per detent** felt **comfortable**.
- **Rapid left–right paddle movement felt smooth, without noticeable catching or lag.**
- For comparison: with the fixed value of 8 used by Stage 1c-1, the paddle moved too little to be playable (REPORT.md §24-1).

### 9-2. How to read this result

- This is a **subjective, game-specific** observation on one host and one game.
- It is **not** evidence of compatibility with other games, emulators or operating systems.
- It is **not** a measurement of encoder-count accuracy, and **not** proof that no counts are lost at high speed. No `clip`, `fail`, `ERR` or `W` values were recorded.
- 48–64 is the author's preferred range for this game, **not** a recommended default. The firmware default remains 8.
- **Binary identity:** the SHA-256 of the file uploaded through the Launcher was **not recorded**. The values in §2 identify the file in this repository; they do not prove that the same file was used in the session.

### 9-3. §6 checklist status after this observation

The §6 checklist was **not** executed item by item. Items are marked as follows:
- **Observed:** reported by the author.
- **Inferred:** follows from the report and the firmware design, but was not separately reported.
- **Not recorded:** no information.

| # | Item (§6) | Status |
|---|---|---|
| 1 | Launcher remains installed | Not recorded |
| 2 | Profile 1 YouTube controls unchanged | Not recorded |
| 3 | `Fn+2` shows the Spinner screen | Not recorded |
| 4 | Sensitivity changes with the Cardputer keys | **Inferred** for raising: the firmware starts at 8 after every power-on, so reaching 48–64 requires `Fn+;` to have worked. Lowering (`Fn+.`) and the 1/128 limits were not reported |
| 5 | Display follows the change immediately | Not recorded |
| 6 | Higher sensitivity → larger pointer movement in Finder | Not recorded (the Breakout result in item 7 points the same way, but Finder was not reported) |
| 7 | Breakout paddle movement increases | **Observed** (subjective): played successfully at 48–64 |
| 8 | Lower sensitivity allows fine positioning | Not recorded |
| 9 | Fast rotation without severe lag | **Observed** (subjective): "smooth, without noticeable catching or lag". No `clip`/`W` values recorded |
| 10 | Drag with knob held | Not recorded for 1d (worked in 1c-1) |
| 11 | `Fn+1` releases a drag | Not recorded for 1d (worked in 1c-1) |
| 12 | Cardputer keys (including `Fn+;`, `Fn+.`) not sent to the Mac | Not recorded for 1d (verified for `Fn+1`/`Fn+2` and other keys in 1b) |
| Extra | `Fn+;`/`Fn+.` do nothing in Profile 1 | Not recorded |

Pointer direction (clockwise → right) was not separately recorded. The firmware sends +X for clockwise rotation, and HID +X means right.

## 10. Repository path note (appended 2026-10-13)

- The original encoder probe was moved from the repository root into a sketch folder (by 2026-10-13):
  - `U135_Probe/U135_Probe.ino`
  - `U135_Probe/U135_Probe.ino.bin`
- Earlier documents, including §2 of this file, refer to the old root paths. Those references are kept as written (historical record).
- SHA-256 after the move (checked 2026-10-13), **unchanged** from the recorded values:
  - `U135_Probe/U135_Probe.ino`: `14aaca87d57e018e2eab64a2e183e38c95f4cfee14f29fe597b6b6a0cbbe80fa`
  - `U135_Probe/U135_Probe.ino.bin`: `0323b74b4e57a826278de6c32be93fbde1ac45dca8bc4b510f5eddaafbcbd375`
- `MediaJog_1d/MediaJog_1d.ino` and `MediaJog_1d/build/MediaJog_1d.ino.bin` still match `MediaJog_1d/SHA256SUMS.txt` (checked 2026-10-13).
- **Later on 2026-10-13:** `U135_Probe/U135_Probe.ino` received an attribution-only comment fix (new SHA-256 `3bb64b72…`). The tested original (`14aaca87…`) is preserved byte-for-byte in `archive/U135_Probe/U135_Probe.ino`, and the binary is unchanged. See REPORT §28.
