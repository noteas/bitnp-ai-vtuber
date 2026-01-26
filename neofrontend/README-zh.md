# 编译说明

## 前置条件

- [Godot 4.5+](https://godotengine.org)
- C++ 编译器（如MSVC、MinGW、g++、Clang等）
- [SCons](https://scons.org)

## 编译过程

在neofrontend目录下运行命令`scons platform=windows|linux|macos custom_api_file=extension_api.json`即可编译项目，在bin目录下即可找到编译好的动态库。

Release构建可以添加`target=template_release`参数。

## 运行/测试

用Godot打开项目，在节点类型列表中即可找到C++代码新建的类型，新建一个节点即可测试。
