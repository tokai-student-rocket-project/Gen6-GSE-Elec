
![Hero](./Documents/Pictures/Hero.JPG)

# 第6世代 Ground Support Equipment (GSE)

これまで運用してきたGSEから

- 小型化
- バッテリーからの給電
- Arduinoでの開発
- PlatformIOでの開発

に対応しました．

以前のGSEから，セミオートマチックシークエンスも引き継いでいます．

## ドキュメント
- [仕様](./Documents/Specifications.md)
- [パーツリスト](./Documents/)

## フォルダ構成

## .pio
: 基本的に自動生成されるため必要ないですが，ライブラリの関係で置いてあります．.pioフォルダの役割は外部ライブラリのインストール先とbuild時のファイル生成先です．（たぶん）

## [Document](./Documents/)
: 資料保管場所として使用しています．できる限りの情報を残せるように頑張ります．
## [include](./include/)
: 特に使用していません．(PlatformIO自動生成)
## [KiCad](./KiCad/)
: サテライトコントローラーランチコントローラーの基板が入っています．フォルダ名の通り，オープンソースのEDAである[KiCad](https://www.kicad.org/)を使用して作成しました．バージョンは 8.0 です．(2025年1月中に9.0へと上がるみたいでびっくりしています．)
## [lib](./lib/)
: ライブラリが入っています．新しくライブラリを作成する際はこちらに収納すると分かりやすいです．(PlatformIO自動生成)
## [src](./src/)
: ランチコントローラーとサテライトコントローラーのソースファイルが入っています．Arduinoで記述していますが，[PlatformIO](https://platformio.org/)の仕様上 .ino ファイルではなく，.cppファイルになっています．(PlatformIO自動生成)
## [test](./test/)
: test機能を使っていないので意味はありません．(PlatformIO自動生成)
## [LICENCE](./LICENSE)
: ライセンスを記載しています．
## [platformio.ini](./platformio.ini)
: PlatformIOを使用するための設定ファイルとなっています．参照：[platform.iniについて](#platforminiについて)

## platform.iniについて

platform.iniはPlatformIOを使用するための設定ファイルです．外部ライブラリがインストールされていなくてもこのplatform.iniファイルに記載があれば自動的にインストールしてくるため環境構築を行いやすくなり，とても便利です．参照：[公式](https://docs.platformio.org/en/latest/projectconf/index.html)

platform.iniファイルにはセクションがあり，それぞれに対してKeyとValueのペアがあります．

> [!NOTE]
> ここの説明を読むより，[公式](https://docs.platformio.org/en/latest/projectconf/index.html)の説明を読んだ方が正確で分かりやすいです！

### サテライトコントローラー
[env:Satellite]
> 環境を示しています．今回はサテライトコントローラーの環境になります．

platform = atmelavr
> 使用するマイコンに合わせています．参照：[公式](https://docs.platformio.org/en/latest/projectconf/sections/env/options/platform/platform.html)

board = ATmega2560
> 使用するマイコンが持つ固有の値を入れています．参照：[公式](https://docs.platformio.org/en/latest/projectconf/sections/env/options/platform/board.html)

framework = arduino
> 使用したいフレームワークを指定ています．参照：[公式](https://docs.platformio.org/en/latest/frameworks/index.html#frameworks)

monitor_speed = 115200
> シリアルモニターのボーレートを指定しています．なお，VScodeの拡張機能である Serial Monitor を使用する場合は拡張機能側で設定する必要があるため関係ありません．

upload_protocol = arduinoisp
> ArduinoISP を使用して書き込むため指定しています．（シリアル変換ICの影響か，USB経由で書き込みができなかったため．つけていて良かった．）

upload_speed = 19200
> ArduinoISP 周りの設定です．
upload_flags = ...
> ArduinoISP 周りの設定です．
upload_command = ...
> ArduinoISP 周りの設定です．

build_src_filter = +<SatelliteController/SatelliteController.cpp>
> env:Satellite の環境に書き込みたいソースファイルを指定できます．

lib_deps = ...
> 外部ライブラリを指定しています．

### ランチコントローラー
[env:Launch]
platform = atmelavr
> 環境を示しています．今回はランチコントローラーの環境になります．

board = ATmega2560
> [platform.ini > サテライトコントローラー参照](#サテライトコントローラー)

framework = arduino
> [platform.ini > サテライトコントローラー参照](#サテライトコントローラー)

monitor_speed = 115200
> [platform.ini > サテライトコントローラー参照](#サテライトコントローラー)

lib_deps = ...
> [platform.ini > サテライトコントローラー参照](#サテライトコントローラー)

build_src_filter = +<LaunchController/LaunchController.cpp>
> [platform.ini > サテライトコントローラー参照](#サテライトコントローラー)

### 手順書
- [せいさくちゅう](./)

### データフロー

- [ランチコントローラー](./Documents/Pictures/DataFlowDiagram/LaunchController_DataFlowDiagram.png)

- [サテライトコントローラー](./Documents/Pictures/DataFlowDiagram/SatelliteController_DataFlowDiagram.png)

## License

Copyright (c) 2025 Tokai Student Rocket Project

This project is licensed under the MIT License. See the [LICENSE](https://github.com/tokai-student-rocket-project/Gen6-GSE-Elec/blob/main/LICENSE) file for mare details.



