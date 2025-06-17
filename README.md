# C++ Pipeline Framework

[English Version](#english-version)

一个现代、灵活、可配置、并行的C++流水线（计算图）框架。

该项目旨在提供一个简单的基础平台，用于构建复杂的数据处理流程，例如图像处理、数据ETL、机器学习特征工程等。

## 核心特性

-   **基于图的架构 (DAG)**: 支持任意复杂度的多分枝、多合并流水线，而不仅仅是线性流程。
-   **配置驱动**: 通过修改 `JSON` 配置文件即可改变整个数据处理流程，无需重新编译代码。
-   **可扩展的节点系统**: 通过简单的类继承和注册宏，可以轻松添加新的处理功能模块。
-   **并行执行**: 自动利用线程池并行化执行图中无依赖关系的分支，充分利用多核CPU资源。
-   **类型安全与线程安全**: `Workspace` 在节点间安全地传递任意类型的数据，并保证并发访问的线程安全。
-   **健壮的错误处理**: 在图执行和节点执行层面都加入了异常处理，能清晰地报告错误。
-   **现代化的构建系统**: 使用 Bazel 和 Google Test 进行项目管理和单元测试。

## 项目结构

```
.
├── BUILD.bazel
├── MODULE.bazel
├── README.md
├── examples
│   ├── BUILD
│   ├── MODULE.bazel
│   ├── image_pipeline.json
│   ├── main.cc
│   ├── main.image.cc
│   └── pipeline.json
└── kpipeline
    ├── BUILD
    ├── graph.h
    ├── graph_builder.h
    ├── node.cc
    ├── node.h
    ├── node_factory.cc
    ├── node_factory.h
    ├── test
    │   ├── BUILD.bazel
    │   ├── graph_test.cc
    │   └── workspace_test.cc
    ├── thread_pool.h
    └── workspace.h

```

## 快速开始

### 环境要求

-   [Bazel](https://bazel.build/install) (推荐版本 7.0 或更高)
-   一个支持 C++17 的编译器 (如 GCC 9+, Clang 10+)

### 构建项目

克隆仓库后，在项目根目录运行以下命令来构建所有目标，包括库、示例和测试：

```bash
bazel build //...
```
