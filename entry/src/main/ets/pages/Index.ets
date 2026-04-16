import testNapi from 'libentry.so';
import { image } from '@kit.ImageKit';
import { picker, fileUri, fileIo as fs } from '@kit.CoreFileKit';

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
  @State private framePixelMap: image.PixelMap | undefined = undefined;
  @State private importStatus: string = '未导入 MRP';

  build() {
    Column() {
      Row() {
        Stack() {
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

          if (this.framePixelMap) {
            Image(this.framePixelMap)
              .width(this.xComponentWidth)
              .height(this.xComponentHeight)
              .interpolation(ImageInterpolation.None)
          }
        }
        .width(this.xComponentWidth)
        .height(this.xComponentHeight)
        .onTouch((event: TouchEvent) => {
          this.handleTouch(event);
        })
      }
      .justifyContent(FlexAlign.Center)
      .width('100%')
      .layoutWeight(1)

      Row() {
        Button('导入MRP')
          .width(120)
          .height(40)
          .onClick(async () => {
            await this.pickAndImportMrp();
          })
        Text(this.importStatus)
          .fontSize(14)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceEvenly)
      .padding(5)

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

    testNapi.onDraw((x: number, y: number, w: number, h: number, pixels: ArrayBuffer): void => {
      this.drawFrame(x, y, w, h, pixels);
    });

    testNapi.onTimerStart((t: number): void => {
      this.stopTimer();
      this.timerId = setInterval(() => {
        testNapi.timer();
      }, t);
    });

    testNapi.onTimerStop((): void => {
      this.stopTimer();
    });

    let ret = testNapi.init(sandboxPath);
    console.info('vmrp init result: ' + ret);

    if (ret === 0) {
      let startRet = testNapi.start('dsm_gm.mrp', 'start.mr');
      console.info('vmrp start result: ' + startRet);
    }

    this.initialized = true;
  }

  private drawFrame(x: number, y: number, w: number, h: number, pixels: ArrayBuffer) {
    if (w <= 0 || h <= 0) {
      return;
    }

    const pixelCount = w * h;
    const src = new Uint16Array(pixels);
    const rgba = new Uint8Array(pixelCount * 4);

    for (let i = 0; i < pixelCount; i++) {
      const c = src[i];
      const r5 = (c >> 11) & 0x1f;
      const g6 = (c >> 5) & 0x3f;
      const b5 = c & 0x1f;

      const r8 = (r5 << 3) | (r5 >> 2);
      const g8 = (g6 << 2) | (g6 >> 4);
      const b8 = (b5 << 3) | (b5 >> 2);

      const base = i * 4;
      rgba[base] = r8;
      rgba[base + 1] = g8;
      rgba[base + 2] = b8;
      rgba[base + 3] = 0xff;
    }

    const opts: image.InitializationOptions = {
      editable: false,
      srcPixelFormat: image.PixelMapFormat.RGBA_8888,
      pixelFormat: image.PixelMapFormat.RGBA_8888,
      size: { width: w, height: h },
      alphaType: image.AlphaType.OPAQUE
    };

    try {
      const next = image.createPixelMapSync(rgba.buffer, opts);
      if (this.framePixelMap) {
        this.framePixelMap.release();
      }
      this.framePixelMap = next;
    } catch (err) {
      console.error(`drawFrame pixelmap error: ${JSON.stringify(err)}`);
    }

    console.info(`drawFrame: x=${x}, y=${y}, w=${w}, h=${h}`);
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

  private async pickAndImportMrp() {
    const sandboxPath = AppStorage.get<string>('sandboxPath') || '/data/storage/el2/base';
    const targetDir = `${sandboxPath}/mythroad`;
    const selectOptions = new picker.DocumentSelectOptions();
    selectOptions.maxSelectNumber = 1;
    selectOptions.fileSuffixFilters = ['.mrp'];

    try {
      const documentViewPicker = new picker.DocumentViewPicker();
      const uris = await documentViewPicker.select(selectOptions);
      if (!uris || uris.length === 0) {
        this.importStatus = '已取消选择';
        return;
      }

      const sourcePath = new fileUri.FileUri(uris[0]).path;
      const fileName = this.getFileName(sourcePath);
      if (!fileName.toLowerCase().endsWith('.mrp')) {
        this.importStatus = '仅支持 .mrp 文件';
        return;
      }

      if (!fs.accessSync(targetDir)) {
        fs.mkdirSync(targetDir, true);
      }

      const destPath = `${targetDir}/${fileName}`;
      fs.copyFileSync(sourcePath, destPath);
      this.importStatus = `导入成功: ${fileName}`;
      console.info(`Imported MRP: ${sourcePath} -> ${destPath}`);
    } catch (err) {
      this.importStatus = '导入失败';
      console.error(`Import MRP failed: ${JSON.stringify(err)}`);
    }
  }

  private getFileName(path: string): string {
    const idx = path.lastIndexOf('/');
    if (idx < 0) {
      return path;
    }
    return path.substring(idx + 1);
  }
}
