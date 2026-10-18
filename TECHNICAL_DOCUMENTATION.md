# cjy 项目技术实现文档

> 适用工程：`cjy 5.8`（Unreal Engine 5.8.3 / 蓝图 + C++ 混合工程）
> 文档范围：工程结构、C++ 模块、CanvasTool 插件、蓝图系统、输入、材质、场景、纹理生成管线、工具链、已知限制
> 生成时间：2026-10-18（基于对工程的实际扫描与验证）

---

## 目录

1. [项目概览](#1-项目概览)
2. [工程结构与规模](#2-工程结构与规模)
3. [C++ 源码模块](#3-c-源码模块)
4. [CanvasTool 插件（本项目核心技术）](#4-canvastool-插件本项目核心技术)
5. [蓝图系统](#5-蓝图系统)
6. [输入系统](#6-输入系统)
7. [材质与纹理系统](#7-材质与纹理系统)
8. [场景与环境系统](#8-场景与环境系统)
9. [纹理生成管线（离线 Python）](#9-纹理生成管线离线-python)
10. [工具链与自动化工作流](#10-工具链与自动化工作流)
11. [已知限制与踩坑记录](#11-已知限制与踩坑记录)
12. [附录 A：关键资产路径与坐标](#附录-a关键资产路径与坐标)
13. [附录 B：CanvasTool 完整 API 参考](#附录-bcanvastool-完整-api-参考)

---

## 1. 项目概览

| 项目 | 内容 |
|---|---|
| 引擎 | Unreal Engine **5.8.3**（`D:\UE_5.8`） |
| 工程文件 | `cjy.uproject` |
| 主要关卡 | `Content/FirstPerson/Lvl_FirstPerson` |
| 关卡技术 | **World Partition**（外部 Actor 保存，2533 个 `.uasset`） |
| 代码形态 | 以**蓝图**为主 + **1 个运行时 C++ 模块（cjy）** + **1 个功能插件（CanvasTool）** |
| 编译工具链 | Visual Studio Build Tools **17.14.42**（MSVC 14.50）+ Windows SDK 10.0.19041+ |
| 版本管理 | Git，`main` / `feature/decal-eraser` / `feature/canvas-tool-scene` |

### 1.1 工程启用的插件（`cjy.uproject`）

| 插件 | 用途 |
|---|---|
| `Water` | 湖泊水体（`WaterBodyLake` + 水体样条） |
| `Landmass` | 地形/地貌建模 |
| `ModelingToolsEditorMode` | 模型编辑 |
| `GameplayStateTree` | 状态树 |
| `PythonScriptPlugin` | **自动化核心**：所有批量操作用它 |
| `EditorScriptingUtilities` | 资产导入/保存/检查 |
| `ProceduralVegetationEditor` | 程序化植被 |
| **`CanvasTool`** | **本项目自研插件**：画板绘制系统（见第 4 章） |

---

## 2. 工程结构与规模

### 2.1 顶层目录

```
cjy 5.8/
├── cjy.uproject
├── Config/                     # DefaultEngine.ini / DefaultInput.ini / DefaultGame.ini
├── Source/                     # C++ 源码（2 个 Target + 1 个 Runtime 模块）
├── Plugins/CanvasTool/         # 自研画板插件
├── Content/
│   ├── FirstPerson/            # 第一人称模板：角色、游戏模式、HUD、关卡
│   ├── CanvasTool/             # 画板材质（M_BoardSurface / M_BrushDot / M_BoardHighlight）
│   ├── Environment/Lake/       # 湖泊、地形、材质、纹理（本项目主要打磨区域）
│   ├── Environment/Pavilion/   # 亭子（Blender 导出）
│   ├── Input/                  # IMC_Default / IMC_MouseLook
│   ├── jinyu/                  # 色卡圆柱的材质库（1073 个材质）
│   ├── Fab/ Megascans/         # 商城/扫描资产（泥土、植物）
│   ├── Megaplant_Library/      # 树木库
│   ├── StylizedGrassFlowersPack/ # 风格化草花
│   ├── SimpleRocks/ HighPoly_Tree_Model/ huashu/ mianhua/ mymodel/
│   └── __ExternalActors__/     # World Partition 外部 Actor（2533 个）
└── Saved/DSH/                  # 自动化脚本、日志、备份、生成的纹理
```

### 2.2 规模统计（扫描值）

| 指标 | 数量 |
|---|---|
| 关卡 Actor 总数 | **2533** |
| 蓝图资产 | **265**（其中 `BP_RisingCylinder` 系列 **251** 个） |
| `/Game/jinyu` 材质 | **1073** |
| World Partition 外部 Actor 文件 | **2533** |

---

## 3. C++ 源码模块

### 3.1 Target 文件

| 文件 | 说明 |
|---|---|
| `Source/cjy.Target.cs` | 打包用 Target（`cjy`） |
| `Source/cjyEditor.Target.cs` | 编辑器 Target（`cjyEditor`） |

```csharp
// 两者一致的关键设置
DefaultBuildSettings = BuildSettingsVersion.Latest;
IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
ExtraModuleNames.Add("cjy");
```

### 3.2 Runtime 模块 `cjy`

| 文件 | 内容 |
|---|---|
| `Source/cjy/cjy.Build.cs` | 依赖：`Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput` |
| `Source/cjy/Public/cjy.h` | 模块头 |
| `Source/cjy/Private/cjy.cpp` | `IMPLEMENT_PRIMARY_GAME_MODULE(FCjyModule, cjy, "cjy")` |

> **为什么需要 C++ 模块**：本工程原本是纯蓝图工程；自研插件 `CanvasTool` 是 C++ 插件，**必须有 C++ 模块才能编译**，因此补充了最小化的 `Source/cjy`。

### 3.3 编译命令

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' cjyEditor Win64 Development `
    "-project=<工程绝对路径>\cjy.uproject" -waitmutex -NoHotReload
```

> ⚠️ **必须先完全关闭 `UnrealEditor.exe`**（`CrashReportClient` 不用管），否则文件占用导致编译失败。

---

## 4. CanvasTool 插件（本项目核心技术）

一个**运行时 C++ 插件**，实现"游戏内手持画板 + 鼠标绘画"系统。全部界面与资源由代码在运行时创建，**不依赖任何蓝图资产**。

### 4.1 插件文件结构

```
Plugins/CanvasTool/
├── CanvasTool.uplugin
├── Source/CanvasTool/
│   ├── CanvasTool.Build.cs
│   ├── Public/
│   │   ├── CanvasTool.h              # 模块入口声明
│   │   ├── CanvasBoardActor.h        # 画板 Actor（4.3）
│   │   ├── CanvasToolSubsystem.h     # 世界子系统（4.4）
│   │   └── CanvasToolLibrary.h       # 编辑器工具函数库（4.5）
│   └── Private/
│       ├── CanvasTool.cpp            # IMPLEMENT_MODULE
│       ├── CanvasBoardActor.cpp
│       ├── CanvasToolSubsystem.cpp
│       └── CanvasToolLibrary.cpp
└── Content/                          # （空，材质放在工程 Content 下）
```

`CanvasTool.uplugin`：

```json
{
  "FileVersion": 3,
  "FriendlyName": "CanvasTool",
  "Version": 1,
  "VersionName": "1.0",
  "Modules": [
    { "Name": "CanvasTool", "Type": "Runtime", "LoadingPhase": "Default" }
  ]
}
```

`CanvasTool.Build.cs` 依赖：

| 类型 | 模块 |
|---|---|
| Public | `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput` |
| Private | `RenderCore`, `RHI`, `Slate`, `SlateCore`, `AssetRegistry` |
| **仅编辑器**（`if (Target.bBuildEditor)`） | `UnrealEd`, `Kismet` |

---

### 4.2 运行时架构总览

```
                    ┌───────────────────────────────────────────┐
                    │  UCanvasToolSubsystem (UWorldSubsystem)   │
                    │  · OnWorldBeginPlay：创建隐藏的画板 Actor  │
                    │  · 运行时生成 IMC + 输入动作，绑定到 Pawn   │
                    │  · Slate 右上角按键提示层                 │
                    │  · 右键映射摘除 / 恢复（冲突处理）          │
                    └───────────────┬───────────────────────────┘
                                    │ 每帧驱动 / 输入转发
                    ┌───────────────▼───────────────────────────┐
                    │      ACanvasBoardActor (Actor)            │
                    │  · BoardMesh   ：画板面（UStaticMesh）     │
                    │  · CursorMesh  ：橙色画笔光标点            │
                    │  · HighlightMesh：选中高亮边框             │
                    │  · CanvasRT    ：1024×1024 RGBA8 渲染目标  │
                    │  · Tick：锁定视角 / 跟随视角 / 鼠标射线 / 绘制│
                    └───────────────┬───────────────────────────┘
                                    │ DrawMaterialToRenderTarget
                    ┌───────────────▼───────────────────────────┐
                    │   材质三件套（Content/CanvasTool）         │
                    │  M_BoardSurface  ：显示 RT（Unlit）        │
                    │  M_BrushDot      ：笔刷圆点（Translucent）  │
                    │  M_BoardHighlight：选中边框（Unlit 不透明） │
                    └───────────────────────────────────────────┘
```

### 4.3 `ACanvasBoardActor` —— 画板 Actor

**文件**：`Public/CanvasBoardActor.h` / `Private/CanvasBoardActor.cpp`

#### 关键 UPROPERTY

| 属性 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `BoardMesh` | `UStaticMeshComponent*` | 引擎 Plane | 画板面（根组件） |
| `CursorMesh` | `UStaticMeshComponent*` | 引擎 Plane | 画笔位置可视化点 |
| `HighlightMesh` | `UStaticMeshComponent*` | 引擎 Plane | 选中高亮边框（比画板大 7cm） |
| `CanvasRT` | `UTextureRenderTarget2D*` | 运行时创建 | 画布纹理，**1024×1024 RGBA8** |
| `BaseColor` | `FLinearColor` | `(0.93, 0.92, 0.87, 1)` | 画板底色 / 橡皮擦颜色 |
| `CanvasSize` | `int32` | `1024` | RT 分辨率 |
| `BoardWidth` | `float` | `40` | 画板宽（cm） |
| `BoardHeight` | `float` | `50` | 画板高（cm） |
| `BrushRadius` | `float` | `0.020` | 笔刷半径（UV 单位，2%） |
| `FollowDistance` | `float` | `70` | 画板在视线前方的距离（cm） |
| `FollowVerticalOffset` | `float` | `-16` | 相对视线的垂直偏移（cm） |
| `FollowHorizontalOffset` | `float` | `0` | 拖动产生的水平偏移（cm） |
| `InkOverride` | `FLinearColor` | 深色 | 取色结果（优先于 `PickColor`） |
| `bHasInkOverride` | `bool` | `false` | 是否已取到颜色 |
| `bIsOut` / `bDrawing` / `bErase` | `bool` | `false` | 状态位 |
| `bViewLocked` | `bool` | `false` | 视角是否被锁定 |
| `LockedViewRot` | `FRotator` | 零 | 锁定时的视角朝向 |
| `bSelected` / `bDragging` | `bool` | `false` | 选中 / 拖动状态 |
| `CursorUV` / `bCursorOnBoard` | `FVector2D` / `bool` | — | 鼠标在画板上的 UV |

#### 构造函数（组件与碰撞）

```cpp
// 引擎基础平面（100×100 单位）
static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
BoardMesh->SetStaticMesh(PlaneMesh.Object);
SetRootComponent(BoardMesh);

// 画板碰撞：只响应 Visibility 射线，绝不影响角色移动
BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
BoardMesh->SetCollisionObjectType(ECC_WorldDynamic);
BoardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
BoardMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
// ECC_Pawn / ECC_Camera 均为 Ignore
```

> **设计要点**：画板必须能被"鼠标射线"命中（`ECC_Visibility`），但**绝不能被角色胶囊碰撞**（历史上曾因画板挡住角色导致"人物无法移动"）。

#### `SetupBoard()` —— 首次创建资源

1. **创建 RT**（仅第一次）：`UKismetRenderingLibrary::CreateRenderTarget2D(this, CanvasSize, CanvasSize, RTF_RGBA8, BaseColor, false)`
2. **清空 RT**：**只在首次创建时执行** —— 保证收起/打开画板**笔迹保留**
3. **创建画板材质实例**：加载 `/Game/CanvasTool/M_BoardSurface`，依次尝试参数名 `CanvasTexture` / `Canvas` / `Texture` / `CanvasTex` / `RenderTarget`，用 `GetTextureParameterValue` **验证绑定成功**（`bTextureBound`）
4. **创建笔刷材质实例**：`/Game/CanvasTool/M_BrushDot`
5. **设置网格缩放**：`SetRelativeScale3D(BoardWidth/100, BoardHeight/100, 1)`
6. **画笔光标材质**：基于 `M_BrushDot` 创建 MID，`Color = (1, 0.35, 0.1)`，`Center = (0.5, 0.5)`，`Radius = 0.5`
7. **高亮边框材质**：加载 `/Game/CanvasTool/M_BoardHighlight`（不透明纯青色）

#### `Tick()` —— 每帧核心逻辑

```
① 获取 PlayerController 的 view point
② 若 bViewLocked：每帧强制 PC->SetControlRotation(LockedViewRot)
③ 放置画板：ViewLoc + Fwd*FollowDistance + Up*FollowVerticalOffset + Right*FollowHorizontalOffset
   旋转：FRotationMatrix::MakeFromZX(-Fwd, Right).Rotator()（让画板正对视线、不倾斜）
④ 鼠标射线：PC->DeprojectMousePositionToWorld → LineTraceSingleByChannel(ECC_Visibility, 4000cm)
   命中本 Actor → 计算 UV：
        UV.X = clamp(Local.X / 100 + 0.5, 0, 1)     // U 沿 +X
        UV.Y = clamp(Local.Y / 100 + 0.5, 0, 1)     // V 沿 +Y
⑤ 光标点：CursorMesh->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.8)
⑥ 若 bSelected && bDragging：读取真实鼠标屏幕坐标差值 → 按 FOV 换算成相机空间厘米 → 位移画板
⑦ 若 bDrawing && bCursorOnBoard：StrokeToUV(UV, Ink, BrushRadius)
```

#### 绘制管线

```cpp
void ACanvasBoardActor::PaintAtUV(const FVector2D& UV, const FLinearColor& Color, float Radius)
{
    BrushMID->SetVectorParameterValue(TEXT("Center"), FLinearColor(UV.X, UV.Y, 0.f, 0.f));
    BrushMID->SetScalarParameterValue(TEXT("Radius"), FMath::Max(0.002f, Radius));
    BrushMID->SetVectorParameterValue(TEXT("Color"), Color);
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, CanvasRT, BrushMID);
    ++PaintedDots;
}

void ACanvasBoardActor::StrokeToUV(const FVector2D& UV, const FLinearColor& Ink, float Radius)
{
    if (!bHasLastUV) { PaintAtUV(UV, Ink, Radius); LastUV = UV; bHasLastUV = true; return; }
    const float Spacing = FMath::Max(Radius * 0.25f, 0.002f);   // 步长
    const float Dist = FVector2D::Distance(LastUV, UV);
    const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dist / Spacing), 1, 96);
    for (int32 i = 1; i <= Steps; ++i)
        PaintAtUV(FMath::Lerp(LastUV, UV, (float)i / Steps), Ink, Radius);
    LastUV = UV;
}
```

#### `GetInkColor()` —— 墨水颜色来源（3 级优先）

```cpp
if (bHasInkOverride) return InkOverride;                    // ① 取色结果（插件内）
// ② 读取角色蓝图的 PickColor 变量（反射）
if (FStructProperty* P = CastField<FStructProperty>(Pawn->GetClass()->FindPropertyByName("PickColor")))
    if (P->Struct == TBaseStructure<FLinearColor>::Get())
    {
        FLinearColor C = *P->ContainerPtrToValuePtr<FLinearColor>(Pawn);
        if (!(接近纯白)) return C;
    }
return FLinearColor(0.05f, 0.05f, 0.09f, 1.f);              // ③ 深色兜底（白笔画白板看不见）
```

#### 其它成员函数

| 函数 | 作用 |
|---|---|
| `ClearBoard()` | `ClearRenderTarget2D(this, CanvasRT, BaseColor)` —— **只有主动清空才调用** |
| `SetupBoard()` | 首次创建资源 / 重置画板尺寸与材质 |

---

### 4.4 `UCanvasToolSubsystem` —— 世界子系统（驱动器）

**文件**：`Public/CanvasToolSubsystem.h` / `Private/CanvasToolSubsystem.cpp`

#### 职责

1. `OnWorldBeginPlay`：创建隐藏的画板 Actor（`SpawnActor<ACanvasBoardActor>`，初始隐藏）
2. **运行时生成输入**：新建 `UInputMappingContext` + 6 个 `UInputAction`，`MapKey` 绑定按键，`AddMappingContext` 注册
3. 定时器（0.5s）重试 `TrySetupInput()`，直到 Pawn 的 `UEnhancedInputComponent` 就绪，再 `BindAction`
4. 维护**右上角 Slate 提示层**
5. **右键冲突处理**：临时摘除/恢复工程 IMC 的右键映射
6. 画板状态机（拿出 / 收起 / 选中 / 清除 / 笔刷大小 / 取色）

#### 运行时输入映射表

| 输入动作 | 键位 | 触发 | 处理函数 | 功能 |
|---|---|---|---|---|
| `IA_Board` | **B** | Started | `OnToggleBoard` | 拿出/收起画板 |
| `IA_Draw` | **鼠标右键** | Started/Completed/Canceled | `OnDrawStarted/Stopped` | 按住连续画线 |
| `IA_Eraser` | **X** | Started | `OnToggleEraser` | 橡皮擦开关 |
| `IA_Clear` | **C** | Started | `OnClearBoard` | 清空画布 |
| `IA_Smaller` | **Z** + **滚轮下** | Started | `OnBrushSmaller` | 笔刷变细（×0.7） |
| `IA_Larger` | **V** + **滚轮上** | Started | `OnBrushLarger` | 笔刷变粗（×1.4） |
| `IA_Pick` | **鼠标左键** | Started/Completed | `OnSelectBoard/OnSelectRelease` | 选中/拖动/取消选中画板 |

> 笔刷半径限制：`[0.004, 0.12]`（UV 单位）

#### `OnToggleBoard()` —— 状态机核心

```cpp
Board->bIsOut = !Board->bIsOut;

if (Board->bIsOut)   // === 拿出画板 ===
{
    Board->SetupBoard();
    // ① 锁定视角 + 释放鼠标
    Board->LockedViewRot = ViewPC->GetControlRotation();
    Board->bViewLocked = true;
    ViewPC->SetShowMouseCursor(true);
    ViewPC->SetIgnoreLookInput(true);                    // ★ 彻底阻止视角转动（防抖）
    ViewPC->SetInputMode(FInputModeGameAndUI());          // 鼠标自由移动

    // ② 通知角色蓝图"画板已打开"（它的左键取色被 Branch 门控）
    if (FBoolProperty* P = CastField<FBoolProperty>(
            Pawn->GetClass()->FindPropertyByName("bCanvasBoardOpen")))
        P->SetPropertyValue_InContainer(Pawn, true);

    // ③ 冲突处理：摘除工程 IMC 的右键映射
    SetProjectRightMouseEnabled(false);

    // ④ 显示右上角提示层
    ShowHints(true);

    // ⑤ 开启画板碰撞 + 显示
    Board->BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Board->SetActorHiddenInGame(false);
}
else                 // === 收起画板 ===
{
    Board->bDrawing = false;
    Board->SetActorHiddenInGame(true);
    Board->BoardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Board->bViewLocked = false;
    Board->CursorMesh->SetVisibility(false);
    ViewPC->SetShowMouseCursor(false);
    ViewPC->SetIgnoreLookInput(false);
    ViewPC->SetInputMode(FInputModeGameOnly());

    // bCanvasBoardOpen = false
    // SetProjectRightMouseEnabled(true);
    // ShowHints(false);
}
```

> **笔迹保留**：收起只是 `SetActorHiddenInGame(true)`，**不销毁 RT**；再次拿出时 `SetupBoard()` 里"只有 `!CanvasRT` 才创建/清空"，所以**笔迹完整保留**，只有 `C` 键主动清空才擦除。

#### 左键选中 / 拖动

```cpp
void UCanvasToolSubsystem::OnSelectBoard(const FInputActionValue& /*Value*/)
{
    if (!Board || !Board->bIsOut || !Board->bCursorOnBoard) return;   // 必须点在画板上
    Board->bSelected = !Board->bSelected;      // 切换选中
    Board->bDragging = Board->bSelected;       // 选中时开始拖动
}
void UCanvasToolSubsystem::OnSelectRelease(const FInputActionValue& /*Value*/)
{
    if (Board) Board->bDragging = false;
}
```

拖动换算（在 `ACanvasBoardActor::Tick`）：

```cpp
float VpX, VpY;                                    // 视口尺寸
float FovDeg = PC->PlayerCameraManager->GetFOVAngle();
const float WorldPerPixel = (2.f * FollowDistance * tan(deg2rad(FovDeg*0.5f))) / VpX;
FollowHorizontalOffset += (MouseX - LastMouseX) * WorldPerPixel;
FollowVerticalOffset   -= (MouseY - LastMouseY) * WorldPerPixel * (VpX / VpY);
// 限制范围：Horizontal ∈ [-150, 150]，Vertical ∈ [-120, 80] cm
```

#### 右键冲突处理（摘除 / 恢复工程 IMC 映射）

```cpp
void UCanvasToolSubsystem::SetProjectRightMouseEnabled(bool bEnabled)
{
    // 1) 用 AssetRegistry 扫描 /Game 下所有 UInputMappingContext
    // 2) 找出 Key == RightMouseButton 的映射，缓存（含 Triggers/Modifiers）
    // 3) 调 IMC->UnmapKey(Action, RightMouseButton) 摘除
    // 4) 调 RebuildProjectContexts()：RemoveMappingContext + AddMappingContext(0)
    //    强制 Enhanced Input 重建内部按键表（★ 不重建则不生效）
    // 恢复时：IMC->MapKey(...) 并把 Triggers/Modifiers 原样写回
}
```

> **安全保证**：只修改**内存中的** IMC 对象，**不保存**资产；重启编辑器即自动复原。

#### 右上角 Slate 提示层

```cpp
void UCanvasToolSubsystem::ShowHints(bool bShow)
{
    if (!bShow) { GEngine->GameViewport->RemoveViewportWidgetContent(HintsWidget.ToSharedRef()); ... return; }

    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 16);
    const TCHAR* Lines[] = {
        TEXT("BOARD OUT"), TEXT("hold RMB = draw"), TEXT("X = eraser"),
        TEXT("C = clear"), TEXT("wheel = brush size"), TEXT("B = put away"),
    };
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    for (auto* Line : Lines)
        Box->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0, 2)
           [ SNew(STextBlock).Font(Font).ColorAndOpacity(FSlateColor(FLinearColor(0.75f,1,0.75f)))
             .Text(FText::FromString(Line)) ];

    TSharedRef<SWidget> Root =
        SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 44, 22, 0))
        [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
          .BorderBackgroundColor(FLinearColor(0,0,0,0.45f)).Padding(FMargin(14,10)) [ Box ] ];

    GEngine->GameViewport->AddViewportWidgetContent(Root, 100);
    HintsWidget = Root;
}
```

> **只显示英文的原因**：工程自带中文字体 `ChillHuoFangSong_ConRegular` 是**离屏（Offline）字体**，Slate 无法渲染 → 中文显示为方块，故改用 ASCII。

---

### 4.5 `UCanvasToolLibrary` —— 编辑器工具函数（C++ 侧批量操作）

**文件**：`Public/CanvasToolLibrary.h` / `Private/CanvasToolLibrary.cpp`
**性质**：`UBlueprintFunctionLibrary`，所有函数 `WITH_EDITOR`，可在 Python 中直接调用。

```cpp
UFUNCTION(BlueprintCallable, Category = "CanvasTool")
static int32 RetargetCylinderMeshes(
    const FString& BlueprintFolder,   // 蓝图目录，如 /Game/FirstPerson/Blueprints
    const FString& LeafMeshPath,      // 新网格，如 /Game/mymodel/heye/base
    FVector RelativeLocation,         // 相对位置（默认 0,0,-33.025）
    FVector RelativeScale);           // 相对缩放（默认 1,1,0.5）
```

**实现要点**：

```cpp
FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
TArray<FAssetData> Assets; AR.Get().GetAssetsByPath(FName(*BlueprintFolder), Assets, true);

for (const FAssetData& Data : Assets)
{
    UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset());
    if (!BP || !BP->SimpleConstructionScript) continue;

    for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
    {
        if (Node->GetVariableName().ToString() != TEXT("CylinderMesh")) continue;
        UStaticMeshComponent* T = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
        T->Modify();
        T->SetStaticMesh(Leaf);
        T->SetRelativeLocation(RelativeLocation);
        T->SetRelativeScale3D(RelativeScale);
        T->SetRelativeRotation(FRotator::ZeroRotator);
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
}
```

> **为什么必须用 C++**：Python 无法访问 `UBlueprint::SimpleConstructionScript`（详见第 11 章），C++ 可以。这是本项目"批量改 251 个蓝图组件"的唯一可行路径。

---

### 4.6 画板材质三件套

| 资产 | 类型 | 关键设置 | 节点结构 |
|---|---|---|---|
| `Content/CanvasTool/M_BoardSurface` | Unlit | `TextureSampleParameter2D` 名为 **`CanvasTexture`**（SamplerType=Color）→ **Emissive** | 直接显示 RT |
| `Content/CanvasTool/M_BrushDot` | Unlit + Translucent | 参数：`Center`(Vector)、`Radius`(Scalar)、`Color`(Vector) | 圆点遮罩：`smoothstep(0.35, 1.0, dist)` → OneMinus → **Opacity**；`Color × mask` → **Emissive** |
| `Content/CanvasTool/M_BoardHighlight` | Unlit + **Opaque** + TwoSided | 纯色 `(0.10, 0.85, 1.0)` → Emissive/BaseColor | 用于选中边框（**必须不透明**，半透明圆点材质做不出边框效果） |

---

## 5. 蓝图系统

### 5.1 蓝图总览

| 蓝图 | 路径 | 作用 |
|---|---|---|
| `BP_FirstPersonCharacter` | `FirstPerson/Blueprints/` | **玩家角色**（移动/视角/取色/喷涂/画板门控） |
| `BP_FirstPersonGameMode` | 同上 | 游戏模式（DefaultPawn / PlayerController / HUD） |
| `BP_FirstPersonPlayerController` | 同上 | 玩家控制器 |
| `BP_PickHUD` | 同上 | HUD |
| `BP_Sticker_Placer` | 同上 | 贴花喷涂工具（右键喷涂的原实现） |
| **`BP_RisingCylinder` × 251** | 同上 | **色卡圆柱**（上升/悬停/文字/描述）→ 现已改为**荷叶** |
| `BP_Cotton` `BP_Mianhua1~3` | 同上 | 棉花 / 花丛 |
| `BP_DoorFrame` `BP_JumpPad` `BP_WobbleTarget` | `LevelPrototyping/` | 关卡原型交互物 |
| `BP_GlobalFoliageActor` | `Fab/Megascans/Plants/` | 程序化植被 |

### 5.2 `BP_FirstPersonCharacter`（核心角色蓝图）

**已知对外变量（供 C++ / 插件读取）**

| 变量 | 类型 | 用途 | 读取方 |
|---|---|---|---|
| `PickColor` | `FLinearColor` | 当前取到的颜色 | `ACanvasBoardActor::GetInkColor()`（反射读取） |
| `BrushSize` | `float` | 喷涂笔刷大小（原为滚轮控制） | 蓝图自身 |
| **`bCanvasBoardOpen`** | `bool` | **画板是否打开**（插件写入） | 左键取色的 Branch 条件 |
| `SpawnLocation` | `Vector` | 出生点记录 | 蓝图自身 |

**事件图关键节点（经过验证）**

| 节点 | 键 | 现状 |
|---|---|---|
| `K2Node_InputKey_2` | **LeftMouseButton** | **保留** ✔（取色入口，被 Branch 门控） |
| `K2Node_InputKey_0` | `E` | 保留 |
| `K2Node_InputKey_1` | `Q` | 保留 |
| ~~`K2Node_InputKey_5`~~ | ~~RightMouseButton~~ | **已删除**（右键喷涂，交由插件画线使用） |
| ~~`K2Node_InputAxisKeyEvent_0`~~ | ~~滚轮轴~~ | **已删除**（原用于调喷涂大小） |
| `K2Node_IfThenElse_10` | — | 已有的一个 Branch（原左键逻辑分支） |

**左键门控的接线（通过 `BlueprintGraphPin.try_create_connection` 自动完成）**

```
LeftMouseButton.Pressed ──► Branch_2.execute
bCanvasBoardOpen ─────────► Branch_2.Condition
Branch_2.else ────────────► K2Node_IfThenElse_10.execute   （原取色逻辑）
Branch_2.then ────────────► （空：画板打开时跳过取色）
```

> 完整链路：`Pressed → Branch(bCanvasBoardOpen) → false → 原取色` ｜ `true → 什么都不做`

**其它蓝图侧系统**（由他人/前序会话实现，本会话仅读取）：

- **悬停高亮 / 取色**：左键射线命中色卡 → 写入 `PickColor`
- **贴花喷涂**：`BP_Sticker_Placer` + `M_ColorDecal`，原绑定右键
- **俯视模式**（历史实现）：`bOverviewMode` / `OverviewStep` / `SavedYaw` 等变量与 `Overview_Toggle` 函数图

### 5.3 `BP_RisingCylinder` 系列（251 个）—— 色卡/荷叶系统

**组件结构**（每个蓝图一致）

```
Scene (DefaultSceneRoot)
├── CylinderMesh        ← 主体网格（现为荷叶 /Game/mymodel/heye/base）
│   └── base            ← 部分蓝图有此子组件（不统一，非必需）
├── Plane  / Plane1 / Plane2   ← 文字/图片展示面
├── ProximityTrigger    ← 悬停触发（Sphere/Box）
└── TextRenderer        ← 文字渲染组件
```

**变量**

| 变量 | 类型 | 说明 |
|---|---|---|
| `StartLocation` | Vector | 上升动画起点 |
| `EndLocation` | Vector | 上升动画终点 |
| `HoverTimerHandle` | TimerHandle | 悬停计时器 |
| `description` | Text | 该色卡的描述文本 |

**函数**：21 个（含 `Construction Script`），实现"上升 + 悬停高亮 + 文字显示 + 鼠标跟随"等。

**本次改造（251 个批量）**

| 属性 | 改后值 |
|---|---|
| `CylinderMesh` 的 StaticMesh | **`/Game/mymodel/heye/base`** |
| 相对位置 | `(0, 0, -33.025)` |
| 相对缩放 | `(1, 1, 0.5)` |
| 相对旋转 | `(0, 0, 0)`（平躺） |
| **材质 / 其它组件 / 变量 / 事件图 / 函数** | **全部未动** ✔ |

---

## 6. 输入系统

### 6.1 工程侧（Enhanced Input）

| 资产 | 说明 |
|---|---|
| `Content/Input/IMC_Default` | 默认映射上下文（Move / Look / Jump） |
| `Content/Input/IMC_MouseLook` | 鼠标视角上下文 |
| `DefaultInput.ini` | **无任何传统 ActionMapping**（全部 Enhanced Input） |

- 角色蓝图通过 `K2Node_EnhancedInputAction` 节点处理 Move / Look / Jump / MouseLook
- **遗留的传统输入节点**：`K2Node_InputKey`（LeftMouseButton / E / Q）、`K2Node_InputAxisKeyEvent`（滚轮，已删）

### 6.2 插件侧（运行时生成）

`UCanvasToolSubsystem::OnWorldBeginPlay()`：

```cpp
IA_Board  = NewObject<UInputAction>(this, TEXT("IA_CanvasBoard"));
IA_Draw   = NewObject<UInputAction>(this, TEXT("IA_CanvasDraw"));
// ... 6 个动作，ValueType = Boolean

MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CanvasTool"));
MappingContext->MapKey(IA_Board,  EKeys::B);
MappingContext->MapKey(IA_Draw,   EKeys::RightMouseButton);
MappingContext->MapKey(IA_Eraser, EKeys::X);
MappingContext->MapKey(IA_Clear,  EKeys::C);
MappingContext->MapKey(IA_Smaller, EKeys::Z);
MappingContext->MapKey(IA_Larger,  EKeys::V);
MappingContext->MapKey(IA_Larger,  EKeys::MouseScrollUp);      // 滚轮上 = 变粗
MappingContext->MapKey(IA_Smaller, EKeys::MouseScrollDown);    // 滚轮下 = 变细
MappingContext->MapKey(IA_Pick,    EKeys::LeftMouseButton);    // 选中/拖动

ULocalPlayer* LP = PC->GetLocalPlayer();
UEnhancedInputLocalPlayerSubsystem* EIS =
    LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
EIS->AddMappingContext(MappingContext, 0);
```

绑定时机：`SetTimer(0.5s, TrySetupInput, true)` 重试，直到 `Pawn->InputComponent` 是 `UEnhancedInputComponent`，再 8 次 `BindAction`。

### 6.3 完整按键表（当前最终状态）

| 键 | 功能 | 归属 |
|---|---|---|
| **B** | 拿出 / 收起画板 | 插件 |
| **鼠标右键（按住）** | 画线 | 插件 |
| **鼠标滚轮上 / 下** | 笔刷变粗 / 变细 | 插件 |
| **鼠标左键** | 选中画板 / 拖动 / 取消选中 | 插件 |
| **鼠标左键（画板关闭时）** | 取色（调用角色蓝图逻辑） | 蓝图 |
| **X** | 橡皮擦 | 插件 |
| **C** | 清空画板 | 插件 |
| **Z / V** | 笔刷变细 / 变粗（备用键） | 插件 |
| **WASD / 空格 / 鼠标移动** | 移动 / 跳跃 / 视角 | Enhanced Input（工程） |

---

## 7. 材质与纹理系统

### 7.1 湖泊/地形材质（`Content/Environment/Lake/Materials/`）

| 材质 | 使用对象 | 说明 |
|---|---|---|
| `M_LakeWater` / `MI_LakeWater` | `Lake_WaterZone`（水体） | **水务体自带材质**（本会话未修改 ✔） |
| `M_LakeSand` | `Lake_terrain_bed`（湖床） | UV 平铺已改为 **0.35** |
| `M_LakeGrass` | `Lake_terrain_land`（陆地） | UV 平铺已改 0.35；**现已被替换为 Megascans 泥土材质** |
| `M_MountainRock` | `Lake_terrain_mountains`（山） | UV 平铺已改 0.35；**现已被 `M_MountainMoss_01` 替换** |
| `M_LakeRock` / `M_LakeBark` / `M_LakeLeaves` | 石头 / 树皮 / 树叶 | — |
| **`M_MountainMoss_01`** | 山（当前） | 新材质（见 7.2） |
| `M_PondMud_01` / `M_PondGrass_01` / `M_PondHill_01` | 备用（未指定） | 程序化纹理材质（见第 9 章） |

### 7.2 `M_MountainMoss_01` —— 山体材质（含山脚高度混合）

```
① 平铺
   TextureCoordinate → Multiply(const_b = 0.35) → TextureSample(T_MountainMoss_01)

② 山脚高度混合（关键）
   WorldPosition → ComponentMask(B)          // 取世界 Z
                 → Subtract(66)              // 以水面 65.9 为基准
                 → Divide(54)                // 66 → 120 之间过渡
                 → Saturate → OneMinus
                 → Lerp(A = 贴图, B = 草地绿(0.085, 0.26, 0.040 线性), Alpha)

③ Roughness = 0.9
```

> **目的**：让山脚 66~120 m 高度范围内**渐变为草地绿**，消除"暗山 / 亮草"的硬边接缝。

### 7.3 亭子材质（`Content/Environment/Pavilion/Materials/`）

| 材质 | 部位 |
|---|---|
| `M_PavStoneC` / `M_PavStone` | 台基 / 石阶 |
| `M_PavColumnC` / `M_PavColumn` | 立柱 |
| `M_PavBeamC` / `M_PavWood` / `M_PavWood2` | 梁 / 木构 |
| `M_PavCoupletC` | 楹联 |
| `M_PavTileC` / `M_PavTile` | 瓦顶（TwoSided ✔） |
| `M_PavGoldC` / `M_PavGold` | 宝顶 / 金饰 |
| `M_PavPlaque` | 匾额 |

均为**纯色**材质（无贴图），瓦顶基色 `RGB(0.165, 0.180, 0.200)`、Roughness 0.72。

### 7.4 色卡材质库（`/Game/jinyu`）

- **1073 个材质**，命名如 `hong/81.81`、`bai/11.11`、`qing/49.49`、`gaoyu11`、`chidan11` 等
- 结构：绝大多数为**单表达式纯色**材质（一个 `Constant3Vector`）
- 默认 `MSM_DEFAULT_LIT`（受光）—— 曾临时批量改为 Unlit 后又**完整还原**
- 名称 `/Game/jinyu` 下的 `M_Cotton_*`（56 个）为棉花植株材质，多表达式

### 7.5 Megascans 泥土

`/Game/Fab/Megascans/3D/Gigantic_Tundra_Terrain_vi3hcjw/Medium/vi3hcjw_tier_2/Materials/MI_vi3hcjw`
—— 当前已指定给 **`Lake_terrain_land`**。

---

## 8. 场景与环境系统

### 8.1 关卡 `Lvl_FirstPerson`（World Partition）

- 2533 个 Actor，其中 **2533 个外部 Actor 文件**（`Content/__ExternalActors__/`）
- 世界分区关闭了传统 Level Streaming，全部按 One File Per Actor 存储

### 8.2 湖泊环境

| 元素 | Actor | 坐标 / 值 |
|---|---|---|
| 湖中心 | — | **(12600, 12600)** |
| **水面高度** | `Lake_WaterZone`（WaterBodyLake） | **Z = 65.9** |
| 湖床 | `Lake_terrain_bed` | −100 左右（约 1.66 m 深） |
| 陆地 | `Lake_terrain_land` | 环绕湖泊 |
| 远山 | `Lake_terrain_mountains` | 外围环形山脉 |
| 碰撞栅格 | `Lake_ColGrid`（多个） | `WorldGridMaterial`（碰撞用） |

### 8.3 亭子（Blender → UE）

| Actor | 说明 |
|---|---|
| `Pavilion_Platform` | 台基（含 4 段石阶） |
| `Pavilion_Columns` | 立柱（下层 4 + 上层 4） |
| `Pavilion_Beams` | 梁枋 |
| `Pavilion_Couplets` | 楹联 |
| `Pavilion_Roof` | 双层瓦顶（约 25k 三角形瓦垄） |
| `Pavilion_Finial` | 金色宝顶 |
| `Pavilion_PlaqueText` | 3D 文字匾额「集贤亭」 |

- **位置**：`(12600, 12600, 145.9)` —— **台面高出水面 80 cm**
- **建模流程**：Blender 5.2.2 headless（Python 脚本生成）→ 导出 OBJ（Y-up）→ **交换 Y/Z** → UE 导入
- **关键脚本**：`Saved/DSH/make_pavilion_blender2.py` → `Saved/DSH/rings/generated/pav4_*.obj` → `pav5_*.obj`

### 8.4 光照与大气

| Actor | 当前值 |
|---|---|
| `Lake_Sun`（DirectionalLight） | 颜色**纯白 (255,255,255)**、强度 **25**、pitch **−38°**、`AtmosphereSunLight = true` |
| `Lake_SkyLight` | 强度 **2.5**、RealTimeCapture |
| `Lake_SkyAtmosphere` | 默认 + `aerial_perspective_view_distance_scale = 0.08` |
| `PostProcessVolume` | **Unbound**、曝光补偿 **1.3**、Bloom **0.675** |
| `ExponentialHeightFog` | **不存在**（历史会话已删除） |

> 曾尝试"紫粉晚霞 / 金色夕阳 / 水墨风"三套方案，最终**全部回滚**至上述原始值（`git restore Content/`）。

### 8.5 植被与装饰

| 系统 | 资产 | 说明 |
|---|---|---|
| 高模树 | `SM_HP_Tree`（`M_HP_Tree_Trunk/Leaf`） | 52 株，11–17 m |
| 枸杞树 | `Tree_Japanese_Medlar_01_C/D` | 25 株，9–14 m |
| 石头 | `SimpleRocks/*` | 多个 |
| 风格化草花 | `StylizedGrassFlowersPack`（`SM_Grass_b` 等） | 可用 Foliage 铺满 |
| 荷叶 | `/Game/mymodel/heye/base` | **251 个色卡蓝图的新主体网格** |
| 棉花 / 花丛 | `BP_Cotton` `BP_Mianhua1~3` | — |

---

## 9. 纹理生成管线（离线 Python）

本项目的自定义纹理（淤泥 / 草地 / 山丘 / 山体苔藓）全部由**离线 Python（numpy + PIL）**程序化生成，可无损重跑。

### 9.1 工具位置

```
C:\Users\<user>\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe
```

> ⚠️ **UE 内置 Python 没有 PIL**，所以纹理生成必须在**系统/携带的 Python** 里跑，再导入 UE。

### 9.2 核心技术

| 技术 | 实现 |
|---|---|
| **可平铺值噪声** | 随机网格 → `np.pad(mode='wrap')` 补齐 → smoothstep 双线性插值 |
| **fBm 多层叠加** | 4/8/16/32/64/128/256 格 × 递减振幅 |
| **可平铺 Voronoi** | 种子点距离用**环形距离** `min(\|dx\|, 1-\|dx\|)`，保证无缝 |
| **域扭曲** | 用噪声偏移采样坐标，使细胞/斑块变成有机形状 |
| **调色板映射** | 分段 `lerp`（stops 数组） |
| **细节层** | 细颗粒 `rng.normal`、有机碎屑点、水平沉积丝纹、亮/暗矿物点 |

### 9.3 已产出纹理

| 纹理 | 尺寸 | 用途 | 配色 |
|---|---|---|---|
| `T_PondMud_01` | 512² | 池塘淤泥 | 深绿橄榄 `#0A0E09` → `#505A38` |
| `T_PondGrass_01` | 512² | 池边草地 | 深绿 `#0C160A` → `#304E2C`（与淤泥同族） |
| `T_PondHill_01` | 512² | 山丘岩石 | 深褐 `#0E0B08` → `#76644A`（Voronoi 石板 + 域扭曲） |
| `T_MountainMoss_01` | 512² | **山体（已启用）** | 深褐 + 26~38% 中亮绿苔藓（连续噪声，无块状） |

源 PNG 位于 `Saved/DSH/textures/`，已导入 `Content/Environment/Lake/Textures/`。

---

## 10. 工具链与自动化工作流

### 10.1 命令行 Python（Commandlet）

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
    "<工程路径>\cjy.uproject" `
    -run=PythonScript -Script="<临时脚本.py>" `
    -stdout -unattended -nosplash -nopause -DDC-ForceMemoryCache -NoZenAutoLaunch
```

> ⚠️ **编辑器必须完全关闭**。Commandlet 使用 **Null RHI**：**不能渲染、不能测输入、不能自检视觉结果**。

### 10.2 编译

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' cjyEditor Win64 Development `
    "-project=<工程路径>\cjy.uproject" -waitmutex -NoHotReload
```

### 10.3 已实现的自动化能力（本会话验证）

| 能力 | API |
|---|---|
| 导入纹理/模型 | `unreal.AssetImportTask` + `AssetTools.import_asset_tasks` |
| 创建材质 + 节点连线 | `unreal.MaterialEditingLibrary`（`create_material_expression` / `connect_material_expressions` / `connect_material_property`） |
| **读取/编辑蓝图图表** | `unreal.BlueprintGraphEditor`（`get_graph_editor_by_name` / `list_all_nodes` / `add_branch_node` / `add_get_member_variable_node` / `remove_nodes`） |
| **连接引脚** | `FBlueprintGraphPin.try_create_connection` / `break_pin_links` / `list_connected_pins` |
| 加变量 | `editor.add_member_variable(Name, GetBasicTypeByName("bool"))` |
| **批量改蓝图组件模板** | **仅 C++**：`UBlueprint::SimpleConstructionScript->GetAllNodes()` → `ComponentTemplate` |
| 移动 Actor（并持久化） | `Actor.modify()` → `SetActorLocation` → `save_current_level()` |
| 材质替换（并持久化） | `Actor.modify()` → `Comp.SetMaterial(0, M)` → `save_current_level()` |
| 保存资产目录 | `EditorAssetLibrary.save_directory(path, false, true)` |

### 10.4 重要持久化规则（本项目实测）

| 操作 | 是否需要 `modify()` | 是否持久化 |
|---|---|---|
| 修改**已放置 Actor** 的变换/属性 | **必须** `actor.modify()` | ✅ 调用后才持久 |
| 修改**已放置 Actor** 的材质 | **必须** `actor.modify()` + `comp.modify()` | ✅ |
| 设置 Actor 的 `AutoPossessPlayer` | 不需要 | ✅ 直接持久 |
| 修改**蓝图图表/组件模板** | — | 只能用 `BlueprintGraphEditor` / C++ |
| 修改**材质表达式** | — | `recompile_material` + `save_loaded_asset` |

---

## 11. 已知限制与踩坑记录

> 这一章是本项目最有价值的"经验库"，能避免重复踩坑。

### 11.1 蓝图相关

| 坑 | 现象 | 结论 / 解法 |
|---|---|---|
| **`bp.simple_construction_script`** | `Failed to find property` | ❌ **未暴露给 Python**，无法用 Python 拿 SCS |
| **CDO 上的组件** | `get_components_by_class` 返回空 | ❌ SCS 组件不在 CDO 上 |
| **`SubobjectDataSubsystem`** | 能 gather 到 12 个 handle，但 `to_dict()` 全空 | ❌ 句柄不透明，拿不到组件对象 |
| **`unreal.InstancedStaticMeshActor`** | `module 'unreal' has no attribute` | ❌ 未暴露；铺草请用 **Foliage 工具** |
| **批量改 251 个蓝图组件** | Python 无解 | ✅ **写 C++**（`UCanvasToolLibrary::RetargetCylinderMeshes`） |
| **`BlueprintGraphEditor`** | — | ✅ **可用**：`add_branch_node()`（无参数）/ `add_get_member_variable_node(Name, ClassPath)` / 引脚连接 `pin.try_create_connection(other)` |
| **`GetBasicTypeByName`** | 传 `"vector"` / `"rotator"` 会被**静默降级为 int** | ⚠️ 只支持 8 种基础类型（bool/byte/int/int64/real/name/string/text）；Vector/Rotator 要**从已有节点取引脚类型** |
| **输入事件节点的 `find_then_pin()`** | 返回 `None` | `K2Node_InputKey` 的输出引脚叫 **`Pressed`** / `Released`，不是 `then` |
| **`asset_class_path.asset_name`** | 返回 `Name` 类型，不能 `in` 判断 | 先 `str()` 转换 |
| **`get_static_mesh()`** | 不存在 | 用 `get_editor_property("static_mesh")` |

### 11.2 渲染 / 场景相关

| 坑 | 现象 | 结论 |
|---|---|---|
| **世界空间 `SetActorScale3D`** | 旋转平面被压扁 | 用**组件相对变换** |
| **引擎 Plane 的 UV** | 笔迹与鼠标错位（上下偏） | 平面 UV 是 **U 沿 +X、V 沿 +Y**：`UV = (Local.X/100+0.5, Local.Y/100+0.5)` |
| **`unreal.Color(r,g,b,a)`** | 实际写入颜色 **R/B 互换** | 传参时**反着给**（要 `(255,176,205)` 就传 `(205,176,255)`） |
| **画板挡住角色** | 角色无法移动 | 画板 `ECC_Pawn` / `ECC_Camera` 必须 **Ignore**，且收起时 `NoCollision` |
| **视角抖动** | 每帧回正仍抖（相机更新早一帧） | 改用 `PC->SetIgnoreLookInput(true)` 从源头禁止 |
| **`SetInputMode(GameAndUI)`** | 鼠标不出现 | 需同时 `SetShowMouseCursor(true)` |
| **Slate 中文字体** | 显示方块 | 工程自带中文字体是 **Offline 字体**，Slate 不支持 → 用 ASCII |
| **半透明圆点材质做边框** | 只看到 4 个角的小弧 | 边框必须用**不透明材质** |

### 11.3 工程 / 工具链

| 坑 | 结论 |
|---|---|
| UE 5.8 禁用了 MSVC 14.39 / 14.40–14.43 / 14.44≤35210 | 最终使用 **VS Build Tools 17.14.42（MSVC 14.50）** |
| 工程级 `Config/Windows/Windows_SDK.json` | 会让 UBT 报 `LoadJsonFile` 异常 → **删除，只用引擎配置** |
| `WindowsPlatform.bStrictConformanceMode=false` | 被拒（"cjyEditor modifies the values…"） |
| `EditorAssetLibrary.h` 在插件里 | 需加 `EditorScriptingUtilities` 模块；本项目改为**C++ 不保存、Python 保存** |
| `FBlueprintEditorUtils::CompileBlueprint` | 不存在 → 用 **`FKismetEditorUtilities::CompileBlueprint`**（`Kismet2/KismetEditorUtilities.h`） |
| UE 内置 Python 无 PIL | 用携带的 Python 生成图片，再导入 UE |
| Commandlet 输出中文乱码 | 控制台编码问题，不影响执行 |
| `Get-Member` 打印 `✔` 字符 | GBK 控制台 `UnicodeEncodeError` → 结果写 JSON 再读 |

### 11.4 撤销与回滚

| 场景 | 命令 |
|---|---|
| 回滚全部 Content 改动 | `git restore Content/` |
| 回滚单个材质 | `git restore Content/Environment/Lake/Materials/M_LakeGrass.uasset` |
| 蓝图备份 | `Saved/DSH/backup_bp_<时间戳>/`（角色/游戏模式/控制器 + 两个 IMC） |
| 回滚到干净基线 | `git checkout 29f8ed0 -- Content/`（= 合并后的完整项目快照） |

---

## 附录 A：关键资产路径与坐标

### A.1 路径速查

| 内容 | 路径 |
|---|---|
| 关卡 | `/Game/FirstPerson/Lvl_FirstPerson` |
| 角色蓝图 | `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` |
| 游戏模式 | `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode` |
| 玩家控制器 | `/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController` |
| HUD | `/Game/FirstPerson/Blueprints/BP_PickHUD` |
| 喷涂工具 | `/Game/FirstPerson/Blueprints/BP_Sticker_Placer` |
| 色卡圆柱 | `/Game/FirstPerson/Blueprints/BP_RisingCylinder`（+1~251） |
| 画板材质 | `/Game/CanvasTool/M_BoardSurface`、`M_BrushDot`、`M_BoardHighlight` |
| 湖泊材质 | `/Game/Environment/Lake/Materials/*` |
| 湖泊纹理 | `/Game/Environment/Lake/Textures/*` |
| 亭子 | `/Game/Environment/Pavilion/*` |
| 荷叶模型 | `/Game/mymodel/heye/base` |
| 色卡材质库 | `/Game/jinyu/**` |
| 输入 | `/Game/Input/IMC_Default`、`IMC_MouseLook` |
| 插件源码 | `Plugins/CanvasTool/Source/CanvasTool/**` |
| C++ 模块 | `Source/cjy/**` |
| 自动化脚本 | `Saved/DSH/*.py`（生成器、批处理） |

### A.2 世界坐标

| 锚点 | 坐标 |
|---|---|
| 湖心 | `(12600, 12600)` |
| **水面 Z** | **65.9** |
| 亭子台面 Z | **145.9** |
| **玩家出生点** | `(12600, 12600, 250)`（亭子中央，台面上方） |
| 湖床 Z | ≈ −100 |

### A.3 Python 脚本清单（`Saved/DSH/`）

| 脚本 | 作用 |
|---|---|
| `make_canvas_materials.py` | 创建画板三件套材质 |
| `make_pavilion_blender2.py` | Blender 生成亭子几何 |
| `mudtex*.py` / `grasstex.py` / `hilltex*.py` / `mountaintex*.py` | 程序化纹理生成 |
| `lights*.py` / `colorfix.py` / `golden.py` / `inkwash.py` | 灯光方案（已回滚） |
| `removenodes.py` / `lmbwire.py` / `branchwire.py` / `wire2.py` | 蓝图图表编辑 |
| `cylleaf.py` / `cylerun.py` / `mudapply.py` / `playerstart.py` | 场景/蓝图批处理 |
| `cylunlit*.py` | 色卡材质 Unlit 转换（已回滚） |

---

## 附录 B：CanvasTool 完整 API 参考

### B.1 `UCanvasToolSubsystem`（UWorldSubsystem）

| 成员 | 签名 | 说明 |
|---|---|---|
| `Board` | `UPROPERTY() TObjectPtr<ACanvasBoardActor>` | 当前画板实例 |
| `OnWorldBeginPlay` | `virtual void()` | 创建画板 + 输入 + 提示层 |
| `Deinitialize` | `virtual void()` | 恢复 IMC 映射、移除提示层、销毁画板 |
| `OnToggleBoard` | `UFUNCTION() void(const FInputActionValue&)` | B 键：拿出/收起 |
| `OnDrawStarted` / `OnDrawStopped` | 同上 | 右键按下 / 松开 |
| `OnToggleEraser` | 同上 | X 键 |
| `OnClearBoard` | 同上 | C 键 |
| `OnBrushSmaller` / `OnBrushLarger` | 同上 | 滚轮下 / 上、Z / V |
| `OnSelectBoard` / `OnSelectRelease` | 同上 | 左键按下 / 松开（选中/拖动） |
| `OnPickColour` | 同上 | （保留未绑定）调用角色蓝图取色函数 |
| `TrySetupInput` | `void()` | 重试绑定 Enhanced Input |
| `SetupBoardActor` | `void()` | 创建/重建画板 Actor |
| `AddMappingContext` | `void()` | 注册插件 IMC（并确保工程 IMC 也在） |
| `SetProjectRightMouseEnabled` | `void(bool)` | 摘除/恢复工程 IMC 的右键映射 |
| `RebuildProjectContexts` | `void()` | Remove+Add 工程 IMC，强制重建按键表 |
| `ShowHints` | `void(bool)` | 显示/隐藏右上角 Slate 提示层 |
| `Message` | `void(const FString&, const FColor&)` | 屏幕调试信息 |

### B.2 `ACanvasBoardActor`（Actor）

| 成员 | 签名 | 说明 |
|---|---|---|
| `SetupBoard()` | `void()` | 首次创建 RT/材质；重置尺寸 |
| `PaintAtUV(UV, Color, Radius)` | `void(FVector2D, FLinearColor, float)` | 在 UV 处画一个笔刷点 |
| `StrokeToUV(UV, Ink, Radius)` | `void(FVector2D, FLinearColor, float)` | 从 LastUV 插值到目标 UV |
| `ClearBoard()` | `void()` | 用 BaseColor 清空 RT |
| `GetInkColor()` | `FLinearColor() const` | 取色（InkOverride → PickColor → 深色兜底） |
| `Tick(DeltaTime)` | `virtual void(float)` | 锁定视角 / 跟随 / 鼠标射线 / 绘制 / 拖动 / 光标 / 高亮 |

### B.3 `UCanvasToolLibrary`（BlueprintFunctionLibrary）

| 函数 | 签名 | 说明 |
|---|---|---|
| `RetargetCylinderMeshes` | `static int32(const FString&, const FString&, FVector, FVector)` | 批量替换蓝图 `CylinderMesh` 组件的网格（返回改动数量，-1 失败） |

Python 调用示例：

```python
n = unreal.CanvasToolLibrary.retarget_cylinder_meshes(
    "/Game/FirstPerson/Blueprints",     # 蓝图目录
    "/Game/mymodel/heye/base",          # 新网格
    unreal.Vector(0, 0, -33.025),       # 相对位置
    unreal.Vector(1, 1, 0.5))           # 相对缩放
unreal.EditorAssetLibrary.save_directory("/Game/FirstPerson/Blueprints", False, True)
```

---

## 文档维护说明

- 本文件为**实际验证结果**汇总（不是推测）；文中所有"已验证""实测"均来自本会话对工程的直接读取与执行日志。
- 涉及数值（坐标、光照、材质参数）可能因后续编辑而变化，改动后请同步更新本文档。
- 相关日志与中间结果存放于 `Saved/DSH/rings/*.json` 与 `Saved/DSH/run_*.log`，可作为复核依据。
