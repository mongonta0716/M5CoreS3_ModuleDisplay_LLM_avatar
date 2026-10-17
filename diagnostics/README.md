# CoreS3 / ModuleDisplay調査（2026-10-17）

CoreS3をこのMacBookへ直接USB接続して比較。ModuleDisplayのアダプターはユーザー申告で12V。
M5GFX 0.2.33 + ソフトウェアI2CでHDMI初期化と1280×720への描画に成功し、ユーザーもHello Worldの実表示を確認した。

| 比較 | 結果 |
| --- | --- |
| 既存のM5Unified使用Hello World / M5GFX 0.2.32 | `reg:ff val:81`で失敗、HDMI未登録、描画されない |
| M5GFXのみ / 0.2.33 / HW I2C port=1 | IDは読めるが、最初のバンク81書き込みエラー以降は書き込みと読み出しが連続失敗。初期化失敗 |
| 失敗後のHW I2C 100kHz・25kHz | 全診断バンクの書き込みと読み出しが失敗 |
| 同じSDA=12 / SCL=11のSW I2C | バンク80・82・84の読み出し成功。バンク81は書き込み失敗扱いでも内容が切り替わる |
| M5GFXのみ / 0.2.33 / SW I2C port=-1 | `init(): done.`、`HelloWorld: text drawn on HDMI 1280x720`。実表示も確認 |

SW I2Cで要求バンク80のレジスタ00〜03は`17 02 e2 f0`、要求バンク81では`02 08 12 00`、
80へ戻すと再び`17 02 e2 f0`。この比較から、バンク81選択は失敗扱いでも受理されていると判断できる。
`error=2`単独からNACKの種類や発生したバイトを断定したわけではない。

[公式PR #339](https://github.com/m5stack/M5GFX/pull/339)は、ModuleDisplayのI2C直列抵抗により
ACKのLow電圧が高くなり、受理済みの書き込みがNACKと誤判定される事例を報告している。
0.2.32はその時点で設定列を打ち切るが、[0.2.33](https://github.com/m5stack/M5GFX/releases/tag/0.2.33)は
全設定を送信する。今回のCoreS3ではこれに加え、エラー後も通信を継続できるSW I2Cが必要だった。
電気的な原因の確定にはACK波形の測定が必要で、CoreS3での実際のACK電圧と
HW I2Cが連続失敗する内部状態は未測定。チップ故障・電源不足とは断定しない。

SPIは従来どおりハードウェア転送。変更したのはHDMI送信ICのI2C制御だけで、
CoreS3のLCD・PMIC・タッチは初期化せず、Avatarもリンクしていない。
Avatar版への適用はまだ行っていない。

再現用ビルド:

```sh
pio run -e m5stack-cores3-hello       # 成功したSW I2C構成
pio run -e m5stack-cores3-hdmi-hw     # 標準M5ModuleDisplay / HW I2C比較
pio run -e m5stack-cores3-hdmi-diag   # HW初期化失敗後のHW/SW読み戻し診断
```

取得した生ログはこのディレクトリの`2026-10-17-*.log`へ保存（Git対象外）。
初期比較時のSW版は環境名`m5stack-cores3-hdmi-soft`で実行し、成功後に同じ構成を
既定の`m5stack-cores3-hello`へ反映した。
