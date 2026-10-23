# Widgetの入力とEventTrigger

入力判定はButtonから分離しています。Widget / TextWidget / SpriteWidget / WidgetGroup / Buttonのいずれも、EventTriggerを追加すれば入力を受け取れます。画像は不要です。

## 登録例

```cpp
#include "UI/EventTrigger.h"
#include <functional>

auto widget = std::make_shared<Widget>("Clickable Area");
widget->rect.position = {100, 100};
widget->rect.size = {300, 80};

auto* trigger = widget->AddComponent<EventTrigger>();
trigger->AddListener(EventTriggerType::PointerEnter,
    std::bind(&MyScene::OnPointerEnter, this, std::placeholders::_1));
trigger->AddListener(EventTriggerType::PointerClick,
    std::bind(&MyScene::OnPointerClick, this, std::placeholders::_1));
trigger->AddListener(EventTriggerType::Submit,
    std::bind(&MyScene::OnSubmit, this, std::placeholders::_1));
widgetManager.Register(widget);
```

コールバックの引数は `UIEventData&` です。位置、移動量、押下開始位置、マウスボタン、ホイール、ナビゲーション方向などを取得できます。登録は複数可能で、`RemoveListeners(type)` で解除します。Moveで `data.Use()` を呼ぶと、既定の選択移動を止められます。

左クリックと決定に同じ処理を登録する場合は、引数なしの関数を `AddActivationListener(std::bind(&MyScene::OnActivate, this))` へ渡せます。右・中クリックではこの簡易登録の処理を呼びません。

## イベント

[Unity EventTriggerのイベント分類](https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/SupportedEvents.html)に合わせた17種類を用意しています。

| 操作 | イベント |
| --- | --- |
| ポインターの出入り | PointerEnter / PointerExit |
| 押下・解放・クリック | PointerDown / PointerUp / PointerClick |
| ドラッグ準備・開始・移動・終了・受け取り | InitializePotentialDrag / BeginDrag / Drag / EndDrag / Drop |
| ホイール | Scroll |
| 選択・選択解除・選択中の更新 | Select / Deselect / UpdateSelected |
| スティック・方向キーの移動 | Move |
| A・Enter・Spaceでの決定 | Submit |
| B・Escapeでの取り消し | Cancel |

パッド操作でPointerEnterは発火しません。マウスのホバーと、コントローラーの選択は別の状態です。移動は既存GamePadの入力（左スティック・十字キー・矢印キー・WASD）を利用します。

## 配送とナビゲーション

- WidgetManager::UpdateでWidgetと子階層を更新した後、UIEventSystemが1回入力を配送します。個々のWidgetで生の入力を再処理する必要はありません。
- 矩形の位置・サイズ・anchor・回転で判定し、重なった入力対象では最後に描画される対象を優先します。
- クリックは同じ対象上で押して離した場合に発火します。ドラッグ開始後はクリックしません。
- ドラッグの標準しきい値は5ピクセルです。左右・中央のボタンごとに押下対象を保持します。
- 親グループの非表示、破棄、コンポーネント無効化、ウィンドウのフォーカス喪失、ImGuiの入力捕捉を考慮します。
- `SetNavigation(UINavigation::None / Horizontal / Vertical / Automatic)` で移動方向を設定します。既定はAutomaticです。
- `Select()` は次の配送時に選択する要求です。選択移動では位置から次の対象を探します。`SetWrapNavigation(true)` で端から反対側へ循環します。
- `SetInteractable(false)` で入力を止められます。`SetInputDelay(seconds)` はページ切替直後の持ち越し入力の抑制に使用します。
- グループへの登録が必要な場合は、グループをWidgetManagerに登録します。親子両方をルートへ二重登録しないでください。

## Button

Buttonは通常画像・選択時画像・ラベル・フォーカス装飾を描くクラスです。EventTriggerは自動では追加しません。入力が必要な場所で明示的に追加してください。

`SetTexture(path)` と `SetSelectionTexture(path)` は任意です。空文字や見つからない画像、画像の読込失敗は画像未設定として扱い、文字や入力処理を継続します。画像不要のWidgetは画像を作る必要がありません。

## ビルド後の確認項目

1. タイトルの全項目とポーズメニューをマウス・パッドで操作し、決定が1回ずつ実行されること。
2. 画像を設定しないButtonと通常のWidgetにEventTriggerを付け、出入り・押下・解放・クリックを確認すること。
3. 重ねたWidgetでは手前だけが反応し、領域外で離した場合やドラッグした場合はクリックしないこと。
4. ドラッグ中のPointerUp / Drop / EndDragと、ホイール、右・中クリックのbutton値を確認すること。
5. 選択・押下中に親を非表示にする、破棄する、Alt+Tabする、ImGuiを操作する場合に、入力が残らないこと。
6. ページ切替中にA・Enterを押し続けても、次の画面で再決定しないこと。

## 配置の調整

UILayout::Apply(widget, {x, y}, {width, height}, fontSize)で配置をまとめて指定します。
位置とサイズは画面比率、文字サイズは1080p基準です。画像の比率を保つ場合はImage、画像の透明余白を切り出す場合はSource、ゲージ内部の範囲はFillを使用します。

実行中のWidgetの確認・編集には、F2で開くヒエラルキーとインスペクターを使用します。