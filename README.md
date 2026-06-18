# 这是一个开发者不敢保证明天能跑起来的玩具
# HxHLanguage
---
由C++编写(早期用C,因此仍有C风格)的虚拟机HXVM及编译器HXC。
---

* **HXC**：负责把源代码变成 `.hxo` 二进制文件。
* **HXVM**：一个简单的基于栈的解释器。

---

## 构建

如果你真的勇士，想在你的机器上编译它：
```bash
mkdir -p build
cd build
cmake ..
make -j2
./hxc
./hxvm
```
如果你打算把它用到生产环境，那你是个人物