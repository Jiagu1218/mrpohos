import testNapiImport from 'libentry.so';

/** 与 libentry NAPI 导出一致（默认导入的类型推断在部分工具链下不完整，故显式收窄）。 */
interface MrpNativeBindings {
  init: (sandboxPath: string) => number;
  start: (filename: string, extName: string) => number;
  event: (code: number, p0: number, p1: number) => number;
  timer: () => number;
  onTimerStart: (callback: (t: number) => void) => void;
  onTimerStop: (callback: () => void) => void;
  onEditRequest: (callback: (title: string, text: string, type: number, maxSize: number) => void) => void;
  setEditResult: (text: string) => void;
}

const testNapi = testNapiImport as MrpNativeBindings;

const MR_KEY_PRESS = 0;
const MR_KEY_RELEASE = 1;
const MR_MOUSE_DOWN = 2;
const MR_MOUSE_UP = 3;
const MR_MOUSE_MOVE = 12;

const MR_DIALOG_EVENT = 6;
const MR_DIALOG_KEY_OK = 0;
const MR_DIALOG_KEY_CANCEL = 1;

const MR_EDIT_NUMERIC = 1;
const MR_EDIT_PASSWORD = 2;

const SCREEN_WIDTH = 240;
const SCREEN_HEIGHT = 320;

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
  /** 由 MRP timerStart(t) 调度：必须在 t 毫秒后调用一次 timer()，不能用固定短间隔轮询。 */
  private mrpTimerTimeoutId: number = -1;
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
        testNapi.setEditResult(this.editDlgText);
        testNapi.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_OK, 0);
      },
      onCancel: (): void => {
        testNapi.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
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
        testNapi.event(MR_DIALOG_EVENT, MR_DIALOG_KEY_CANCEL, 0);
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

      Row() {
        Button('←')
          .width(60)
          .height(40)
          .onClick(() => testNapi.event(MR_KEY_PRESS, 15, 0))
          .gesture(
            LongPressGesture()
              .onAction(() => {
              })
              .onActionEnd(() => testNapi.event(MR_KEY_RELEASE, 15, 0))
          )
        Button('↑')
          .width(60)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 18, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 18, 0), 100);
          })
        Button('↓')
          .width(60)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 19, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 19, 0), 100);
          })
        Button('→')
          .width(60)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 20, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 20, 0), 100);
          })
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceEvenly)
      .padding(5)

      Row() {
        Button('左')
          .width(80)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 21, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 21, 0), 100);
          })
        Button('OK')
          .width(80)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 22, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 22, 0), 100);
          })
        Button('右')
          .width(80)
          .height(40)
          .onClick(() => {
            testNapi.event(MR_KEY_PRESS, 23, 0);
            setTimeout(() => testNapi.event(MR_KEY_RELEASE, 23, 0), 100);
          })
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceEvenly)
      .padding(5)
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

    testNapi.onEditRequest((title: string, text: string, type: number, maxSize: number): void => {
      this.editDlgGen++;
      this.editDlgTitle = title;
      this.editDlgText = text;
      this.editDlgMax = maxSize > 0 ? maxSize : 256;
      this.editDlgType = type;
      if (this.editDialogController !== null) {
        this.editDialogController.open();
      }
    });

    // 须在 init/start 之前注册：MRP 会通过 timerStart(t) 通知平台，平台须在 t ms 后调用一次 timer()（mr_timer）。
    // 若此处为空且用短间隔 setInterval 调 timer()，会在远小于 t 时误触发，表现为「几秒对话框一闪就关」。
    testNapi.onTimerStop((): void => {
      this.clearMrpTimerTimeout();
    });
    testNapi.onTimerStart((t: number): void => {
      this.clearMrpTimerTimeout();
      this.mrpTimerTimeoutId = setTimeout((): void => {
        this.mrpTimerTimeoutId = -1;
        testNapi.timer();
      }, t);
    });

    let ret = testNapi.init(sandboxPath);
    console.info('vmrp init result: ' + ret);

    if (ret === 0) {
      let startRet = testNapi.start('dsm_gm.mrp', 'start.mr');
      console.info('vmrp start result: ' + startRet);
    }

    this.initialized = true;
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

    switch (event.type) {
      case TouchType.Down:
        testNapi.event(MR_MOUSE_DOWN, mx, my);
        break;
      case TouchType.Move:
        testNapi.event(MR_MOUSE_MOVE, mx, my);
        break;
      case TouchType.Up:
        testNapi.event(MR_MOUSE_UP, mx, my);
        break;
    }
  }

  private clearMrpTimerTimeout(): void {
    if (this.mrpTimerTimeoutId !== -1) {
      clearTimeout(this.mrpTimerTimeoutId);
      this.mrpTimerTimeoutId = -1;
    }
  }
}
