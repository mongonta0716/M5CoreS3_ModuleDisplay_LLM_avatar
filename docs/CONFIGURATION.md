# 構成・接続・操作の詳細

[READMEへ戻る](../README.md)

## CoreS3の環境とフラグ

`src/AppConfig.h`の2つの値で、コンパイル時に使用するモジュールを選択します。
ヘッダーの既定値はどちらも`0`です。現在の`platformio.ini`では実機構成に合わせ、
共通ビルドフラグで`USE_MODULEDISPLAY=1`を指定しています。この指定は継承先にも適用されます。
以下は、その共通指定を外した場合の環境ごとの標準構成です。

| USE_MODULELLM | USE_MODULEDISPLAY | マイク・解析先 | 表示・音声出力 | PlatformIO環境 |
| --- | --- | --- | --- | --- |
| `0` | `0` | CoreS3内蔵マイク・CoreS3 | 本体LCD・本体スピーカー | `m5stack-cores3` |
| `0` | `1` | CoreS3内蔵マイク・CoreS3 | 本体LCD＋HDMI・HDMI音声 | `m5stack-cores3-display` |
| `1` | `0` | ModuleLLM内蔵マイク・AX630C | 本体LCD・本体スピーカー | `m5stack-cores3-modulellm-vowel` |
| `1` | `1` | ModuleLLM内蔵マイク・AX630C | 本体LCD＋HDMI・HDMI音声 | `m5stack-cores3-modulellm-display` |

通常の`pio run`と`pio run -e m5stack-cores3`は、共通ビルドフラグとヘッダーの値を使います。
`m5stack-cores3-hello`も同じアプリの互換用エイリアスです。
上表の追加モジュール用環境は、対応する値を`-D...=1`で上書きします。
ビルドフラグがヘッダーより優先されるため、モジュールを無効に戻す場合は
共通フラグの`-DUSE_MODULEDISPLAY=1`も外し、ヘッダーを`0`にして
`m5stack-cores3`で再ビルド・書き込みしてください。
古い診断用`m5stack-cores3-legacy`はこの切り替えの対象外です。

`USE_MODULEDISPLAY=0`ではHDMIへのアクセスを行わず、Hello WorldとAvatarを本体LCDに表示し、
起動時とAの「ピポ」は本体スピーカーから再生します。
`1`では従来どおりHello WorldをHDMIに表示した後、両画面でAvatarを動かし、HDMIへ音声を出します。
どちらもHello Worldは2秒、下40pxのA/B/Cタッチボタンを使い、再生中は口パクを抑止します。

`USE_MODULELLM=0`では個人設定をCoreS3のNVSへ保存します。
`1`ではCoreS3のマイクを起動せず、ModuleLLMからUARTで受けた母音・開き量を使い、
調整と保存もModuleLLM側で行います。2つの個人設定は独立し、互いの値を削除・流用しません。
ModuleLLM未接続時も画面とAの操作は継続します。

## AtomS3／AtomS3R＋EchoBase／PDMUnit

実機に合わせてPlatformIOの環境を選びます。

| 実機 | 環境 | Flash／PSRAM |
| --- | --- | --- |
| AtomS3 | `AtomS3-noPSRAM` | 8MB／なし |
| AtomS3R | `AtomS3R` | 8MB／8MB Octal |

既存の`AtomS3`環境は、互換性のため従来どおりAtomS3R向けとして残しています。
通常のAtomS3には`AtomS3-noPSRAM`を指定してください。
両環境とも128×128 LCDと外部音声入力を使い、ModuleLLM／ModuleDisplayは初期化しません。

```sh
# AtomS3
pio run -e AtomS3-noPSRAM
pio run -e AtomS3-noPSRAM -t upload
pio device monitor -e AtomS3-noPSRAM

# AtomS3R
pio run -e AtomS3R
pio run -e AtomS3R -t upload
```

起動時にEchoBaseのES8311（GPIO38=SDA、GPIO39=SCL、I²Cアドレス`0x18`）を確認します。
応答すればEchoBaseを優先し、応答しなければGroveへ接続したPDMUnitを選択します。
PDMの接続は白=CLK（GPIO1）、黄=DATA（GPIO2）、赤=5V、黒=GNDです。
PDMUnitにはI²Cの識別情報がないため、非接続でもPDM側が選ばれます。
入力の選択は起動時に行います。機器を変更するときは電源を切って接続し直してください。

AはAtomS3／AtomS3R本体の画面を押し込むボタンです。

| AtomS3／AtomS3RのボタンA | 動作 |
| --- | --- |
| 1回クリック | 離して350ms後にCal：静音2秒→各母音3秒×2回、発声の間に息つぎ2秒 |
| ダブルクリック | 画面を90度ずつ回転し、向きをNVSへ保存 |
| 3秒間押し続ける | 現在選択中の入力の調整値をリセット（1回だけ） |

ダブルクリックは、1回目を離してから350ms以内に2回目を押し、離したときに確定します。
単押しはダブルクリックの待ち時間の後にCalを開始します。2回目を3秒長押しした場合もリセットを優先し、
長押しを離したときにCalや回転は実行しません。調整中の単押しは調整を最初から開始します。

画面の向きは0→90→180→270→0度と切り替わり、再起動後はHello Worldから保存済みの向きになります。
向きは音声調整と別のNVS領域に保存するため、入力切り替えや音声調整のリセットで変更されません。
回転時はAvatarの描画領域を作り直し、表情・口パクのデモ状態と、進行中の音声調整を引き継ぎます。
128×128画面では、調整・保存結果を読みやすい専用画面へ表示し、その後Avatarへ戻ります。
通常時はライブラリのAvatarと母音表示を使い、画面下部のタッチボタンは描きません。
EchoBaseでは起動時に「ピポ」を鳴らし、再生を終えてから録音を始めます。
PDMUnitにはスピーカーがないため起動音はありません。CoreS3のA/B/C操作は従来どおりです。

録音にはM5Unified 0.2.26の標準`M5.Mic`を使います。
EchoBaseでは`external_speaker.atomic_echo`によるコーデック初期化と、
ES8311に合わせた左チャンネル入力・左右チャンネルへの起動音出力、
PDMではI2S0のPDM入力を設定し、共通のFFT・フォルマント解析へ渡します。
EchoBase用とPDM用の調整値は別のNVSキーへ保存し、CoreS3用の既存値も保持します。
初めて選択した入力ではAを単押しして調整してください。
シリアルの`c`（Cal）、`x`（現在の入力のリセット）も利用できます。

## 接続

1. 使用する構成に合わせてフラグを設定します。モジュールを使用する場合は、電源を切ってM5-Busでスタックします。
2. ModuleDisplayを使用する場合は外部電源と720p対応のHDMIモニターを接続し、モニターの入力を選びます。
3. CoreS3をUSB-CでPCへ接続し、スタックへ給電します。

CoreS3用M5GFXのModuleDisplay設定はSPIのCS=G7、MOSI=G37、MISO=G35、SCLK=G36、I2CのSDA=G12、SCL=G11を使用します。
ModuleLLMの工場出荷時UART配線（CoreS3側RX=G18、TX=G17）とは重複しません。
ModuleLLMのピンを変更済みの場合、ModuleDisplayが使用するG7を選択しないでください。

ModuleLLMの給電は機能フラグとは独立しています。現在のアプリはM-Bus給電の有効化を要求します。
起動しない場合はModuleLLMのUSB-C給電も確認し、[電源とマイクの切り分け](TROUBLESHOOTING.md#modulellmを重ねるとマイクが動かない)を参照してください。

配線の根拠: [M5GFX / M5ModuleDisplay.h](https://github.com/m5stack/M5GFX/blob/0.2.32/src/M5ModuleDisplay.h)、[ModuleLLMのピン切替](https://docs.m5stack.com/en/compute/module_llm/pins_change)。

## CoreS3の調整画面と操作

1. 本体画面下部の **B: Cal** を押します。
2. 吹き出しに「静かに」と残り秒数が出るので、2秒間静かにします。
3. 画面の吹き出しの指示と残り秒数に従って「あー」「いー」「うー」「えー」「おー」を、それぞれ **3秒ずつ2回** 発声します。各発声の間と母音の切り替え前に2秒の息つぎが入ります。
4. 吹き出しに「声の調整」「保存完了」と表示されたら完了です。再起動後も同じ設定を使います。

Aで調整を中止し「ピポ」を再生、C: Resetで保存した声の設定を消して初期値へ戻します。
十分な有声音が取れない場合、測定のばらつきが大きい場合、5母音の特徴が重なった場合は保存せず、以前の設定を使います。
「音声不足」「母音不安定」が出たら、静かな場所でマイクに近づいてBからやり直してください。
調整中の吹き出しは発声指示とカウントダウン、終了後の6秒間は完了・不足・中止などの結果を示します。
ModuleDisplay有効時は両画面に表示します。案内は顔の右上に24ピクセルの文字で2行表示します。「あー 1回目」「あと3秒」、休憩中は「息つぎ」「あと2秒」と表示します。調整全体は約50秒です。
その後は推定母音の表示へ戻ります。口は調整中もマイクの推定結果で動き、調整完了後に新しい特徴値へ切り替えます。

## 設定の保存先

| 設定 | 保存先 |
| --- | --- |
| CoreS3内蔵マイクの調整 | NVSの`vowel-lipsync`、キー`model` |
| AtomのEchoBase／PDMUnitの調整 | NVSの`vowel-lipsync`、キー`atom-echo`／`atom-pdm` |
| Atomの画面の向き | NVSの`atom-display`、キー`rotation`（0〜3） |
| ModuleLLMのマイクの調整 | ModuleLLM側のモデルファイル。詳細は[ModuleLLMガイド](../modulellm/README.md) |

音声調整値は入力ごとに独立しています。画面の向きはマイクを切り替えても維持し、音声調整のリセットでも消しません。

## USBシリアル操作

通常のCoreS3／Atomアプリで使えます。

| シリアル入力 | 操作 |
| --- | --- |
| `n` | Neutral |
| `h` | Happy |
| `a` | Angry |
| `s` | Sad |
| `d` | Doubt |
| `z` | Sleepy |
| `t` | 口パクのデモをON / OFF（音声出力なし） |
| `c` | 母音の声の調整を開始 |
| `x` | 保存した声の設定を消して初期値へ戻す |

CoreS3の旧診断用`m5stack-cores3-legacy`では、本体画面のタッチで
Neutral → Happy → Angry → Sad → Doubt → Sleepyの順に表情を切り替えます。
診断用環境は[トラブルシューティング](TROUBLESHOOTING.md)を参照してください。

## Atomの参考実装

自動選択は[washishi氏の参考実装](https://github.com/washishi/m5stack-avatar-mic-nekomimi_led/blob/main/src/main.cpp)を参考にしています。
旧版で使用されていた別のI2Sドライバーは併用せず、M5Unifiedが録音とスピーカーのポートを管理します。
ボード定義は[M5Stack公式のAtomS3R設定](https://github.com/m5stack/M5Utility/blob/main/boards/m5stack-atoms3r.json)に基づきます。
