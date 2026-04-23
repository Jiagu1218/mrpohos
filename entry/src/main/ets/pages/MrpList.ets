import { fileIo as fs, picker, fileUri } from '@kit.CoreFileKit';
import { promptAction, router } from '@kit.ArkUI';
import { util } from '@kit.ArkTS';

export interface MrpInfo {
  filename: string;
  appname: string;
  appid: number;
  version: number;
  vendor: string;
  description: string;
  filePath: string;
}

function readNullTerminatedString(buffer: ArrayBuffer, offset: number, maxLen: number): string {
  const bytes = new Uint8Array(buffer, offset, maxLen);
  let end = maxLen;
  for (let i = 0; i < maxLen; i++) {
    if (bytes[i] === 0) {
      end = i;
      break;
    }
  }
  return Array.from(bytes.subarray(0, end))
    .map(b => String.fromCharCode(b))
    .join('');
}

function readGb2312String(buffer: ArrayBuffer, offset: number, maxLen: number): string {
  const bytes = new Uint8Array(buffer, offset, maxLen);
  let end = maxLen;
  for (let i = 0; i < maxLen; i++) {
    if (bytes[i] === 0) {
      end = i;
      break;
    }
  }
  const slice = bytes.subarray(0, end);
  try {
    const decoder = util.TextDecoder.create('gbk');
    return decoder.decodeToString(slice);
  } catch (_e) {
    return '';
  }
}

export function parseMrpHeader(filePath: string): MrpInfo | null {
  try {
    const fd = fs.openSync(filePath, fs.OpenMode.READ_ONLY);
    const headerBuf = new ArrayBuffer(240);
    const readLen = fs.readSync(fd.fd, headerBuf);
    fs.closeSync(fd);

    if (readLen < 240) {
      return null;
    }

    const magicView = new Uint8Array(headerBuf, 0, 4);
    if (magicView[0] !== 0x4D || magicView[1] !== 0x52 || magicView[2] !== 0x50 || magicView[3] !== 0x47) {
      return null;
    }

    const filename = readNullTerminatedString(headerBuf, 16, 12);
    const appname = readGb2312String(headerBuf, 28, 24);
    const appidView = new DataView(headerBuf);
    const appid = appidView.getUint32(68, true);
    const version = appidView.getUint32(72, true);
    const vendor = readGb2312String(headerBuf, 88, 40);
    const description = readGb2312String(headerBuf, 128, 64);

    return {
      filename: filename || filePath.substring(filePath.lastIndexOf('/') + 1),
      appname: appname || filename,
      appid,
      version,
      vendor: vendor || '',
      description: description || '',
      filePath,
    };
  } catch (_e) {
    return null;
  }
}

export function scanMythroadDir(sandboxPath: string): MrpInfo[] {
  const mythroadDir = `${sandboxPath}/mythroad`;
  if (!fs.accessSync(mythroadDir)) {
    return [];
  }

  const results: MrpInfo[] = [];
  try {
    const entries = fs.listFileSync(mythroadDir);
    for (const entry of entries) {
      if (!entry.toLowerCase().endsWith('.mrp')) {
        continue;
      }
      const fullPath = `${mythroadDir}/${entry}`;
      const info = parseMrpHeader(fullPath);
      if (info !== null) {
        results.push(info);
      }
    }
  } catch (_e) {
    return [];
  }

  results.sort((a, b) => a.appname.localeCompare(b.appname));
  return results;
}

function mrpFileNameFromUri(uri: string): string {
  const noQuery = uri.split('?')[0];
  let segment = noQuery.substring(noQuery.lastIndexOf('/') + 1);
  try {
    segment = decodeURIComponent(segment);
  } catch (_e) {
  }
  return segment;
}

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
      throw new Error('无法 stat 源文件');
    }
  }

  const srcFile = fs.openSync(srcPath, fs.OpenMode.READ_ONLY);
  const dstFile = fs.openSync(destPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
  try {
    fs.copyFileSync(srcFile.fd, dstFile.fd);
  } catch (e) {
    try { fs.closeSync(srcFile); } catch (_e2) { }
    try { fs.closeSync(dstFile); } catch (_e2) { }
    try { fs.unlinkSync(destPath); } catch (_e3) { }
    throw e instanceof Error ? e : new Error(String(e));
  }
  fs.closeSync(srcFile);
  fs.closeSync(dstFile);

  return destPath;
}

@Entry
@Component
struct MrpList {
  @State mrpList: MrpInfo[] = [];
  @State loading: boolean = true;
  private sandboxPath: string = '';

  aboutToAppear(): void {
    this.sandboxPath = AppStorage.get<string>('sandboxPath') || '';
    const rawfilesReady = AppStorage.get<boolean>('rawfilesReady') || false;
    if (rawfilesReady) {
      this.refreshList();
    } else {
      this.waitForRawfiles();
    }
  }

  private waitForRawfiles(): void {
    if (AppStorage.get<boolean>('rawfilesReady') || false) {
      this.refreshList();
      return;
    }
    setTimeout(() => this.waitForRawfiles(), 100);
  }

  private refreshList(): void {
    this.loading = true;
    this.mrpList = scanMythroadDir(this.sandboxPath);
    this.loading = false;
  }

  private async pickMrpFile(): Promise<void> {
    const uiContext = getContext(this);
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
      const destPath = await copyPickerUriToMythroad(uris[0], this.sandboxPath);
      const name = destPath.substring(destPath.lastIndexOf('/') + 1);
      promptAction.showToast({ message: `已导入 ${name}`, duration: 2000 });
      this.refreshList();
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      promptAction.showToast({ message: `导入失败：${msg}`, duration: 3000 });
    }
  }

  build() {
    Column() {
      Row() {
        Text('MRP 应用')
          .fontSize(22)
          .fontWeight(FontWeight.Bold)
          .fontColor('#e8e8e8')
          .layoutWeight(1)

        Button() {
          Text('+')
            .fontSize(20)
            .fontColor('#e8e8e8')
            .fontWeight(FontWeight.Bold)
        }
        .width(40)
        .height(40)
        .backgroundColor('#2a3040')
        .borderRadius(20)
        .type(ButtonType.Normal)
        .onClick(() => void this.pickMrpFile())
      }
      .width('100%')
      .padding({ left: 20, right: 20, top: 16, bottom: 12 })

      if (this.loading) {
        Column() {
          LoadingProgress()
            .width(48)
            .height(48)
            .color('#4a9da8')
        }
        .width('100%')
        .layoutWeight(1)
        .justifyContent(FlexAlign.Center)
      } else if (this.mrpList.length === 0) {
        Column() {
          Text('暂无 MRP 应用')
            .fontSize(16)
            .fontColor('#7a8090')
            .margin({ bottom: 12 })
          Text('点击右上角 + 导入 .mrp 文件')
            .fontSize(13)
            .fontColor('#5a6070')
        }
        .width('100%')
        .layoutWeight(1)
        .justifyContent(FlexAlign.Center)
      } else {
        List({ space: 8 }) {
          ForEach(this.mrpList, (item: MrpInfo) => {
            ListItem() {
              this.MrpListItem(item)
            }
            .onClick(() => {
              router.pushUrl({
                url: 'pages/Index',
                params: { mrp: item.filePath.substring(item.filePath.lastIndexOf('/') + 1) },
              });
            })
          }, (item: MrpInfo) => item.filePath)
        }
        .width('100%')
        .layoutWeight(1)
        .padding({ left: 16, right: 16, top: 4, bottom: 16 })
        .divider({ strokeWidth: 0 })
      }
    }
    .width('100%')
    .height('100%')
    .linearGradient({
      angle: 165,
      colors: [['#12151c', 0.0], ['#1e232d', 0.45], ['#0d1015', 1.0]]
    })
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }

  @Builder
  MrpListItem(item: MrpInfo) {
    Row() {
      Column() {
        Text(item.appname || item.filename)
          .fontSize(16)
          .fontColor('#e8e8e8')
          .fontWeight(FontWeight.Medium)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })

        if (item.vendor) {
          Text(item.vendor)
            .fontSize(12)
            .fontColor('#7a8090')
            .margin({ top: 2 })
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }

        if (item.description) {
          Text(item.description)
            .fontSize(11)
            .fontColor('#5a6570')
            .margin({ top: 2 })
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
        }
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start)

      Column() {
        Text(`v${item.version}`)
          .fontSize(11)
          .fontColor('#5a6570')
        Text(`ID:${item.appid}`)
          .fontSize(9)
          .fontColor('#3a4050')
          .margin({ top: 2 })
      }
      .alignItems(HorizontalAlign.End)
      .margin({ left: 12 })
    }
    .width('100%')
    .padding({ left: 16, right: 16, top: 14, bottom: 14 })
    .backgroundColor('#1a2030')
    .borderRadius(12)
    .shadow({ radius: 4, color: '#20000000', offsetY: 2 })
  }
}
