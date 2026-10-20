# CoreS3 + ModuleLLM + Display Module 13.2 Avatar

M5Stack CoreS3のAvatarを、内蔵マイクまたはModuleLLMの音声解析で口パクさせるPlatformIO / Arduinoプロジェクトです。
ModuleDisplayを使うと、本体LCDに加えてHDMIモニターにも表示できます。
アバターには指定の[M5Unified-Avatar](https://github.com/mongonta0716/M5Unified-Avatar)を使用しています。

## 構成の切り替え（USE_MODULELLM / USE_MODULEDISPLAY）

`src/AppConfig.h`の2つの値で、コンパイル時に使用するモジュールを選択します。
既定値はどちらも`0`で、CoreS3単体で動作します。

| USE_MODULELLM | USE_MODULEDISPLAY | マイク・解析先 | 表示・音声出力 | PlatformIO環境 |
| --- | --- | --- | --- | --- |
| `0` | `0` | CoreS3内蔵マイク・CoreS3 | 本体LCD・本体スピーカー | `m5stack-cores3` |
| `0` | `1` | CoreS3内蔵マイク・CoreS3 | 本体LCD＋HDMI・HDMI音声 | `m5stack-cores3-display` |
| `1` | `0` | ModuleLLM内蔵マイク・AX630C | 本体LCD・本体スピーカー | `m5stack-cores3-modulellm-vowel` |
| `1` | `1` | ModuleLLM内蔵マイク・AX630C | 本体LCD＋HDMI・HDMI音声 | `m5stack-cores3-modulellm-display` |

通常の`pio run`と`pio run -e m5stack-cores3`は、ヘッダーの値を使います。
`m5stack-cores3-hello`も同じアプリの互換用エイリアスです。
上表の追加モジュール用環境は、対応する値を`-D...=1`で上書きします。
ビルドフラグがヘッダーより優先されるため、モジュールを無効に戻す場合は
ヘッダーを`0`に戻して`m5stack-cores3`で再ビルド・書き込みしてください。
古い診断用`m5stack-cores3-legacy`はこの切り替えの対象外です。

`USE_MODULEDISPLAY=0`ではHDMIへのアクセスを行わず、Hello WorldとAvatarを本体LCDに表示し、
起動時とAの「ピポ」は本体スピーカーから再生します。
`1`では従来どおりHello WorldをHDMIに表示した後、両画面でAvatarを動かし、HDMIへ音声を出します。
どちらもHello Worldは2秒、下40pxのA/B/Cタッチボタンを使い、再生中は口パクを抑止します。

`USE_MODULELLM=0`では個人設定をCoreS3のNVSへ保存します。
`1`ではCoreS3のマイクを起動せず、ModuleLLMからUARTで受けた母音・開き量を使い、
調整と保存もModuleLLM側で行います。2つの個人設定は独立し、互いの値を削除・流用しません。
ModuleLLM未接続時も画面とAの操作は継続します。

ModuleLLMの給電はこれらのフラグとは別です。CoreS3からのM-Bus給電は、M5Unified標準と同じ
`cfg.output_power=true`で有効化を要求します。起動ログの`M-Bus power: requested=on actual=...`で実際の状態を確認できます。
CoreS3＋ModuleLLMを重ねた`USE_MODULELLM=0` / `USE_MODULEDISPLAY=0`の実機では、CoreS3のUSB1本で
ModuleLLMが起動し、起動音・Aの音・口パク・A再生後の復帰が正常なことを確認しました。
この動作確認後も`setExtPower(true) is canceled`と`actual=off`のログが残っており、実際の給電経路と状態判定の関係は未確認です。
`USE_MODULELLM=0`でも、接続したModuleLLMは電気的に切り離されません。
以前の`output_power=false`の実機では、未給電のModuleLLMを重ねるとCoreS3のマイク入力がすべて0になりました。
同じファームウェアで、ModuleLLMを取り外すか、そのUSB-Cにも給電すると音と口パクが動くことを確認済みです。
M-Busからの給電が有効にならない場合は、ModuleLLM側にもUSB給電するか、使用しないモジュールを取り外してください。
未給電時に影響を受ける内部信号や電気的な仕組みまでは特定していません。

```sh
# AppConfig.hの値でビルド・書き込み（既定はCoreS3単体）
pio run -e m5stack-cores3 -t upload

# ModuleLLM＋ModuleDisplayを両方使う
pio run -e m5stack-cores3-modulellm-display
bash scripts/deploy_modulellm.sh modulellm-host  # 自分のSSH接続先に置き換える
```

[ModuleLLMの配置・起動・停止、UART仕様、調整と復旧手順](modulellm/README.md)も参照してください。

## Hello World → Avatar（ModuleDisplay有効時）

以下は`USE_MODULELLM=0`、`USE_MODULEDISPLAY=1`での動作です。
`USE_MODULEDISPLAY=0`では本体LCD・本体スピーカーだけを使います。
CoreS3 + ModuleDisplayで、HDMIモニターに紺色背景・白い枠・中央の`Hello World`を
**2秒間表示してから、アニメーションするAvatarへ切り替えます**。
HDMI側は320×180で描画し、ModuleDisplayで1280×720へ4倍拡大します。
Avatarは4:3の顔を240×180で描画し、モニター上では960×720の大きさで中央表示します。
まばたき・視線・呼吸を更新し、等倍描画に比べてHDMIへ転送する画素数を約1/16、
従来の640×360描画に比べて約1/4に減らします。
CoreS3本体にも同じ表情・動きのAvatarを表示します。下40ドットのボタン領域を避け、
上部320×200に約267×200の顔を中央配置します。両画面は共通の状態で順に描画します。
CoreS3の内蔵マイクを16kHzステレオで録音し、
[m5stack-avatar-mic](https://github.com/mongonta0716/m5stack-avatar-mic)と同じ256点FFTで
312.5〜3750Hz（bin 5〜60）の音量を集計して、両画面の口を動かします。
音量に応じた感度調整と口の開閉の平滑化を行い、無音が続くと口を閉じます。
さらに32msごとにLPCで声の共鳴周波数F1・F2を推定し、日本語の5母音に応じて
口の幅・高さ・丸みを変えます。判定が弱い区間は音量による開閉に戻ります。
両画面で口の横の吹き出しに、推定した母音を「あ・い・う・え・お」で表示します。
無音や判定が不確かな区間、口パクデモ中は「…」を表示します。
初期設定でも動作しますが、Bの声の調整で本人の声と周囲の雑音に合わせられます。
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
[調査結果と比較手順](diagnostics/README.md)を参照してください。
従来の2画面Avatar版（`m5stack-cores3-legacy`）の依存ライブラリは現時点では0.2.32のままです。

ModuleDisplayのDC入力仕様は**9〜24V**です。給電ケーブルの接続だけでなく、アダプターの
出力電圧とモジュールの電源スイッチも確認してください。

```sh
pio run -e m5stack-cores3-display
```

Webツールで`↻`を押し、名前に`m5stack-cores3-display`を含む今回の環境の新しいbinを選んで、
Board=`CoreS3`で書き込みます。Monitorを開始してDevice Resetすると、
`APP: Hello World -> Avatar (CoreS3 + HDMI + mic FFT)`と、成功時には`HelloWorld: text drawn on HDMI ...`が記録されます。
2秒後に`HelloWorld: held for 2000 ms; starting Avatar`、`Avatar ready: ...`、
`Avatar: first frame rendered; animation running.`が続きます。
両画面が利用可能なら`Avatar targets: CoreS3=ready HDMI=ready`も記録されます。
`HDMI: output 1280x720@60, logical 320x180, hardware scale 4`で拡大設定を確認できます。
約5秒ごとの`Avatar perf: ... fps, mean draw ... ms (2 displays)`で、
2画面を一組として実際に更新できた速度と平均描画時間を確認できます。
変更前の640×360・2倍拡大では2026-10-17の実機で約5.8fps・平均描画時間約172msとなり、表示・動き・Aの発音・
再生後の口パク復帰をユーザーが確認済みです。
現在の320×180・4倍拡大では約10.3fps・平均描画時間約96.6msとなり、更新速度は約1.8倍になりました。
こちらも表示・動き・Aの発音・再生後の口パク復帰をユーザーが実機確認済みです。
シリアルの`n/h/a/s/d/z`で表情変更、`t`で口パクのデモを切り替えられます。
通常はマイクのFFTで口パクします。`t`でデモをONにすると周期的な開閉を優先し、OFFでマイクへ戻ります。
`Mic FFT: ready, 16000 Hz stereo, 256 points, ...`で録音開始、
毎秒の`Mic FFT: listening frames=... level=... peak=... mouth=...`で処理の継続と音量を確認できます。
`frames`は解析済みフレーム数、`level`は帯域の振幅合計を2048で割った値、
`peak`はPCMの最大絶対値、`mouth`は0〜1の口の開きです。
録音バッファの完了通知でFFTを実行するため、HDMIの描画中も解析を継続します。
修正版は`HDMI test: M5GFX 0.2.33 / transmitter ACK fix #339`でも識別できます。
さらに`HDMI test: software I2C port=-1 SDA=12 SCL=11`が表示されます。
音声の初期化は`HDMI audio: ready, 48000 Hz ...`、再生要求は
`HDMI chime: startup queued` / `HDMI chime: BtnA queued`で確認できます。
ログは音の再生要求までを示し、実際に鳴ったかはモニター側で確認してください。
HDMIキャプチャーをQuickTime経由で聞く場合は、QuickTime/macOSの音声出力先も確認してください。
2026-10-17の無音報告は出力先の設定が原因で、設定変更後に発音を確認しました。
Hello World版はファイルシステムを使用しないため、結合binにはSPIFFSを含めません。
初期化に失敗した場合は`HelloWorld: HDMI initialization failed; text was not drawn.`と表示します。
描画完了ログだけではモニター上の表示までは確認できないため、実際の映像も確認してください。

旧診断用アプリは`pio run -e m5stack-cores3-legacy`でビルドできます。

## 現在の機能

- CoreS3: 320×240。ModuleDisplay有効時はHDMI出力1280×720 / 60Hzにも表示します。
- 外部画面には4:3の顔を960×720で中央表示。縦横比を維持し、左右に余白を設けます。
- 表情・まばたき・視線・呼吸を描画し、ModuleDisplay有効時は両画面で同じ状態を表示します。
- 内蔵マイクのFFTでAvatarを口パクさせます（既定ビルド）。
- LPCによる母音の簡易推定で、口の幅・高さ・丸みを変えます。声の調整結果はNVSに保存します。
- 画面下40ドットのAで「ピポ」を再生します。出力先は本体スピーカーまたはHDMIです。
- 従来の`m5stack-cores3-legacy`ビルドでは、本体画面のタッチで6種類の表情を順に切り替えます。
- USBシリアルから表情の変更と口パクのデモを操作できます。
- ModuleDisplayが検出されない場合も、本体画面は動作します。

既定環境ではCoreS3内蔵マイクを使います。ModuleLLMマイクとUART通信を使う場合は、上記の別ビルドを選んでください。

## 接続

1. 使用する構成に合わせてフラグを設定します。モジュールを使用する場合は、電源を切ってM5-Busでスタックします。
2. ModuleDisplayを使用する場合は外部電源と720p対応のHDMIモニターを接続し、モニターの入力を選びます。
3. CoreS3をUSB-CでPCへ接続し、スタックへ給電します。

CoreS3用M5GFXのModuleDisplay設定はSPIのCS=G7、MOSI=G37、MISO=G35、SCLK=G36、I2CのSDA=G12、SCL=G11を使用します。
ModuleLLMの工場出荷時UART配線（CoreS3側RX=G18、TX=G17）とは重複しません。
ModuleLLMのピンを変更済みの場合、ModuleDisplayが使用するG7を選択しないでください。

配線の根拠: [M5GFX / M5ModuleDisplay.h](https://github.com/m5stack/M5GFX/blob/0.2.32/src/M5ModuleDisplay.h)、[ModuleLLMのピン切替](https://docs.m5stack.com/en/compute/module_llm/pins_change)。

## ビルド・書き込み

PlatformIO Coreが必要です。初回ビルドで依存ライブラリをダウンロードします。

```sh
pio run -e m5stack-cores3
pio run -e m5stack-cores3 -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

起動ログで、内部画面とHDMI画面の検出結果、論理解像度、アバター用バッファの確保結果、空きヒープ / PSRAMを確認できます。
`Config: USE_MODULELLM=... USE_MODULEDISPLAY=...`で実際に組み込まれた構成を確認します。
CoreS3単体では`Avatar targets: CoreS3=ready HDMI=disabled`、ModuleDisplay有効時は
`HDMI: output 1280x720@60, logical 320x180, hardware scale 4`と`HDMI=ready`を確認してください。
CoreS3のPSRAMはQuad接続のため、`board_build.arduino.memory_type = qio_qspi`を使用します。
`qio_opi`ではPSRAMの初期化に失敗します。設定変更後は上記の`pio run ... -t upload`でブートローダーとアプリを再書き込みしてください。
HDMI信号の60Hzとアバターの更新速度は別です。描画は最大約15fpsに制限し、実際の更新速度はSPI転送・描画時間に依存します。

既存の`scripts/copy_firmware.py`も使用します。ビルド後に結合ファームウェアが`.pio_build_firmware/`へ保存されます。
現在のアプリも旧診断用アプリも実行時のファイルシステムを使用しないため、SPIFFSの生成・同梱を行いません。
`data/`は現在空で、追加の画像・音声ファイルは不要です。

### Apple Siliconでmkspiffsが実行できない場合

`mkspiffs_espressif32_arduino: Bad CPU type in executable`は、Intel向けの
SPIFFS作成ツールをホストMacが実行できない場合のエラーです。
旧設定の`m5stack-cores3`は空のSPIFFSを生成していましたが、現在は
`custom_bundle_filesystem = no`を指定し、通常のビルド・uploadでは呼び出しません。
VS Codeでは`m5stack-cores3`を選択して書き込めます。ModuleLLMを使う場合は
`AppConfig.h`の2つの構成フラグ、または上表の環境を選択してください。

### HDMIが表示されない場合

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
診断後は共有I2Cピンの設定を復元します。実機のリセットはWebツール側で行ってください。

## メモリと拡大表示

顔のキャンバスは1bit、転送用バッファは16bit×高さ8ピクセルです。1280×720の16bitフルフレームをCoreS3側には確保しません。
等倍時、外部画面の正方形キャンバスは960×960で約115,200バイト、転送バッファは15,360バイトです。
既定の4倍拡大では240×240で7,200バイト、転送バッファは3,840バイトになります。
本体画面のキャンバスは267×267で約9,078バイト、転送バッファは4,272バイトです。
キャンバスはPSRAMを利用可能な設定にし、転送バッファは内部RAMに置きます。

拡大率を変更する場合は、`platformio.ini`の対象環境の`-DAVATAR_HDMI_SCALE`を変更して再ビルドします。
既定の`m5stack-cores3`（`m5stack-cores3-hello`も同じ）は`4`、従来の`m5stack-cores3-legacy`は`1`です。

| 設定 | 論理解像度 | 論理的な顔のサイズ | HDMI出力 |
| --- | --- | --- | --- |
| `1` | 1280×720 | 960×720 | 1280×720 |
| `2` | 640×360 | 480×360 | 1280×720、2倍拡大 |
| `4`（既定ビルド） | 320×180 | 240×180 | 1280×720、4倍拡大 |

低解像度モードではModuleDisplayがハードウェアで拡大します。720pの16:9比率に合わせて論理解像度も16:9にし、その中央に4:3の顔を表示します。
従来の`m5stack-cores3-legacy`ビルドでは、起動時に外部画面用バッファの確保に失敗した場合、2倍、4倍の低解像度モードへ順に切り替えて再試行します。HDMI出力は1280×720のままです。
すべて失敗した場合や、動作中の再確保に失敗した場合は該当画面の描画を停止してログにエラーを表示し、他の画面の描画を続けます。

## 操作

既定ビルドでは、話しかけると口が動き、下部のAを押すと「ピポ」が鳴ります。
従来の`m5stack-cores3-legacy`ビルドでは、本体画面のタッチでNeutral → Happy → Angry → Sad → Doubt → Sleepyの順に変化します。

### 母音に合わせる声の調整（既定ビルド）

1. 本体画面下部の **B: Cal** を押します。
2. 吹き出しに「静かに」と残り秒数が出るので、2秒間静かにします。
3. 両画面の吹き出しの指示と残り秒数に従って「あー」「いー」「うー」「えー」「おー」を、それぞれ **3秒ずつ2回** 発声します。各発声の間と母音の切り替え前に2秒の息つぎが入ります。
4. 吹き出しに「声の調整」「保存完了」と表示されたら完了です。再起動後も同じ設定を使います。

Aで調整を中止し「ピポ」を再生、C: Resetで保存した声の設定を消して初期値へ戻します。
十分な有声音が取れない場合、測定のばらつきが大きい場合、5母音の特徴が重なった場合は保存せず、以前の設定を使います。
「音声不足」「母音不安定」が出たら、静かな場所でマイクに近づいてBからやり直してください。
調整中の吹き出しは発声指示とカウントダウン、終了後の6秒間は完了・不足・中止などの結果を示します。
案内は両画面の顔の右上に24ピクセルの文字で2行表示します。「あー 1回目」「あと3秒」、休憩中は「息つぎ」「あと2秒」と表示します。調整全体は約50秒です。
その後は推定母音の表示へ戻ります。口は調整中もマイクの推定結果で動き、調整完了後に新しい特徴値へ切り替えます。

| 母音 | Avatarの口の表現 |
| --- | --- |
| あ | 幅のある大きな縦開き |
| い | 横に広く薄く開く |
| う | 小さく丸くすぼめる |
| え | 横に広い角丸の形 |
| お | 縦長の丸い形 |

弱めの声や中程度の確信度でも母音の幅・丸みの特徴を残し、音量は主に高さへ反映します。
無音へ移る際には閉じた形へ戻し、形は滑らかにつなぎます。
吹き出しの文字は口の形と同じ判定結果を使うので、形が似ている場合と母音判定の違いを確認できます。
これは音声認識の確定結果ではなく、
口パク用の簡易推定です。ささやき声・無声化した母音・雑音・速い発話・高い声では
判定できないことがあり、その場合は音量による開閉に戻ります。
初期の母音中心値は調整用の初期値で、全話者に共通する境界値ではありません。

母音解析は16kHzの音声を低域フィルターに通して8kHzへ間引き、32ms窓、
プリエンファシス、Hamming窓、10次自己相関LPC、LPC多項式の根からの共鳴周波数・帯域幅の抽出を行います。
スペクトルの山が重なる場合でもF1/F2を分けるため、極の周波数を使います。
単一音やスペクトルの傾きを拾う極を除外し、調整では静音の中央値と外れ値を除いた代表的なF1/F2の組を使います。
保存前に、各母音の測定値の65%以上が同じ母音として判別できるかも検査します。
調整用の特徴抽出は以前の静音しきい値で止めず、今回測った静音レベルで声を選別します。
同じ母音の2回分をまとめて評価し、息つぎ中の音は収集しません。
3秒×2回の手順で、実機ログから5母音の調整完了と新しい設定の保存を確認しています。
従来のピーク抽出版で保存した設定は消さずに読み込めますが、新しい解析に合わせてBで再調整してください。
有声判定と2フレーム以上の継続判定を使い、通常のFFT音量処理はそのまま残します。
LPCの背景と根からの共鳴周波数の算出は[PraatのLPC資料](https://praat.org/manual/Sound__To_LPC__autocorrelation____.html)と
[フォルマント変換の資料](https://praat.org/manual/LPC__To_Formant.html)を参照しています。
`Vowel: a confidence=... F1=... F2=... rms=... voice=... analysis=... us`で推定結果と
処理時間、`Vowel calibration: saved to NVS`で保存を確認できます。
起動時は保存した5母音のF1/F2と静音レベルも出力します。判定ログの`raw`と`distance`は
継続判定前の確信度と基準値からの距離です。
母音解析追加後の実測は約10.1fps・平均描画時間約98msです。
2026-10-17に、本人の声での調整・保存、母音に応じた口の形の変化、
Aの発音と再生後の口パク復帰をユーザーが実機確認済みです。
その後の起動ログで保存済み設定のNVS読み込みを確認しています。
形の差を強めて吹き出しを追加した版も約10.1fpsで動作しています。
両画面の母音表示が正常で「あ・う・え」の口の形も見分けやすくなったことを、ユーザーが実機確認済みです。

| シリアル入力 | 操作 |
| --- | --- |
| `n` | Neutral |
| `h` | Happy |
| `a` | Angry |
| `s` | Sad |
| `d` | Doubt |
| `z` | Sleepy |
| `t` | 口パクのデモをON / OFF（音声出力なし） |
| `c` | 母音の声の調整を開始（既定ビルド） |
| `x` | 保存した声の設定を消して初期値へ戻す（既定ビルド） |

## 構成と実装上の注意

- `src/main.cpp`: CoreS3 / ModuleDisplayの初期化、タッチ・USBシリアル入力。
- `src/hello_world.cpp`: 既定ビルドの起動・タッチ・表示構成と音声入出力の切り替え。
- `src/MicLipSync.h`: 内蔵マイクの録音、完了通知からのFFT処理。
- `src/MicFftAnalyzer.h`: FFTの帯域集計、感度調整、口の開閉の平滑化。
- `src/VowelAnalyzer.h` / `src/LpcFormants.h`: 有声判定、LPCの極からのF1/F2推定、母音判定の継続処理。
- `src/VowelModel.h` / `src/VowelMouth.h`: 母音の特徴値、幅・高さ・丸みを変える口パーツ、日本語の母音を表示する吹き出し。
- `src/VowelCalibration*.h`: 段階的な声の調整とNVS保存。
- `lib/AvatarMicFFT`: avatar-micから取り込んだFFTとMITライセンス。
- `src/AppConfig.h`: マイク・ModuleDisplayの有無、HDMIの論理解像度・ハードウェア拡大率。
- `src/DualDisplayAvatar.*`: 顔の配置、2画面への描画、共通のアニメーション状態。

M5Unified-Avatarはコミット`69e8045bf98f49509611563b9d16ea433c7b8ea7`に固定しています。
この版の標準コンストラクターには720×1280の固定値があるため、`Face`の詳細コンストラクターで各画面に合ったパーツ配置とキャンバスを渡しています。
また、`Avatar::start()`の描画タスクハンドルはライブラリ全体で共有されるため、2つの描画タスクを起動せず、共通の`DrawContext`相当の状態を使って`Face::draw()`を順に呼びます。
これにより、同じSPIバスを使用する本体LCDとModuleDisplayの描画を直列に実行します。ライブラリ本体の変更は不要です。

FFTの単体テストはMac上で次のように実行できます。

```sh
clang++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc -Ilib/AvatarMicFFT/src tests/test_mic_fft.cpp -o /tmp/test_mic_fft
/tmp/test_mic_fft
```

無音・音声帯域・帯域外・感度の復帰・最大振幅・時刻の周回を検証します。

母音推定と調整処理のテストは次のように実行します。

```sh
clang++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc tests/test_vowel.cpp -o /tmp/test_vowel
/tmp/test_vowel
```

5母音×3種類の基本周波数を持つ合成音、無音・雑音・単一音の除外、口の形、
重なった共鳴の分離、調整の完了・音声不足・中止・時刻の周回、静音の一時的な雑音とF3の取り違えの除外を検証します。
合成音の結果は実話者の認識精度を保証しません。
