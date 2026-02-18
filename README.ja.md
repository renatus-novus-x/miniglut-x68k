[English](README.md) | [日本語](README.ja.md)

[![windows](https://github.com/renatus-novus-x/miniglut-x68k/workflows/windows/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Awindows)
[![macos](https://github.com/renatus-novus-x/miniglut-x68k/workflows/macos/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Amacos)
[![ubuntu](https://github.com/renatus-novus-x/miniglut-x68k/workflows/ubuntu/badge.svg)](https://github.com/renatus-novus-x/miniglut-x68k/actions?query=workflow%3Aubuntu)

![demo](images/tether.gif)

miniglut-x68k (experimental)
============================

X68000（elf2x68k / Human68k）向けの、超小型 GLUT 互換(風の) サブセットです。

freeglut の移植ではありません。ウィンドウシステムはなく、フルスクリーン 1画面のみです。
最小イベントループと、GVRAM へ直接描画する OpenGL 風（immediate-mode）の API を提供します。

目的
----
- X68000 上で「GLUT サンプルっぽいコード」を最小限の修正で動かす足場を作る
- まずはワイヤフレームと簡単なフラットシェーディングから始め、段階的に機能追加する
- 実機/エミュ上で追いやすい小さなコードベースにする

実装している GLUT API（コア）
------------------------------
- glutInit
- glutCreateWindow
- glutDisplayFunc
- glutKeyboardFunc
- glutIdleFunc
- glutMainLoop

互換ヘルパ（GLUT サンプルでよく使われるもの）
----------------------------------------------
- glutInitDisplayMode（GLUT_SINGLE / GLUT_DOUBLE）
- glutInitWindowSize
- glutSwapBuffers
- glutPostRedisplay

画面モードとバッファ
--------------------
- glutInitWindowSize() の指定から、512x512 または 256x256 を実行時に選択します。
  256/512 以外が指定された場合は、近い方に丸めます。
- ダブルバッファは、glutInitDisplayMode() に GLUT_DOUBLE を指定した場合にのみ有効です。
  - 512x512: RAM バックバッファへ描画し、swap 時に GVRAM へコピーして表示
  - 256x256: 可能なら IOCS HOME を用いた 256x256 タイル切替で高速に表示
- 終了時に元の画面モードへ復帰し、テキストカーソルも復帰します。

OpenGL 風サブセット（最小の固定パイプライン風）
----------------------------------------------
include/GL にヘッダがあります。OpenGL の完全実装ではなく、最小サブセットです。

- クリア: glClearColor, glClear
- 行列: glMatrixMode, glLoadIdentity, glPushMatrix, glPopMatrix
- 変換: glTranslatef, glRotatef, glScalef, glOrtho
- 即時モード: glBegin/glEnd（GL_LINES, GL_TRIANGLES）, glVertex3f
- 色: glColor3ub
- 互換スイッチ（最小実装）:
  - 三角形のフラットシェーディングは以下のときに有効です:
    - glShadeModel(GL_FLAT)
    - glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
  - ワイヤフレームは以下で選択します:
    - glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
- GLU 風: gluPerspective のみ

ファイル構成
------------
- include/GL/glut.h : GLUT サブセット
- include/GL/gl.h   : OpenGL 風サブセット
- include/GL/glu.h  : GLU 風サブセット（gluPerspective のみ）
- src/miniglut.c    : GLUT 実装
- src/minigl.c      : ソフトウェア描画と X68000 の初期化/復帰処理
- src/demo_wirecube.c : デモ（ワイヤフレームのキューブが回転します）
- src/demo_objflat.c  : デモ（.obj 読込、ワイヤフレーム/フラットシェーディング切替）

ビルド（elf2x68k）
-----------------
必要なもの:
- m68k-xelf-gcc（elf2x68k toolchain）が PATH にあること

ビルド:
- make

生成物:
- demo_wirecube.x
- demo_objflat.x

デモ
----
demo_wirecube
- 実行: demo_wirecube.x
- キー:
  - ESC: 終了

demo_objflat
- 実行:
  - demo_objflat.x model.obj
  - 例: demo_objflat.x bin/bunny.obj
- キー:
  - t または Space: フラットシェーディング <-> ワイヤフレーム切替('t'oggle)
  - f: フラットシェーディング('f'lat shading)
  - w: ワイヤフレーム('w'ire frame)
  - ESC: 終了

注意点 / 制限
-------------
- テクスチャなし、αブレンディングなし、Z バッファなし。
- demo_objflat の「隠面消去っぽい見た目」は、三角形の簡易ソート（Painter’s algorithm）によるものです。
  複雑形状や自己交差メッシュでは破綻する場合があります。
- クリッピングは簡易（主に bounds check）。
- reshape / mouse / menu などは未実装です。

ダウンロード（ビルド済み）
--------------------------
- [demo_wirecube.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_wirecube.x)
- [demo_objflat.x](https://raw.githubusercontent.com/renatus-novus-x/miniglut-x68k/main/bin/demo_objflat.x)
