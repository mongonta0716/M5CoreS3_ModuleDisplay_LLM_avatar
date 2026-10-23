# トラブルシューティング

[READMEへ戻る](../README.md)

## 最初に構成を確認する

実機とPlatformIO環境が一致しているか、[構成ガイド](CONFIGURATION.md)を確認します。
特に、通常のAtomS3は`AtomS3-noPSRAM`、AtomS3Rは`AtomS3R`です。
互換用の`AtomS3`環境はAtomS3R向けです。

CoreS3は起動ログの`Config: USE_MODULELLM=... USE_MODULEDISPLAY=...`で実際の構成を確認します。
共通ビルドフラグはヘッダーより優先されます。ModuleDisplayなしで使う場合は
`platformio.ini`共通設定の`-DUSE_MODULEDISPLAY=1`を外し、ヘッダーを`0`にします。

CoreS3のPSRAMは`qio_qspi`、AtomS3Rは`qio_opi`です。通常のAtomS3で`PSRAM=0`は正常です。
CoreS3／AtomS3RでPSRAMを検出できない場合は、環境とメモリ設定を確認し、通常アップロードで
ブートローダーとアプリを書き直してください。

## Apple Siliconでmkspiffsが実行できない

`mkspiffs_espressif32_arduino: Bad CPU type in executable`は、Intel向けの
SPIFFS作成ツールをホストMacが実行できない場合のエラーです。
旧設定の`m5stack-cores3`は空のSPIFFSを生成していましたが、現在は
`custom_bundle_filesystem = no`を指定し、通常のビルド・uploadでは呼び出しません。
VS Codeでは`m5stack-cores3`を選択して書き込めます。ModuleLLMを使う場合は
`AppConfig.h`の2つの構成フラグ、または[構成ガイド](CONFIGURATION.md)の環境を選択してください。

## HDMIが表示されない

現在のアプリはソフトウェアI2CでHDMIを初期化します。以下の最大3回のリトライと追加診断は
`m5stack-cores3-legacy`の動作です。
起動ログの`Power`はM-Bus出力の有効状態、USB電圧、AXP2101のTS入力電圧（電源判定用）を表示します。

- `Chip ID: 17 02 e2`と`FPGA ID:48440004`が読めても、`failed to initialize the HDMI transmitter`が出るとHDMI画面は登録されません。描画用バッファを小さくしても、この初期化失敗は解消しません。
- `setExtPower(true) is canceled`はM5Unifiedが外部出力の有効化を取り消したログです。外部給電の検出や電源状態の読み取り失敗などで発生するため、これだけで電源不足とは断定できません。電源保護を直接レジスタ操作で解除しないでください。
- 再書き込み後も失敗する場合は、電源を切ってModuleLLMを外し、CoreS3 + ModuleDisplayだけで比較してください。併せてスタックの接触、給電方法、HDMIケーブルとモニター入力を確認します。
- `free PSRAM: 0`が続く場合は、まずPSRAMの設定と書き込んだファームウェアを確認してください。

HDMI初期化が3回失敗した場合は、`HDMI diag:`で追加診断を行います。
同じSDA=G12 / SCL=G11で、ハードウェアI2Cの100kHz / 25kHzとソフトウェアI2Cの25kHzを比較します
（周波数は要求値で、特にソフトウェアI2Cの実効速度はGPIO操作の時間にも依存します）。
診断はバンク選択レジスタ`0xff`とID読み取り許可`0xee`だけを書き込み、電源設定は変更しません。
バンク`81`が低速またはソフトウェアI2Cでだけ成功するかを調べるもので、HDMI出力の復旧処理ではありません。
診断後は共有I2Cピンの設定を復元します。再試行する場合は実機を再起動してください。

### HDMIドライバーとI2Cの背景

Hello World版はM5GFX **0.2.33**を使用します。0.2.32には、HDMI送信ICへの書き込みが
失敗扱いになると初期化シーケンスを途中で打ち切る処理があります。
[公式修正 #339](https://github.com/m5stack/M5GFX/pull/339)では、ModuleDisplayのI2C直列抵抗に
よりACKのLow電圧が高くなり、書き込みが受理されていてもNACKと誤判定する事例が報告されています。
特に`reg:ff val:81`の失敗と、その後のHDMI初期化失敗が今回の過去ログと一致します。
0.2.33では設定シーケンス全体を送信するよう修正されています。
今回のCoreS3では0.2.33への更新だけでは初期化に失敗しました。
バンク`0x81`は失敗扱いでも実際には切り替わっていましたが、その後ハードウェアI2Cの通信が
連続して失敗しました。同じピンでソフトウェアI2Cにすると読み戻しに成功し、
**0.2.33 + ソフトウェアI2Cで初期化・描画に成功、ユーザーもHello Worldの実表示を確認済み**です。
ACK電圧の実測や、ハードウェアI2Cの内部状態が失敗する詳細は未確認です。
[調査結果と比較手順](../diagnostics/README.md)を参照してください。
従来の2画面Avatar版（`m5stack-cores3-legacy`）の依存ライブラリは現時点では0.2.32のままです。

### 表示と音声の実装上の条件

以下はCoreS3内蔵マイク＋ModuleDisplay（`USE_MODULELLM=0`、`USE_MODULEDISPLAY=1`）の構成です。

M5GFXのHDMIドライバーを使用し、`display.init()`で初期化します。
`CoreS3ModuleDisplay`は標準`M5ModuleDisplay`と同じピン・SPI設定を使い、
HDMI送信ICの制御をソフトウェアI2C（port=-1、SDA=12、SCL=11）へ切り替え、
解像度・拡大率は`AppConfig.h`から設定します。
CoreS3本体には操作画面を表示します。M5Unifiedの`setTouchButtonHeight(40)`で画面下40ドットを
A/B/Cのタッチボタン領域にし、左側のAを押すとHDMIモニターから「ピポ」の2音が鳴ります。
起動時も同じ音を鳴らします。Bは声の調整、Cは声の調整を初期値へ戻す操作です。
起動時・A押下時の両方で、ユーザーがモニターからの実際の音を確認済みです。
ModuleDisplay有効時のHDMI音声は48kHzステレオのI2S（BCK=G6、DATA=G13、LRCK=G0）を使います。
内蔵マイク（MCLK=G0、BCK=G34、WS=G33、DATA=G14）とHDMI音声はI2S1とG0を共有するため、
「ピポ」の再生中は録音・口パクを一時停止し、再生後に録音を再開します。
2026-10-17に、両画面の口パク・Aの発音・再生後の口パク復帰をユーザーが実機確認済みです。
モニターのスピーカーまたは音声出力を使用し、HDMI入力の音量を有効にしてください。
既存の`DualDisplayAvatar`とM5Unified-Avatarを使います。`M5.begin()`は本体LCD・タッチのために呼び、
内蔵マイクを有効にし、内蔵スピーカー・IMU・RTC・自動外部画面初期化は無効にしています。
ModuleDisplay側の外部給電が必要です。独自リトライ・追加I2C診断は既定ビルドから外します。

## ModuleLLMを重ねるとマイクが動かない

ModuleLLMの給電は`USE_MODULELLM`／`USE_MODULEDISPLAY`とは別です。CoreS3からのM-Bus給電は、M5Unified標準と同じ
`cfg.output_power=true`で有効化を要求します。
起動ログの`M-Bus power: requested=... enable_latch=...`は要求とBUS_OUT_ENの制御状態を示し、実際の5V電圧を測定した値ではありません。
CoreS3＋ModuleLLMを重ねた`USE_MODULELLM=0` / `USE_MODULEDISPLAY=0`の実機では、CoreS3のUSB1本で
ModuleLLMが起動し、起動音・Aの音・口パク・A再生後の復帰が正常なことを確認しました。
この動作確認後も`setExtPower(true) is canceled`と`actual=off`のログが残っており、実際の給電経路と状態判定の関係は未確認です。
`USE_MODULELLM=0`でも、接続したModuleLLMは電気的に切り離されません。
以前の`output_power=false`の実機では、未給電のModuleLLMを重ねるとCoreS3のマイク入力がすべて0になりました。
同じファームウェアで、ModuleLLMを取り外すか、そのUSB-Cにも給電すると音と口パクが動くことを確認済みです。
M-Busからの給電が有効にならない場合は、ModuleLLM側にもUSB給電するか、使用しないモジュールを取り外してください。
未給電時に影響を受ける内部信号や電気的な仕組みまでは特定していません。
依存先に固定しているM5Unified 0.2.26でも`false`時の同じ現象を確認しました。公式報告との照合結果は
[電源・マイクの調査記録](../diagnostics/README.md#cores3modulellmの電源と内蔵マイク2026-10-20)を参照してください。

## HDMIの音が聞こえない

モニターのスピーカーまたは音声出力を使い、HDMI入力の音量を有効にしてください。
`HDMI audio: ready`、`HDMI chime: startup queued`／`HDMI chime: BtnA queued`は
初期化と再生要求のログです。実際の発音は出力先でも確認します。
HDMIキャプチャーをQuickTimeで聞く場合は、QuickTime／macOSの音声出力先を確認してください。
過去の無音報告は出力先の設定変更で解消しました。
PDMUnitにはスピーカーがないため、Atom＋PDMUnitでは起動音はありません。

## 調整できない・母音が合わない

初回やマイクを変更したときは、[声の調整](../README.md#初回の声の調整)を実行します。
「音声不足」「母音不安定」では前のモデルを保持します。静かな場所で、マイクに近づいてやり直してください。
息つぎ中や各発声の最初の500msは収集から除外します。判定が弱い区間は音量による口の開閉に戻ります。
ささやき声・無声化した母音・雑音・速い発話・高い声では判定できないことがあります。

## ログの見方

| ログ | 確認する内容 |
| --- | --- |
| `HelloWorld: held for 2000 ms; starting Avatar` | 起動画面からAvatarへの切り替え |
| `Avatar ready:`／`Avatar targets:` | 描画先とバッファの準備 |
| `HDMI: output 1280x720@60, logical 320x180, hardware scale 4` | HDMI出力・論理解像度・拡大率 |
| `Avatar perf:` | 実際の描画fpsと平均描画時間。CoreS3の2画面構成は2画面一組の速度 |
| `Mic config:` | I2Sポート・録音ピン・チャンネル設定 |
| `Mic FFT: ready`／`Mic FFT: listening` | 録音開始・処理の継続・音量・口の開き |
| `Atom audio:`／`Atom board:` | 選択入力と検出したボード・LCD |
| `Atom rotation:` | 保存した向きとNVS保存結果・起動時の復元値 |
| `Atom memory:` | 内部RAMの空き・最小空き・最大連続領域・PSRAM容量 |
| `Vowel calibration: saved to NVS`／`Atom calibration: save OK` | 調整値の保存成功 |

`Mic FFT`の`frames`は解析済みフレーム数、`level`は帯域振幅の集計、`peak`はPCMの最大絶対値、
`mouth`は0〜1の開き量です。`Vowel:`には母音・確信度・F1／F2・音量・有声度・解析時間が出ます。
`raw`と`distance`は継続判定前の確信度と基準値からの距離です。
起動時には保存済みの5母音のF1／F2と静音レベルも出力されます。
ログで描画や再生要求が成功していても、実際の画面と音の確認が必要です。

## 比較用ビルドと調査記録

| 環境 | 用途 |
| --- | --- |
| `m5stack-cores3-legacy` | 旧診断用アプリ。HDMIの最大3回リトライと追加I2C診断、低解像度への再試行 |
| `m5stack-cores3-hdmi-diag` | 追加のHDMI読み戻し診断 |
| `m5stack-cores3-hdmi-hw` | ハードウェアI2C経路の比較 |

```sh
pio run -e m5stack-cores3-legacy
```

診断用環境は通常のアプリと依存ライブラリや動作が異なります。
詳細な証拠と比較手順は[調査記録](../diagnostics/README.md)を参照してください。
ModuleLLMサービスの起動・停止・ロールバックは[ModuleLLMガイド](../modulellm/README.md)にまとめています。
