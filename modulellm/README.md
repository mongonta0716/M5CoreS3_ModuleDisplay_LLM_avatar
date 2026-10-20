# ModuleLLMのマイクで判別してUARTで口パク

ModuleLLM内蔵マイク → AX630CのCPUでFFT/LPC/5母音判別 → UARTで日本語文字列と開き量 → CoreS3の2画面Avatar、という構成です。PCMをUARTへ送りません。ASR・LLM・NPU・ネットワークは通常動作に不要です。

`main`に内蔵マイク版とModuleLLM版を統合しています。`src/AppConfig.h`の `USE_MODULELLM=0`（既定）はCoreS3内蔵マイク、`1`はModuleLLM入力です。
`m5stack-cores3-hello` はヘッダーの既定値を使い、`m5stack-cores3-modulellm-vowel` は `-DUSE_MODULELLM=1` で上書きします。切り替え後はCoreS3を再ビルド・書き込みしてください。
履歴整理後の内蔵マイク版の基準はコミット `4a2ac21`、移植直前は `8dbbd16` です。

## 確認した実機環境

2026-10-18、SSH接続した実機を調査しました。以下の `modulellm-host` は接続先の例です。
自分のSSH設定で使えるホスト名・エイリアスに置き換えてください。鍵やパスワードはリポジトリに含めません。

| 項目 | 確認結果 |
| --- | --- |
| CPU/OS | aarch64、Ubuntu 22.04.5、Linux 4.19.125 |
| ABI/コンパイラ | glibc 2.35、実機のg++ 11.4.0 |
| StackFlow | 調査時に音声/UARTサービスは動作していない |
| マイク | ALSA `hw:0,0`（Axera Audio、CAPTURE device 0） |
| PCM | S16_LE / 16000Hz / **2チャンネル必須**。monoを直接指定するとエラー |
| チャンネル | 既定はright。調査時の静音RMSはleft約1.3、right約10.1 |
| ミキサー | FROM AMIC、ADC MUTE off、ALC ENABLE off。既存値を変更せず利用 |
| UART | `/dev/ttyS1`、115200 8N1。事前調査では所有プロセスなし |
| コンソール | `/dev/ttyS0` はagettyが所有しているため使用しない |
| CoreS3側 | **RX=G18、TX=G17**。M5UnifiedのPort C定義と公式ModuleLLM例に一致 |

既存README/依頼プロンプトの参考値にはTX/RXを逆に書いた箇所がありました。CoreS3側RX=18/TX=17で、実機UARTの双方向通信を確認しています。ピンを変更した基板では実配線を確認してください。G7はModuleDisplayのCSです。

現機には `arecord` とコンパイラがありますがALSA開発ヘッダーはありません。音声取得は実機で確認した `arecord` を子プロセスとして使用し、PCMパイプをC++サービスが読みます。依存は標準C++/glibcとalsa-utilsで、推測したSDK APIは使用しません。

別のイメージでStackFlow等がマイク/UARTを使用している場合は、先に所有状況を確認してください。このサービスは他プロセスのUART所有を検出すると終了します。既存サービスを自動的に停止する処理はありません。音声デバイスが使用中の場合も起動エラーとして扱い、既存利用者を止めません。

## 実装の分担

- 共有DSP: `src/VowelAnalyzer.h`、`LpcFormants.h`、`VowelModel.h`、`VowelCalibration.h`、`MicFftAnalyzer.h` と `lib/AvatarMicFFT` をLinux/ESP32で共用。
- `VowelEngine.h`: 32msごとの判別とFFT開き量。調整への入力は100ms以上の間隔に制限し、64サンプルが1回目だけで埋まることを防止。
- `main.cpp`: ALSA子プロセス、UART所有・送受信、制御、統計。音声コールバックは使用しない。古いPCMが64ms相当を超えてパイプにたまった場合は破棄してDSPを再同期し、調整を中止して旧モデルを保持。
- UART送信は「送信中の1行＋差し替え可能な最新1行」に制限。送信が250ms以上止まったら途中行を破棄し、改行で再同期。
- `ModelStore.h`: マイク設定の指紋付きモデル。temp書き込み、fsync、rename、ディレクトリfsyncで保存。失敗は保存失敗として通知。
- CoreS3の `ModuleLlmLipSync.h`: 日本語文字列→既存口形、通信タイムアウト、調整画面。CoreS3マイクは初期化しない。
- A/起動音の間はCoreS3側で即座に口パクを抑止し、ModuleLLMへpauseを送信。HDMIのPCM/DMA終了後に200msの余韻時間を置いてresume。制御世代を変え、古い結果で再び口が開くことを防止。

口形・形の補間・両画面の描画は従来どおりCoreS3です。解析の移動で描画FPSが上がることは保証しません。

## ビルドと試験

ホスト（Mac/Linux）:

```sh
make -C modulellm
bash scripts/test_native.sh
python3 tests/test_modulellm_process.py modulellm/modulellm-vowel
pio run -e m5stack-cores3-modulellm-vowel
```

ネイティブ試験はASan/UBSanで既存DSP/調整、UTF-8の1バイトずつの分割、連結、CRC、範囲外数値、長さ超過、古い連番/制御/セッション、500msタイムアウトと時刻の周回、設定保存とマイク条件不一致を検査します。
プロセス試験は擬似端末（PTY）で実バイナリのpause/resume・再送・再接続を検査し、同じ合成PCMで5母音と無音を確認します。物理UARTやマイクには触れません。

ModuleLLMへの候補配置・実機ビルド:

```sh
bash scripts/deploy_modulellm.sh modulellm-host
```

`/opt/modulellm-vowel/releases/<日時>-<HEAD>/` にソースを転送し、実機g++でビルドします。Macの実行ファイルは転送しません。既存のcurrentやサービスには触れず、候補パスを表示します。
リリース名のHEADには未コミット差分も加わっている場合があるため、最終納品時はソースも一緒に保持してください。

候補を `RELEASE` として、SSH先で試験します（以下のRELEASEは配置結果へ置き換える）。

```sh
cd "$RELEASE"
python3 tests/test_modulellm_process.py modulellm/modulellm-vowel
bash modulellm/launch.sh --uart - --seconds 6 --model /tmp/vowel-probe-model.txt
```

標準出力は通信行、診断は標準エラーです。`--uart -` はUARTに触れない実マイクの診断用です。正常終了時にarecord側がパイプ終了を通知することがあります。
`--input <file>` は16kHz/S16_LE/stereoの生PCMを使うオフライン比較です。`--channel left|right|mix` を明示できます。WAVヘッダー付きファイルはそのまま渡さないでください。
`--test-pattern --uart /dev/ttyS1 --profile test --seconds 60` はマイクなしで「あいうえお・無音・不明」を順送りします。実マイクサービスと同時に起動しないでください。

## 有効化・起動・停止・復旧

実機で候補の試験が通ってから、SSH先で行います。

```sh
# 初回。既存設定は上書きしない。
install -d -m 0750 /var/lib/modulellm-vowel
test -e /etc/default/modulellm-vowel || install -m 0644 "$RELEASE/modulellm/default.conf" /etc/default/modulellm-vowel

# 更新時は先に現在のリンクを記録し、サービスを停止。
readlink /opt/modulellm-vowel/current || true
systemctl stop modulellm-vowel.service 2>/dev/null || true
ln -sfn "$RELEASE" /opt/modulellm-vowel/current
install -m 0644 "$RELEASE/modulellm/modulellm-vowel.service" /etc/systemd/system/modulellm-vowel.service
systemctl daemon-reload
systemctl start modulellm-vowel.service
systemctl status modulellm-vowel.service --no-pager
journalctl -u modulellm-vowel.service -n 30 --no-pager
```

自動起動も必要な場合は `systemctl enable modulellm-vowel.service`。停止は `systemctl stop modulellm-vowel.service`。実機再起動はこれらの確認だけのためには不要です。

CoreS3:

```sh
pio run -e m5stack-cores3-modulellm-vowel -t upload --upload-port /dev/cu.usbmodemXXXX
python3 scripts/monitor_serial.py --port /dev/cu.usbmodemXXXX --log /tmp/modulellm-core.log
```

監視終了はCtrl+C、または別端末から `touch /tmp/modulellm-core-monitor.stop`。CoreS3のポート名は列挙して確認してください。
`ModuleLLM UART: ... RX=18 TX=17; CoreS3 mic disabled` と `ModuleLLM: connected`、両画面の初期化ログを確認します。実表示・音は画面とスピーカーで確認してください。

以前のModuleLLMアプリへ戻すにはサービスを止め、currentリンクを記録した旧リリースに戻し、再度startします。初回導入を取り消すならstopとdisableで十分です。モデルを削除する必要はありません。
ローカル解析の口パクへ戻す場合は、`src/AppConfig.h`を `USE_MODULELLM=0` に戻し、CoreS3へ `m5stack-cores3-hello` 環境を再度uploadしてください。通常のアプリuploadはCoreS3のNVSを消しません。erase_flashや全領域の結合イメージ書き込みで代用しないでください。

## 調整

B: 静音2秒→各母音3秒×2回、全発声の間に息つぎ2秒（約50秒）。両画面の案内に従います。
A: 調整中止＋HDMI「ピポ」。C: ModuleLLMの個人設定を初期化します。

マイクが変わったため、CoreS3の旧NVS値は移植しません。ModuleLLM上の `/var/lib/modulellm-vowel/model.txt` に新しく保存します。
設定形式は `VOWELROOTS 1 <profile>` と静音RMS、5組のF1/F2です。profileは起動時に入力デバイス・形式・チャンネル・ミキサー設定からSHA-256で作ります。入力設定やゲイン等を変更したらサービスを再起動し、Bで再調整してください。変更前の条件の保存値は読み込みません。CoreS3のNVSは保持されます。
調整失敗・保存失敗時は従来の有効モデルを保持し、成功と表示しません。

## UART形式 v1

独立UARTを利用するため、今回はJSON/StackFlowの外枠を使いません。UTF-8テキストをタブ区切り、LF終端、末尾にCRC32（8桁小文字16進）で送ります。以下の `\t` は実際のタブです。

```text
C1\tclient\tepoch\tcommand\tcrc32\n
V1\tserver\tclient\tepoch\tseq\ttext\topen\tconfidence\tstatus\tstep\tround\trest\tseconds\tcrc32\n
```

CRCは最後のタブより前の全UTF-8バイトが対象です。最大行長は383バイト（LF除く）。textは「あ」「い」「う」「え」「お」「無音」「不明」のどれか、open/confidenceは有限の0～1です。「無音」は両方0。「不明」は音量による開閉が可能です。

commandは `resume/pause/calibrate/cancel/reset/status`。clientはCoreS3側のセッション乱数、serverはModuleLLMプロセスの起動識別子、epochは制御ごとに増える世代です。結果は最新client/epochだけを使い、seqの重複・巻き戻りを破棄します。同じepochの再送は冪等です。CoreS3は応答が来るまで200ms間隔で制御を再送します。
受信が500ms途絶えると閉口指令、1秒途絶えると新しいclientで再同期します。サーバー再起動後の古い結果も再利用しません。

statusは `0 Idle / 1 Running / 2 Complete / 3 Insufficient / 4 Invalid / 5 Cancelled / 6 Reset / 7 SaveFailed`。
stepは静音=0、A/I/U/E/O=1～5、完了後=6。roundは1/2、restは0/1、secondsはその段階の残り秒数です。状態を結果ごとに同送し、同じ母音が続く間も受信の継続を確認できます。

## 性能と確認済みの範囲

最初の実マイク6秒試験では16ms入力373回、平均処理105us/入力、最大308us、PCMの破棄・結果の置換は0、RSS約3.2MiBでした。解析結果は32msごとです。値はその試験条件の実測で、最終稼働時はjournalの統計を確認してください。
同じ合成PCMをMacとAX630Cで処理した164結果は、母音ラベルの差0、開き量の差0、確信度の最大差0.0001でした。これは実声の認識率ではありません。
CoreS3との物理UARTで固定の日本語母音、無音の開き量0、pause/resumeの往復、両画面の起動と約9.7fpsをログ確認済みです。
ユーザーも固定母音表示/口形/閉口/A音と復帰、および実マイク調整の保存・両画面の母音/口形/A音と復帰を正常と確認しました。
その後、サービス再起動で `model=loaded` とCoreS3との再接続を確認し、モデルファイルのハッシュも一致しました。
216秒間の実マイク稼働は13,501入力、DSP平均99us/16ms入力・最大1,932us、送信479,892バイト、音声欠落/送信結果の差し替え0でした。サービス全体のCPU時間は2.766秒（この区間で1コア換算約1.3%）です。
ユーザー確認は定量的な認識率の測定とは区別してください。確認範囲は本節と `AGENTS.md` を参照してください。

異なる機器の時計は直接減算していません。音声/送信キューの滞留、解析時間、CoreS3到着後の鮮度、描画時間を別々に記録します。口の視覚的な遅れは実機でも評価してください。

参考となる一次資料: [M5Stack公式UARTサンプル](https://github.com/m5stack/M5Stack_Linux_Libs/blob/master/doc/example_doc/linux_uart.md)、[CoreS3公式例](https://github.com/m5stack/M5Module-LLM/blob/main/examples/VoiceAssistant/VoiceAssistant.ino)、[StackFlow](https://github.com/m5stack/StackFlow)、[ModuleLLM](https://docs.m5stack.com/en/module/Module_LLM)。
