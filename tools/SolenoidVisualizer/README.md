# Gen6-GSE Solenoid Visualizer

Gen6-GSEシステムの電磁弁（ソレノイド）の動作状態を可視化・シミュレーションするためのGUIツールです．

## 動作要件 (Requirements)

Python 3 環境が必要です．
実行前に以下の外部ライブラリを `pip` でインストールしてください．

```bash
pip install customtkinter pyserial
```

※ `tkinter` は通常Pythonに標準で同梱されていますが，Linux環境等では別途インストールが必要な場合があります（例: `sudo apt-get install python3-tk`）．

## 実行方法 (How to Run)

Pythonインタプリタを使用して `main.py` を実行します．
（注: `python tools/SolenoidVisualizer` のようにディレクトリを指定するだけではエラーになります．必ず `main.py` まで指定してください）

**方法1: プロジェクトのルートディレクトリから実行する場合**
```bash
python tools/SolenoidVisualizer/main.py
```

**方法2: ツールのディレクトリに移動して実行する場合**
```bash
cd tools/SolenoidVisualizer
python main.py
```

## 主な機能 (Features)

- **シリアル通信モニタ:** Launch Controllerとシリアル通信で接続し，リアルタイムのコマンド(CMD)とフィードバック(FB)の状態を可視化します．
- **デバッグモード (DEBUG MODE):** ハードウェアがなくても，チェックボックスから各電磁弁を強制的にON/OFFし，配管内の流体（GN2, LN2O, GO2）の動きをシミュレーションできます．
- **自動シーケンス (AUTO LAUNCH):** `LaunchController2.0.cpp` のタイミング（充填，酸素，点火，主弁開，パージなど）に合わせた自動打ち上げシーケンスをGUI上でテスト・可視化できます．
