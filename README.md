# 这是一个开发者不敢保证明天能跑起来的玩具
# HxHLanguage：一个半编译半解释型语言
**如果你打算把它用到生产环境，那你是个人物**
[语法](docs/Grammar.md)
---
由C++编写(早期用C,因此仍有C风格)的虚拟机HXVM及编译器HXC。
---

* **HXC**：负责把源代码变成 `.hxo` 二进制文件。
* **HXVM**：一个简单的基于栈的解释器。

---

## 构建

配置文件(config.txt)：
- LANG=zh_CN ：语言为简中
- HX_DEBUG=OFF：不输出调试信息
- OBJECT_OS=ANDROID_TERMUX：目标平台，不设置该项默认生成构建平台的二进制文件(ANDROID_TERMUX(仅支持在Termux编译)，LINUX(仅支持在Linux编译), WINDOWS(仅支持在Linux编译))

如果你真的勇士，想在你的机器上编译它：
```bash
mkdir -p build
cd build
cmake ..
make -j2
./hxc -v
./hxvm -v
```

## 使用
### hxc
`hxc -v`：输出版本信息
`hxc <源文件>`：编译，产物输出至out.hxo
`hxc <源文件> -o <目标文件>`：编译，产物输出至目标文件
### hxvm
`hxvm -v`：输出版本信息
`hxvm <文件>`：解释运行
