XNSIS构建指南

### 环境准备
* 确保已安装 Python 3.x
* 确保已安装 Git（用于克隆仓库）

### 构建步骤
#### 第一步：安装 SCons
pip install scons

#### 第二步：执行构建脚本
* 克隆 XNSIS 仓库（如未克隆）：
git clone https://github.com/kabofang/nsis32
git submodule update --init --recursive
cd nsis32
* 执行构建脚本（下载目录需在仓库外）：
python build.py --ddir E:\downloads

#### 第三步：执行构建命令
执行脚本输出的类似命令：
scons ZLIB_W32="E:\nsis32\Zlib32" NSIS_CONFIG_LOG=yes NSIS_MAX_STRLEN=8192 dist-zip
