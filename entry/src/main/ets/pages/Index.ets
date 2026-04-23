import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker, fileUri } from '@kit.CoreFileKit';
import { promptAction, router } from '@kit.ArkUI';
import { preferences } from '@kit.ArkData';
import testNapiImport from 'libentry.so';

/** 与 libentry NAPI 导出一致（默认导入的类型推断在部分工具链下不完整，故显式收窄）。 */
interface MrpNativeBindings {
  init: (sandboxPath: string) => number;
  start: (filename: string, extName: string) => number;
  event: (code: number, p0: number, p1: number) => number;
  timer: () => number;
  /** mr_dialog* / mr_text*（可选：旧 .so 无此导出时无系统框） */
  onMessageUi?: (callback: (op: number, viewKind: number, dialogType: number, title: string, text: string) => void) => void;
  onEditRequest: (callback: (title: string, text: string, type: number, maxSize: number) => void) => void;
  setEditResult: (text: string) => void;
  setRendererMode?: (mode: number) => void;
  getRendererMode?: () => number;
  getActiveRendererMode?: () => number;
}

/** libentry 若因缺依赖 .so 未加载成功，导入值为 undefined，须避免直接访问成员导致进程退出。 */
function getMrpNative(): MrpNativeBindings | undefined {
  return testNapiImport as MrpNativeBindings | undefined;
}

const MR_KEY_PRESS = 0;
const MR_KEY_RELEASE = 1;
const MR_MOUSE_DOWN = 2;
const MR_MOUSE_UP = 3;
const MR_MOUSE_MOVE = 12;

/** mythroad `mrporting.h` 物理键码（MR_KEY_PRESS/MR_KEY_RELEASE 的 param0），与 MR_KEY_0… 枚举顺序一致 */
const MR_KEYCODE_0 = 0;
const MR_KEYCODE_1 = 1;
const MR_KEYCODE_2 = 2;
const MR_KEYCODE_3 = 3;
const MR_KEYCODE_4 = 4;
const MR_KEYCODE_5 = 5;
const MR_KEYCODE_6 = 6;
const MR_KEYCODE_7 = 7;
const MR_KEYCODE_8 = 8;
const MR_KEYCODE_9 = 9;
const MR_KEYCODE_STAR = 10;
const MR_KEYCODE_POUND = 11;
const MR_KEYCODE_UP = 12;
const MR_KEYCODE_DOWN = 13;
const MR_KEYCODE_LEFT = 14;
const MR_KEYCODE_RIGHT = 15;
const MR_KEYCODE_POWER = 16;
const MR_KEYCODE_SOFTLEFT = 17;
const MR_KEYCODE_SOFTRIGHT = 18;
const MR_KEYCODE_SEND = 19;
const MR_KEYCODE_SELECT = 20;
const MR_KEYCODE_CLEAR = 23;
const MR_KEYCODE_CAPTURE = 26;

const MR_DIALOG_EVENT = 6;
const MR_DIALOG_KEY_OK = 0;
const MR_DIALOG_KEY_CANCEL = 1;

/** mythroad mr_dialogCreate / mr_textCreate 的 type，与 mrporting.h 一致 */
const MR_DIALOG_OK = 0;
const MR_DIALOG_OK_CANCEL = 1;
const MR_DIALOG_CANCEL = 2;

const MR_EDIT_NUMERIC = 1;
const MR_EDIT_PASSWORD = 2;

const SCREEN_WIDTH = 240;
const SCREEN_HEIGHT = 320;

/** 功能机外壳 / 键盘区配色（与 mythroad 内屏无关，仅宿主 UI） */
const PHONE_SHELL = '#2c3139';
const PHONE_SHELL_EDGE = '#1e2228';
const PHONE_KEYPAD_BG = '#23272e';
const KEY_FACE = '#3d4656';
const KEY_FACE_BORDER = '#1a1e26';
const KEY_LABEL = '#EEF2F7';
const SCREEN_BEZEL = '#0a0a0c';
const ACCENT_IMPORT = '#0a7a72';
/** 机身右侧侧键条宽度基准 */
const SIDE_KEY_W = 15;
const SIDE_KEY_H = 38;

const RENDERER_AUTO = 0;
const RENDERER_VULKAN = 1;
const RENDERER_GLES = 2;
const RENDERER_CPU = 3;
const RENDERER_PREFS_KEY = 'mrp_renderer_mode';
const RENDERER_PREFS_STORE = 'mrp_prefs';

function mrpFileNameFromUri(uri: string): string {
  const noQuery = uri.split('?')[0];
  let segment = noQuery.substring(noQuery.lastIndexOf('/') + 1);
  try {
    segment = decodeURIComponent(segment);
  } catch (_e) {
  }
  return segment;
}

/**
 * 将 DocumentPicker 返回的 URI 导入 mythroad。
 * 依据 DevEco CodeGenie / 官方文档：用 FileUri 转成可 stat 的路径；整文件拷贝优先 fs.copyFileSync(fd, fd)（云上传示例），
 * 避免 read/write 与 stat(fd) 在 Picker 资源上的不一致导致「大小校验失败」。
 */
async function copyPickerUriToMythroad(srcUri: string, sandboxPath: string): Promise<string> {
  const mythroadDir = `${sandboxPath}/mythroad`;
  if (!fs.accessSync(mythroadDir)) {
    fs.mkdirSync(mythroadDir, true);
  }
  let baseName = mrpFileNameFromUri(srcUri);
  if (baseName.length === 0 || !baseName.toLowerCase().endsWith('.mrp')) {
    baseName = `import_${Date.now().toString()}.mrp`;
  }
  const destPath = `${mythroadDir}/${baseName}`;

  let srcPath: string = srcUri;
  try {
    srcPath = new fileUri.FileUri(srcUri).path;
  } catch (_e) {
    srcPath = srcUri;
  }

  let srcSize = 0;
  try {
    srcSize = fs.statSync(srcPath).size;
  } catch (_e) {
    try {
      srcSize = fs.statSync(srcUri).size;
      srcPath = srcUri;
    } catch (_e2) {
      throw new Error('无法 stat 源文件（请确认 Picker 返回的 URI 有效）');
    }
  }

  const srcFile = fs.openSync(srcPath, fs.OpenMode.READ_ONLY);
  const dstFile = fs.openSync(destPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
  try {
    fs.copyFileSync(srcFile.fd, dstFile.fd);
    try {
      fs.fsyncSync(dstFile.fd);
    } catch (_e) {
      /* 部分版本无 fsyncSync */
    }
  } catch (e) {
    try {
      fs.closeSync(srcFile);
    } catch (_e2) {
    }
    try {
      fs.closeSync(dstFile);
    } catch (_e2) {
    }
    try {
      fs.unlinkSync(destPath);
    } catch (_e3) {
    }
    throw e instanceof Error ? e : new Error(String(e));
  }
  fs.closeSync(srcFile);
  fs.closeSync(dstFile);

  const outSz = fs.statSync(destPath).size;
  if (outSz !== srcSize) {
    try {
      fs.unlinkSync(destPath);
    } catch (_e) {
    }
    throw new Error(`目标大小校验失败：${outSz} != ${srcSize}`);
  }

  return destPath;
}

function mrpUnicodeCharCount(s: string): number {
  let n = 0;
  for (let i = 0; i < s.length;) {
    const cp = s.codePointAt(i);
    if (cp === undefined) {
      break;
    }
    n++;
    i += cp > 0xffff ? 2 : 1;
  }
  return n;
}

@CustomDialog
struct MrpEditDialog {
  /** 须由父组件传入 `CustomDialogController.open()` 的同一实例，否则无法 close。 */
  controller?: CustomDialogController;
  /** 每次打开递增，用于 .key 强制重建输入框，避免回显仍用旧内部 buffer。 */
  @Prop gen: number = 0;
  @Link title: string;
  @Link text: string;
  @Link maxSize: number;
  @Link editType: number;
  onConfirm: () => void = (): void => {
  };
  onCancel: () => void = (): void => {
  };

  build() {
    Column({ space: 12 }) {
      Text(this.title)
        .fontSize(18)
        .fontWeight(FontWeight.Medium)
        .fontFamily('HarmonyOS Sans')
        .width('100%')
      if (this.editType === MR_EDIT_PASSWORD) {
        TextInput({ text: $$this.text })
          .key('mrp_pwd_' + this.gen.toString())
          .type(InputType.Password)
          .fontFamily('HarmonyOS Sans')
          .width('100%')
      } else if (this.editType === MR_EDIT_NUMERIC) {
        TextInput({ text: $$this.text })
          .key('mrp_num_' + this.gen.toString())
          .type(InputType.Number)
          .fontFamily('HarmonyOS Sans')
          .width('100%')
          .onChange((v: string) => {
            let filtered = '';
            for (let i = 0; i < v.length; i++) {
              const ch = v.charAt(i);
              if (ch >= '0' && ch <= '9') {
                filtered += ch;
              }
            }
            this.text = filtered;
          })
      } else {
        TextArea({ text: $$this.text })
          .key('mrp_txt_' + this.gen.toString())
          .fontFamily('HarmonyOS Sans')
          .width('100%')
          .height(140)
      }
      Text(`${mrpUnicodeCharCount(this.text)}/${this.maxSize}`)
        .fontSize(12)
        .fontFamily('HarmonyOS Sans')
        .fontColor(mrpUnicodeCharCount(this.text) > this.maxSize ? Color.Red : Color.Gray)
      Row({ space: 16 }) {
        Button('取消')
          .layoutWeight(1)
          .onClick((): void => {
            this.onCancel();
            if (this.controller !== undefined) {
              this.controller.close();
            }
          })
        Button('确定')
          .layoutWeight(1)
          .type(ButtonType.Capsule)
          .onClick((): void => {
            if (mrpUnicodeCharCount(this.text) > this.maxSize) {
              return;
            }
            this.onConfirm();
            if (this.controller !== undefined) {
              this.controller.close();
            }
          })
      }
      .width('100%')
    }
    .padding(16)
    .width('90%')
  }
}

@Entry
@Component
struct Index {
  @State editDlgTitle: string = '';
  @State editDlgText: string = '';
  @State editDlgMax: number = 256;
  @State editDlgType: number = 0;
  @State editDlgGen: number = 0;
  private xComponentWidth: number = 240;
  private xComponentHeight: number = 320;
  private initialized: boolean = false;
  private editDialogController: CustomDialogController | null = null;
  private editDialogControllerReady: boolean = false;

  @State msgDlgTitle: string = '';
  @State msgDlgBody: string = '';
  @State msgDlgType: number = MR_DIALOG_OK;
  @State msgDlgIsText: boolean = false;
  @State msgDlgGen: number = 0;
  private msgDialogController: CustomDialogController | null = null;
  private msgDialogControllerReady: boolean = false;

  @State rendererSelectedMode: number = RENDERER_AUTO;
  @State rendererActiveMode: number = 0;
  @State pressedKey: number = -1;
  private rendererDialogController: CustomDialogController | null = null;

  aboutToDisappear(): void {
    this.initialized = false;
  }
  private rendererDialogControllerReady: boolean = false;
  private keyRepeatTimer: number = -1;

  /** $变量 仅在本组件的 @Builder 内合法；不能直接写在 CustomDialogController 的匿名 builder 里。 */
  @Builder
  MrpEditDialogBuilder() {
    MrpEditDialog({
      controller: this.editDialogController as CustomDialogController,
      gen: this.editDlgGen,
      title: $editDlgTitle,
      text: $editDlgText,
      maxSize: $editDlgMax,
      editType: $editDlgType,
      onConfirm: (): void => {
        getMrpNative()?.setEditResult(this.editDlgText);
        getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_OK, 0);
      },
      onCancel: (): void => {
        getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
      },
    });
  }

  @Builder
  MrpMessageDialogBuilder() {
    Column({ space: 12 }) {
      Text(this.msgDlgTitle)
        .fontSize(18)
        .fontWeight(FontWeight.Medium)
        .fontFamily('HarmonyOS Sans')
        .width('100%')
      Scroll() {
        Text(this.msgDlgBody)
          .fontSize(14)
          .fontFamily('HarmonyOS Sans')
          .width('100%')
      }
      .key('mr_msg_scroll_' + this.msgDlgGen.toString())
      .width('100%')
      .height(this.msgDlgIsText ? 280 : 120)
      .scrollable(ScrollDirection.Vertical)
      .scrollBar(BarState.Auto)

      if (this.msgDlgType === MR_DIALOG_CANCEL) {
        Button('返回')
          .width('100%')
          .type(ButtonType.Capsule)
          .onClick((): void => {
            getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
            if (this.msgDialogController != null) {
              this.msgDialogController.close();
            }
          })
      } else {
        Row({ space: 16 }) {
          if (this.msgDlgType === MR_DIALOG_OK_CANCEL) {
            Button('取消')
              .layoutWeight(1)
              .onClick((): void => {
                getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
                if (this.msgDialogController != null) {
                  this.msgDialogController.close();
                }
              })
          }
          Button('确定')
            .layoutWeight(1)
            .type(ButtonType.Capsule)
            .onClick((): void => {
              getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_OK, 0);
              if (this.msgDialogController != null) {
                this.msgDialogController.close();
              }
            })
        }
        .width('100%')
      }
    }
    .padding(16)
    .width('92%')
   }
   @Builder
   MrpRendererDialogBuilder() {
     Column({ space: 12 }) {
       Text('渲染模式')
         .fontSize(18)
         .fontWeight(FontWeight.Medium)
         .fontFamily('HarmonyOS Sans')
         .width('100%')
       Column({ space: 8 }) {
         this.RendererRadioRow('自动（推荐）', RENDERER_AUTO)
         this.RendererRadioRow('Vulkan', RENDERER_VULKAN)
         this.RendererRadioRow('GLES', RENDERER_GLES)
         this.RendererRadioRow('CPU', RENDERER_CPU)
       }
       .width('100%')
       Text(this.activeRendererLabel())
         .fontSize(12)
         .fontColor('#8a9ab5')
         .fontFamily('HarmonyOS Sans')
         .width('100%')
         .textAlign(TextAlign.Center)
     }
     .padding(16)
     .width('90%')
   }
   @Builder
   RendererRadioRow(label: string, mode: number) {
     Row({ space: 8 }) {
       Radio({ value: label, group: 'rendererModeGroup' })
         .checked(this.rendererSelectedMode === mode)
         .onChange((isChecked: boolean) => {
           if (isChecked) {
             this.rendererSelectedMode = mode;
             const napi = getMrpNative();
             if (napi) {
               napi.setRendererMode?.(mode);
             }
             this.saveRendererPrefs(mode);
           }
         })
         .width(20)
         .height(20)
       Text(label)
         .fontSize(15)
         .fontFamily('HarmonyOS Sans')
     }
     .width('100%')
   }
   @Styles sideBtn(){
    .width(SIDE_KEY_W)
    .height(SIDE_KEY_H)
    .padding({ left: 0, right: 0, top: 3, bottom: 3 })
    .borderRadius({ topLeft: 5, bottomLeft: 5, topRight: 11, bottomRight: 11 })
    .border({ width: 0.5, color: '#080a0d' })
    .shadow({ radius: 5, color: '#60000000', offsetX: 3, offsetY: 0 })
  }

  /** 机身右侧细长侧键（无背景条，整体外移贴外框） */
  @Builder
  MrpSideKey(label: string, key: number) {
    Button() {
      Text(label)
        .fontSize(8)
        .lineHeight(10)
        .fontColor('#d0d6e0')
        .fontWeight(FontWeight.Medium)
        .textAlign(TextAlign.Center)
    }
    .sideBtn()
    .linearGradient({
      angle: 90,
      colors: [['#1f252d', 0], ['#3a424d', 0.45], ['#252b33', 1]]
    })
    .border({ width: 0.5, color: '#080a0d' })
    .shadow({ radius: 5, color: '#60000000', offsetX: 3, offsetY: 0 })
    .type(ButtonType.Normal)
    .onClick((): void => {
      this.mrpKeyTap(key);
    })
  }

  @Builder
  MrpSideImportKey() {
    Button() {
      Text('导\n入')
        .fontSize(8)
        .lineHeight(10)
        .fontColor('#ecf4f2')
        .fontWeight(FontWeight.Medium)
        .textAlign(TextAlign.Center)
    }
    .sideBtn()
    .linearGradient({
      angle: 90,
      colors: [['#065850', 0], [ACCENT_IMPORT, 0.5], ['#054a45', 1]]
    })
    .border({ width: 0.5, color: '#033330' })
    .shadow({ radius: 5, color: '#50000000', offsetX: 3, offsetY: 0 })
    .type(ButtonType.Normal)
    .onClick((): void => {
      void this.pickMrpIntoMythroad();
    })
  }

  @Builder
  MrpSideAboutKey() {
    Button() {
      Text('关\n于')
        .fontSize(8)
        .lineHeight(10)
        .fontColor('#ecf4f2')
        .fontWeight(FontWeight.Medium)
        .textAlign(TextAlign.Center)
    }
    .sideBtn()
    .linearGradient({
      angle: 90,
      colors: [['#065850', 0], [ACCENT_IMPORT, 0.5], ['#054a45', 1]]
    })
    .border({ width: 0.5, color: '#033330' })
    .shadow({ radius: 5, color: '#50000000', offsetX: 3, offsetY: 0 })
    .type(ButtonType.Normal)
    .onClick((): void => {
      router.pushUrl({
        url: 'pages/OpenSourceNotices'
      });
    })
  }

  @Builder
  MrpSideRendererKey() {
    Button() {
      Text('渲\n染')
        .fontSize(8)
        .lineHeight(10)
        .fontColor('#ecf4f2')
        .fontWeight(FontWeight.Medium)
        .textAlign(TextAlign.Center)
    }
    .sideBtn()
    .linearGradient({
      angle: 90,
      colors: [['#065850', 0], [ACCENT_IMPORT, 0.5], ['#054a45', 1]]
    })
    .border({ width: 0.5, color: '#033330' })
    .shadow({ radius: 5, color: '#50000000', offsetX: 3, offsetY: 0 })
    .type(ButtonType.Normal)
    .onClick((): void => {
      if (this.rendererDialogController !== null) {
        this.rendererDialogController.open();
      }
    })
  }

  /** 三键一行均分；h/fs 行间距 rowGap、圆角 br 可调 */
  @Builder
  MrpPadRow3(
    l1: string,
    k1: number,
    l2: string,
    k2: number,
    l3: string,
    k3: number,
    h: number = 36,
    fs: number = 15,
    rowGap: number = 12,
    br: number = 14
  ) {
    Row({ space: rowGap }) {
      Button(l1)
        .layoutWeight(1)
        .height(h)
        .fontSize(fs)
        .fontColor(KEY_LABEL)
        .fontWeight(FontWeight.Medium)
        .backgroundColor(this.pressedKey === k1 ? '#5a6a82' : KEY_FACE)
        .borderRadius(br)
        .border({ width: 0.5, color: KEY_FACE_BORDER })
        .shadow({ radius: 4, color: '#40000000', offsetX: 0, offsetY: 2 })
        .type(ButtonType.Normal)
        .onTouch((e: TouchEvent): void => {
          if (e.type === TouchType.Down) {
            this.keyPressed(k1);
          } else if (e.type === TouchType.Up || e.type === TouchType.Cancel) {
            this.keyReleased(k1);
          }
        })
      Button(l2)
        .layoutWeight(1)
        .height(h)
        .fontSize(fs)
        .fontColor(KEY_LABEL)
        .fontWeight(FontWeight.Medium)
        .backgroundColor(this.pressedKey === k2 ? '#5a6a82' : KEY_FACE)
        .borderRadius(br)
        .border({ width: 0.5, color: KEY_FACE_BORDER })
        .shadow({ radius: 4, color: '#40000000', offsetX: 0, offsetY: 2 })
        .type(ButtonType.Normal)
        .onTouch((e: TouchEvent): void => {
          if (e.type === TouchType.Down) {
            this.keyPressed(k2);
          } else if (e.type === TouchType.Up || e.type === TouchType.Cancel) {
            this.keyReleased(k2);
          }
        })
      Button(l3)
        .layoutWeight(1)
        .height(h)
        .fontSize(fs)
        .fontColor(KEY_LABEL)
        .fontWeight(FontWeight.Medium)
        .backgroundColor(this.pressedKey === k3 ? '#5a6a82' : KEY_FACE)
        .borderRadius(br)
        .border({ width: 0.5, color: KEY_FACE_BORDER })
        .shadow({ radius: 4, color: '#40000000', offsetX: 0, offsetY: 2 })
        .type(ButtonType.Normal)
        .onTouch((e: TouchEvent): void => {
          if (e.type === TouchType.Down) {
            this.keyPressed(k3);
          } else if (e.type === TouchType.Up || e.type === TouchType.Cancel) {
            this.keyReleased(k3);
          }
        })
    }
    .width('100%')
   }

   private activeRendererLabel(): string {
     const napi = getMrpNative();
     if (napi) {
       this.rendererActiveMode = napi.getActiveRendererMode?.() ?? 0;
     }
     const names: string[] = ['未初始化', 'Vulkan', 'GLES', 'CPU'];
     const label = `当前: ${names[this.rendererActiveMode] || '?'}`;
     if (this.rendererSelectedMode !== RENDERER_AUTO && this.rendererSelectedMode !== this.rendererActiveMode && this.rendererActiveMode > 0) {
       const preferred = names[this.rendererSelectedMode] || '?';
       return `${label}（${preferred} 不可用）`;
     }
     return label;
   }

   private saveRendererPrefs(mode: number): void {
     try {
       const ctx = getContext(this) as common.UIAbilityContext;
       const pf = preferences.getPreferencesSync(ctx, { name: RENDERER_PREFS_STORE });
       pf.putSync(RENDERER_PREFS_KEY, mode);
       pf.flush();
     } catch (_e) {
     }
   }

   private loadRendererPrefs(): number {
     try {
       const ctx = getContext(this) as common.UIAbilityContext;
       const pf = preferences.getPreferencesSync(ctx, { name: RENDERER_PREFS_STORE });
       const val = pf.getSync(RENDERER_PREFS_KEY, RENDERER_AUTO) as number;
       return val;
     } catch (_e) {
       return RENDERER_AUTO;
     }
   }

   aboutToAppear(): void {
    if (!this.editDialogControllerReady) {
      this.editDialogControllerReady = true;
      this.editDialogController = new CustomDialogController({
        builder: (): void => {
          this.MrpEditDialogBuilder();
        },
        cancel: (): void => {
          getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
        },
        autoCancel: true,
        alignment: DialogAlignment.Center,
        cornerRadius: 8,
        customStyle: false,
      });
    }
    if (!this.msgDialogControllerReady) {
      this.msgDialogControllerReady = true;
      this.msgDialogController = new CustomDialogController({
        builder: (): void => {
          this.MrpMessageDialogBuilder();
        },
        cancel: (): void => {
          getMrpNative()?.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
        },
        autoCancel: true,
        alignment: DialogAlignment.Center,
        cornerRadius: 8,
        customStyle: false,
      });
    }
    if (!this.rendererDialogControllerReady) {
      this.rendererDialogControllerReady = true;
      this.rendererDialogController = new CustomDialogController({
        builder: (): void => {
          this.MrpRendererDialogBuilder();
        },
        autoCancel: true,
        alignment: DialogAlignment.Center,
        cornerRadius: 8,
        customStyle: false,
      });
    }
    const savedMode = this.loadRendererPrefs();
    this.rendererSelectedMode = savedMode;
    const napi = getMrpNative();
    if (napi) {
      napi.setRendererMode?.(savedMode);
    }
  }

  build() {
    Column() {
      Stack({ alignContent: Alignment.Center }) {
        Column({ space: 12 }) {
          this.MrpSideKey('清\n除', MR_KEYCODE_CLEAR)
          this.MrpSideKey('拍\n照', MR_KEYCODE_CAPTURE)
          this.MrpSideImportKey()
          this.MrpSideAboutKey()
          this.MrpSideRendererKey()
        }
        .width("100%")
        .padding({bottom:150})
        .alignItems(HorizontalAlign.End)
        Column() {
          Row()
            .width(44)
            .height(5)
            .borderRadius(3)
            .backgroundColor('#3a3f48')
            .margin({ top: 4, bottom: 4 })

          Column() {
            Column() {
              Row() {
                XComponent({
                  id: 'mrpXComponent',
                  type: XComponentType.SURFACE,
                  libraryname: 'entry'
                })
                  .width(this.xComponentWidth)
                  .height(this.xComponentHeight)
                  .onLoad(() => {
                    this.initEmulator();
                  })
                  .onTouch((event: TouchEvent) => {
                    this.handleTouch(event);
                  })
              }
              .justifyContent(FlexAlign.Center)
              .width('100%')
              .layoutWeight(1)
            }
            .width('100%')
            .layoutWeight(1)
            .padding(6)
            .backgroundColor(SCREEN_BEZEL)
            .borderRadius(12)
            .border({ width: 1, color: '#000000' })

            Stack({ alignContent: Alignment.TopStart }) {
              Column() {
                Text('MYTHROAD')
                  .fontSize(9)
                  .fontColor('#5c6570')
                  .letterSpacing(2)
                  .margin({ bottom: 4 })
                  .width('100%')
                  .textAlign(TextAlign.Center)

                Scroll() {
                  Column({ space: 10 }) {
                    this.MrpPadRow3('左软', MR_KEYCODE_SOFTLEFT, '↑', MR_KEYCODE_UP, '右软', MR_KEYCODE_SOFTRIGHT, 38, 12, 14, 16)
                    this.MrpPadRow3('←', MR_KEYCODE_LEFT, 'OK', MR_KEYCODE_SELECT, '→', MR_KEYCODE_RIGHT, 38, 15, 14, 16)
                    this.MrpPadRow3('接听', MR_KEYCODE_SEND, '↓', MR_KEYCODE_DOWN, '挂断', MR_KEYCODE_POWER, 38, 12, 14, 16)

                    this.MrpPadRow3('1', MR_KEYCODE_1, '2', MR_KEYCODE_2, '3', MR_KEYCODE_3, 36, 16, 12, 14)
                    this.MrpPadRow3('4', MR_KEYCODE_4, '5', MR_KEYCODE_5, '6', MR_KEYCODE_6, 36, 16, 12, 14)
                    this.MrpPadRow3('7', MR_KEYCODE_7, '8', MR_KEYCODE_8, '9', MR_KEYCODE_9, 36, 16, 12, 14)
                    this.MrpPadRow3('*', MR_KEYCODE_STAR, '0', MR_KEYCODE_0, '#', MR_KEYCODE_POUND, 36, 16, 12, 14)
                  }
                  .width('100%')
                  .padding({ left: 10, right: 10, bottom: 12 })
                }
                .layoutWeight(1)
                .scrollBar(BarState.Off)
                .edgeEffect(EdgeEffect.Spring)
              }
              .width('100%')
              .height('100%')
              .padding({ top: 2, left: 2, right: 2, bottom: 6 })
              .backgroundColor(PHONE_KEYPAD_BG)
              .borderRadius({ topLeft: 14, topRight: 14, bottomLeft: 20, bottomRight: 20 })
            }
            .layoutWeight(1)
            .width('100%')
            .clip(false)
          }
          .layoutWeight(1)
          .width('100%')
          .clip(false)

          Blank()
            .height(4)
        }
        .constraintSize({ maxWidth: 400 })
        .width('90%')
        .height('98%')
        .padding({
          top: 10,
          bottom: 10,
          left: 12,
          right: 12
        })
        .borderRadius(36)
        .backgroundColor(PHONE_SHELL)
        .border({ width: 1, color: PHONE_SHELL_EDGE })
        .shadow({ radius: 28, color: '#55000000', offsetX: 0, offsetY: 12 })
        .justifyContent(FlexAlign.Start)
        .clip(false)
      }
      .width('100%')
      .layoutWeight(1)
    }
    .width('100%')
    .height('100%')
    .linearGradient({
      angle: 165,
      colors: [['#12151c', 0.0], ['#1e232d', 0.45], ['#0d1015', 1.0]]
    })
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }

  private initEmulator() {
    if (this.initialized) {
      return;
    }

    let rawfilesReady = AppStorage.get<boolean>('rawfilesReady') || false;
    if (!rawfilesReady) {
      setTimeout((): void => {
        this.initEmulator();
      }, 100);
      return;
    }

    let sandboxPath = AppStorage.get<string>('sandboxPath') || '/data/storage/el2/base';

    const napi = getMrpNative();
    if (!napi) {
      console.error('libentry.so 未加载（常见原因：HAP 中缺少 libfluidsynth.so.3 等依赖 .so）');
      this.initialized = true;
      return;
    }

    if (napi.onMessageUi !== undefined) {
      napi.onMessageUi((op: number, viewKind: number, dialogType: number, title: string, text: string): void => {
        if (op === 0) {
          if (this.msgDialogController !== null) {
            try {
              this.msgDialogController.close();
            } catch (_e) {
            }
          }
          return;
        }
        if (op === 1 || op === 2) {
          try {
            if (this.editDialogController !== null) {
              this.editDialogController.close();
            }
          } catch (_e) {
          }
        }
        if (op === 1) {
          this.msgDlgGen++;
          this.msgDlgTitle = title;
          this.msgDlgBody = text;
          if (dialogType >= 0) {
            this.msgDlgType = dialogType;
          }
          this.msgDlgIsText = viewKind === 2;
          if (this.msgDialogController !== null) {
            this.msgDialogController.open();
          }
          return;
        }
        if (op === 2) {
          this.msgDlgTitle = title;
          this.msgDlgBody = text;
          if (dialogType >= 0) {
            this.msgDlgType = dialogType;
          }
          this.msgDlgIsText = viewKind === 2;
          return;
        }
      });
    } else {
      console.warn('libentry 无 onMessageUi：mr_dialog/mr_text 将无宿主 UI');
    }

    napi.onEditRequest((title: string, text: string, type: number, maxSize: number): void => {
      try {
        if (this.msgDialogController !== null) {
          this.msgDialogController.close();
        }
      } catch (_e) {
      }
      this.editDlgGen++;
      this.editDlgTitle = title;
      this.editDlgText = text;
      this.editDlgMax = maxSize > 0 ? maxSize : 256;
      this.editDlgType = type;
      if (this.editDialogController !== null) {
        this.editDialogController.open();
      }
    });

    // MRP 的 timerStart(t) / timerStop 已在 libentry（napi_init）内用 native 线程 + steady_clock 等待，
    // 再在 JS 线程调用 timer()；此处勿再用 setTimeout，否则会比 PC/SDL 模拟器明显偏慢。

    let ret = napi.init(sandboxPath);
    console.info('vmrp init result: ' + ret);

    if (ret === 0) {
      const params = router.getParams() as Record<string, string> | null;
      const mrpFile = params?.mrp || 'dsm_gm.mrp';
      let startRet = napi.start(mrpFile, 'start.mr');
      console.info('vmrp start result: ' + startRet);
    }

    this.initialized = true;
  }

  private isRepeatableKey(key: number): boolean {
    return key === MR_KEYCODE_UP || key === MR_KEYCODE_DOWN ||
      key === MR_KEYCODE_LEFT || key === MR_KEYCODE_RIGHT;
  }

  private cancelKeyRepeat(): void {
    if (this.keyRepeatTimer >= 0) {
      clearInterval(this.keyRepeatTimer);
      this.keyRepeatTimer = -1;
    }
  }

  private keyPressed(key: number): void {
    const n = getMrpNative();
    if (!n) {
      return;
    }
    this.cancelKeyRepeat();
    this.pressedKey = key;
    n.event(MR_KEY_PRESS, key, 0);

    if (this.isRepeatableKey(key)) {
      let count = 0;
      this.keyRepeatTimer = setInterval((): void => {
        count++;
        if (count < 4) {
          return;
        }
        const napi = getMrpNative();
        if (!napi) {
          this.cancelKeyRepeat();
          return;
        }
        napi.event(MR_KEY_RELEASE, key, 0);
        napi.event(MR_KEY_PRESS, key, 0);
      }, 60);
    }
  }

  private keyReleased(key: number): void {
    this.cancelKeyRepeat();
    if (this.pressedKey === key) {
      this.pressedKey = -1;
    }
    getMrpNative()?.event(MR_KEY_RELEASE, key, 0);
  }

  /** 短按：按下后 holdMs 毫秒发抬起，与 mythroad 键程一致 */
  private mrpKeyTap(key: number, holdMs: number = 90): void {
    const n = getMrpNative();
    if (!n) {
      return;
    }
    n.event(MR_KEY_PRESS, key, 0);
    setTimeout((): void => {
      getMrpNative()?.event(MR_KEY_RELEASE, key, 0);
    }, holdMs);
  }

  private handleTouch(event: TouchEvent) {
    let touch = event.touches[0];
    let scaleX = SCREEN_WIDTH / this.xComponentWidth;
    let scaleY = SCREEN_HEIGHT / this.xComponentHeight;
    let mx = Math.floor(touch.x * scaleX);
    let my = Math.floor(touch.y * scaleY);

    if (mx < 0) {
      mx = 0;
    }
    if (my < 0) {
      my = 0;
    }
    if (mx >= SCREEN_WIDTH) {
      mx = SCREEN_WIDTH - 1;
    }
    if (my >= SCREEN_HEIGHT) {
      my = SCREEN_HEIGHT - 1;
    }

    const napi = getMrpNative();
    if (!napi) {
      return;
    }
    switch (event.type) {
      case TouchType.Down:
        napi.event(MR_MOUSE_DOWN, mx, my);
        break;
      case TouchType.Move:
        napi.event(MR_MOUSE_MOVE, mx, my);
        break;
      case TouchType.Up:
        napi.event(MR_MOUSE_UP, mx, my);
        break;
    }
  }

  onBackPress(): boolean {
    router.back();
    return true;
  }

  private async pickMrpIntoMythroad(): Promise<void> {
    const uiContext = getContext(this) as common.UIAbilityContext;
    const sandboxPath = AppStorage.get<string>('sandboxPath') || uiContext.filesDir;
    const documentPicker = new picker.DocumentViewPicker(uiContext);
    const selectOptions: picker.DocumentSelectOptions = {
      maxSelectNumber: 1,
      fileSuffixFilters: ['.mrp'],
    };
    try {
      const uris = await documentPicker.select(selectOptions);
      if (uris === null || uris === undefined || uris.length === 0) {
        return;
      }
      const destPath = await copyPickerUriToMythroad(uris[0], sandboxPath);
      const name = destPath.substring(destPath.lastIndexOf('/') + 1);
      const sz = fs.statSync(destPath).size;
      promptAction.showToast({
        message: `已导入 mythroad/${name}（${sz} 字节）`,
        duration: 2500,
      });
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      promptAction.showToast({
        message: `导入失败：${msg}`,
        duration: 3500,
      });
    }
  }
}
