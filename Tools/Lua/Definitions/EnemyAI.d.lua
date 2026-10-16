---@meta

-- VS Code / Lua Language Server用の型定義。
-- 実際の関数はEnemyAI::BindEnvironmentが各AIスクリプトへ登録する。
-- ゲームからこのファイルをLoad・requireしないこと。

---@class EnemyAIVector3
---@field x number X座標
---@field y number Y座標
---@field z number Z座標

---@class EnemyAITarget
---@field name string アクターの名前
---@field distance number 自分からの距離
---@field position EnemyAIVector3 対象のワールド座標

---@alias EnemyAIActionPhase
---| 1 # 開始時の処理
---| 2 # 毎フレームの更新
---| 3 # フレーム後半の更新
---| 4 # 終了時の処理

---C++の真偽値を取得する。未登録の名前ならfalse。
---例：HasTarget、IsTargetVisible、HasDestination、IsFacingTarget。
---@param name string パラメーター名
---@return boolean value
function GetBool(name) end

---C++の数値を取得する。未登録の名前なら0。
---例：TargetDistance、TurnAngle（度）、StateTime（秒）、調整値の名前。
---@param name string パラメーター名
---@return number value
function GetFloat(name) end

---C++側と共有する真偽値を設定する。
---@param name string パラメーター名
---@param value boolean 設定する値
function SetBool(name, value) end

---C++側と共有する数値を設定する。
---@param name string パラメーター名
---@param value number 設定する値
function SetFloat(name, value) end

---デバッグ表示に使う現在の状態名を設定する。行動の切り替え自体はLuaで行う。
---@param name string 状態名
function SetState(name) end

---C++で登録した行動の開始・更新・終了処理を呼ぶ。
---@param name string C++で登録した行動名
---@param phase EnemyAIActionPhase 呼び出す処理の種類
function Action(name, phase) end

---C++のベクトル値を取得する。未登録の名前ならx・y・zすべて0。
---例：GetVector3("TargetPosition")で現在の対象位置を取得する。
---@param name string パラメーター名
---@return EnemyAIVector3 value
function GetVector3(name) end

---索敵距離内にいる、対象タグに一致する有効なアクターの一覧を取得する。
---距離だけで検索する。視認できているかの判定とは別。
---@return EnemyAITarget[] targets
function GetTargetsInSearchRange() end

---相手へ向き直る際の補間速度を設定する。
---TurnAngularSpeedが設定されている場合は、その角速度の指定が優先される。
---@param speed number 0以上の補間速度
function SetTrackingTurnSpeed(speed) end

---現在の対象へNavMeshで移動し、その方向を向く。移動禁止中・対象なしなら停止する。
---@param speed number 移動速度
function MoveToTarget(speed) end

---NavMeshによる移動を停止する。
function StopMovement() end

---現在の対象の方向へ向き直る。RotationPausedがtrueなら回転しない。
function FaceTarget() end
