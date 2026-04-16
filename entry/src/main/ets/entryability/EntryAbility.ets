import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { fileIo as fs } from '@kit.CoreFileKit';

const DOMAIN = 0x0000;
const TAG = 'EntryAbility';

async function copyRawfileToSandbox(context: Context, sandboxPath: string) {
  let mythroadDir = sandboxPath + '/mythroad/system';
  if (!fs.accessSync(sandboxPath + '/mythroad')) {
    fs.mkdirSync(mythroadDir, true);
  }

  let files = [
    'cfunction.ext',
    'mythroad/dsm_gm.mrp',
    'mythroad/system/gb12.uc2',
    'mythroad/system/gb16.uc2',
  ];

  for (let file of files) {
    let destPath = sandboxPath + '/' + file;
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

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, 'Failed to load the content. Cause: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, TAG, 'Succeeded in loading the content.');
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
