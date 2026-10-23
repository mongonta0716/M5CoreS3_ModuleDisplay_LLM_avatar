# M5Unified Avatar Mic

M5Stack CoreS3／AtomS3／AtomS3RのAvatarを、音量と日本語の母音推定に合わせて口パクさせるプロジェクトです。
描画には[M5Unified-Avatar](https://github.com/mongonta0716/M5Unified-Avatar)を使用します。

## 準備と環境の選択

電源を切って機器を接続し、実機に合うPlatformIO環境を選びます。

| 構成 | PlatformIO環境 |
| --- | --- |
| AtomS3＋EchoBaseまたはPDMUnit | `AtomS3-noPSRAM` |
| AtomS3R＋EchoBaseまたはPDMUnit | `AtomS3R` |
| CoreS3単体 | `m5stack-cores3` |
| CoreS3＋ModuleDisplay | `m5stack-cores3-display` |
| CoreS3＋ModuleLLM | `m5stack-cores3-modulellm-vowel` |
| CoreS3＋ModuleLLM＋ModuleDisplay | `m5stack-cores3-modulellm-display` |

**ModuleDisplayなしのCoreS3**では、[platformio.ini](platformio.ini)の共通設定にある
`-DUSE_MODULEDISPLAY=1`を外し、[AppConfig.h](src/AppConfig.h)の`USE_MODULEDISPLAY`を`0`にしてください。
現在の共通設定はHDMI有効です。Atom用環境はこの設定を上書きするため、変更不要です。

- **AtomS3／AtomS3R**：EchoBaseを底面、またはPDMUnitをGrove端子へ接続します。入力は起動時に自動選択します。
- **ModuleDisplay**：DC 9〜24Vの外部電源とHDMIモニターを接続し、電源スイッチを入れます。
- **ModuleLLM**：CoreS3の書き込みに加え、[ModuleLLM側の配置・起動](modulellm/README.md)が必要です。

通常のAtomS3には`AtomS3-noPSRAM`を使います。互換用の`AtomS3`環境はAtomS3R向けです。
配線・フラグ・給電の詳細は[構成ガイド](docs/CONFIGURATION.md)を参照してください。

## ビルド・書き込み

PlatformIO CoreまたはVS CodeのPlatformIOを使います。リポジトリのルートで実行してください。
以下はAtomS3の例です。ほかの構成では環境名を上の表から選んで置き換えます。

```sh
pio run -e AtomS3-noPSRAM
pio run -e AtomS3-noPSRAM -t upload
pio device monitor -e AtomS3-noPSRAM
```

初回ビルド時に依存ライブラリを取得します。接続先を自動選択できない場合は、
`pio device list`でポートを確認し、書き込みに`--upload-port`、モニターに`--port`を指定してください。

## 使い方

起動するとHello Worldを2秒表示してからAvatarへ切り替わります。マイクに話しかけると口が動き、
推定した「あ・い・う・え・お」を吹き出しに表示します。「…」は無音や判定が不確かな状態です。

CoreS3のボタンは画面下部のタッチ領域、AtomS3／AtomS3RのAは画面を押し込むボタンです。

| 機種 | 操作 | 動作 |
| --- | --- | --- |
| CoreS3 | A | 「ピポ」を再生。調整中は中止 |
| CoreS3 | B | 声の調整を開始 |
| CoreS3 | C | 声の調整値をリセット |
| AtomS3／AtomS3R | Aを単押し | 声の調整を開始 |
| AtomS3／AtomS3R | Aをダブルクリック | 90度ずつ回転し、向きを保存 |
| AtomS3／AtomS3R | Aを3秒長押し | 選択中のマイクの調整値をリセット |

Atomの単押しは、ダブルクリックを待つ350msの後に確定します。保存した画面の向きは再起動後も復元されます。

### 初回の声の調整

1. CoreS3はB、AtomはAの単押しで調整を始めます。
2. 画面の案内に従い、最初の2秒は静かにします。
3. 「あー・いー・うー・えー・おー」を各3秒ずつ2回発声します。発声の間は2秒の息つぎです。
4. 約50秒で「保存完了」と出たら終了です。調整値は再起動後も使われます。

「音声不足」「母音不安定」の場合は以前の設定を保持します。静かな場所でマイクに近づいてやり直してください。
母音判定は口パク向けの簡易推定です。弱い判定では音量による開閉に戻ります。

## 詳細ガイド

- [構成・接続・操作の詳細](docs/CONFIGURATION.md)：フラグ、ピン、入力選択、保存設定、シリアル操作
- [音声解析・描画・検証](docs/IMPLEMENTATION.md)：FFT／LPC、口形、拡大率、RAM使用量、実装とテスト
- [トラブルシューティング](docs/TROUBLESHOOTING.md)：HDMI、音声、給電、ビルドエラー、ログと診断
- [ModuleLLMのセットアップ](modulellm/README.md)：AX630C側の配置・サービス・UART仕様
