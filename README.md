# CoreS3 + ModuleLLM + Display Module 13.2 Avatar

M5Stack CoreS3にModuleLLMとDisplay Module 13.2をスタックして使う実験用のPlatformIO / Arduinoプロジェクトです。
最初の段階として、CoreS3本体とHDMIモニターの両方にアバターを表示します。
アバターには指定の[M5Unified-Avatar](https://github.com/mongonta0716/M5Unified-Avatar)を使用しています。

## 現在の切り分け: ModuleDisplay Hello World

現在の既定ビルドは`m5stack-cores3-hello`です。
CoreS3 + ModuleDisplayで、外部画面だけに紺色背景・白い枠・中央の`Hello World`を表示します。
M5GFXの`M5ModuleDisplay`だけを使用し、`display.init()`と既定解像度で確認します。
CoreS3本体LCD・タッチ・電源管理の初期化は行いません。ModuleDisplay側の外部給電が必要です。
M5Unified・Avatar・独自リトライ・追加I2C診断はビルド対象から外します。

```sh
pio run -e m5stack-cores3-hello
```

Webツールで`↻`を押し、名前に`m5stack-cores3-hello`を含む新しいbinを選んで、
Board=`CoreS3`で書き込みます。Monitorを開始してDevice Resetすると、
`TEST: ModuleDisplay Hello World (M5GFX only)`と、成功時には`HelloWorld: text drawn on HDMI ...`が記録されます。
初期化に失敗した場合は`HelloWorld: HDMI initialization failed; text was not drawn.`と表示します。
描画完了ログだけではモニター上の表示までは確認できないため、実際の映像も確認してください。

Avatar版は`pio run -e m5stack-cores3`で引き続きビルドできます。

## 現在の機能

- CoreS3: 320×240、ModuleDisplay: HDMI出力1280×720 / 60Hz。
- 外部画面には4:3の顔を960×720で中央表示。縦横比を維持し、左右に余白を設けます。
- 両画面で同じ表情・まばたき・視線・呼吸を描画します。
- CoreS3のタッチで6種類の表情を順に切り替えられます。
- USBシリアルから表情の変更と口パクのデモを操作できます。
- ModuleDisplayが検出されない場合も、本体画面は動作します。

ModuleLLMの推論・UART通信・音声処理は次の実装段階です。現在はスタックした状態で表示を確認するためのファームウェアで、ModuleLLMの接続確認は行いません。

## 接続

1. 電源を切ってCoreS3、ModuleLLM、Display Module 13.2をM5-Busでスタックします。
2. ModuleDisplayと720p対応のHDMIモニターを接続し、モニターの入力を選びます。
3. CoreS3をUSB-CでPCへ接続し、スタックへ給電します。

CoreS3用M5GFXのModuleDisplay設定はSPIのCS=G7、MOSI=G37、MISO=G35、SCLK=G36、I2CのSDA=G12、SCL=G11を使用します。
ModuleLLMの工場出荷時UART配線（CoreS3側TX=G18、RX=G17）とは重複しません。
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
`HDMI: output 1280x720@60, logical 1280x720, scale 1`と、2画面分の`Avatar ready`が表示されることを確認してください。
CoreS3のPSRAMはQuad接続のため、`board_build.arduino.memory_type = qio_qspi`を使用します。
`qio_opi`ではPSRAMの初期化に失敗します。設定変更後は上記の`pio run ... -t upload`でブートローダーとアプリを再書き込みしてください。
HDMI信号の60Hzとアバターの更新速度は別です。描画は最大約15fpsに制限し、実際の更新速度はSPI転送・描画時間に依存します。

既存の`scripts/copy_firmware.py`も使用します。ビルド後にSPIFFSを含む結合ファームウェアが`.pio_build_firmware/`へ保存されます。
`data/`は現在空で、追加の画像・音声ファイルは不要です。

### HDMIが表示されない場合

HDMIはM5Unifiedの電源初期化後に500ms待ち、最大3回初期化します。
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
本体画面のキャンバスは12,800バイト、転送バッファは5,120バイトです。キャンバスはPSRAMを利用可能な設定にし、転送バッファは内部RAMに置きます。

メモリ使用量や転送時間を減らしたい場合は、`platformio.ini`の`-DAVATAR_HDMI_SCALE=1`を変更して再ビルドします。

| 設定 | 論理解像度 | 論理的な顔のサイズ | HDMI出力 |
| --- | --- | --- | --- |
| `1`（初期値） | 1280×720 | 960×720 | 1280×720 |
| `2` | 640×360 | 480×360 | 1280×720、2倍拡大 |
| `4` | 320×180 | 240×180 | 1280×720、4倍拡大 |

低解像度モードではModuleDisplayがハードウェアで拡大します。720pの16:9比率に合わせて論理解像度も16:9にし、その中央に4:3の顔を表示します。
起動時に外部画面用バッファの確保に失敗した場合は、2倍、4倍の低解像度モードへ順に切り替えて再試行します。HDMI出力は1280×720のままです。
すべて失敗した場合や、動作中の再確保に失敗した場合は該当画面の描画を停止してログにエラーを表示し、他の画面の描画を続けます。

## 操作

本体画面をタッチすると、Neutral → Happy → Angry → Sad → Doubt → Sleepyの順に変化します。

| シリアル入力 | 操作 |
| --- | --- |
| `n` | Neutral |
| `h` | Happy |
| `a` | Angry |
| `s` | Sad |
| `d` | Doubt |
| `z` | Sleepy |
| `t` | 口パクのデモをON / OFF（音声出力なし） |

## 構成と実装上の注意

- `src/main.cpp`: CoreS3 / ModuleDisplayの初期化、タッチ・USBシリアル入力。
- `src/AppConfig.h`: HDMIの論理解像度・ハードウェア拡大率。
- `src/DualDisplayAvatar.*`: 顔の配置、2画面への描画、共通のアニメーション状態。

M5Unified-Avatarはコミット`69e8045bf98f49509611563b9d16ea433c7b8ea7`に固定しています。
この版の標準コンストラクターには720×1280の固定値があるため、`Face`の詳細コンストラクターで各画面に合ったパーツ配置とキャンバスを渡しています。
また、`Avatar::start()`の描画タスクハンドルはライブラリ全体で共有されるため、2つの描画タスクを起動せず、共通の`DrawContext`相当の状態を使って`Face::draw()`を順に呼びます。
これにより、同じSPIバスを使用する本体LCDとModuleDisplayの描画を直列に実行します。ライブラリ本体の変更は不要です。
