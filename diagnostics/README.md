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

## CoreS3＋ModuleLLMの電源と内蔵マイク（2026-10-20）

以下は交換後の、以前に正常動作を確認したCoreS3での比較。最初に調査した別個体の
音声不動作とは区別する。`USE_MODULELLM=0 USE_MODULEDISPLAY=0`でCoreS3の内蔵マイクを使用。
取り込みはM5Unified標準の`M5.Mic.begin/record/end`で、FFT・母音推定はその後段にある。

| 実機構成・設定 | 確認結果 |
| --- | --- |
| CoreS3単体、`output_power=false` | ユーザーが音・口パク正常を確認。PCMも非ゼロ |
| ModuleLLMを重ね、CoreS3のUSBのみ、`false` | ModuleLLMは起動せず、Aの音は鳴るがPCMがゼロで口パクしない |
| 同じ`false`でModuleLLMのUSB-Cにも給電 | ユーザーが音・口パク正常を確認 |
| ModuleLLMを重ね、CoreS3のUSBのみ、`true` | ユーザーがModuleLLM起動・音・口パク・再生後復帰を確認。ただし別の起動ログでは有効化キャンセルとBUS_OUT_EN=0も観測 |
| 最新安定版0.2.26で再度`false` | ユーザーがModuleLLM未起動・Aの音あり・口パクなしを確認。A再生後も復帰せず |

最新の比較では、監視開始直後の一度の起動ログだけ非ゼロPCMを取得した一方、
ユーザーの確認後、DTR/RTSを操作しないシリアル読み取りでは継続してゼロだった。
起動・給電履歴や監視時の制御線操作の影響は未確定。全起動で同一状態とは扱わない。
`getExtOutput()`はAW9523のBUS_OUT_ENビットであり、5Vの実測値ではない。
比較版ではBUS_OUT_EN・BOOST_EN・PMIC状態・VBUS/TS ADCを読み取り専用で出力する。
NVSの個人調整値は保持し、保護判定を直接レジスタ書き込みで解除していない。

### 公式情報との照合

- [CoreS3 Issue #72](https://github.com/m5stack/M5CoreS3/issues/72): メーカーはCoreS3の双方向Port A/M-Bus電源経路と、USB給電・バス給電を確実に区別できないことによる保護判定を説明している。ModuleLLMを重ね、CoreS3のUSBだけでは起動せず、両方にUSB給電すると動くという類似報告もある。ただし報告の症状は起動・画面消灯で、今回のPCMゼロそのものではない。
- [M5Unified PR #352](https://github.com/m5stack/M5Unified/pull/352): CoreS3で`false`へ切り替える順序により5Vが残り、以後の`true`がキャンセルされる不具合を修正。0.2.22で公開済みで0.2.26にも含まれる。保護判定自体は残っている。
- [M5Unified PR #381](https://github.com/m5stack/M5Unified/pull/381)と[M5GFX PR #319](https://github.com/m5stack/M5GFX/pull/319): CoreS3系の5Vバス投入時の突入電流でPMICが停止する別問題を修正。今回の「Aは鳴るが録音ゼロ」と同一とは確認していない。
- [M5Unified 0.2.26](https://github.com/m5stack/M5Unified/releases/tag/0.2.26)は調査時点の最新安定版で、masterも同じコミット。インストール済み主要ソースは公式タグと一致する。既にこの版だったため、今回の比較は実際のバージョン更新ではない。
- 未リリースのdevelopも確認。CoreS3の外部出力保護条件は0.2.26と同一。出力APIの成否返却などの変更はあるが、今回の未給電ModuleLLMによる録音ゼロを直す変更とは確認できない。

現在の証拠は、CoreS3固有の電源経路と、未給電のModuleLLMを接続した条件を優先して
調べる根拠になる。交換後のCoreS3単体のマイク故障、M5Unifiedの録音処理の回帰、
特定信号のクランプや衝突は確定していない。検索した公式Issues/PRでは、今回と同じ
「未給電ModuleLLM接続時だけCoreS3のPCMがゼロ」という報告は見つかっていない。
電気的な原因の確定には、給電あり／なしでのマイク電源・I2S・共有信号の比較が必要。
