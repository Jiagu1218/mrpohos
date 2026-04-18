import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker, fileUri } from '@kit.CoreFileKit';
import { promptAction } from '@kit.ArkUI';
import testNapiImport from 'libentry.so';

/** 与 libentry NAPI 导出一致（默认导入的类型推断在部分工具链下不完整，故显式收窄）。 */
interface MrpNativeBindings {
  init: (sandboxPath: string) => number;
  start: (filename: string, extName: string) => number;
  event: (code: number, p0: number, p1: number) => number;
  timer: () => number;
  onEditRequest: (callback: (title: string, text: string, type: number, maxSize: number) => void) => void;
  setEditResult: (text: string) => void;
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
const MR_KEYCODE_VOLUME_UP = 21;
const MR_KEYCODE_VOLUME_DOWN = 22;
const MR_KEYCODE_CLEAR = 23;
const MR_KEYCODE_A = 24;
const MR_KEYCODE_B = 25;
const MR_KEYCODE_CAPTURE = 26;

const MR_DIALOG_EVENT = 6;
const MR_DIALOG_KEY_OK = 0;
const MR_DIALOG_KEY_CANCEL = 1;

const MR_EDIT_NUMERIC = 1;
const MR_EDIT_PASSWORD = 2;

const SCREEN_WIDTH = 240;
const SCREEN_HEIGHT = 320;

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

  aboutToAppear(): void {
    if (this.editDialogControllerReady) {
      return;
    }
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

  build() {
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

      Scroll() {
        Column({ space: 4 }) {
          Row() {
            Button('←')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_LEFT);
              })
            Button('↑')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_UP);
              })
            Button('↓')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_DOWN);
              })
            Button('→')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_RIGHT);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)

          Row() {
            Button('左软')
              .width(72)
              .height(36)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_SOFTLEFT);
              })
            Button('确认')
              .width(72)
              .height(36)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_SELECT);
              })
            Button('右软')
              .width(72)
              .height(36)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_SOFTRIGHT);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)

          Row() {
            Button('*')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_STAR);
              })
            Button('0')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_0);
              })
            Button('#')
              .width(56)
              .height(36)
              .fontSize(14)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_POUND);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)

          Row() {
            Button('1')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_1);
              })
            Button('2')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_2);
              })
            Button('3')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_3);
              })
            Button('4')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_4);
              })
            Button('5')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_5);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)

          Row() {
            Button('6')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_6);
              })
            Button('7')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_7);
              })
            Button('8')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_8);
              })
            Button('9')
              .width(48)
              .height(34)
              .fontSize(13)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_9);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceEvenly)

          Row() {
            Button('挂机')
              .layoutWeight(1)
              .height(34)
              .fontSize(11)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_POWER);
              })
            Button('接听')
              .layoutWeight(1)
              .height(34)
              .fontSize(11)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_SEND);
              })
            Button('清除')
              .layoutWeight(1)
              .height(34)
              .fontSize(11)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_CLEAR);
              })
            Button('拍照')
              .layoutWeight(1)
              .height(34)
              .fontSize(11)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_CAPTURE);
              })
          }
          .width('100%')

          Row() {
            Button('音量+')
              .layoutWeight(1)
              .height(34)
              .fontSize(12)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_VOLUME_UP);
              })
            Button('音量−')
              .layoutWeight(1)
              .height(34)
              .fontSize(12)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_VOLUME_DOWN);
              })
            Button('A')
              .layoutWeight(1)
              .height(34)
              .fontSize(12)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_A);
              })
            Button('B')
              .layoutWeight(1)
              .height(34)
              .fontSize(12)
              .onClick((): void => {
                this.mrpKeyTap(MR_KEYCODE_B);
              })
          }
          .width('100%')

          Row() {
            Button('选择 .mrp 导入 mythroad')
              .layoutWeight(1)
              .height(40)
              .fontSize(13)
              .margin({ left: 8, right: 8, bottom: 6 })
              .onClick((): void => {
                void this.pickMrpIntoMythroad();
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.Center)
        }
        .width('100%')
      }
      .layoutWeight(1)
      .scrollBar(BarState.Auto)
      .edgeEffect(EdgeEffect.Spring)
    }
    .width('100%')
    .height('100%')
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

    napi.onEditRequest((title: string, text: string, type: number, maxSize: number): void => {
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
      let startRet = napi.start('dsm_gm.mrp', 'start.mr');
      console.info('vmrp start result: ' + startRet);
    }

    this.initialized = true;
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
