# 自動シークエンスタイミングの考え方

## 基本的な考え方
酸化剤充填が完了した時，つまり充填確認スイッチが押された時を基準としてシークエンスが進む．
シークエンスが進む順番は以下の通り．
> FILL: 酸化剤充填 / パージ: 窒素放出による消火

1. FILL 開始
2. 酸素充填開始
3. イグナイター点火開始
4. バルブ開
5. FILL 停止
6. 酸素充填停止
7. イグナイター点火停止
8. パージ開始
9. パージ停止

## ignition関数

基本的なシークエンスをプログラムでは void sequence::ignition() が担っている．戻り値なし，引数なしの sequence 名前空間，ignition関数である．

ignition関数を下記に示す．

```
void sequence::ignition()
{
    // 重複実行防止
    if (sequence::ignitionSequenceIsActive)
        return;
    
    // 通信失敗したら点火シーケンスに進めない
    if (!communication::statusLamp.isHigh())
    {
        sequence::peacefulStop();
        return;
    }

    // エマスト中は点火シーケンスを始めない
    if (sequence::emergencyStopSequenceIsActive)
        return;

    // 充填開始前は点火シーケンスを始めない
    if (!control::fill.isAutomaticRaised())
        return;

    // 手動のFILLがONの間は点火シーケンスを始めない
    if (control::fill.isManualRaised())
        return;

    sequence::ignitionSequenceIsActive = true;
    sequence::canConfirm = false;

    control::sequenceStart.setAutomaticOn();
    mp3_play(4); // 0104_ignitionSequenceStart

    Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5);

    Tasks[control::IGNITER_START]->startOnceAfterSec(6.0);

    Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
    Tasks[control::OPEN_START]->startOnceAfterSec(10.0);

    Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
    Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);
    Tasks[control::PURGE_START]->startOnceAfterSec(20.5);
    Tasks[control::PURGE_STOP]->startOnceAfterSec(25.5);
}
```
ここから特に必要な箇所を下記に示す．

```
    mp3_play(4); // 0104_ignitionSequenceStart

    Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5);

    Tasks[control::IGNITER_START]->startOnceAfterSec(6.0);

    Tasks[control::FILL_STOP]->startOnceAfterSec(10.0);
    Tasks[control::OPEN_START]->startOnceAfterSec(10.0);

    Tasks[control::OXYGEN_STOP]->startOnceAfterSec(10.5);
    Tasks[control::IGNITER_STOP]->startOnceAfterSec(10.5);
    Tasks[control::PURGE_START]->startOnceAfterSec(20.5);
    Tasks[control::PURGE_STOP]->startOnceAfterSec(25.5);
```
上の行から順番に見ていく．
```
    mp3_play(4); // 0104_ignitionSequenceStart
```
この行はマイクロSDカードに保存された4番目のファイルを指定し音声出力している．つまり，「充填が確認されました．点火します．5秒前 4, 3, 2, 1」である．
```
    Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5);
```
この行からシークエンスの動作を実行している．イメージは「Tasks」というやることリストに「control::OXYGEN_START」を追加し，startOnceAfterSec(4.5)にあるように，4.5秒後に1度だけ実行するといった感じ．
「control::OXYGEN_START」は酸素の流路につながっている電磁弁をONにするという動作．

ここで，Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5); をよく観察してみると主に2つの要素で出来上がっていることが分かる．
1つ目は control::OXYGEN_START，2つ目は startOnceAfterSec(4.5) である．
1つ目の control::OXYGEN_START は電磁弁の動作に置き換えると分かりやすいと思う．さらに，startOnceAfterSec(4.5) も（）の中の数字を n とおけば，n 秒後に動作する指示であることがなんとなく分かると思う．

続いて，この行を見ていく．
```
    Tasks[control::IGNITER_START]->startOnceAfterSec(6.0);
```
こちらも，やることリストに追加していくイメージは変わらない．
「Tasks」というやることリストに「control::IGNITER_START」を追加し，startOnceAfterSec(6.0) のとおり，6.0秒後に1度だけ実行する．

ここで，[基本的な考え方](#基本的な考え方)に戻ってみると，今まで見てきたことは2. 3. の項目に書かれていた動作と一致すると思う．これは，プログラムのルールとして，上から下に実行されるためである．そのため，これ以降の動作も[基本的な考え方](#基本的な考え方)に書かれている通りの順番で実行されていく．

## 実行タイミングの考え方
ここまで，シークエンスとプログラムの対応関係について見てきたがここからはその動作がどのタイミングで実行されるのかを見ていく．

基本的には，startOnceAfterSec()の引数を見ていけば良い．（）の中に書いてある有効数字2桁の数が引数である．

下記の行から見ていく．
```
    Tasks[control::OXYGEN_START]->startOnceAfterSec(4.5);
```
この行では startOnceAfterSec()の引数が4.5である．この理由は，この以前の動作
```
    mp3_play(4); // 0104_ignitionSequenceStart
```
にて「充填が確認されました．点火します．5秒前」を言い終わるまでに4.5秒必要なためである．
これは，ignition関数が呼ばれた直後から4.5秒後に実行されると考えるとさらに分かりやすいと思う．

では，エンジンに点火するタイミングつまり，主流路弁が開するタイミングは
```
    Tasks[control::OPEN_START]->startOnceAfterSec(10.0);
```
となっており，ignition関数が呼ばれた直後から10.0秒後である．この時間を基準として点火操作を時系列順にまとめればどの動作がどのタイミングで実行されるか理解しやすいと思う．

時系列順に整理した点火シークエンスを下記に示す．

|T|実行時間|動作|引数の値|
|-|-|-|-|
|T+|15|パージ 停止|25.5|
|T+|10|パージ 開始|20.5|
|T+|0.5|酸素充填 停止|10.5|
|T+|0.5|イグニッション 停止|10.5|
|T-|0|FILL 停止|10.0|
|T-|0|主流路弁 開|10.0|
|T-|4|イグニッション 開始|6.0|
|T-|5|酸素充填 開始|4.5|

音声と合うように0.1秒単位でそろえるのは難しい．ちょっとずれる．

