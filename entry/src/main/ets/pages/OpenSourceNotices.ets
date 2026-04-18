import { router } from '@kit.ArkUI';

/** 开源组件信息 */
interface OpenSourceComponent {
  name: string;
  version: string;
  license: string;
  homepage: string;
  sourceUrl: string;
  description: string;
  licenseText?: string;
}

/** 开源组件列表 */
const OPEN_SOURCE_COMPONENTS: OpenSourceComponent[] = [
  {
    name: 'VMRP (Virtual MythRoad Platform)',
    version: '1.0.1',
    license: 'GPL-3.0',
    homepage: 'https://github.com/vmrp/vmrp',
    sourceUrl: 'https://github.com/Jiagu1218/mrpohos',
    description: 'MRP 应用运行时的核心实现，用于仿真和执行 MRP 格式的应用程序。',
    licenseText: `This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.`
  },
  {
    name: 'Unicorn Engine',
    version: '2.1.4',
    license: 'GPL-2.0 / LGPL-2.0',
    homepage: 'https://www.unicorn-engine.org/',
    sourceUrl: 'https://github.com/unicorn-engine/unicorn',
    description: '轻量级的多架构 CPU 仿真框架，用于执行 ARM 指令集。',
    licenseText: `This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.`
  },
  {
    name: 'FluidSynth',
    version: '未知',
    license: 'LGPL-2.1+',
    homepage: 'https://www.fluidsynth.org/',
    sourceUrl: 'https://github.com/FluidSynth/fluidsynth',
    description: '实时 MIDI 合成器，用于播放 MRP 应用中的 MIDI 音乐。\n\n注意：以动态链接库（.so）形式使用，符合 LGPL 许可证要求。',
    licenseText: `This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.`
  },
  {
    name: 'libsndfile',
    version: '未知',
    license: 'LGPL-2.1',
    homepage: 'http://www.mega-nerd.com/libsndfile/',
    sourceUrl: 'https://github.com/libsndfile/libsndfile',
    description: '音频文件读写库（FluidSynth 依赖）。'
  },
  {
    name: 'libogg',
    version: '未知',
    license: 'BSD 3-Clause',
    homepage: 'https://www.xiph.org/ogg/',
    sourceUrl: 'https://github.com/xiph/ogg',
    description: 'Ogg 比特流库（FluidSynth 依赖）。'
  },
  {
    name: 'zlib',
    version: '未知',
    license: 'zlib/libpng',
    homepage: 'https://zlib.net/',
    sourceUrl: 'https://github.com/madler/zlib',
    description: '数据压缩库。'
  }
];

@Entry
@Component
struct OpenSourceNotices {
  @State expandedIndex: number = -1;

  build() {
    Column() {
      // 标题栏
      Row()
      .width('100%')
      .height(56)
      .backgroundColor('#1a1e26')
      .padding({ left: 8, right: 8 })

      // 说明文本
      Column({ space: 8 }) {
        Text('本应用使用了以下开源软件组件。我们感谢这些开源项目的贡献者。')
          .fontSize(14)
          .fontColor('#b0b8c4')
          .lineHeight(20)
          .width('100%')

        Text('点击组件可查看详细的许可证信息。')
          .fontSize(12)
          .fontColor('#808894')
          .lineHeight(18)
          .width('100%')
      }
      .width('100%')
      .padding(16)
      .backgroundColor('#23272e')

      // 组件列表
      List({ space: 8 }) {
        ForEach(OPEN_SOURCE_COMPONENTS, (component: OpenSourceComponent, index: number) => {
          ListItem() {
            this.ComponentItem(component, index)
          }
        }, (component: OpenSourceComponent, index: number) => `${component.name}_${index}`)
      }
      .width('100%')
      .layoutWeight(1)
      .padding({ left: 16, right: 16, top: 8, bottom: 16 })
      .scrollBar(BarState.Auto)
      .edgeEffect(EdgeEffect.Spring)

      // 底部说明
      Column({ space: 8 }) {
        Divider().color('#2a2f38').strokeWidth(1)

        Text('如何获取源码')
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor('#d0d6e0')
          .width('100%')
        Hyperlink('https://github.com/Jiagu1218/mrpohos',"项目源码")
        Text('根据 GPL 和 LGPL 许可证的要求，您可以通过以下方式获取上述开源组件的完整源代码：\n\n' +
          '1. 在线获取：大多数组件的源码可以在其官方仓库获取（见上方链接）\n\n' +
          '2. 项目源码：本应用完整源码已开源，访问 https://github.com/Jiagu1218/mrpohos\n\n' +
          '3. 重新链接：对于 LGPL 组件，您可以替换库文件版本，我们提供必要的目标文件以便重新链接')
          .fontSize(12)
          .fontColor('#808894')
          .lineHeight(18)
          .width('100%')

        Text('最后更新：2026-04-18 | 应用版本：1.0.0 | 旧梦曲奇')
          .fontSize(11)
          .fontColor('#606874')
          .width('100%')
          .textAlign(TextAlign.Center)
          .margin({ top: 8 })
      }
      .width('100%')
      .padding(16)
      .backgroundColor('#23272e')
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#1a1e26')
  }

  @Builder
  ComponentItem(component: OpenSourceComponent, index: number) {
    Column({ space: 8 }) {
      // 组件头部
      Row() {
        Column({ space: 4 }) {
          Text(component.name)
            .fontSize(15)
            .fontWeight(FontWeight.Medium)
            .fontColor('#eef2f7')
            .width('100%')

          Row({ space: 8 }) {
            Text(`v${component.version}`)
              .fontSize(11)
              .fontColor('#808894')
              .padding({ left: 6, right: 6, top: 2, bottom: 2 })
              .backgroundColor('#2a2f38')
              .borderRadius(4)

            Text(component.license)
              .fontSize(11)
              .fontColor(this.getLicenseColor(component.license))
              .padding({ left: 6, right: 6, top: 2, bottom: 2 })
              .backgroundColor(this.getLicenseBgColor(component.license))
              .borderRadius(4)
          }
        }
        .layoutWeight(1)

        Image(this.expandedIndex === index ? 
          $r('sys.media.ohos_ic_public_arrow_up') : 
          $r('sys.media.ohos_ic_public_arrow_down'))
          .width(20)
          .height(20)
          .fillColor('#606874')
      }
      .width('100%')
      .padding(12)
      .backgroundColor('#2a2f38')
      .borderRadius(8)
      .onClick(() => {
        this.expandedIndex = this.expandedIndex === index ? -1 : index;
      })

      // 展开的详细信息
      if (this.expandedIndex === index) {
        Column({ space: 12 }) {
          // 描述
          Text(component.description)
            .fontSize(13)
            .fontColor('#b0b8c4')
            .lineHeight(20)
            .width('100%')

          // 主页链接
          if (component.homepage) {
            Row({ space: 8 }) {
              Text('主页：')
                .fontSize(12)
                .fontColor('#808894')
              
              Hyperlink(component.homepage)
                .height(14)
                .color('#4a9eff')
            }
            .width('100%')
          }

          // 源码链接
          if (component.sourceUrl) {
            Row({ space: 8 }) {
              Text('源码：')
                .fontSize(12)
                .fontColor('#808894')

              Hyperlink(component.sourceUrl.startsWith('http') ? component.sourceUrl : component.sourceUrl)
                .height(14)
                .color(component.sourceUrl.startsWith('http') ? '#4a9eff' : '#b0b8c4')
            }
            .width('100%')
          }

          // 许可证文本
          if (component.licenseText) {
            Column({ space: 4 }) {
              Text('许可证摘要：')
                .fontSize(12)
                .fontWeight(FontWeight.Medium)
                .fontColor('#808894')
                .width('100%')

              Text(component.licenseText)
                .fontSize(11)
                .fontColor('#606874')
                .lineHeight(16)
                .width('100%')
                .padding(10)
                .backgroundColor('#1a1e26')
                .borderRadius(6)
            }
            .width('100%')
          }
        }
        .width('100%')
        .padding(12)
        .backgroundColor('#23272e')
        .borderRadius(8)
        .border({ width: 1, color: '#2a2f38' })
      }
    }
    .width('100%')
  }

  /** 根据许可证类型返回文字颜色 */
  getLicenseColor(license: string): string {
    if (license.includes('GPL-3.0')) {
      return '#ff6b6b';
    } else if (license.includes('GPL-2.0')) {
      return '#ffa94d';
    } else if (license.includes('LGPL')) {
      return '#51cf66';
    } else if (license.includes('BSD')) {
      return '#4dabf7';
    } else {
      return '#808894';
    }
  }

  /** 根据许可证类型返回背景颜色 */
  getLicenseBgColor(license: string): string {
    if (license.includes('GPL-3.0')) {
      return '#2d1f1f';
    } else if (license.includes('GPL-2.0')) {
      return '#2d261f';
    } else if (license.includes('LGPL')) {
      return '#1f2d22';
    } else if (license.includes('BSD')) {
      return '#1f262d';
    } else {
      return '#2a2f38';
    }
  }
}
