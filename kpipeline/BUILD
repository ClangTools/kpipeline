package(default_visibility = ["//visibility:public"])

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

# 图库
cc_library(
    name = "graph",
    hdrs = ["graph.h"],
    includes = ["."],
    deps = [
        ":node",
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
        ":node_factory",
    ],
)
