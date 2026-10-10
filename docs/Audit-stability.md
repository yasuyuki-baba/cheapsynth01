# 安定性監査・実装結果（2026-10-09 JST）

各節は記載した日時・ソースでの測定記録であり、全節が現在の実装を説明するものではない。
現在の構成は [DSP責任範囲](DSP-responsibility-boundaries.md)、
[EGモデル](EG-stateful-model.md)、[発音境界](Note-onset-continuity.md) に記載する。

作業開始時のGitHub最新mainは `4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`。
監査対象と同一だった。ローカルの古いorigin/mainや既存ビルドを基準にはしていない。
元checkoutは変更せず、独立worktreeの `fix/audit-stability` で実装した。
リポジトリ内にAGENTS.mdはなく、CONTRIBUTING.md、Tests/README.md、JUCEの規約・ソースを確認した。
push、PR、リリース公開はしていない。

以下の初回測定・テストのソースは `ec12e588f57b78fc199a001418a632463f2aa626`。
主要実装はa280bb7、キャッシュ復元の補完は9db9a2c、試験用型の補正はec12e58。
初回結果の報告・梱包commitでは本番DSPを変更していない。以後の追加修正は末尾に別記する。
機械可読の条件・件数・ログハッシュは [stability-results.json](../artifacts/audit/stability-results.json)。
XMLの時刻はUTC、ここでの日付はJSTである。

## 再検証と修正

| 指摘 | 現在mainでの判定・原因 | 修正・直接検証 |
|---|---|---|
| 1 UI通知 | 非メッセージスレッドのChoice通知で直接UIを変更する。基準の再現試験も失敗 | 通知はatomicのdirty印のみ。60 Hz（既存Modulationは120 Hz）のメッセージスレッドTimerで最新値を読む。GUIクリックは同スレッドで即反映。worker通知、最新値、破棄後の通知を検証 |
| 2 Save/Overwrite | raw this/manager捕捉を実コードで確認。旧監査の異常終了原因は未特定で、UAFの実証とは扱わない | パネル所有AlertWindowとSafePointerへ統一。入力中、入力完了待ち、確認中、確認完了待ちの4段階でエディタを閉じる試験 |
| 3 Release寿命 | 48 kHz、50 msでNote Off、20 ms後に1秒へ変更すると音源が先に止まる。基準再現失敗 | EGの残り内部サンプル数を音源へ渡す。Tone/Noise、深さ0/1、分割1/7/64で延長・短縮・連続変更・音源切替、EGと音源のactive一致を検証 |
| 4 MIDI/Noise終端 | N位置と0サンプルのイベント消失、Noiseのブロック越えを再現 | 通常範囲0..N-1。範囲外は0またはNへclampし、Nでは描画後に適用。空ブロックはホストイベントのみ適用してDSPを進めない。-1/N-1/N/N+1、同位置のOn/Off/CC120、SysEx混在の順序を検証。Noiseは残り数だけ生成 |
| 5 プリセット | 保存結果を返さず、読み込み前に選択確定する経路を確認 | portableな単一ファイル名検査、boolの成功結果、一時ファイル置換、成功後の選択確定。壊れたXML、削除済みファイル、保存先が非空ディレクトリの場合、旧データ・選択を保護 |
| 6 RT処理 | segment/panel MidiBuffer、長いメッセージコピー、collector/keyboardロック、通知投稿を確認。追加計測でJUCEグラフscratchとIIRの初回確保も発見 | 非所有のホストイベント走査、再利用ストレージ、2048短イベントの固定MPSCキュー、鍵盤の逆向き表示キュー。満杯/競合ではpanicで発音残りを防ぐ。再準備は世代破棄で行い、送受信・再準備の並行試験を追加 |
| 7 program/経路 | JUCE VST3 ProgramChangeParameter・LV2 state経路はsetCurrentProgramを呼び、メッセージスレッド限定とは判断できない。グラフ変更は元々メッセージ側への遅延で、音声側の直変更ではない | Factory/Userを非RTで解析した不変カタログから次callbackで値適用。通知・解放はメッセージ側。選択はfilename/typeで保持。並行更新、7 Factory、Userキャッシュと不正XML更新を検証。UI/セッション復元前にはAPVTSの古いadapterキャッシュを同期 |
| 8 数値回復 | Original入力結合へのNaN/Infで以後の有限入力でも戻らない。基準試験失敗。通常演奏のNaN発生は未証明 | 入力・モデル・出力結合をまとめてreset。VCAも異常audio/EGで内部状態をresetし、非有限depthでsmooth状態を汚染しない。回復後をfreshインスタンスと比較し、非ゼロの有限出力を確認 |

基準ソースを新規コンパイルした7件は **7件とも期待どおり失敗**。
境界MIDI、同位置順序、Release、Noise終端、Original数値回復、VCA数値回復、UI通知を再現した。
基準の保存GUIをクラッシュさせる試験は行っていない。
最初の混在した古いビルドでの異常終了は無効な再現として除外した。

## 初回修正の検証

| 構成 | 件数 | 結果・制約 |
|---|---:|---|
| Debug --all | 238 | failures/errors/disabled = 0 |
| Release --all | 238 | failures/errors/disabled = 0 |
| project ASan + UBSan + LeakSanitizer --all | 236 | 全成功、ログに検出なし。52の本体/試験cppを計装。JUCE/GoogleTestのmodule objectは未計装。ELF確保・ロックprobeの2件を除外 |
| Releaseの独立RT/whole-graph測定 | 4 | 全成功、exit 0。別のビルド/試験を停止して測定 |
| Python配布検査 | 8 | 正常・欠落CLAP・通知欠落・version/commit不一致等の合成ZIP試験 |
| EG式検査 | 4 | 成功。本番DSPやハードウェアの測定ではない |
| header / format | 102 C++ filesのformat、check_headers | 成功。clang-format 21.1.7。diffの空白検査も成功 |

途中の失敗も除去・弱化していない。全体試験の初回2件（初回確保、並行音源切替）は修正後成功。
Userキャッシュ→Factoryの追加試験でadapterキャッシュ不整合を発見し、セッション復元も含め修正した。
最初のsanitizerは65件成功後、既存Original VCF試験のBool→Float不正downcastで停止。
VCOのglissando fixtureにもBool代用があったため、両fixtureを本番Float型へ揃えた。
ケース・判定条件は維持している。以前の当該VCF試験のHigh側観測を正常な本番測定とは扱わない。
Windows/macOS、実DAW、実オーディオデバイス、全面的なJUCE計装、ThreadSanitizerは未実行。

再実行手順（依存関係はCONTRIBUTING.mdを参照）:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DSTANDALONE_ONLY=ON -DCOPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build --target CheapSynth01Tests check_headers --parallel 2
xvfb-run -a bash scripts/run-linux-gui-tests.sh build/Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests --all
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSTANDALONE_ONLY=OFF -DCOPY_PLUGIN_AFTER_BUILD=OFF -DCMAKE_SKIP_RPATH=ON
cmake --build build-release --parallel 2
xvfb-run -a bash scripts/run-linux-gui-tests.sh build-release/Tests/CheapSynth01Tests_artefacts/Release/CheapSynth01Tests --all
python3 Tests/tools/build-project-sanitized.py --build build --output build-sanitized
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 xvfb-run -a bash scripts/run-linux-gui-tests.sh build-sanitized/audit-sanitized --all
python3 Tests/release_validation_test.py -v
python3 Tests/eg_decay_equation_check.py -v
```

今回の環境はDebian 13、GCC 14.2.0、CMake 3.31.6、JUCE 9.0.3、Linux 6.18.44、x86_64。
AMD EPYC 9V74が3論理CPUとして見え、cgroup割当は2 CPU。GUIはXvfb/Openbox。
依存ソースは別checkoutに取得済みの同じ固定JUCE/GoogleTestをFetchContent source overrideで使用した。
JUCE原本は編集せず、検証済みcontextのCMakeレシピでgraph scratch再利用のoverrideをbuild内に生成した。
アップグレード時は再監査を必須にしている。

## RT・切替の測定と残存事項

48 kHz、全グラフ4x、各位置でNote On/CC/Off、各8ブロックでprogram変更、GUI操作を同時実行。
各ブロック長1000回、通常優先度のstd::threadによる単回観測。

| ブロック長 | p95 µs | p99 µs | 最大 µs | program変更時p99/最大 µs | 期間 µs |
|---:|---:|---:|---:|---:|---:|
| 1 | 4.447 | 6.179 | 95.405 | 5.649 / 11.277 | 20.833 |
| 16 | 123.096 | 224.560 | 1806.780 | 175.305 / 280.215 | 333.333 |
| 64 | 373.144 | 530.893 | 1152.950 | 469.410 / 537.763 | 1333.333 |

最大値は1/16サンプルで期間を超えた。平均CPUを締切保証にしない。
密集MIDI100×64callbackと初回/可変長/SysEx混在のprobeでは、検出対象の確保・解放は0。
mutex取得は44,900回（グラフ44,800 + programスレッド確認100）。
共有ライブラリ内部の全確保、aligned allocation、ホストwrapper全体のRT保証にはならない。
JUCE 9.0.3のMessageManagerスレッド確認もmutexを使う。非保護getterへの置換は競合を招くため行わない。
標準JUCE attachmentのホスト通知によるAsyncUpdater投稿も残る。

filter/lfo経路はTimer→メッセージ側のグラフ再構築で、sample-accurate切替ではない。
保持音での切替ブロック最大隣接差は0.170279、全体peakは0.317756（校正済みの可聴閾値なし）。
現グラフで両フィルタへ非ゼロ入力を追加した場合、64サンプルの中央値は114.574→117.789 µs（約2.8%増）。
これは固定グラフ＋selector/crossfadeを完成した比較ではない。この監査段階では
切替状態・追加ノードのCPU/互換性が未検証で、グラフを維持していた。
この段階のVCOは1サンプルAPIと反復パラメータ更新を使用していた。後段に変更後の測定を記録する。

## 互換性、配布、資料

パラメータID/version hint、XML構造・単位、Factory資源、live-control除外は維持。
通常の有限入力でのDSP構造回帰はbit単位の一致を検証。音源寿命の延長/短縮と終端補正は意図した音への変更。
非メッセージ側のprogramは次callbackへ適用し、GUI通知は60 Hz。外部Userファイル編集は一覧更新でキャッシュへ反映。
新しい保存/renameはWindows予約名等を拒否するが、既存ファイルの読み込みを名前規則で排除しない。
Original VCFの複数channel APIは一つの状態を共有する。本番mono契約を明記し、独立多声/多channelへ拡張していない。

Windows CLAP梱包を追加し、tag/製品version、全asset、通知、source commit一致を検査する。
Linux実成果物はStandalone/VST3/LV2/CLAP、ELF x86_64、ローカル動的依存解決を確認。
シンボル要件はGLIBC 2.38、GLIBCXX 3.4.32、CXXABI 1.3.15。古いLinuxへの互換性は未検証。
ワークスペース絶対パスのRPATHは除去。Windows/macOSのCPU/OS下限・署名/公証は未確認。
対応ソース、ライセンス、ファイルhash、runner情報を梱包する手順は [Distribution.md](Distribution.md)。

既存CSVは [dsp-baselines.md](dsp-baselines.md) ですでに理想参照/過去実験と区別されていた。
その分類を重複変更せず、[provenance.json](../artifacts/dsp/provenance.json) に保存内容のhashと履歴commitを補った。
履歴commitは測定時のソースcommitの証明ではなく、未記録の測定条件・toolchainはnullのままにした。
新しい本番グラフ測定は今回のcommit・環境・手順付きで別保存した。
VCOのみoversamplingする古い説明は、現行の全グラフ4xへ更新した。

LICENSEはGPLv3のまま。JUCE 9のAGPLv3／商用ライセンス状況は作者の判断が必要。
GPLv3 §13によるAGPL合成配布か、有効なJUCE商用ライセンスを利用するかを確認し、配布条件を確定すること。
商用ライセンスの保有は推測していない。通知や対応ソース梱包だけで適法性の確認を済ませたとはしない。

実機忠実度を達成したとは報告しない。校正には識別可能なCS-01個体/改版、電源・温度・部品状態、
信号レベル、入力/出力負荷、breath制御電圧、波形/feet/EG設定、録音系の帯域・sample rate・不確かさ、
生録音と測定点の対応が必要。許容誤差、個体差、制御曲線の基準は実機資料が得られるまで未解決。


## 追加修正：ホストprogram経路（2026-10-09 JST）

追加ソースは `904da992198ef7bd93e8e050faea34a286fcb540`。
初回の数値・測定は上記commitの記録として維持する。最新結果は
[program-rt-followup.json](../artifacts/audit/program-rt-followup.json)。

`setCurrentProgram`のスレッド判定によるMessageManager mutexを撤去し、
すべてのホスト要求を既存の不変キャッシュへ予約する。GUI選択・保存後の読込は
ProgramManagerの明示的な非RT経路を使い、実ファイル検査と即時読み込みを維持した。
JUCE VST3のProgramChangeParameterは現在のprogramと同じ要求を省略するため、
予約した選択は即時に返す。音声値は次callback（空ブロックを含む）へ適用する。
予約→元のprogramへの取消、最後の有効要求、無効index、適用前のsession保存を検証。
保存時には予約した値とidentityを一緒にsnapshotし、保存形式は変更しない。
成功したUI/session読込は以前の予約を取り消す。音声側は適用途中の新要求をCASで保護する。

最終Debug **242/242**、Release **242/242**、変更経路のproject
ASan/UBSan/LeakSanitizer **29/29** が成功。今回sanitizer全ケースの再実行はしていない。
JUCE等の未計装範囲と、初回の236件の全sanitizer検証は別commitの記録である。
既存User cache試験の適用前metadata期待値は予約選択へ揃え、代わりに適用前の
cutoffと音源modeが変わらない判定を追加した。ケース・音声側判定を削除していない。
最初のXvfb起動失敗は再実行で解消した環境起動失敗として記録した。

ホスト要求単体は両スレッド計204回で確保/解放/mutex取得が0。
同じ100×64密集callback試験のmutex取得は44,900→44,800回。
残りはJUCEグラフであり、完全なRT安全性・締切保証ではない。
追加でパラメータID/version hint、XML形式、DSPの音色・制御曲線は変更していない。

## 追加修正：配布物の整合性検査（2026-10-09 JST）

検査ソースは `0ec2ab1`。詳細は
[package-integrity-followup.json](../artifacts/audit/package-integrity-followup.json)。
従来の検査はCRCが正常な内容差し替えを拒否しなかった。拒否を期待する基準の1件が
失敗することを再現し、同じケースが修正後に成功することを確認した。

製品・対応ソースの両manifestへ全ファイルのSHA-256を付け、実内容と照合する。
manifest自身を除く完全な一覧を要求し、欠落・混入・不正hash・重複path・曖昧なmanifestを拒否。
Windowsでもhashのpathは `/` に統一する。タグの最終jobはcheckoutしたcommitを渡すため、
versionと内部commitが揃った古いasset一式も公開前検査で拒否する。
Python回帰をCI lintへ追加した。

Python **22/22** 成功。Linuxの実製品4形式＋対応ソースの**5 ZIP**で全hashを照合した。
実ZIP一式に別のsource commitを要求する負例も期待どおり拒否された。
Windows/macOS形式の合成fixtureは実OS成果物の成功とは扱わない。GitHub Actions自体は未実行。
本体C++とDSP試験は904da99から変更しておらず、C++回帰の追加再実行はしていない。
hashの一致は梱包内容の整合性を示し、署名や再現可能ビルドの証明ではない。
LICENSE・本番DSP・プリセット形式に変更なし。push・公開もしていない。


## 追加修正：測定後のVCO最適化（2026-10-09 JST）

本体・試験ソースは `386eded41921fbe94a76d4d2b09b7b060602e0cb`。
比較基準は `f42b3a35c8161cf323b9aa42cb874dc8b584ac83`。
whole-graph比較の旧Release実行ファイルは904da99のビルドであり、f42b3a3まで本体C++は同一。
条件・件数・hash・3回分の測定値は
[vco-optimization.json](../artifacts/audit/vco-optimization.json)、
測定ログは [vco-performance.log](../artifacts/audit/vco-performance.log)。

毎サンプル繰り返していたparameterの文字列検索とRTTIを、構築時に束縛したparameter objectへの参照に置き換えた。
値は以前と同じ時点で毎回読むため、automation値をブロック単位に固定していない。
Tone経路の1サンプルAPI呼出しを連続描画APIへまとめたが、LFO/PWM/glissando、
active/tail判定、加算と符号付きゼロの扱いは同じ順序で進める。
parameter ID・version hint・XML形式・制御曲線は変更していない。

凍結した旧Tone描画器との比較は **480条件・3,932,160 float値のビット一致**。
44.1/48/96 kHz、内部/外部oversampling、5波形、4 feet、1/7/64/255サンプル分割、
連続LFO、ライブpitch/bend/PWM/wave/glissando/Release更新、Note Off/再発音、graph所有tailを含む。
7 FactoryとsessionのValueTree置換後も、束縛先から現在の値を読むことを検証した。
凍結referenceはYM10150/WaveformStrategiesを共有しているため、この試験はそれら自体の独立oracleではない。

| 構成 | 件数 | 結果・制約 |
|---|---:|---|
| Debug --all | 245 | failures/errors/disabled = 0 |
| Release --all | 245 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 242 | 全成功・検出なし。54 project/test cppを計装。JUCE/GoogleTest/systemは未計装、ELF確保・mutex probe 3件を除外 |
| 独立Release観測 | 旧2件＋新6件 × 3回 | 全成功。旧→新を逐次実行、他のprojectビルド/試験を停止 |
| header / format | Debug/Release、105 C++ files | 成功。clang-format 21.1.7、diff空白検査成功 |

最初の比較はDebugビルドと並行したため、音声一致の検証は有効だが性能結論から除外した。
以下は再測定3回の範囲。Linux共有仮想CPU・通常priority、cgroup 2 CPU枠。
48 kHz host / 192 kHz processing、PWM、LFO depth 0.73。
32 warm-up＋1000 measured blocks、各trialの旧/新順序を交互にする。
VCO単体のmedianは各条件で約74–77%減少。p99・maxはスケジューリング等の影響を含み、締切保証ではない。

| glissando | host block | 旧median µs | 新median µs | 新p99 µs | 新max µs |
|---|---:|---:|---:|---:|---:|
| 一定pitch | 1 | 1.563–1.573 | 0.370–0.390 | 0.551–0.581 | 16.966–286.803 |
| 一定pitch | 16 | 20.511–20.592 | 5.268–5.278 | 18.378–44.838 | 80.541–2790.180 |
| 一定pitch | 64 | 81.172–81.423 | 20.932–20.991 | 79.580–96.105 | 248.736–2554.170 |
| 進行中 | 1 | 1.642–1.653 | 0.381–0.381 | 0.471–0.591 | 0.570–16.145 |
| 進行中 | 16 | 21.683–21.883 | 5.368–5.398 | 42.474–68.814 | 178.980–353.033 |
| 進行中 | 64 | 86.059–86.410 | 21.282–21.312 | 76.164–151.329 | 344.269–1499.360 |

whole-graphの既存spectrum/processing試験も再利用した。
下表は音声1秒を処理するmsの範囲で、callback締切の測定とは異なる。
記録された6条件のfolded-bin値は旧/新で同じ。実機の音色忠実度の証明にはしない。

| host Hz | waveform | 旧 ms/audio-second | 新 ms/audio-second |
|---:|---:|---:|---:|
| 44100 | 1 | 88.221–115.760 | 41.035–42.529 |
| 44100 | 2 | 83.345–107.028 | 38.134–40.379 |
| 48000 | 1 | 97.203–115.417 | 43.537–46.121 |
| 48000 | 2 | 94.793–114.520 | 40.950–45.569 |
| 96000 | 1 | 196.687–212.316 | 84.484–102.480 |
| 96000 | 2 | 190.612–228.507 | 80.462–107.000 |

MIDI密集・GUI同時操作・program切替の既存whole-graph試験（各block 1000 callbacks）の新実装結果：

| host block | p95 µs | p99 µs | max µs | program切替p99 µs | program切替max µs |
|---:|---:|---:|---:|---:|---:|
| 1 | 3.445–3.716 | 14.382–36.014 | 210.662–318.700 | 18.317–20.511 | 54.082–210.662 |
| 16 | 88.934–117.627 | 154.573–602.970 | 351.145–1777.180 | 127.532–229.196 | 179.330–426.333 |
| 64 | 251.830–408.206 | 384.190–1015.980 | 516.900–18019.600 | 279.461–619.495 | 338.140–877.714 |

48 kHzの締切はblock 1/16/64で20.833/333.333/1333.333 µs。
いずれも一部runでmaxが超えており、RT締切保証は達成していない。
GUI編集回数は壁時計依存でrunごとに異なるため、同一GUI負荷を厳密に対にした測定ではない。
100×64 callbackのprobeは確保/解放0、mutex取得44,800。
残るJUCE graph mutex、APVTS attachmentの通知投稿、メッセージループ依存の経路切替は未解決。
Windows/macOS・実DAW・実デバイス・全面JUCE計装・TSan・実機校正は今回も未実行。

再現には既存Release test executableを使い、他のビルドを止めて次を3回逐次実行する：

```sh
xvfb-run -a bash scripts/run-linux-gui-tests.sh \
  build-release/Tests/CheapSynth01Tests_artefacts/Release/CheapSynth01Tests \
  --all --gtest_filter='VcoOptimizationTest.*:VcoOptimizationObservationTest.*:WholeGraphObservationTest.*:AuditRealtimeTest.*'
```

旧whole-graphは基準ソースのRelease executableで同じWholeGraphObservationTest filterを実行する。
同一processのpaired VCO比較は凍結referenceを使うため、最新実行ファイルだけでも再実行できる。


## 追加修正：ホスト通知からのUI投稿を除去（2026-10-09 JST）

基準は `57e9b300617696efab8246029702e2fbc885b874`、
本体・試験commitは `5ac834fa4163c8f7e9e7905f6ce257a902deead9`。
詳細は [polling-notifications.json](../artifacts/audit/polling-notifications.json)、
測定値は [polling-notifications.log](../artifacts/audit/polling-notifications.log)。

JUCE 9.0.3のParameterAttachmentは非メッセージスレッドで
`isThisTheMessageThread()` と `triggerAsyncUpdate()` を呼ぶ。
同じworker回帰を標準JUCEのSlider/Button/ComboBox attachmentで実行すると、
3,000通知で**確保1・解放0・mutex 6,003**となり、確保0を要求する1件が期待どおり失敗した。
これはbindingを標準JUCE型へ置き換えてビルドした独立probeであり、
旧main全体の再ビルド・旧whole-graph比較ではない。

本番のSlider/Button/ComboBox bindingをmessage-thread所有のポーリングへ置換した。
parameter listenerを登録せず、構築時に初期値、以後60 Hzで現在のparameter値を読む。
音声側にメッセージ投稿やスレッド判定を追加していない。
値域・独自skew/snapping・テキスト変換・default double-clickはJUCE 9.0.3と比較。
GUI操作は即時にホストへ通知し、drag、Button/ComboBoxのcomplete gesture、
APVTSのUndoManager、drag中の破棄でのgesture終了を保つ。
外部変更の表示は約16.7 ms間隔、busyなmessage loopではさらに遅れる。DSPは待たない。

GUI変更→次tick前にホストが前回観測値へ戻すケースでも最新値を表示するため、
GUI書き込み後に観測cacheを無効化する。通知を伴わないMIDI値も直接観測する。
worker通知のcoalescing・GUIに触らないこと・同時automation中の破棄・tick前の破棄を直接検証した。

さらに中間実装の初回ホスト通知probeで、GUIを閉じた状態でも確保3回を検出。
APVTSのfeet/filterType/lfoTarget ListenerListのiterator vectorが初回通知で拡張する。
feetは既存音声側で毎回確認しているため、VCO/processorの冗長なAPVTS登録を除去。
経路変更はprocessorの既存メッセージtimerで現在のfilterType/lfoTargetを読む。
固定グラフ化は行わず、メッセージループ依存の切替契約を維持した。

| 検証 | 件数 | 結果・制約 |
|---|---:|---|
| Debug --all | 252 | failures/errors/disabled = 0 |
| Release --all | 252 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 247 | 検出なし。55 project/test cppを計装。JUCE/GoogleTest/systemは未計装、ELF probe 5件を除外 |
| 対象GUI/format/RT回帰 | 17 | 全成功、新規GUI 5件＋RT 2件を含む |
| 独立Release RT/whole-graph観測 | 7 | 全成功。他のprojectビルド・全回帰終了後に実行 |
| header / format | Debug/Release、107 C++ files | 成功。clang-format 21.1.7、diff空白検査成功 |

新binding単体の3,000 worker通知は**確保0・解放0・mutex 3,000**。
残る3,000はJUCEのAudioProcessorParameter通知lockである。
全parameterを100回変更するprocessorのprobeも、**初回を含め確保/解放0**。
mutex 4,530回はGUI開/閉で同じで、エディタによる追加取得は0。
このprobeは音声callback全体のlock-free保証ではない。

既存whole-graphのMIDI密集・GUI編集・program切替（各block 1000 callbacks）の観測：

| block | p95 µs | p99 µs | max µs | program切替p99 µs | program切替max µs | deadline µs |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2.994 | 4.788 | 78.318 | 4.487 | 19.36 | 20.8333 |
| 16 | 59.07 | 116.826 | 267.795 | 102.755 | 176.287 | 333.333 |
| 64 | 238.931 | 1106.57 | 2229.82 | 349.168 | 2210.77 | 1333.33 |

block 1/64のmaxは締切超過。今回1 runの観測であり、締切保証や分位値の確度を主張しない。
GUI編集回数は壁時計依存で旧測定と厳密には一致しないため、
この値から通知変更のCPU改善率を出さない。
100×64 callbackの確保/解放0・mutex 44,800回は維持。
JUCEグラフのlockと、ホスト通知自体のlockは残る。
記録された6 spectrum条件のfolded-bin値は前回と同じ。実機忠実度の証明ではない。

途中の対象16件では2件失敗した。Slider比例値をsnapping済みparameter値と直接比較した
誤ったoracleは標準JUCE Sliderとの比較へ直し、float精度の期待値も実値へ合わせた。
もう1件の初回APVTS確保は上記の登録除去で修正。ケース・判定を削除していない。
最初のDebug全回帰起動はlinkerと重なりpermission denied（exit 126）となった。
判定前の起動失敗として除外し、ビルド完了後に252件すべてを再実行した。
中間のcompile/format不備も修正済みで、古い実行ファイルの成功を最新結果に加算していない。
Windows/macOS・実DAW・実デバイス・全面JUCE計装・TSan・実機校正は未実行。

再現filterは `PollingAttachmentTest.*:PollingAttachmentRealtimeTest.*`。
全回帰は前述のXvfb手順に `--all` を指定する。
この追加変更でもparameter ID/version hint、保存XML、DSPの数式・制御曲線に変更なし。
通知の設計と寿命契約は [MIDI-realtime-control.md](MIDI-realtime-control.md)。

## 追加検証：filter/LFO経路の音声側選択（2026-10-10 JST）

この段階の基準は `0de5c948297bf0ff15eadc9185c0fb5cb30aaca7`。
当初監査対象/作業開始mainは引き続き `4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`。
基準実装の経路変更はメッセージtimerによる遅延グラフ変更であり、音声側の直接変更ではない。
message loopを動かさずworkerからchoiceを変更してrenderする再現は、
基準で1件失敗（実際の選択値2項目）、既存観測2件成功となった。

既存グラフを維持して固定接続にし、各host callbackの先頭でpending programを適用した後、
filterType/lfoTargetの現在値を読み、音声所有のinput/output/LFO maskを設定する。
0サンプルcallbackでも適用する。GUIの60 Hz表示timerは音の切替を決めない。
2つのchoice読取りは一括transactionではなく、任意sample内の切替を保証するものでもない。
非選択フィルターは以前の未接続状態と同様、ゼロ入力と既存EG sidechainで状態を進め、
VCAへ寄与する出力をゼロにする。両モデルの処理は残し、inactive状態履歴を保つ。
グラフ全体の撤去・新規node・crossfadeは行っていない。

本番変更commitは `360b547c158e03a7b4f63c47285fa04286b52d55`。
最終テストsourceは `3d72d88a0ef53bd09328e5784b4c6a3e6ce0d6f3`。
テスト限定macroで旧動的接続と両フィルター非ゼロ入力案を比較した。
12条件（48 kHz、内部192 kHz、block 1/16/64、2filter×2target）の各modeを
32 callback warmup後1000回、試行ごとに順序を交替して測定。
試作の対象10件は全成功、通常CPU中央値が旧方式と同程度であり、
両フィルター非ゼロ入力案の中央値負荷は増加したため、固定接続＋inactiveゼロ入力を採用した。
この旧方式は製品の追加modeやparameterではない。

出力比較は44.1/48/96 kHz、2filter×2target、block 1/7/64の36条件、
147,528 float値で数値として完全一致した。符号付きゼロも含むbit一致の主張ではない。
同じcallback時刻で切り替えた64,000 sampleも差0。
その観測peak=0.305222、switch sample step=0.159228で、旧hard switchの過渡音を保つ。
クリック解消、聴感評価、実機音色忠実度を達成したとは扱わない。
Release延長・短縮、Tone/Noise切替、連続filter/LFO切替、block分割をwhole graphで検証した。
parameter ID/version hint、保存XML、製品version、DSP数式・制御曲線、LICENSEに変更なし。

| 検証 | 件数 | 結果・制約 |
|---|---:|---|
| Debug --all | 259 | failures/errors/disabled = 0 |
| Release --all | 259 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 252 | 検出なし。56 project/test cppを計装。JUCE/GoogleTest/system未計装、ELF probe 7件除外 |
| 独立Release観測 | 9 × 3回 | すべて成功。全build/回帰終了後、各runを順に実行 |
| header / format | Debug/Release、108 C++ files | 成功。clang-format 21.1.7、diff空白検査成功 |

毎callbackで切替するactive-note probe（block 0/1/16/64、1000 callbacks）は
3回とも**確保0・解放0・mutex取得5,250・観測競合0・wait 0 ns**。
host choice通知自体はcallback probe外。意図的な競合controlでは各runで1回、
待ち501/922/281 nsを検出した。取得回数と待ちの有無を区別する。
既存の密集MIDI/GUI queue callback probeも3回とも確保/解放0、mutex 44,800回。

paired比較の代表値（target=VCO、3回の各run中央値の範囲、µs）：

| block / filter | 旧動的経路 | 固定・inactiveゼロ入力 | 固定・両方非ゼロ入力 |
|---|---:|---:|---:|
| 64 / Original | 53.211–56.836 | 52.720–53.140 | 55.794–55.994 |
| 64 / Modern | 44.838–44.958 | 44.247–44.397 | 55.885–55.924 |

全12条件×3mode×3runのmedian/p99/maxはJSON/logに残す。
中央値だけをCPU改善率や締切保証へ変換しない。scheduler等のばらつきと高いmaxは残る。

既存whole-graphの密集MIDI・GUI編集・program切替（各run/block 1000 callbacks）、
各列は3runの最小–最大、µs：

| block | p95 | p99 | max | program切替p99 | program切替max | deadline |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 3.034–3.735 | 4.537–51.808 | 107.673–293.934 | 6.230–24.367 | 32.409–77.427 | 20.8333 |
| 16 | 50.466–58.228 | 87.382–160.813 | 230.078–310.820 | 74.422–84.378 | 142.295–235.856 | 333.333 |
| 64 | 231.230–337.079 | 315.156–538.534 | 1172.280–2365.860 | 280.284–390.651 | 317.751–2365.860 | 1333.33 |

block 1では3runすべて、64では2runのmaxがdeadlineを超えた。
GUI編集回数は壁時計に依存し旧測定と厳密には一致しない。
短い試験の上位分位や平均からRT締切を保証しない。
whole-graph spectrumの6条件も記録し、実機校正とは区別する。


途中の全回帰はDebug/Release各259件中、SessionGraphTest 1件が失敗した。
旧構造の「非選択フィルターは物理的に未接続」という期待値を、固定接続確認に変更し、
復元後callbackのchoice値と選択側のみoutput maskが有効である判定を追加した。
保持ノートが復元されず、次のNote Onで発音するケース・判定は維持して再実行した。
試作buildのOriginalVCFProcessor include不足も修正してから検証した。
失敗ケースの削除、音声判定の削除、古い実行ファイルによる成功扱いはしていない。

Linux callback probeは有効な間だけpthread trylockのEBUSYを競合として数え、
続く実際のblocking lockの待ち時間を測る。EOWNERDEADなどの戻り値契約を維持する。
通常CPU観測はこのinterpositionを有効にしない。意図的に保持したmutexの競合1回と正の待ちを
検出するcontrolも実行した。競合0という観測はホストでも待たない保証ではない。
JUCEのgraph callback mutexとhost parameter通知mutexは残る。
グラフ接続編集はautomationから除去したが、host bus-layout lifecycleの更新経路は残る。
Windows/macOS、実DAW/デバイス、全面JUCE/GoogleTest/system計装、TSan、実機校正は未実行。
JUCE商用ライセンス取得状況・配布ライセンス経路は作者の判断が引き続き必要。

記録・再現条件は `artifacts/audit/routing-selection.json` と `.log`。
全回帰は前述Xvfb手順に `--all`、対象は `RoutingSelectionTest.*:RoutingSelectionRealtimeTest.*`。
性能観測はビルド/全回帰終了後に独立して3回実行した。
平均や中央値をRT締切保証として扱わず、p99/maxを併記する。
