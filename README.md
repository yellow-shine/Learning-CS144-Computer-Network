本树是 CS144 Winter 2025 / minnow 的本地学习参考实现，不公开发布解答。容器入口 `docs/RUN.md`，全景 `docs/overview.md`，验证总表 `docs/verification.md`。

Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: <https://cs144.stanford.edu>

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

## 测试与 macOS 开发环境

### 1. 优先运行仓库已有测试

在 Linux 开发环境中，在项目根目录执行：

```bash
cmake -S . -B build -G Ninja
cmake --build build --target check1
```

`check1` 定义在 `etc/tests.cmake`，选择 ByteStream、Reassembler 和 `no_skip` 测试，也会选中相应的 speed tests。CTest 的 fixture 会先编译所需目标；功能测试使用 AddressSanitizer 和 UndefinedBehaviorSanitizer。

看到 `100% tests passed` 表示通过现有测试，不代表对所有可能输入的数学证明。

| 测试文件（位于 `tests/`） | 主要检查 |
| --- | --- |
| `reassembler_single.cc` | 基础插入、空字符串、EOF |
| `reassembler_seq.cc` | 顺序插入 |
| `reassembler_dup.cc` | 重复数据去重 |
| `reassembler_holes.cc` | 乱序和缺口补齐 |
| `reassembler_overlapping.cc` | 重叠区间合并 |
| `reassembler_cap.cc` | 容量限制 |
| `reassembler_win.cc` | 接收窗口 |
| `reassembler_speed_test.cc` | 性能 |

### 2. macOS：本机编辑，Docker 内测试

Reassembler 本身是标准 C++，但项目整体包含 Linux 相关依赖，因此优先复用仓库的容器环境，不必为跑这个测试移植整个项目。

安装并启动 Docker Desktop，先确认 Docker 可用：

```bash
docker info
```

然后在项目根目录运行：

```bash
./scripts/dev.sh bash -lc \
  'cmake -S . -B build-linux -G Ninja && cmake --build build-linux --target check1'
```

- 脚本把项目目录挂载到容器的 `/src`，测试的就是本机修改后的源码。
- 使用独立的 `build-linux`，避免混用 macOS 原生构建的 CMake 缓存。
- Reassembler 单元测试不需要 TUN/TAP，也不需要设置 `CS144_NET=1`。
- 不需要代理时，不必复制 `docs/RUN.md` 中的代理地址；需要时换成自己可用的地址。

失败后单独查看某项测试的详细输出，例如：

```bash
./scripts/dev.sh bash -lc \
  "ctest --test-dir build-linux -R '^reassembler_overlapping$' -V"
```

### 3. 用的是什么 Docker image？

依据根目录 `Dockerfile` 和 `scripts/dev.sh`：

- 默认镜像名：`cs144-minnow`（即 `cs144-minnow:latest`）。
- 基础镜像：`ubuntu:24.04`。
- 安装工具：`g++`、CMake、Ninja、Git、Python 3 和 CA 证书。
- 镜像在本地不存在时，脚本会根据仓库的 Dockerfile 自动构建，并传递代理构建参数。
- 已有镜像会直接复用；修改 Dockerfile 后需要手动重新构建。

手动构建：

```bash
docker build -t cs144-minnow .
```

覆盖默认镜像名：

```bash
CS144_IMAGE=你的镜像 ./scripts/dev.sh bash
```

注意：当前脚本在镜像构建失败时会回退到 `jenkins-agent:task2-fixes`。这是定制镜像名，其他机器不一定可用；遇到这种情况应先查看前面的构建错误，不要把回退镜像当成课程官方依赖。

### 4. 正确性不仅是最终字符串，还包括中间状态

容量为 10 时，可以手算下面的例子：

| 操作 | 累计写入 ByteStream | pending 字节数 | writer 是否关闭 |
| --- | ---: | ---: | --- |
| `insert(3, "def", true)` | 0 | 3 | 否，前面有缺口 |
| 再次 `insert(3, "def", true)` | 0 | 3 | 否，重复不能多计 |
| `insert(0, "abc", false)` | 6 | 0 | 是，已拼齐到 EOF |

最后 reader 应读出 `"abcdef"`。writer 关闭不代表 reader 已完成：缓冲区也被读空后，`reader().is_finished()` 才为真。

已有测试使用 `ReassemblerTestHarness`，例如 `tests/reassembler_single.cc` 中：

```cpp
ReassemblerTestHarness test { "insert a @ 0 [last]", 65000 };
test.execute( Insert { "a", 0 }.is_last() );
test.execute( BytesPushed( 1 ) );
test.execute( ReadAll( "a" ) );
test.execute( IsFinished { true } );
```

注意测试工具的 `Insert` 参数顺序是“字符串、下标”，实际接口 `insert()` 则是“下标、字符串、是否最后一段”。

排查顺序：跑 `check1` → 读取失败用例 → 手算窗口、pending 和 EOF → 对照实现。

**本次 session 的验证范围：查看了源码、测试配置、Dockerfile 和启动脚本，尚未实际运行构建或测试，不能据此声称当前实现已经通过。**
