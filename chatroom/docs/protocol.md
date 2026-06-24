# Chatroom Protocol

外层帧格式：

```text
4-byte body length, network byte order
body bytes
```

内层 body 是 KV 文本：

```text
seq=1
type=SEND
room_id=1
content=hello%20world

```

空行结束。值使用 percent-encoding。

## 通用响应

```text
seq=1
ok=1
code=OK
message=...

```

错误响应：

```text
seq=1
ok=0
code=BAD_REQUEST
message=content%20is%20required

```

## 命令

### REGISTER

```text
seq=1
type=REGISTER
name=alice
password=secret

```

### LOGIN

```text
seq=2
type=LOGIN
name=alice
password=secret

```

成功返回 `token`。

### AUTH

```text
seq=3
type=AUTH
token=...

```

### JOIN

```text
seq=4
type=JOIN
room_id=1

```

### SEND

```text
seq=5
type=SEND
room_id=1
content=hello

```

在线房间成员会收到：

```text
type=ROOM_MESSAGE
message_id=1
room_id=1
from_user=1
from_name=alice
content=hello

```

### DM

```text
seq=6
type=DM
to=bob
content=private%20hello

```

目标在线时收到：

```text
type=DM_MESSAGE
message_id=2
from_user=1
from_name=alice
content=private%20hello

```

### HISTORY

```text
seq=7
type=HISTORY
room_id=1
limit=20

```

