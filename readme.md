
# DanmakuServer

A Windows-Composition-Based desktop danmaku renderer, providing a simple 
HTTP Restful API for clients to send danmaku messages.

Rendering result:

* Topmost, click-through for mouse. You can use it with any preferred
  video player.
* White danmaku text with `Microsoft Yahei` font, 28dip size and
  5dip-radius blured shadow.
* Right to left *linear* danmaku marquee animation with 10s duration.

Performance:

* Total WS consumption maybe quite large on some machine, but Private WS is
  just ~28MB.
* Animation may flicker, because of the limitation of current implementation
  and Windows Composition itself.
* High-GPU occupation on `dwm.exe`, nearly inevitable. When your GPU is not
  powerful enough, you may experience **lag** on **the whole Windows Shell Experience**.

Best experience when used with:

- Windows 11
- https://github.com/iceking2nd/real-url (with the git-patch below)
- Daum PotPlayer

```diff
diff --git a/danmu/main.py b/danmu/main.py
index be295d3..eb1ac84 100644
--- a/danmu/main.py
+++ b/danmu/main.py
@@ -4,12 +4,25 @@
 
 import asyncio
 import danmaku
-
+import requests
+
+def send_to_danmaku_server(name, content):
+    PORT = 7654
+    try:
+        resps = requests.get(f'http://localhost:{PORT}/danmaku', params={
+            'name': name,
+            'content': content
+        }).json()
+        if resps['code'] != 0:
+            print('[DanmakuServer] error: ', resps['message'])
+    except:
+        pass

 async def printer(q):
     while True:
         m = await q.get()
         if m['msg_type'] == 'danmaku':
+            send_to_danmaku_server(m['name'], m['content'])
             print(f'{m["name"]}：{m["content"]}')


```

---

## Screenshots

[2023-08-04 13-11-39.webm](https://github.com/seven-mile/DanmakuServer/assets/56445491/649419b3-dcdf-4eda-8dc4-35167b7fd4a0)

## Known Issues

> [!WARNING]
> 
> Due to some limitation of [Windows Notify Icon GUID Registry](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa#troubleshooting),
>   the icon may not be able to be shown.
>   
>   | | Windows 10 | Windows 11 |
>   |---|---|---|
>   | MSIX Packaged | Cannot update | Perfect |
>   | Non Packaged | Cannot move path | Cannot move path |
> 
> * If you are using **Windows 10**, 
> 
>   * update MSIX-packaged installations, or
>   * move the path of non-packaged installations
> 
> * If you are using **Windows 11**, the second situation still exists, but the combination of Win11 + MSIX Package works perfectly, at least on my machine.
>   However, I cannot find any official document about this, so I'm not sure if it's a bug or a feature.
> 
> If you encounter such issue, you can try [this workaround](https://support.microsoft.com/en-gb/topic/system-icons-do-not-appear-in-the-notification-area-in-windows-vista-or-in-windows-7-until-you-restart-the-computer-eed17e13-f80a-fde3-39de-2adfc94d56e1)
> to reset your notification settings.

## Implementation

Thanks to:

* [Windows Composition](https://docs.microsoft.com/en-us/windows/uwp/composition/)
  * Windows Composition means `Windows.UI.Composition` API from WinRT.
  * It's a GPU-accelerated UI rendering engine, which is provided by DWM.
  * The interface is mostly like a 2D graphics library, but bitmap only.
  * Like every window manager, the compositor is responsible for the rendering
	of layers of windows on the screen. But for Windows Composition, it also
	performs extra works like animations and visual effects.
  * These features makes it a good choice for rendering danmaku.
* [ADeltaX's reverse-engineering efforts on Composition Interop](https://blog.adeltax.com/interopcompositor-and-coredispatcher/)
  * This interop is between `Windows.UI.Composition` and Direct Composition.
  * This is only responsible for creating a WinRT Compositor for a classic Win32 HWND.
* [Composition Interop with DX and D2D](https://learn.microsoft.com/en-us/windows/uwp/composition/composition-native-interop#cwinrt-usage-example)
  * This interop is between `Windows.UI.Composition` and D2D and DWrite.
  * Because Windows Composition only supports bitmap, we have to render text
    by ourselves. After rendering text to a bitmap, we can use this interop
	to create a `SurfaceBrush`, and then use it under `SpriteVisual` with `DropShadow`.

This project uses C++/WinRT and C++ 20 features, and is compiled with MSVC 19.37.32820.

Tested under 

* Build 22621 with NVIDIA GeForce RTX 3080 Ti.
* Build 25915 with Intel(R) UHD Graphics 630.
