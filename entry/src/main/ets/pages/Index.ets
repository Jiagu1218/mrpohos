import testNapi from 'libentry.so';

const MR_KEY_PRESS = 0;
const MR_KEY_RELEASE = 1;
const MR_MOUSE_DOWN = 2;
const MR_MOUSE_UP = 3;
const MR_MOUSE_MOVE = 12;

const SCREEN_WIDTH = 240;
const SCREEN_HEIGHT = 320;

@Entry
@Component
struct Index {
  private xComponentWidth: number = 240;
  private xComponentHeight: number = 320;
  private timerId: number = -1;
  private initialized: boolean = false;

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
              .onAction(() => {})
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
    if (this.initialized) return;

    let rawfilesReady = AppStorage.get<boolean>('rawfilesReady') || false;
    if (!rawfilesReady) {
      setTimeout((): void => { this.initEmulator(); }, 100);
      return;
    }

    let sandboxPath = AppStorage.get<string>('sandboxPath') || '/data/storage/el2/base';

    let ret = testNapi.init(sandboxPath);
    console.info('vmrp init result: ' + ret);

    if (ret === 0) {
      let startRet = testNapi.start('dsm_gm.mrp', 'start.mr');
      console.info('vmrp start result: ' + startRet);

      this.timerId = setInterval(() => {
        testNapi.timer();
      }, 200);
    }

    this.initialized = true;
  }

  private handleTouch(event: TouchEvent) {
    let touch = event.touches[0];
    let scaleX = SCREEN_WIDTH / this.xComponentWidth;
    let scaleY = SCREEN_HEIGHT / this.xComponentHeight;
    let mx = Math.floor(touch.x * scaleX);
    let my = Math.floor(touch.y * scaleY);

    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx >= SCREEN_WIDTH) mx = SCREEN_WIDTH - 1;
    if (my >= SCREEN_HEIGHT) my = SCREEN_HEIGHT - 1;

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

  private stopTimer() {
    if (this.timerId !== -1) {
      clearInterval(this.timerId);
      this.timerId = -1;
    }
  }
}
