# ByteStream：接口隔离 + 空派生类视图

`examples/byte_stream_example.cc` 只构造一个 `ByteStream`，再取出两种引用：

```cpp
ByteStream stream { 8 };
Writer& out = stream.writer();
Reader& in = stream.reader();
```

`out`、`in`、`stream` 是同一个对象。后面的断言就是在说这件事：`&in` 和 `&out` 转回 `ByteStream*` 之后等于 `&stream`。

## 这是什么 pattern

没有单独的 GoF 名字。设计意图是 **接口隔离（Interface Segregation）**：调用方只该看见自己要用的操作。实现手法是 **空派生类视图**：`Reader` / `Writer` 继承 `ByteStream`，自己不加成员、不加虚函数，只是给同一块内存换一张类型面孔。

也可以把它理解成 **capability reference（能力引用）**。交出 `Writer&` 表示“可以写”，交出 `Reader&` 表示“可以读”。状态不复制，权限靠类型分开。

和 Role Object 不完全一样。Role Object 通常是另建角色对象再指回主体；这里没有第二个对象，只是 `static_cast`。

## 代码怎么接上

`Reader` / `Writer` 只有方法，数据全在基类：

```cpp
class Writer : public ByteStream { /* push / close，无成员 */ };
class Reader : public ByteStream { /* peek / pop，无成员 */ };
```

`reader()` / `writer()` 把 `*this` 向下转成引用：

```cpp
return static_cast<Reader&>( *this );
```

旁边的 `static_assert(sizeof(Reader) == sizeof(ByteStream))` 锁住前提：子类一旦加成员，布局就不再相同，这个转换不能再用。

单继承、无虚函数时，基类子对象就在对象开头，三个类型大小相同、地址相同。`push` / `peek` 用的仍是基类里的 `buffer_`、`capacity_`。

## 为什么类前面要前向声明

```cpp
class Reader;
class Writer;

class ByteStream {
  Reader& reader();
  Writer& writer();
};
```

这是循环依赖，不是 pattern 本身：

- `ByteStream` 的方法要返回 `Reader&` / `Writer&`，所以得先知道这两个名字。
- `Reader` / `Writer` 又要继承 `ByteStream`，基类必须先完整定义。

前向声明只引入名字。引用和指针不需要知道对象多大，所以够用。继承不行，所以两个子类必须写在基类后面。声明必须和后面的定义在同一作用域；写进 `ByteStream` 里面会变成另一个类型 `ByteStream::Reader`。

## 好处

- **一份状态。** 读写看的是同一个缓冲，不用在两个对象之间同步，也不用拷贝。
- **类型把能力切开。** `TCPSender` 拿 `Reader&` 取字节，`TCPReceiver` 拿 `Writer&` 写入。签名上不能把 `push` 和 `peek` 调反。
- **不变量集中。** 容量、`error_`、`closed_`、已读写计数都在基类。容量限制的是还没读走的字节，不是整条流的总长度。
- **零额外对象。** 没有 vptr，也没有外包一层 `Reader` 对象。课程用 `static_assert` 把“别往子类加成员”变成编译期检查。

## 不要当成一般写法

把基类对象 `static_cast` 成派生类，只有对象的动态类型真的是那个派生类时才有定义。这里实际构造的是 `ByteStream`，不是 `Reader`。能工作，是因为空子类、无虚函数、地址和布局相同。这是实验用的零开销技巧，不是可以随手复制的安全向下转型。

子类不能加成员，两边的引用也不是线程隔离，只是类型隔离。拿到 `ByteStream&` 的人仍然能碰到 `protected` 状态。

要同样的好处、又要严格合法，用组合：`Reader` / `Writer` 持有 `ByteStream&`，方法转发过去。多两个视图对象，但没有未定义行为。Go 的 `io.Reader` / `io.Writer` 是更干净的版本：一个具体类型同时满足两个接口，调用方只接收需要的那一个，没有继承，也没有向下转。
