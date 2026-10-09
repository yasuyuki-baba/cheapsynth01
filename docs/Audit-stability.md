# 安定性監査・実装結果（2026-10-09 JST）

作業開始時のGitHub最新mainは `4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`。
監査対象と同一だった。ローカルの古いorigin/mainや既存ビルドを基準にはしていない。
元checkoutは変更せず、独立worktreeの `fix/audit-stability` で実装した。
リポジトリ内にAGENTS.mdはなく、CONTRIBUTING.md、Tests/README.md、JUCEの規約・ソースを確認した。
push、PR、リリース公開はしていない。

以下の初回測定・テストのソースは `ec12e588f57b78fc199a001418a632463f2aa626`。
主要実装はa280bb7、キャッシュ復元の補完は9db9a2c、試験用型の補正はec12e58。
以後の報告・梱包commitでは本番DSPを変更しない。
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
これは固定グラフ＋selector/crossfadeを完成した比較ではない。切替状態・クロスフェード方針・追加ノードの
CPU/互換性試験が必要なため、この変更ではグラフを撤去しなかった。
VCOの1サンプルAPIや反復パラメータ更新も、LFO/PWM/glissando進行を保つ別の最適化候補として残した。

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
