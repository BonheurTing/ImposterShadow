# Imposter动态阴影

![](Resource/0.png)

# 简介
这是一个在UE4中实现的Imposter，主要实现了以下功能：
- bake过程。
- imposter渲染。
- 采多个视角插值，防止旋转与跳变。
- 各个通道的dilation。
- alpha通道生成距离场。
- 正确写入场景深度，可以结合contact shadow产生细碎的阴影。
- 利用深度进行简单的视差矫正。
- 正确渲染shadowmap。

# 效果
动态阴影，左边为imposter，右边为原模型。

![](Resource/0.gif)

各个视角观察：

![](Resource/1.gif)

普通物件：

![](Resource/2.gif)  

# 安装
将 *Source/Plugins/ImposterBaker* 目录拷贝到项目或者引擎的Plugins文件夹下，在引擎插件中启用。

将Source/Shader/目录下的文件拷贝到引擎中(本人使用的引擎是4.26.1)。

插件的Example/目录下有一个示例场景，场景中已经放置了一个baker，可以将任意模型拖入场景里烘焙。

# 工具使用方式

**1. 烘焙**

将插件的Blueprint/目录下的ImposterBakeProxy拖到场景中，detail面板有对应配置，默认已经有一套可以直接使用的配置，直接选择物体烘焙即可：

![](Resource/4.png)

烘焙完成后，场景中会在原模型旁边自动出现一个imposter，材质与贴图的输出路径默认是在Content/ImposterOutout/下。

**2. (可选) 修改shading model**

自动生成的imposter是默认pbr材质，如果是给植被用，可以考虑改成two sided foliage。

生成的材质是material instance，母材质是插件目录下的ImposterRender材质，在material instance下方可以修改shading model，如下：

![](Resource/5.png)

**3. 打开主光源接触阴影**

shadowmap的精度较难支撑叶子间细碎的阴影，选择场景中的directional light然后在detail面板中搜索contact shadow，然后将距离调整为0.01~0.02就可以看到叶子间的阴影。

![](Resource/6.png)

开关contact shadow的对比：

![](Resource/3.gif)
