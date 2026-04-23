import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { fileIo as fs } from '@kit.CoreFileKit';

const DOMAIN = 0x0000;
const TAG = 'EntryAbility';

async function collectRawfiles(context: Context, dir: string): Promise<string[]> {
  let result: string[] = [];
  try {
    let entries: string[] = await context.resourceManager.getRawFileList(dir);
    for (let entry of entries) {
      let path = dir.length > 0 ? dir + '/' + entry : entry;
      try {
        let sub: string[] = await context.resourceManager.getRawFileList(path);
        let children = await collectRawfiles(context, path);
        result = result.concat(children);
      } catch (_e) {
        result.push(path);
      }
    }
  } catch (_e) {
    result.push(dir);
  }
  return result;
}

async function copyRawfileToSandbox(context: Context, sandboxPath: string) {
  let files = [
    'cfunction.ext',
  ];
  let mythroadFiles = await collectRawfiles(context, 'mythroad');
  files = files.concat(mythroadFiles);
  /* NAPI harmony_midi_init 使用 <filesDir>/vmrp/default.sf2，须从 rawfile/vmrp 拷入沙箱 */
  let vmrpFiles = await collectRawfiles(context, 'vmrp');
  files = files.concat(vmrpFiles);

  for (let file of files) {
    let destPath = sandboxPath + '/' + file;
    let destDir = destPath.substring(0, destPath.lastIndexOf('/'));
    if (!fs.accessSync(destDir)) {
      fs.mkdirSync(destDir, true);
    }
    if (fs.accessSync(destPath)) {
      hilog.info(DOMAIN, TAG, 'Already exists: %{public}s', file);
      continue;
    }
    try {
      let content = await context.resourceManager.getRawFileContent(file);
      let fd = fs.openSync(destPath, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE);
      fs.writeSync(fd.fd, content.buffer);
      fs.closeSync(fd);
      hilog.info(DOMAIN, TAG, 'Copied rawfile: %{public}s', file);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to copy %{public}s: %{public}s', file, JSON.stringify(err));
    }
  }
}

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'Failed to set colorMode. Cause: %{public}s', JSON.stringify(err));
    }
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onCreate');

    let sandboxPath = this.context.filesDir;
    AppStorage.SetOrCreate<string>('sandboxPath', sandboxPath);

    this.copyRawfiles(sandboxPath);
  }

  private async copyRawfiles(sandboxPath: string) {
    try {
      await copyRawfileToSandbox(this.context, sandboxPath);
    } catch (err) {
      hilog.error(DOMAIN, TAG, 'copyRawfileToSandbox failed: %{public}s', JSON.stringify(err));
    }
    AppStorage.SetOrCreate<boolean>('rawfilesReady', true);
  }

  onDestroy(): void {
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onWindowStageCreate');

    windowStage.loadContent('pages/MrpList', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, TAG, 'Succeeded in loading the content.');
      try {
        const win = windowStage.getMainWindowSync();
        win.setWindowLayoutFullScreen(true);
        win.setWindowSystemBarProperties({
          statusBarColor: '#00000000',
          statusBarContentColor: '#FFE8E8E8',
          navigationBarColor: '#00000000',
          navigationBarContentColor: '#FFE8E8E8',
        });
      } catch (e) {
        hilog.warn(DOMAIN, TAG, 'immersive window props: %{public}s', JSON.stringify(e));
      }
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onForeground');
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, '%{public}s', 'Ability onBackground');
  }
}
