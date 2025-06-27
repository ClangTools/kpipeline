package(default_visibility = ["//visibility:public"])

cc_library(
    name = "logger",
    srcs = ["logger.cc"],
    hdrs = ["logger.h"],
    includes = ["."],
    linkopts = ["-pthread"],
    deps = [
        "@fmt",
    ],
)

# workspace 库
cc_library(
    name = "workspace",
    hdrs = ["workspace.h"],
    includes = ["."],
)

# 线程池库
cc_library(
    name = "thread_pool",
    hdrs = ["thread_pool.h"],
    includes = ["."],
    linkopts = select({
        "@platforms//os:linux": ["-lpthread"],
        "//conditions:default": [],
    }),
)

# 节点库
cc_library(
    name = "node",
    srcs = ["node.cc"],
    hdrs = ["node.h"],
    includes = ["."],
    deps = [
        ":workspace",
        "@jsoncpp",
    ],
)

# 节点工厂库
cc_library(
    name = "node_factory",
    srcs = ["node_factory.cc"],
    hdrs = ["node_factory.h"],
    includes = ["."],
    deps = [
        ":node",
        "@jsoncpp",
    ],
)

cc_library(
    name = "profiler",
    hdrs = ["profiler.h"],
)

# 图库
cc_library(
    name = "graph",
    hdrs = ["graph.h"],
    includes = ["."],
    deps = [
        ":logger",
        ":node",
        ":profiler",
        ":thread_pool",
    ],
)

# 图构建器库
cc_library(
    name = "graph_builder",
    hdrs = ["graph_builder.h"],
    includes = ["."],
    deps = [
        ":graph",
        ":logger",
        ":node_factory",
    ],
)
